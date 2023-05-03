#include "../moka/thread.h"
#include "../moka/log.h"
#include "../moka/config.h"
#include <vector>
#include <unistd.h>

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

int count = 0;
moka::RWmutex mutex;
moka::Mutex mutex2;

void func1() {
  MOKA_LOG_INFO(g_logger) << "name: " << moka::Thread::GetName()
                          << " this.name: " << moka::Thread::GetThis()->GetName()
                          << " id: " << moka::GetThreadId()
                          << " this.id: " << moka::Thread::GetThis()->get_id();
  for (int i = 0; i < 1000000; ++i) {
    // moka::RWmutex::WriteLock lock(mutex); // 测试写锁
    // moka::RWmutex::ReadLock lock(mutex);  // 测试读锁
    moka::Mutex::LockGuard lock(mutex2);     // 测试互斥锁
    ++count;
  }
}

void func2() {
  while (true)
    MOKA_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
}

void func3() {
  while (true)
    MOKA_LOG_INFO(g_logger) << "======================================================";
}

int main(int argc, char** argv) {
  MOKA_LOG_INFO(g_logger) << "thread test begin";
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test_log.yml");
  moka::Config::LoadFromYaml(root);

  std::vector<moka::Thread::ptr> thread_pool;
  // 创建线程
  for (int i = 0; i < 2; ++i) {
    moka::Thread::ptr thr(new moka::Thread(&func2, "name_" + std::to_string(i*2)));
    moka::Thread::ptr thr2(new moka::Thread(&func3, "name_" + std::to_string(i*2+1)));
    thread_pool.push_back(thr);
    thread_pool.push_back(thr2);
  }
  for (size_t i = 0; i < thread_pool.size(); ++i)  {
    thread_pool[i]->join();
  }
  MOKA_LOG_INFO(g_logger) << "thread test end";
  MOKA_LOG_INFO(g_logger) << "count=" << count;
  return 0;
}