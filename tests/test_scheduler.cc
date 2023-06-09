#include "../moka/scheduler.h"
#include "../moka/log.h"
#include "../moka/fiber.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();


void test_fiber() {
  MOKA_LOG_INFO(g_logger) << "test in fiber and counts=" << moka::Fiber::GetFiberCounts();
  static int s_count = 3;
  // hook之后普通的协程调度就不能使用该版本了，因为在IOManager的时候才对父类TimerManager进行初始化
  // sleep中会调用addTimer，但是此时TimerManager对象并未初始化，因此就会出现segFault
  // sleep(1);
  if (--s_count >= 0) {
    // moka::Scheduler::GetThis()->schedule(test_fiber, moka::GetThreadId());  // 指定线程执行
    moka::Scheduler::GetThis()->schedule(test_fiber);
  }
}

void test_scheduler() {
  moka::Scheduler s(1, true, "test");
  s.start();
  // 添加函数作为任务执行
  moka::Fiber::ptr task(new moka::Fiber(test_fiber));
  // test_fiber一共执行五次，因为三次schedule放入任务是固定的，两个线程最开始分别放入一次任务。
  // 添加协程任务执行
  s.schedule(task);
  // 添加函数执行
  s.schedule(test_fiber); 
  s.stop();
}

int main(int agrc, char** argv) {
  test_scheduler();
  return 0;
}