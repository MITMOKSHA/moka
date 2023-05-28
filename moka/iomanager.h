#ifndef __MOKA_IOMANAGER_H__
#define __MOKA_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace moka {

class IOManager: public Scheduler, public TimerManager {
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
    EventContext& get_context(Event event); // 根据宏获取fd上下文对应的事件上下文对象
    void resetContext(EventContext& ctx);   // 重置事件上下文
    void trigger(Event event);              // 触发(调度执行)fd上下文中的读写事件的回调函数/任务协程

    int fd = 0;           // 事件关联的文件描述符
    EventContext read;    // 读事件上下文
    EventContext write;   // 写事件上下文
    Event events = NONE;  // 当前文件描述符注册的掩码集合(epoll监听事件的集合)
    Mutex mutex;          // 互斥锁
  };

 public:
  IOManager(size_t thread_nums = 1, bool use_caller = true, const std::string& name = "");
  ~IOManager();
  // 0 success, -1 eeror
  int addEvent(int fd, Event event, std::function<void()> cb);  // 增加回调事件
  int delEvent(int fd, Event event);                            // 删除回调事件
  int cancelEvent(int fd, Event event);                         // 找到fd上对应的事件强制触发执行
  int cancelAll(int fd);                                        // 强制触发fd上的所有事件

  static IOManager* GetThis();                                  // 获取当前IO协程调度器

 protected:
  virtual void notify() override; 
  virtual bool stopping() override;
  virtual void idle() override;
  // 用于有更早超时的定期器插入到定时器堆中，这时候需要通知对epoll的超时时间进行调整
  virtual void onTimerInsertedAtFront() override; 

  void contextResize(size_t size);                  // 对fd上下文数组扩容
  bool stopping(uint64_t& timeout);                 // IO调度器判断停止的条件

 private:
  int epfd_ = 0;
  int notify_fds_[2];                               // pipe对应的fd
  std::atomic<size_t> pending_event_counts_ = {0};  // 记录正在等待执行的事件数量
  RWmutex mutex_;
  std::vector<FdContext*> fd_contexts_;             // socket事件的上下文容器
};

}

#endif