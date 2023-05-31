#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "config.h"

moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

namespace moka {

// 5s超时
static moka::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    moka::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");
// 线程局部变量记录当前线程是否hook
static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
  XX(sleep) \
  XX(usleep) \
  XX(nanosleep) \
  XX(socket) \
  XX(connect) \
  XX(accept) \
  XX(read) \
  XX(readv) \
  XX(recv) \
  XX(recvfrom) \
  XX(recvmsg) \
  XX(write) \
  XX(writev) \
  XX(send) \
  XX(sendto) \
  XX(sendmsg) \
  XX(close) \
  XX(fcntl) \
  XX(ioctl) \
  XX(getsockopt) \
  XX(setsockopt)


void hook_init() {
  static bool is_inited = false;
  if (is_inited) {
    return;
  }

// disym在动态链接符号表中找到HOOK_FUN中的库函数的地址，并存入各自_f后缀的变量中
// 在宏定义的范围中展开另一个宏
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HookIniter {
  _HookIniter() {
    hook_init();
    s_connect_timeout = g_tcp_connect_timeout->get_value();
    g_tcp_connect_timeout->addListener(111, [](const int& old_val, const int& new_val) {
      MOKA_LOG_INFO(g_logger) << "tcp connect timeout changed from "
                              << old_val << " to " << new_val;
      // 每次修改时都更新该配置变量
      s_connect_timeout = new_val;
    });
  }
};

// 在main函数前初始化HookIniter
static _HookIniter s_hook_initer;

bool is_hook_enable() {
  return moka::t_hook_enable;
}

void set_hook_enable(bool flag) {
  moka::t_hook_enable = flag;
}

// 条件定时器的条件
struct TimerInfo {
  int cancelled = 0;
};

// hook通用读写函数(自动推导出函数类型)，函数模板可变参数
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
        uint32_t event, int timeout_so, Args&&... args) {
  if (!moka::t_hook_enable) {
    // forward完美转发保留参数左右值属性(引用折叠)
    return fun(fd, std::forward<Args>(args)...);
  }
  moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(fd);
  if (!ctx) {
    // 当前fd不在信息集合中
    return fun(fd, std::forward<Args>(args)...);
  }
  if (ctx->isClosed()) {
    // 要进行IO操作的fd已经被关闭
    // 设置error number
    errno = EBADF;  // bad file number
    return -1;
  }

  if (!ctx->isSocket() || ctx->get_user_nonblock()) {
    // 用户已经设置了非阻塞或者不是套接字，也不需要hook
    return fun(fd, std::forward<Args>(args)...);
  }
  // 获取fd的超时时间
  uint64_t timeout = ctx->get_timeout(timeout_so);

  // 条件定时器的条件。如果addEvent出错退出了，这时局部智能指针变量析构，条件也不存在了
  // 那么条件定时器的回调函数则也会退出
  std::shared_ptr<TimerInfo> t_info(new TimerInfo);

retry:
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR)  {
    // 表示被信号中断，循环不断重试
    n = fun(fd, std::forward<Args>(args)...);
  }
  if (n == -1 && errno == EAGAIN) {
    // 阻塞状态等待数据(如没有数据可以read或者没有数据可写，需要做异步操作)
    moka::IOManager* iom = moka::IOManager::GetThis();
    // 创建定时器
    moka::Timer::ptr timer;
    // 加定时条件
    std::weak_ptr<TimerInfo> w_info(t_info);
    if (timeout != (uint64_t)(-1)) {
      // 如果存在超时时间(比如read需要读timeout秒)
      // 往定时器堆中添加条件定时器，并返回
      timer = iom->addConditionalTimer(timeout, [w_info, iom, event, fd]() {
        // 获取weak_ptr的对象转换为shared_ptr
        auto t = w_info.lock();
        if (!t || t->cancelled) {
          // 当条件不存在或者cancelled已经被设置
          return;
        }
        t->cancelled = ETIMEDOUT;  // 设置成连接/操作超时
        // 取消该事件(强制唤醒)，逻辑设置由当前协程来执行事件回调函数
        iom->cancelEvent(fd, (moka::IOManager::Event)(event));
      }, w_info);
    }

    // 再注册读写事件到epoll内核事件表中(在一段时间后定时器事件会触发，通过cancelEvent来强制执行)
    // 没有第三个参数，则默认使用当前协程来执行该事件回调函数(保证唤醒时从YieldToHoldSched的下一行开始执行)
    int ret = iom->addEvent(fd, (moka::IOManager::Event)(event));
    if (ret == -1) {
      // 失败
      MOKA_LOG_INFO(g_logger) << hook_fun_name << "addEvent("
                              << fd << ", " << event << ")";
      // 取消定时器并退出
      if (timer) {
        timer->cancel();
      }
      return -1;
    } else {
      // 执行成功就yield，将调度权让给其他协程执行
      moka::Fiber::YieldToHoldSched();
      // 通过注册的读写事件/定时事件唤醒(定时事件会唤醒epoll_wait然后获取超时定时器的回调函数列表调度执行)
      // 之后关闭该定时器
      if (timer) {
        timer->cancel();
      }
      // 如果是操作超时则设置一下errno，直接退出(不可能会出现超时5s之后再超时5s的情况)
      if(t_info->cancelled) {
        errno = t_info->cancelled;
        return -1;
      }
      // IO操作又一次失败(如又阻塞了)则再次执行fun
      goto retry;
    }
  }
  return n;
}

}

