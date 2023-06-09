#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include "iomanager.h"
#include "macro.h"
#include "log.h"

namespace moka {

static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

IOManager::IOManager(size_t thread_nums, bool use_caller, const std::string& name) 
    : Scheduler(thread_nums, use_caller, name) {
  // 使用epoll_wait监听管道读端(调用notify会往管道中写入1字节数据唤醒)
  epfd_ = epoll_create(5);   // 创建epoll的实例，返回epfd，size参数2.6以后就被忽略了
  MOKA_ASSERT(epfd_ >= 0);

  // 创建管道
  int ret = pipe(notify_fds_);
  MOKA_ASSERT(!ret);

  epoll_event event;
  bzero(&event, sizeof(epoll_event));
  // 初始化epoll事件
  event.events = EPOLLIN | EPOLLET;
  // 初始化用户数据，检测管道读端的io事件
  event.data.fd = notify_fds_[0];     

  // 设置管道读端为非阻塞(配合边沿触发)
  ret = fcntl(notify_fds_[0], F_SETFL, O_NONBLOCK);
  MOKA_ASSERT(ret != -1);

  // 往epoll内核事件表中插入管道读端以及其相关的事件
  ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, notify_fds_[0], &event);
  MOKA_ASSERT(!ret);

  // 初始化socket事件的上下文容器
  contextResize(32);

  start();   // 开始调度
}

IOManager::~IOManager() {
  stop();    // 停止调度器
  close(epfd_);
  close(notify_fds_[0]);
  close(notify_fds_[1]);
  // 释放堆空间
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (fd_contexts_[i]) {
      delete fd_contexts_[i];
    }
  }
}

// 0 success, -1 error
// 没有第三个参数，则表示添加的事件对应的任务，为当前协程(而不是函数)，并放入事件上下文
int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
  // 将文件描述符对应的事件加入到epoll内核事件表中(通过自定义的fd上下文，指针存储在data联合体中)
  RWmutex::ReadLock lock(mutex_);
  FdContext* fd_ctx = nullptr;
  // 获取fd对应的fd上下文结构体对象
  if ((int)fd_contexts_.size() > fd) {
    // 直接从fd上下文集合中获取
    fd_ctx = fd_contexts_[fd];
    lock.unlock();  //释放读锁
  } else {
    // 扩容，之后再获取
    lock.unlock();  // 释放读锁
    RWmutex::WriteLock lock2(mutex_);  // 加写锁
    contextResize(fd * 1.5);
    fd_ctx = fd_contexts_[fd];
  }
  // 对fd上下文加锁
  Mutex::LockGuard lock2(fd_ctx->mutex);
  if (fd_ctx->events & event) {
    // epoll监听集合中已经存在该event，意味着可能有两个不同线程在操作该方法
    MOKA_LOG_ERROR(g_logger) << "addEvenet assert fd=" << fd
        << " event=" << event
        << "fd_ctx.event=" << fd_ctx->events;
    MOKA_ASSERT(!(fd_ctx->events & event));
  }

  int op = fd_ctx->events? EPOLL_CTL_MOD: EPOLL_CTL_ADD;
  epoll_event epevent;
  // 将对应的fd上下文的指针和事件类型存储到epoll事件结构体中
  // 如果该事件发生则从对应的epoll事件结构体中将它取出
  epevent.events = EPOLLET | fd_ctx->events | event;
  // 数据
  // epoll_data_t是一个联合体
  epevent.data.ptr = fd_ctx;
  // 更新epoll内核事件表
  int ret = epoll_ctl(epfd_, op, fd, &epevent);
  if (ret) {
    MOKA_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", "
                             << op << ", " << fd << ", " << epevent.events << "):"
                             << ret << " (" << errno << ") (" << strerror(errno) << ")";
    return -1;
  }
  // 待执行的事件数量
  ++pending_event_counts_;
  // 初始化fd上下文结构体对象
  // 更新当前fd上下文
  fd_ctx->events = (Event)(fd_ctx->events | event);
  // 获取当前fd事件的事件上下文(注意是引用)
  FdContext::EventContext& event_ctx = fd_ctx->get_context(event);
  // 确保当前fd事件的上下文是重置的状态
  MOKA_ASSERT(!(event_ctx.scheduler || event_ctx.fiber || event_ctx.cb));
  // 初始化事件上下文
  event_ctx.scheduler = Scheduler::GetThis();  // 初始化事件的调度器
  if (cb) {
    event_ctx.cb = std::move(cb);
  } else {
    // 将该协程作为任务协程(执行事件回调函数)
    event_ctx.fiber = Fiber::GetThis();
    MOKA_ASSERT(event_ctx.fiber->get_state() == Fiber::state::EXEC);
  }
  return 0;
}

