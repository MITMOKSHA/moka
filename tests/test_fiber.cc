#include "../moka/fiber.h"
#include "../moka/log.h"
#include "../moka/thread.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void run_in_fiber() {
  MOKA_LOG_INFO(g_logger) << "run_in_fiber begin";
  moka::Fiber::YieldToHold();  // 调度执行主协程
  MOKA_LOG_INFO(g_logger) << "run_in_fiber end";
  moka::Fiber::YieldToHold();  // 调度执行主协程
}

void test_fiber() {
  MOKA_LOG_INFO(g_logger) << "main begin -l";
  {
    moka::Fiber::GetThis();  // 初始化当前线程的主协程
    MOKA_LOG_INFO(g_logger) << "main begin";
    moka::Fiber::ptr fiber(new moka::Fiber(run_in_fiber));  // 新建子协程
    fiber->reset(run_in_fiber);  // 测试回收资源的方法
    fiber->shed();  // 调度执行当前子协程
    MOKA_LOG_INFO(g_logger) << "main after shed";
    fiber->shed();  // 调度执行子协程
    MOKA_LOG_INFO(g_logger) << "main after end";
    fiber->shed();
    // 测试引用计数
    // std::weak_ptr<moka::Fiber> ptr(fiber);
    // MOKA_LOG_DEBUG(g_logger) << ptr.use_count();
  }
  MOKA_LOG_DEBUG(g_logger) << "end";
}

int main(int argc, char** argv) {
  moka::Thread::SetName("main");  // 设置主线程的名称
  std::vector<moka::Thread::ptr> thread_pool;
  // 测试三个线程，每个线程两个协程(一个主协程，一个子协程)
  for (int i = 0; i < 3; ++i) {
    thread_pool.push_back(moka::Thread::ptr(new moka::Thread(test_fiber, "t_"+std::to_string(i))));
  }
  for (auto i : thread_pool) {
    i->join();
  }
  return 0;
}