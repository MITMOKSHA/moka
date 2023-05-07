#ifndef __MOKA_SCHEDULER_H__
#define __MOKA_SCHEDULER_H__

#include <memory>
#include <list>
#include <vector>
#include <functional>
#include <atomic>  // 保证线程安全

#include "fiber.h"
#include "thread.h"

namespace moka {

class Scheduler {
 public:
  using ptr = std::shared_ptr<Scheduler>;

  // use_caller表示是否使用利用当前线程作为调度线程(即会自动为该线程创建一个调度协程，同时线程数自动减1)
  // use_caller为false表示创建一个线程进行协程调度
  Scheduler(size_t thread_nums = 1, bool use_caller = true, const std::string& name = "");
  virtual ~Scheduler();

  const std::string& get_name() const { return name_; }
  
  static Scheduler* GetThis();   // 获得当前的调度器
  static Fiber* GetSchedFiber();  // 获得调度器的调度协程

  void start();  // 启动调度
  void stop();   // 停止调度

  // caller线程的辅助方法
  void call();

  template<class FiberOrCb>
  void schedule(FiberOrCb fc, int thread = - 1) {
    bool need_notify = false;
    {
      Mutex::LockGuard lock(mutex_);
      need_notify = scheduleNoLock(fc, thread);
    }
    if (need_notify) {
      notify();  // 通知调度协程
    }
  }

  // 往调度器中添加任务(保存到任务队列中)，但不立刻执行
  template<class InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool need_notify = false;
    {
      Mutex::LockGuard lock(mutex_);
      while (begin != end) {
        // 仅需要在一开始往任务队列中添加任务时notify
        need_notify = scheduleNoLock(&(*begin)) || need_notify;
      }
    }
    if (need_notify) {
      notify();
    }
  }

 protected:
  virtual void notify();
  virtual bool stopping();
  virtual void idle();     // 协程idle
  void run();              // 调度协程执行的函数
  void set_this();

 private:
  // 无锁版本，使用FiberOrCb模板参数将函数和协程统一起来，构造任务时会调用对应的调度器构造函数
  template<class FiberOrCb>
  bool scheduleNoLock(FiberOrCb fc, int thread) {
    // 如果一开始任务队列为空，用notify方法通知各调度线程的调度协程有新任务来了
    bool need_notify = tasks_.empty();
    ScheduleTask task(fc, thread);  // 调用对应函数/协程的构造函数
    if (task.fiber || task.cb) {
      tasks_.push_back(task);  // 将任务加入到任务队列中
    }
    return need_notify;
  }
 private:
  // 调度任务(协程/函数)，可指定在具体的线程上调度
  struct ScheduleTask {
    Fiber::ptr fiber;           // 协程
    std::function<void()> cb;   // 函数
    pid_t thread_id;            // 协程/函数的调度线程

    ScheduleTask() : thread_id(-1) {}
    // 协程
    ScheduleTask(Fiber::ptr f, pid_t id) : fiber(f), thread_id(id) {}
    // 调用swap成员赋值智能指针，两个操作数的引用计数不会增加
    ScheduleTask(Fiber::ptr* f, pid_t id) : thread_id(id) { fiber.swap(*f); }
    // 函数
    ScheduleTask(std::function<void()> f, pid_t id) : cb(f), thread_id(id) {}
    ScheduleTask(std::function<void()>* f, pid_t id) : thread_id(id) { cb.swap(*f); }

    // 重置
    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread_id = -1;
    }
  };

 protected:
  std::vector<pid_t> thread_id_set_;               // 线程号集合
  size_t thread_nums_ = 0;                         // 线程总数
  std::atomic<size_t> active_thread_nums_ = {0};   // 活跃线程数量
  std::atomic<size_t> idle_thread_nums_ = {0};     // 空闲线程数量
  bool is_stopping_ = true;                        // 调度器的执行状态
  bool is_auto_stopping_ = false;                  // 是否主动停止
  pid_t thread_id_ = 0;                            // 调度器所在线程的id

 private:
  std::vector<Thread::ptr> thread_pool_;  // 线程池
  std::list<ScheduleTask> tasks_;         // 任务队列
  Mutex mutex_;
  std::string name_;                      // 调度器所属的线程名称
  Fiber::ptr caller_sched_fiber_;         // caller线程的调度协程(如果未使用caller则为空)
};

}

#endif