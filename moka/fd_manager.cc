#include <sys/stat.h>

#include "fd_manager.h"
#include "hook.h"

namespace moka {
FdCtx::FdCtx(int fd) : is_init_(false), is_socket_(false), is_sys_nonblock_(false), 
    is_user_nonblock_(false), is_closed_(false), fd_(fd), recv_timeout_(-1), send_timeout_(-1) {
    init();
}

FdCtx::~FdCtx() {
}

bool FdCtx::init() {
  if (is_init_) {
    // 不需要重复初始化
    return true;
  }
  // -1作为超时参数表示一致阻塞直到发生
  // 这里-1等价于(u_int64的最大值)
  recv_timeout_ = -1;
  send_timeout_ = -1;
  struct stat fd_stat;
  // 获取文件元数据
  if (fstat(fd_, &fd_stat) == -1) {
    // error
    is_init_ = false;
    is_socket_ = false;
  } else {
    is_init_ = true;
    // S_ISSOCK判断该fd是否为socket
    is_socket_ = S_ISSOCK(fd_stat.st_mode);
  }
  if (is_socket_) {
    // 调用原始fcntl，获取当前fd的标志
    int flags = fcntl_f(fd_, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      // 若当前fd是阻塞的，将其设置为非阻塞(系统设置的)
      fcntl_f(fd_, F_SETFL, flags | O_NONBLOCK);
    }
    is_sys_nonblock_ = true;
  } else {
    is_sys_nonblock_ = false;
  }
  is_user_nonblock_ = false;
  is_closed_ = false;
  return is_init_;
}

void FdCtx::set_timeout(int type, uint64_t val) {
  if (type == SO_RCVTIMEO) {
    recv_timeout_ = val;
  } else {
    send_timeout_ = val;
  }
}

uint64_t FdCtx::get_timeout(int type) {
  // SO_RCVTIMEO用于设置套接字接收的超时时间的选项
  if (type == SO_RCVTIMEO) {
    return recv_timeout_;
  } else {
    return send_timeout_;
  }
}

FdManager::FdManager() {
  // 初始化fd信息集合
  datas_.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
  // auto_create表示若当前fd信息不存在集合中，则创建fd信息对象放入集合中
  RWmutex::ReadLock lock(mutex_);
  if ((int)datas_.size() <= fd) {
    if (!auto_create) {
      return nullptr;
    } else {
      lock.unlock();
      RWmutex::WriteLock lock2(mutex_);
      // 扩容
      datas_.resize(fd * 1.5);
      FdCtx::ptr ctx(new FdCtx(fd));
      datas_[fd] = ctx;
    }
  } else {
    if (datas_[fd] == nullptr && auto_create) {
      lock.unlock();
      RWmutex::WriteLock lock2(mutex_);
      FdCtx::ptr ctx(new FdCtx(fd));
      datas_[fd] = ctx;
    }
  }
  return datas_[fd];
}

void FdManager::del(int fd) {
  RWmutex::WriteLock lock(mutex_);
  if ((int)datas_.size() <= fd) {
    // 不存在
    return;
  }
  // 否则将其重置(智能指针)
  datas_[fd].reset();
}

}