#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include <atomic>

namespace moka {

static Logger::ptr g_logger = MOKA_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;               // 标记当前执行的协程
static thread_local Fiber::ptr t_thread_fiber = nullptr;    // 标记主协程

// 协程栈默认大小为1M，注册配置项到集合中
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
  Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");
  
// 协程栈空间分配器
class MallocStackAllocator {
 public:
  static void* alloc(size_t size) {
    return malloc(size);
  }
  static void dealloc(void* vp, size_t size) {
    return free(vp);
  }
};

using StackAllocator = MallocStackAllocator;

// 创建主协程的构造函数(一个线程只有一个，使用单例模式)
Fiber::Fiber() {
  state_ = EXEC;
  SetThis(this);
  // 使用当前线程的上下文初始化uc
  MOKA_ASSERT_2(!getcontext(&uc_), "getcontext");
  ++s_fiber_count;
  MOKA_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

// 子协程
Fiber::Fiber(std::function<void()> cb, size_t stacksize)
    : id_ (++s_fiber_id), cb_(cb) {
  ++s_fiber_count;
  stack_size_ = stack_size_? stack_size_: g_fiber_stack_size->get_value();
  stack_ = StackAllocator::alloc(stack_size_);   // 分配协程栈空间
  MOKA_ASSERT_2(!getcontext(&uc_), "getcontext");
  uc_.uc_link = &(t_thread_fiber->uc_);  // 子协程函数体执行结束后切换回主协程上下文
  uc_.uc_stack.ss_sp = stack_;
  uc_.uc_stack.ss_size = stack_size_;

  // 发生上下文切换时调用MainFunc执行
  makecontext(&uc_, &Fiber::MainFunc, 0);
  MOKA_LOG_DEBUG(g_logger) << "Fiber::Fiber id = " << id_;
}

Fiber::~Fiber() {
  --s_fiber_count;
  if (stack_) {
    // 子协程
    MOKA_ASSERT(state_ == TERM || state_ == INIT || state_ == EXCEPT);
    // 回收栈
    StackAllocator::dealloc(stack_, stack_size_);
  } else {
    // 主协程(不需要协程栈空间)
    MOKA_ASSERT(!cb_);
    MOKA_ASSERT(state_ == EXEC);
    Fiber* cur = t_fiber;

    if (cur == this) {
      SetThis(nullptr);
    }
  }
  MOKA_LOG_DEBUG(g_logger) << "Fiber::~Fiber id = " << id_;
}

// 回收协程的栈资源，使用该栈资源和传入的参数初始化新的协程
void Fiber::reset(std::function<void()> cb) {
  MOKA_ASSERT(stack_);
  // 若当前协程处于以下几种状态即可回收资源
  MOKA_ASSERT(state_ == TERM || state_ == INIT || state_ == EXCEPT);
  // 回收资源
  cb_ = cb;
  MOKA_ASSERT_2(!getcontext(&uc_), "getcontext");
  uc_.uc_link = &(t_thread_fiber->uc_);
  // 使用当前协程的栈资源
  uc_.uc_stack.ss_sp = stack_;        
  uc_.uc_stack.ss_size = stack_size_;
  makecontext(&uc_, &Fiber::MainFunc, 0);  // 设置当发生上下文切换时调用MainFunc(调用回调函数)
  state_ = INIT;
}

// 调度执行当前协程
void Fiber::shed() {
  SetThis(this);     // 标记当前执行协程的变量设置为当前协程
  MOKA_ASSERT(state_ != EXEC);
  state_ = EXEC;     // 切换为执行态
  // 主协程上下文切换到当前协程
  MOKA_ASSERT_2(!swapcontext(&(t_thread_fiber->uc_), &uc_), "swapcontext");
}

// 将当前协程切换到后台，执行主协程
void Fiber::yield() {
  // 标记当前执行协程为主协程
  SetThis(t_thread_fiber.get());
  // 主协程上下文切换到主协程
  MOKA_ASSERT_2(!swapcontext(&uc_, &(t_thread_fiber->uc_)), "swapcontext");
}

void Fiber::SetThis(Fiber* f) {
  t_fiber = f;
}

Fiber::ptr Fiber::GetThis() {
  if (t_fiber) {
    return t_fiber->shared_from_this();
  }
  // 如果没有当前协程(则说明不存在主协程)，则新建主协程
  Fiber::ptr main_fiber(new Fiber);   // 新建时自动初始化t_fiber
  MOKA_ASSERT(t_fiber == main_fiber.get());
  t_thread_fiber = main_fiber;        // 初始化标记主协程的变量
  return t_fiber->shared_from_this();
}

// 当前协程切换到后台并设置状态，切换为主协程
void Fiber::YieldToReady() {
  Fiber::ptr cur = GetThis();
  cur->state_ = READY;
  cur->yield();
}

void Fiber::YieldToHold() {
  Fiber::ptr cur = GetThis();
  cur->state_ = HOLD;
  cur->yield();
}

uint64_t Fiber::GetTotalFiberNums() {
  return s_fiber_count;
}

void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();
  MOKA_ASSERT(cur);
  try {
    cur->cb_();
    cur->cb_ = nullptr;
    cur->state_ = TERM;
  } catch (std::exception& ex) {
    cur->state_ = EXCEPT;
    MOKA_LOG_ERROR(g_logger) << "Fiber Exception: " << ex.what();
  } catch (...) {
    cur->state_ = EXCEPT;
    MOKA_LOG_ERROR(g_logger) << "Fiber Exception";
  }
}

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->get_fiber_id();
  }
  return 0;
}

}

