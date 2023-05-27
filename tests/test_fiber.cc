#include "../moka/fiber.h"
#include "../moka/log.h"
#include "../moka/thread.h"
#include "../moka/macro.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void run_in_fiber() {
  MOKA_LOG_INFO(g_logger) << "run_in_fiber begin";
  moka::Fiber::YieldToHold();  // 调度执行主协程
  MOKA_LOG_INFO(g_logger) << "run_in_fiber end";
  moka::Fiber::YieldToReady();  // 调度执行主协程
}

void test_fiber() {
  MOKA_LOG_INFO(g_logger) << "start";
  // 测试子协程是否会正常析构
  {
    moka::Fiber::GetThis();  // 初始化当前线程的主协程
    MOKA_LOG_INFO(g_logger) << "main begin";
    moka::Fiber::ptr fiber(new moka::Fiber(run_in_fiber));  // 新建子协程
    MOKA_ASSERT(moka::Fiber::GetFiberCounts() == 2);
    fiber->reset(run_in_fiber);  // 测试回收资源的方法
    // 只是资源被重新利用了
    MOKA_ASSERT(moka::Fiber::GetFiberCounts() == 2);
    fiber->sched();  // 调度执行当前子协程
    MOKA_LOG_INFO(g_logger) << "main after shed";
    fiber->sched();  // 调度执行子协程
    MOKA_LOG_INFO(g_logger) << "main after end";
    // 这一步很关键，因为子协程的上下文并未执行结束，意味着run_in_fiber并未执行结束
    // 线程就从主协程中推出了，导致子协程没有析构
    // 且因为在非对称协程模型中，子协程执行结束之后就会返回到主协程中执行
    fiber->sched();
  }
  // 子协程退出，线程数量减少为1
  MOKA_ASSERT(moka::Fiber::GetFiberCounts() == 1);
  MOKA_LOG_INFO(g_logger) << "end";
}

int main(int argc, char** argv) {
  moka::Thread::SetName("main");  // 设置调度线程的名称
  std::vector<moka::Thread::ptr> thread_pool;
  // 测试线程，目前每个线程两个协程(一个主协程，一个子协程)
  for (int i = 0; i < 1; ++i) {
    thread_pool.push_back(moka::Thread::ptr(new moka::Thread(test_fiber, "t_"+std::to_string(i))));
  }
  for (auto i : thread_pool) {
    // 等待所有的线程执行结束退出回收资源
    i->join();
  }
  return 0;
}