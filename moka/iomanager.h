#ifndef __MOKA_IOMANAGER_H__
#define __MOKA_IOMANAGER_H__

#include "scheduler.h"

namespace moka {

class IOManager: public Scheduler {
 public:
  using ptr = std::shared_ptr<IOManager>;

  // I/O事件类型，这里只关心读/写事件
  enum Event {
    NONE  = 0x0,
    READ  = 0x1,     // EPOLLIN
    WRITE = 0x4      // EPOLLOUT
  };

 private:
  // 文件描述符的上下文
  struct FdContext {
    // 事件的上下文
    struct EventContext {
      EventContext() {
        scheduler = nullptr;
        fiber = nullptr;
        cb = nullptr;
      }
      Scheduler* scheduler;        // 事件执行的调度器
      Fiber::ptr fiber;            // 事件协程
      std::function<void()> cb;    // 事件回调函数
    };
    EventContext& get_context(Event event); // 获取对应的事件上下文对象
    void resetContext(EventContext& ctx);   // 重置事件上下文
    void trigger(Event event);              // 触发事件

    int fd = 0;           // 事件关联的文件描述符
    EventContext read;    // 读事件
    EventContext write;   // 写事件
    Event events = NONE;  // 已注册的事件
    Mutex mutex;          // 互斥锁
  };

 public:
  IOManager(size_t thread_nums = 1, bool use_caller = true, const std::string& name = "");
  ~IOManager();
  // 0 success, -1 eeror
  int addEvent(int fd, Event event, std::function<void()> cb);  // 增加回调事件
  bool delEvent(int fd, Event event);                           // 删除回调事件
  bool cancelEvent(int fd, Event event);                        // 找到fd上对应的事件强制触发执行
  bool cancelAll(int fd);                                       // 强制触发fd上的所有事件

  static IOManager* GetThis();                // 获取当前IOManager

 protected:
  virtual void notify() override; 
  virtual bool stopping() override;
  virtual void idle() override;

  void contextResize(size_t size);

 private:
  int epfd_ = 0;
  int notify_fds_[2];                               // pipe对应的fd
  std::atomic<size_t> pending_event_counts_ = {0};  // 记录正在等待执行的事件数量
  RWmutex mutex_;
  std::vector<FdContext*> fd_contexts_;             // socket事件的上下文容器
};

}

#endif