#ifndef __MOKA_FIBER_H__
#define __MOKA_FIBER_H__

#include <ucontext.h>
#include <memory>
#include <functional>

namespace moka {

class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  using ptr = std::shared_ptr<Fiber>;
  enum state {
    INIT,   // 初始状态
    HOLD,   // 逃逸状态(需要显式地将协程加入调度器调度)
    EXEC,   // 执行状态
    TERM,   // 结束状态
    READY,  // 就绪状态(会被调度器自动重新调度)
    EXCEPT  // 异常状态
  };

 private:
  Fiber();  // 用于创建主协程(不需要栈空间)
 public:
  Fiber(std::function<void()> cb, bool link_to_main_fiber = false, size_t stacksize = 0);  // 用于创建子协程
  ~Fiber();

  void reset(std::function<void()> cb, bool link_to_main_fiber = false);  // 重置协程状态(在INIT，TERM状态时重置)

  // 当前上下文为主协程的上下文
  void sched();                             // 调度子协程执行
  void yield();                             // 子协程让出执行权(辅助方法)

  // sched和yield的另一个实现版本(当前上下文为调度协程的上下文)
  void call();
  void back();

  uint64_t get_fiber_id() { return id_; }
  state get_state() const { return state_; }
  void set_state(state st) { state_ = st; }
  std::function<void()>& get_cb() {return cb_;}
 
  static void SetThis(Fiber* f);  // 设置当前协程(用线程局部变量标记)
  static Fiber::ptr GetThis();    // 获取当前执行的协程，如果不存在则新建一个协程作为主协程
  // 将当前执行的子协程切换到后台主协程，并且设置为Ready状态
  static void YieldToReady();
  // 将当前执行的子协程切换到后台主协程，并且设置为Hold状态
  static void YieldToHold();

  // 将当前执行的子协程切换到后台主协程，并且设置为Ready状态
  static void YieldToReadySched();
  // 将当前执行的子协程切换到后台调度协程，并且设置为Hold状态
  static void YieldToHoldSched();

  static uint64_t GetFiberCounts();
  static void MainFunc();          // 发生上下文切换时调用的函数(内部调用回调函数)，执行完之后返回主协程
  static void MainFuncSched();     // 执行完之后返回调度协程
  static uint64_t GetFiberId();        // 获取当前协程的id

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
