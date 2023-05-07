#include "thread.h"
#include "log.h"
#include "util.h"

namespace moka {

// 每个线程都有两个线程局部变量，存储当前线程的指针和线程的名称
static thread_local Thread* t_thread = nullptr;            // 标记当前线程(在多线程环境下)
static thread_local std::string t_thread_name = "UNKNOW";  // 标记当前线程的名称

static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

Thread* Thread::GetThis() {
  return t_thread;
}

const std::string& Thread::GetName() {
  return t_thread_name;
}

void Thread::SetName(const std::string& name) {
  if (t_thread) {
    t_thread->name_ = name;
  }
  t_thread_name = name;
}

void* Thread::Run(void* arg) {
  // 获取this指针赋给thread，因为这是静态方法
  Thread* thread = (Thread*)arg;           // POSIX标准线程函数参数必须是void*，因此需要转型
  t_thread = thread;                     // 创建线程时初始化局部线程变量
  thread->id_ = moka::GetThreadId();
  t_thread_name = thread->name_;
  // 设置线程的名称
  pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->cb_);  // TODO:智能指针的问题?

  thread->sem_.post();   // 确保线程运行起来(Run)时，线程初始化成员变量成功

  cb();  // 调用回调函数
  return nullptr;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : cb_(cb), name_(name) {
  // 构造函数初始化回调函数
  if (name.empty()) {
    name_ = "UNKNOW";
  }
  // 新建线程，成功返回0
  // 新创建的线程执行函数run
  int ret = pthread_create(&thread_, nullptr, Run, this);  // run是static函数，需要传入this
  MOKA_LOG_DEBUG(g_logger) << "Thread::Thread " << name_;
  if (ret) {
    MOKA_LOG_ERROR(g_logger) << "pthread_create thread fail , ret = " << ret
                             << " name=" << name;
    throw std::logic_error("pthread_create error");
  }
   sem_.wait();   // 等待线程运行run起来才结束构造函数
}

Thread::~Thread() {
  // 保证线程对象在析构时自动回收资源
  if (thread_) {
    pthread_detach(thread_);
    MOKA_LOG_DEBUG(g_logger) << "Thread::~Thread " << id_;
  }
}

void Thread::join() {
  if (thread_) {
    // 回收线程资源
    int ret = pthread_join(thread_, nullptr);
    if (ret) {
      MOKA_LOG_ERROR(g_logger) << "pthread_join thread fail , ret = " << ret
                              << " name=" << name_;
      throw std::logic_error("pthread_join error");
    }
    thread_ = 0;  // 将所属线程清空空
  }
}

Semaphore::Semaphore(uint32_t num) {
  if (sem_init(&sem_, 0, num))  // 第二个参数为0表示在线程间共享
    throw std::logic_error("sem_init error");
}

Semaphore::~Semaphore() {
  sem_destroy(&sem_);
}

void Semaphore::wait() {
  if (sem_wait(&sem_)) {
    throw std::logic_error("sem_wait: error");
  }
}
void Semaphore::post() {
  if (sem_post(&sem_)) {
    throw std::logic_error("sem_post: error");
  }
}


}