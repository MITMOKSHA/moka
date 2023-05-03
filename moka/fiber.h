#ifndef __MOKA_FIBER_H__
#define __MOKA_FIBER_H__

#include <ucontext.h>
#include <memory>
#include <functional>

#include "thread.h"

namespace moka {

class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  using ptr = std::shared_ptr<Fiber>;
  enum state {
    INIT,   // 初始状态
    HOLD,   // 阻塞状态
    EXEC,   // 执行状态
    TERM,   // 结束状态
    READY,  // 就绪状态
    EXCEPT  // 异常状态
  };

 private:
  Fiber();  // 用于创建主协程(不需要栈空间)
 public:
  Fiber(std::function<void()> cb, size_t stacksize = 0);  // 用于创建子协程
  ~Fiber();

  void reset(std::function<void()> cb);  // 重置协程状态(在INIT，TERM状态时重置)
  void shed();                           // 调度子协程执行
  void yield();                          // 子协程让出执行权(辅助方法)
  uint64_t get_fiber_id() { return id_; }
 
  static void SetThis(Fiber* f);  // 标记当前协程(用线程局部变量标记)
  static Fiber::ptr GetThis();    // 获取当前协程(从线程局部变量t_fiber中获取)
  // 将当前执行的子协程切换到后台，并且设置为Ready状态
  static void YieldToReady();
  // 将当前执行的子协程切换到后台，并且设置为Hold状态
  static void YieldToHold();

  static uint64_t GetTotalFiberNums();
  static void MainFunc();         // 发生上下文切换时调用的函数(内部调用回调函数)
  static uint64_t GetFiberId();   // 获取当前协程的id

 private:
  uint64_t id_ = 0;
  uint32_t stack_size_ = 0;
  state state_ = INIT;
  ucontext_t uc_;            // 协程上下文结构
  void* stack_ = nullptr;
  std::function<void()> cb_;
};

}

#endif