extern "C" {
// 初始化
#define XX(name) name##_fun name##_f = nullptr;
  HOOK_FUN(XX)
#undef XX

// 重写同名的库函数/系统调用，实现异步功能
unsigned int sleep(unsigned int seconds) {
  if (!moka::t_hook_enable) {
    // 未hook，执行原libc中的库函数
    return sleep_f(seconds);
  }
  
  moka::Fiber::ptr fiber = moka::Fiber::GetThis();
  moka::IOManager* iom = moka::IOManager::GetThis();
  // 添加定时器，在seconds秒之后执行该库函数(实现在libc函数的基础上实现异步)
  // 该任务是由当前正在执行的协程来调度的
  iom->addTimer(seconds * 1000, [iom, fiber](){
    // seconds秒之后回调，当前执行sleep的协程获得执行权(从return 0处开始继续执行退出函数体)
    iom->schedule(fiber);
  });
  // 将当前执行权转移给调度协程(因为当前协程执行sleep会发生阻塞)
  moka::Fiber::YieldToHoldSched();
  return 0;
}

int usleep(useconds_t usec) {
  if (!moka::t_hook_enable) {
    return usleep_f(usec);
  }
  moka::Fiber::ptr fiber = moka::Fiber::GetThis();
  moka::IOManager* iom = moka::IOManager::GetThis();
  iom->addTimer(usec / 1000, [iom, fiber](){
    iom->schedule(fiber);
  });
  moka::Fiber::YieldToHoldSched();
  return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  if (!moka::t_hook_enable) {
    return nanosleep_f(req, rem);
  }
  int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
  moka::Fiber::ptr fiber = moka::Fiber::GetThis();
  moka::IOManager* iom = moka::IOManager::GetThis();
  iom->addTimer(timeout_ms, [iom, fiber](){
    iom->schedule(fiber);
  });
  moka::Fiber::YieldToHoldSched();
  return 0;
}

int socket(int domain, int type, int protocol) {
  if (!moka::t_hook_enable) {
    return socket_f(domain, type, protocol);
  }
  int fd = socket_f(domain, type, protocol);
  if (fd == -1) {
    // 错误
    return -1;
  }
  // hook住socket时，在新建sockfd时，创建其fd信息并加入信息集合中
  moka::FdMgr::GetInstance()->get(fd, true);
  return fd;
}

int connect_with_timeout(int sockfd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
  if (!moka::t_hook_enable) {
    return connect_f(sockfd, addr, addrlen);
  }
  moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(sockfd);
  if (!ctx || ctx->isClosed()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->isSocket()) {
    // 实际上对普通fd connect会返回错误
    return connect_f(sockfd, addr, addrlen);
  }
  if (ctx->get_user_nonblock()) {
    // 如果用户已经设置过非阻塞(已经有异步的效果)
    return connect_f(sockfd, addr, addrlen);
  }
  int ret = connect_f(sockfd, addr, addrlen);
  if (ret == 0) {
    return 0;
  } else if (ret == -1 || errno != EINPROGRESS) {
    // EINPROGRES(表示fd实际上已经连接了，但fd为非阻塞)
    return ret;
  }
  moka::IOManager* iom = moka::IOManager::GetThis();
  moka::Timer::ptr timer;
  std::shared_ptr<moka::TimerInfo> t_info(new moka::TimerInfo);
  std::weak_ptr<moka::TimerInfo> w_info(t_info);
  
  if (timeout_ms != (uint64_t)(-1)) {
    // 超时
    timer = iom->addConditionalTimer(timeout_ms, [w_info, sockfd, iom]{
      auto t = w_info.lock();
      if (!t || t->cancelled) {
        return;
      }
      // ETIMEDOUT表示连接超时
      t->cancelled = ETIMEDOUT;
      iom->cancelEvent(sockfd, moka::IOManager::WRITE);
    }, w_info);
  }

  // WRITE事件connect上了就可写，就会马上触发
  ret = iom->addEvent(sockfd, moka::IOManager::WRITE);
  if (ret == 0) {
    // 成功
    moka::Fiber::YieldToHoldSched();
    if (timer) {
      timer->cancel();
    }
    if (t_info->cancelled) {
      errno = t_info->cancelled;
      return -1;
    }
  } else {
    if (timer) {
      timer->cancel();
    }
    MOKA_LOG_ERROR(g_logger) << "connect addEvent(" << sockfd << ", WRITE) error";
  }
  int error = 0;
  socklen_t len = sizeof(int);
  // 失败时获取最近一次套接字操作的错误代码
  if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
    // error
    return -1;
  }
  if (!error) {
    return 0;
  } else {
    errno = error;
    return -1;
  }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  return connect_with_timeout(sockfd, addr, addrlen, moka::s_connect_timeout);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  // 监听读事件
  int fd = moka::do_io(sockfd, accept_f, "accept", moka::IOManager::Event::READ, SO_RCVTIMEO, addr, addrlen);
  if (fd >= 0) {
    // 将连接套接字放入信息集合中
    moka::FdMgr::GetInstance()->get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
  return do_io(fd, read_f, "read", moka::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, readv_f, "readv", moka::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  return do_io(sockfd, recv_f, "recv", moka::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
  return do_io(sockfd, recvfrom_f, "recvfrom", moka::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  return do_io(sockfd, recvmsg_f, "recvmsg", moka::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return do_io(fd, write_f, "write", moka::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  return do_io(fd, writev_f, "writev", moka::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
  return do_io(sockfd, send_f, "send", moka::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
  return do_io(sockfd, sendto_f, "sendto", moka::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  return do_io(sockfd, sendmsg_f, "sendmsg", moka::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
  if (!moka::t_hook_enable) {
    return close_f(fd);
  }
  // 先把事件取消再关闭fd
  moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(fd);
  if (ctx) {
    auto iom = moka::IOManager::GetThis();
    if (iom) {
      // 再关闭fd之前强制触发fd上所有的读写事件
      iom->cancelAll(fd);
    }
    // 从fd信息集合中删除掉fd相关的信息
    moka::FdMgr::GetInstance()->del(fd);
  }
  return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
  va_list va;  // 存储可变参数列表
  va_start(va, cmd);
  switch (cmd) {
    case F_SETFL: {
      int arg = va_arg(va, int);
      va_end(va);
      moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(fd);
      if (!ctx || ctx->isClosed()) {
        // ctx为空说明该信息上下文集合中没有该fd(不参与hook)
        return fcntl_f(fd, cmd, arg);
      }
      // 保存用户设置的该状态标志
      ctx->set_user_nonblock(arg & O_NONBLOCK);  // 从可变参数中获取用户是否通过fcntl设置了O_NONBLOCK

      // 根据系统设置的来决定
      if (ctx->get_sys_nonblock()) {
        arg |= O_NONBLOCK;
      } else {
        arg &= ~O_NONBLOCK;
      }
      return fcntl_f(fd, cmd, arg);
    }
    case F_GETFL: {
      va_end(va);
      int arg = fcntl_f(fd, cmd);  // 取出fd的状态标志
      moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(fd);
      if (!ctx || ctx->isClosed() || !ctx->isSocket()) {
        return arg;
      }
      // 即便hook设置该fd为非阻塞，也根据用户当前fd的状态返回对应的状态标志
      // 抽象，保证返回用户初始时设置的状态标志
      if (ctx->get_user_nonblock()) {
        return arg | O_NONBLOCK;
      } else {
        return arg & ~O_NONBLOCK;
      }
    }
    // int
    case F_DUPFD:
    case F_DUPFD_CLOEXEC: 
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
    case F_SETPIPE_SZ:
    case F_ADD_SEALS: {
      int arg = va_arg(va, int);  // va_arg获取可变参数列表中指针指向的int型(指定类型)参数并返回
      va_end(va);   // 释放可变参数列表
      return fcntl_f(fd, cmd, arg);
    }
    break;
    // void
    case F_GETFD:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
    case F_GETPIPE_SZ:
    case F_GET_SEALS: {
      va_end(va);
      return fcntl_f(fd, cmd);
    }
    break;

    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {
      struct flock* arg = va_arg(va, struct flock*);
      va_end(va);
      return fcntl_f(fd, cmd, arg);
    }
    break;

    case F_GETOWN_EX:
    case F_SETOWN_EX: {
      struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
      va_end(va);
      return fcntl_f(fd, cmd, arg);
    }
    break;
    default: {
      va_end(va);
      return fcntl_f(fd, cmd);
    }
  }
  return 0;
}

int ioctl(int fd, unsigned long request, ...) {
  va_list va;
  va_start(va, request);
  // 获取第一个可变参数转换为void指针
  void* arg = va_arg(va, void*);
  va_end(va);

  if (FIONBIO == request) {
    // 第三个参数值为0表示禁用非阻塞模式
    bool user_nonblock = !!*((int*)arg);
    moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(fd);
    if (!ctx || ctx->isClosed() || !ctx->isSocket()) {
      return ioctl_f(fd, request, arg);
    }
    // 用户设置为非阻塞(获取用户设置的状态)
    ctx->set_user_nonblock(user_nonblock);
  }
  return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
  // 不需要hook
  return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
  if (!moka::t_hook_enable) {
    return setsockopt_f(sockfd, level, optname, optval, optlen);
  }
  // 处理fd设置超时时间(I/O操作时会有影响)
  if (level == SOL_SOCKET) {
    if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
      moka::FdCtx::ptr ctx = moka::FdMgr::GetInstance()->get(sockfd);
      if (ctx) {
        const timeval* tv = (const timeval*)optval;
        ctx->set_timeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
      }
    }
  }
  return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}

// 外部函数指针变量的声明(一定不要放到moka的命名空间中，否则链接时找不到)
#define XX(name) extern name##_fun name##_f \
  HOOK_FUN(XX)
#undef XX