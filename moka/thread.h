#ifndef __MOKA_THREAD_H__
#define __MOKA_THREAD_H__

#include <pthread.h>
#include <functional>
#include <memory>
#include <semaphore.h>
#include <mutex>
#include <stdint.h>

namespace moka {

// 信号量
class Semaphore {
 public:
  Semaphore(uint32_t num = 0);
  ~Semaphore();

  void wait();
  void post();
 private: 
  Semaphore(const Semaphore&) = delete;
  Semaphore(const Semaphore&&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

 private:
  sem_t sem_;
};


// mutex/spinlock lock_guard(利用构造函数和析构函数来进行加锁/解锁)，使用模板类保证RAII机制
template<class T>
class ScopedLock {
 public:
  ScopedLock(T& mutex) : mutex_(mutex) {
    mutex_.lock();
    is_locked = true;
  }

  ~ScopedLock() {
    mutex_.unlock();
    is_locked = false;
  }

  void lock() {
    if (!is_locked) {
      mutex_.lock();
      is_locked = true;
    }
  }

  void unlock() {
    if (is_locked) {
      mutex_.unlock();
      is_locked = false;
    }
  }
 private:
  T& mutex_;
  bool is_locked = false;
};

class Mutex {
 public:
  using LockGuard = ScopedLock<Mutex>;
  Mutex() {
    pthread_mutex_init(&mutex_, nullptr);
  }

  ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }

  void lock() {
    pthread_mutex_lock(&mutex_);
  }
  
  void unlock() {
    pthread_mutex_unlock(&mutex_);
  }
 private:
  pthread_mutex_t mutex_;
};

// 空锁(测试日志线程安全)
class NullMutex {
 public:
  using LockGuard = ScopedLock<NullMutex>; 
  NullMutex() {}
  ~NullMutex() {}
  void lock() {}
  void unlock() {}
};

// 读锁lock_guard
template<class T>
class ReadScopedLock {
 public:
  ReadScopedLock(T& mutex) : mutex_(mutex) {
    mutex_.rdlock();
    is_locked = true;
  }

  ~ReadScopedLock() {
    mutex_.unlock();
    is_locked = false;
  }

  void lock() {
    if (!is_locked) {
      mutex_.rdlock();
      is_locked = true;
    }
  }

  void unlock() {
    if (is_locked) {
      mutex_.unlock();
      is_locked = false;
    }
  }
 private:
  T& mutex_;
  bool is_locked = false;
};

// 写锁lock_guard
template<class T>
class WriteScopedLock {
 public:
  WriteScopedLock(T& mutex) : mutex_(mutex) {
    mutex_.wrlock();
    is_locked = true;
  }

  ~WriteScopedLock() {
    mutex_.unlock();
  }

  void lock() {
    if (!is_locked) {
      mutex_.wrlock();
      is_locked = true;
    }
  }

  void unlock() {
    if (is_locked) {
      mutex_.unlock();
      is_locked = false;
    }
  }
 private:
  T& mutex_;
  bool is_locked = false;
};


// 读写锁
class RWmutex {
 public:
  using ReadLock = ReadScopedLock<RWmutex>;
  using WriteLock = WriteScopedLock<RWmutex>;
  RWmutex() {
    pthread_rwlock_init(&lock_, nullptr);
  }

  ~RWmutex() {
    pthread_rwlock_destroy(&lock_);
  }

  void rdlock() {
    pthread_rwlock_rdlock(&lock_);
  }

  void wrlock() {
    pthread_rwlock_wrlock(&lock_);
  }

  void unlock() {
    pthread_rwlock_unlock(&lock_);
  }
 private:
  pthread_rwlock_t lock_;
};

class Spinlock {
 public:
  using LockGuard = ScopedLock<Spinlock>;
  Spinlock() {
    pthread_spin_init(&mutex_, 0);
  }

  ~Spinlock() {
    pthread_spin_destroy(&mutex_);
  }

  void lock() {
    pthread_spin_lock(&mutex_);
  }

  void unlock() {
    pthread_spin_unlock(&mutex_);
  }

 private:
  pthread_spinlock_t mutex_;
};

// 线程模块
class Thread {
 public:
  using ptr = std::shared_ptr<Thread>;
  Thread(std::function<void()> cb, const std::string& name);
  ~Thread();

  pthread_t get_id() const { return id_; }
  std::string get_this_name() { return name_; }
  void join();  // 封装join系统调用

  // 这两个变量用局部线程变量存储
  static Thread* getThis();               // 获得当前的线程
  static const std::string& getName();    // 获得当前线程的名称(提供给日志使用)

  static void setName(const std::string& name);

  static void* run(void* arg);            // 线程创建(pthread_create)时调用的函数

 private:
  // 禁用移动
  Thread(const Thread&&) = delete;
  // 禁用拷贝
  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

private:
  pid_t id_ = 0;              // 线程id(linux线程)
  pthread_t thread_ = 0;      // 线程
  std::function<void()> cb_;  // 线程执行的函数
  std::string name_;          // 线程名称

  Semaphore sem_;             // 信号量
};

}

#endif