int IOManager::delEvent(int fd, Event event) {
  RWmutex::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    return -1;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();
  Mutex::LockGuard lock_guard(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    // 没有该事件
    return -1;
  }
  Event new_events = (Event)(fd_ctx->events & ~event);  // 更新fd的event事件
  int op = new_events? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = new_events | EPOLLET;
  epevent.data.ptr = fd_ctx;
  
  int ret = epoll_ctl(epfd_, op, fd, &epevent);
  if (!ret) {
    MOKA_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", "
                             << op << ", " << fd << ", " << epevent.events << "):"
                             << ret << " (" << errno << ") (" << strerror(errno) << ")";
    return -1;
  }
  --pending_event_counts_;

  fd_ctx->events = new_events;
  // 重置该事件的上下文
  fd_ctx->resetContext(fd_ctx->get_context(event));
  return 0;
}

int IOManager::cancelEvent(int fd, Event event) {
  RWmutex::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    return -1;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();
  Mutex::LockGuard lock_guard(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    // 没有该事件
    return -1;
  }
  // 将该事件从在epoll上注册的事件集合删除
  Event new_events = (Event)(fd_ctx->events & ~event);
  // 如果当前fd在epoll上还剩有监听的事件则为MOD操作
  int op = new_events? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = new_events | EPOLLET;
  epevent.data.ptr = fd_ctx;
  
  int ret = epoll_ctl(epfd_, op, fd, &epevent);
  if (ret == -1) {
    MOKA_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", "
                             << op << ", " << fd << ", " << epevent.events << "):"
                             << ret << " (" << errno << ") (" << strerror(errno) << ")";
    return -1;
  }
  // 获取当前fd事件的事件上下文
  FdContext::EventContext& event_ctx = fd_ctx->get_context(event);
  fd_ctx->trigger(event);  // 强制触发事件执行
  --pending_event_counts_;

  fd_ctx->events = new_events;      // 同步更新文件描述符上下文的属性
  fd_ctx->resetContext(event_ctx);
  return 0;
}

int IOManager::cancelAll(int fd) {
  RWmutex::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    return -1;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();
  Mutex::LockGuard lock_guard(fd_ctx->mutex);
  if (!(fd_ctx->events)) {
    // 不存在监听的事件
    return -1;
  }
  int op = EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = 0;
  epevent.data.ptr = fd_ctx;
  
  // 从epoll内核事件表中删除fd的所有事件
  int ret = epoll_ctl(epfd_, op, fd, &epevent);
  if (ret == -1) {
    MOKA_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", "
                             << op << ", " << fd << ", " << epevent.events << "):"
                             << ret << " (" << errno << ") (" << strerror(errno) << ")";
    return -1;
  }
  if (fd_ctx->events & READ) {
    fd_ctx->trigger(READ);     // 强制触发事件的回调函数执行
    --pending_event_counts_;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->trigger(WRITE);
    --pending_event_counts_;
  }
  // 验证是否处理了fd的所有事件(trigger后会更改fd上下文的注册事件集合)
  MOKA_ASSERT(fd_ctx->events == NONE);
  // 清空读和写事件的上下文
  fd_ctx->resetContext(fd_ctx->read);
  fd_ctx->resetContext(fd_ctx->write);
  return 0;
}

void IOManager::contextResize(size_t size) {
  fd_contexts_.resize(size);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    // 不存在的fd上下文才开辟堆空间
    if (!fd_contexts_[i]) {
      fd_contexts_[i] = new FdContext;
      fd_contexts_[i]->fd = i;
    }
  }
}

