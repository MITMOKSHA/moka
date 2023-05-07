#include "../moka/scheduler.h"
#include "../moka/log.h"
#include "../moka/fiber.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test_fiber() {
  MOKA_LOG_INFO(g_logger) << "test in fiber and counts=" << moka::Fiber::GetFiberCounts();
  static int s_count= 5;
  sleep(1);
  if (--s_count >= 0) {
    // moka::Scheduler::GetThis()->schedule(test_fiber, moka::GetThreadId());  // 指定线程执行
    moka::Scheduler::GetThis()->schedule(test_fiber);
  }
}

int main(int agrc, char** argv) {
  MOKA_LOG_INFO(g_logger) << "main";
  moka::Scheduler s(1, false, "test");
  s.start();
  s.schedule(test_fiber);
  s.stop();
  MOKA_LOG_INFO(g_logger) << "over";
  MOKA_LOG_INFO(g_logger) << "fiber counts = " << moka::Fiber::GetFiberCounts();
  return 0;
}