#include "scheduler.h"
#include "fiber.h"
#include "thread.h"
#include "macro.h"
#include "hook.h"
#include "log.h"

namespace moka {

static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;      // 当前线程的调度器
static thread_local Fiber* t_sched_fiber = nullptr;        // 当前线程的调度协程

Scheduler* Scheduler::GetThis() {
  return t_scheduler;
}

Fiber* Scheduler::GetSchedFiber() {
  return t_sched_fiber;
}

// use_caller为true表示使用调用者的线程作为调度线程
Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : name_(name) {
  MOKA_ASSERT(threads > 0);
  if (use_caller) {
    // 当前线程作为调度线程
    // 在当前线程中新建一个调度线程的主协程(注意这个主协程并不是调度协程)
    // (注意这里会在切换到子协程执行时无法回到调度协程，因为uc_link是主协程(在caller线程中不是调度协程)
    moka::Fiber::GetThis();  // 其中初始化了当前线程的主协程
    --threads;  // 使用当前caller线程(不需要在start中新建一个线程)

    // 构造前，当前线程没有调度器
    MOKA_ASSERT(GetThis() == nullptr);

    // 更新当前线程的调度器
    t_scheduler = this;

    // 初始化子调度协程(会调用run函数进行任务调度，因此需要分配栈空间)，bind绑定到function对象
    // true表示当前调度协程执行结束后link到主协程执行
    caller_sched_fiber_.reset(new Fiber(std::bind(&Scheduler::run, this), true));

    // 设置调度器所属的线程的名称
    moka::Thread::SetName(name_);

    // 更新当前线程的调度协程
    t_sched_fiber = caller_sched_fiber_.get();

    // 设置调度器所在的线程id
    thread_id_ = moka::GetThreadId();
    // 将当前线程id放入集合中
    thread_id_set_.push_back(thread_id_);
  } else {
    // 没有使用caller线程作为调度线程，在run方法中新建线程作为调度线程
    thread_id_ = -1; 
  }
  thread_nums_ = threads;  // 更新当前存在的线程数目
}

Scheduler::~Scheduler() {
  MOKA_ASSERT(is_stopping_);
  if (GetThis() == this) {
    // 清空当前的调度器标记
    t_scheduler = nullptr;  
  }
}

// 启动调度
void Scheduler::start() {
  {
    Mutex::LockGuard lock(mutex_);
    if (!is_stopping_) {
      // 已经启动
      return; 
    }
    is_stopping_ = false;  // 更改状态
    // 刚开始启动时线程池为空
    MOKA_ASSERT(thread_pool_.empty());

    // 预留线程池空间
    thread_pool_.resize(thread_nums_);
    for (size_t i = 0; i < thread_nums_; ++i) {
      // 初始化线程池中的线程，新建的线程会执行run函数，并指定线程名称
      thread_pool_[i].reset(new Thread(std::bind(&Scheduler::run, this),
                            name_ + "_" + std::to_string(i)));
      thread_id_set_.push_back(thread_pool_[i]->get_id());
    }
  }
}

void Scheduler::stop() {
  is_auto_stopping_ = true;
  if (caller_sched_fiber_ && thread_nums_ == 0 
                  && (caller_sched_fiber_->get_state() == Fiber::INIT
                  || caller_sched_fiber_->get_state() == Fiber::TERM)) {
    // 针对单一caller线程作为调度线程的停止
    is_stopping_ = true;
    
    if (stopping()) {
      MOKA_LOG_DEBUG(g_logger) << "stop success";
      return;
    }
  }

  // 如果use_caller，保证由caller线程执行stop()
  if (thread_id_ != -1) {
    // caller线程
    MOKA_ASSERT(GetThis() == this);
  } else {
    // 其他线程，执行stop的线程应该为caller线程
    MOKA_ASSERT(GetThis() != this);
  }

  // 保证没有任务就会停止
  is_stopping_ = true;  // 经过测试stop必须放在这个位置

  // 若当前线程的调度器存在调度协程(使用caller线程作为调度线程)
  if (caller_sched_fiber_) {
    caller_sched_fiber_->sched();  // 当前运行start函数的上下文为主协程
  }

  // 等待其他调度线程的调度协程退出调度
  for (size_t i = 0; i < thread_nums_; ++i) {
    notify();  // 使线程唤醒，结束资源
  }
  
  // 如果使用了caller线程
  if (caller_sched_fiber_) {
    // 等待caller线程的调度协程返回
    notify();
  }

  // 这里一定要join等待调度线程执行任务结束后调度器才停止
  for (auto& i : thread_pool_) {
    i->join();
  }
  // 清空线程id集合
  thread_id_set_.clear();
}

void Scheduler::run() {
  MOKA_LOG_INFO(g_logger) << "run";
  // 默认情况下，协程调度器的调度线程会开启hook
  // set_hook_enable(true);

  // 设置当前运行的调度器
  set_this();

  // 保证run的上下文为调度协程
  // 若没有使用caller线程作为调度器的调度线程，则其他新建的线程的id肯定不等于调度线程的id
  if (moka::GetThreadId() != this->thread_id_) {
    // 没有使用caller线程作为调度线程
    // 新建主协程作为调度协程(不需要栈资源，因为run由新建的线程来执行)

    // 新建的线程run起来的时候还没有创建协程，因此GetThis返回的是新建的主协程
    // 调用主协程的构造函数，会SetThis一下，设置当前协程为该新建的协程
    t_sched_fiber = Fiber::GetThis().get();
  }

  // 因为run为调度协程执行的函数，因此idle_fiber要执行结束后要返回调度协程(而不是main协程)
  // idle协程默认返回到调度协程
  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
  
  // 用于执行回调函数的协程(可以使用reset成员函数重复利用)
  Fiber::ptr cb_fiber;

  ScheduleTask task;         // 任务结构体，用于暂存任务队列中的任务
  while (true) {
    task.reset();            // 初始化任务为空(协程，回调函数函数，调度线程为空)
    bool notify_me = false;  // 是否notify其他线程进行任务调度
    bool is_active = false;
    {
      Mutex::LockGuard lock(mutex_);
      auto it = tasks_.begin();
      // 遍历任务队列
      while (it != tasks_.end()) {
        if (it->thread_id != -1 && it->thread_id != moka::GetThreadId()) {
          // 若该任务有调度线程，当前执行的线程不等于任务的调度线程，则不处理这个任务
          // 保证协程处理单个线程内的任务
          ++it;
          notify_me = true;  // 唤醒处于idle状态的该任务的调度线程来处理这个任务(Run)
          continue;
        }
        MOKA_ASSERT(it->fiber || it->cb);
        if (it->fiber && it->fiber->get_state() == Fiber::EXEC) {
          // 该任务(子协程)正在执行，也跳过不处理
          ++it;
          continue;
        }
        task = *it;  // 取出需要执行的任务
        tasks_.erase(it);
        ++active_thread_nums_;
        is_active = true;  // 现在有任务在做
        break;       // 取出后结束循环
      }
    }
    if (notify_me) {
      notify();  // 唤醒其他线程处理任务
    }


    if (task.fiber && task.fiber->get_state() != Fiber::TERM
                   && task.fiber->get_state() != Fiber::EXCEPT) {
      task.fiber->reset(task.fiber->get_cb());
      // 协程
      task.fiber->call();  // 调度执行该任务协程的函数(当前协程上下文为调度协程)
      --active_thread_nums_;

      if (task.fiber->get_state() == Fiber::READY) {
        // 若该协程执行了YeildToReady(说明该任务没有执行完)，则再次将该协程加入任务队列进行调度
        // READY状态下自动调度
        schedule(task.fiber);
      } else if (task.fiber->get_state() != Fiber::TERM
              && task.fiber->get_state() != Fiber::EXCEPT) {
        // 让出执行的状态就是hold状态
        task.fiber->set_state(Fiber::HOLD);
      }
      task.reset();
    } else if (task.cb) {
      // 将函数协程作为执行函数的载体
      if (cb_fiber) {
        // 调用Fiber::reset，重复利用之前的协程资源
        cb_fiber->reset(task.cb);
      } else {
        // 第一次使用，则初始化
        cb_fiber.reset(new Fiber(task.cb));
      }
      // 每次任务处理结束就重置任务结构体
      task.reset();

      // 执行该函数任务
      cb_fiber->call();
      --active_thread_nums_;

      if (cb_fiber->get_state() == Fiber::READY) {
        // 自动重新调度
        schedule(cb_fiber);
        // 按值传参后需要reset一下将引用计数减1
        cb_fiber.reset();  // 释放协程(调用析构函数)
      } else if (cb_fiber->get_state() == Fiber::TERM
              || cb_fiber->get_state() == Fiber::EXCEPT) {
        cb_fiber->reset(nullptr);  // 回收利用协程资源
      } else {
        cb_fiber->set_state(Fiber::HOLD);
        // TODO: BUG，HOLD之后析构就会触发ASSERT断言
        cb_fiber.reset();  // 调用析构函数释放cb_fiber协程
      }
    } else {
      // 如果这次执行没有任务才执行idle
      // 注意如果调度器没有任务，那么idle协程会不停地sched/yield,不会结束，如果idle协程结束一定是调度器停止了
      if (is_active) {
        --active_thread_nums_;
        continue;
      }

      if (idle_fiber->get_state() == Fiber::TERM) {
        MOKA_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++idle_thread_nums_; 
      idle_fiber->call();  // 调度idle协程(执行idle函数)
      --idle_thread_nums_; 

      if (idle_fiber->get_state() != Fiber::TERM
              && idle_fiber->get_state() != Fiber::EXCEPT) {
        idle_fiber->set_state(Fiber::HOLD);
      }
    }
  }
}

void Scheduler::set_this() {
  t_scheduler = this;
}

void Scheduler::notify() {
  MOKA_LOG_INFO(g_logger) << "notify";
}

bool Scheduler::stopping() {
  Mutex::LockGuard lock(mutex_);
  // 只有所有的任务都被执行完了，调度器才可以停止
  return is_auto_stopping_ && is_stopping_
      && tasks_.empty() && active_thread_nums_ == 0;
}

void Scheduler::idle() {
  MOKA_LOG_INFO(g_logger) << "idle";
  // 需要在I/O协程调度模块进行完善(只有在调度器停止时idle才结束，没有任务时idle也不结束)
  while (!stopping()) {
    // 忙等待
    // 这里将idle协程的状态改为了hold，并从当前协程的上下文切换到调度协程的上下文
    // 这里直接跳过了MainFunc中将idle fiber状态设置为TERM的语句
    moka::Fiber::YieldToHoldSched();
  }
}

}