IOManager* IOManager::GetThis() {
  // dynamic_cast用于基类和派生类之间的转型
  return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

IOManager::FdContext::EventContext& IOManager::FdContext::get_context(Event event) {
  switch(event) {
    case IOManager::Event::READ:
      return read;
    case IOManager::Event::WRITE: 
      return write;
    default: 
      MOKA_ASSERT_2(false, "get_context");
  }
}

void IOManager::FdContext::resetContext(EventContext& ctx) {
  ctx.cb = nullptr;
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
}

void IOManager::FdContext::trigger(Event event) {
  // 确保fd注册过event事件
  MOKA_ASSERT(events & event);
  // 从当前注册的事件中删除trigger的事件
  events = (Event)(events & (~event));
  // 获取fd上下文的读/写事件上下文
  EventContext& event_ctx = get_context(event);  // 引用
  // 调度执行
  if (event_ctx.cb) {
    event_ctx.scheduler->schedule(&(event_ctx.cb));
  } else {
    event_ctx.scheduler->schedule(&(event_ctx.fiber));  // 传递智能指针的指针，这样原有的智能指针不需要reset了
  }
  event_ctx.scheduler = nullptr;
}

void IOManager::notify() {
  MOKA_LOG_INFO(g_logger) << "notify";
  if (!hasIdleThreads()) {
    // 如果没有空闲线程则不进行唤醒
    return;
  }
  int ret = write(notify_fds_[1], "T", 1);
  MOKA_ASSERT(ret == 1);
}

bool IOManager::stopping() {
  // 保证协程完成调度，同时IO事件也要完成调度
  uint64_t timeout = 0;
  return stopping(timeout);
}
  
bool IOManager::stopping(uint64_t& timeout) {
  timeout = get_expire();
  return timeout == UINT64_MAX &&
         pending_event_counts_ == 0 &&
         Scheduler::stopping();
}

void IOManager::idle() {
  MOKA_LOG_INFO(g_logger) << "idle";
  // 作为epoll_wait的传出epoll事件数组
  // TODO:epoll事件数组的大小需要进行调整
  epoll_event* events = new epoll_event[64];
  // 使用events来初始化shared_events，显式指定删除器
  // 保证在idle函数体执行结束后局部智能指针变量析构时会调用该删除器释放动态开辟的空间
  std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr) {
    delete[] ptr;
  });

  // while循环保证idle协程yield之后再sched时能过够继续从循环处开始执行
  while (true) {
    uint64_t next_timeout = 0;
    if (stopping(next_timeout)) {
      MOKA_LOG_INFO(g_logger) << "name=" << Scheduler::get_name() << " idle stopping exit";
      break;
    }

    int ret = 0;
    do {
      // epoll最大超时时间(毫秒级)
      static const int MAX_TIMEOUT = 3000;
      // 监听epoll事件数组(第二个参数作为out参数)，成功时返回就绪fd的个数
      if (next_timeout != UINT64_MAX) {
        // 有超时时间，取间隔短的那一个
        next_timeout = (int)next_timeout > MAX_TIMEOUT? MAX_TIMEOUT: next_timeout; 
      } else {
        next_timeout = MAX_TIMEOUT;
      }
      // epoll_wait超时返回(如果在等待时间内有事件发生，则立即返回处理)
      ret = epoll_wait(epfd_, events, 64, (int)next_timeout);
      // MOKA_LOG_DEBUG(g_logger) << "ret=" << ret;
      if (ret < 0 && errno == EINTR) {
      } else {
        break;
      }
    } while (true);

    std::vector<std::function<void()>> cbs;    
    // 获取已经超时的回调函数列表，并显式加入到调度器中进行调度
    listExpiredCb(cbs);
    if (!cbs.empty()) {
      schedule(cbs.begin(), cbs.end());
      cbs.clear();
    }

    // 对就绪的fd进行处理
    for (int i = 0; i < ret; ++i) {
      // 遍历就绪fd
      epoll_event& event = events[i];
      if (event.data.fd == notify_fds_[0]) {
         // 外部有消息notify的
        uint8_t dummy;
        // ET(要一直读到没有数据)
        while (read(notify_fds_[0], &dummy, 1) == 1);
        continue;
      }
      // 取出文件描述符的上下文
      FdContext* fd_ctx = static_cast<FdContext*>(event.data.ptr);
      Mutex::LockGuard lock(fd_ctx->mutex);
      if (event.events & (EPOLLERR | EPOLLHUP)) {
        // 错误或中断(也要进行读/写事件的触发)
        event.events |= EPOLLIN | EPOLLOUT;
      } 
      int real_events = NONE;
      if (event.events & EPOLLIN) {
        real_events |= READ;
      }
      if (event.events & EPOLLOUT) {
        real_events |= WRITE;
      }
      if ((fd_ctx->events & real_events) == NONE) {
        // 没有事件发生
        continue;
      }
      // 剩余事件(将处理的事件从fd对应的epoll内核事件表中删除)
      int left_events = (fd_ctx->events & (~real_events));
      int op = left_events? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
      // 复用epoll监听剩余事件，继续放入内核事件表中
      event.events = EPOLLET | left_events;
      int ret2 = epoll_ctl(epfd_, op, fd_ctx->fd, &event);
      if (ret2) {
        MOKA_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", "
                                << op << ", " << fd_ctx->fd << ", " << event.events << "):"
                                << ret << " (" << errno << ") (" << strerror(errno) << ")";
        continue;
      }
      // 这里可能两个事件都会同时触发
      if (real_events & READ) {
        fd_ctx->trigger(READ);
        --pending_event_counts_;
      }
      if (real_events & WRITE) {
        fd_ctx->trigger(WRITE);
        --pending_event_counts_;
      }
    }
    // 让出执行权给scheduler
    // 直接回到run事件循环中，在事件循环中被设置为HOLD状态(进入idle时还会被调度)
    Fiber::ptr cur = Fiber::GetThis();
    Fiber* row = cur.get();
    cur.reset();
    row->back();
  }
}

void IOManager::onTimerInsertedAtFront() {
  notify();  // 往管道中写触发管道读事件，epoll_wait立即从阻塞态返回
}

}