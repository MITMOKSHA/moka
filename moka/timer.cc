#include "timer.h"
#include "util.h"
#include "log.h"

namespace moka {


Timer::Timer(uint64_t interval, std::function<void()> cb, bool recur, TimerManager* manager) 
    : recur_(recur), interval_(interval), cb_(cb), manager_(manager) {
  // 当前时间(毫秒) + 执行周期 = 到期时间
  expire_ = moka::GetCurrentMs() + interval_;
}

Timer::Timer(uint64_t expire) : expire_(expire) {}

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
  // 由小到大排序
  if (lhs == nullptr && rhs == nullptr) {
    return false;
  }
  if (lhs == nullptr) {{
    return true;
  }}
  if (rhs == nullptr) {
    return false;
  }
  if (lhs->expire_ < rhs->expire_) {
    return true;
  }
  if (rhs->expire_ < lhs->expire_) {
    return false;
  }
  // 如果到期时间一样，则比地址
  return lhs.get() < rhs.get();
}

bool Timer::cancel() {
  RWmutex::WriteLock lock(manager_->mutex_);
  if (cb_) {
    cb_ = nullptr;
    auto it = manager_->timers_.find(shared_from_this());
    manager_->timers_.erase(it);
    return true;
  }
  // 已经超时的定时器会自动erase
  return false;
}

// 重新设置当前定时器的到期时间等价于()
bool Timer::resetExpire() {
  RWmutex::WriteLock lock(manager_->mutex_);
  if (!cb_) {
    // 已经超时的定时器不能重新设置到期时间(在listAllCbs中被设置)
    return false;
  }
  auto it = manager_->timers_.find(shared_from_this());
  if (it == manager_->timers_.end()) {
    // 当前定时器不在定时堆中
    return false;
  }
  // 这里不能直接修改set中排序的key，得先删除再添加
  manager_->timers_.erase(it);
  // 更新当前定时器的到期时间
  // 等价于resetIntervalAndExpire(interval_, true);
  this->expire_ = moka::GetCurrentMs() + interval_;
  manager_->timers_.insert(shared_from_this());
  return true;
}

bool Timer::resetIntervalAndExpire(uint64_t interval, bool from_now) {
  if (interval == interval_ && !from_now) {
    // 执行周期不变且不从现在开始，没必要重置
    return true;
  }
  RWmutex::WriteLock lock(manager_->mutex_);
  if (!cb_) {
    // 已经超时的定时器不能重新设置到期时间
    return false;
  }

  // 从定时器堆中找到当前定时器
  auto it = manager_->timers_.find(shared_from_this());
  if (it == manager_->timers_.end()) {
    return false;
  }
  manager_->timers_.erase(it);
  uint64_t start = 0;
  // 获取定时器的设置的时间点
  if (from_now) {
    start = moka::GetCurrentMs();
  } else {
    // 找到最开始设置定时器的时间点
    start = expire_ - interval_;
  }
  // 更新执行周期
  this->interval_ = interval;
  // 更新到期时间
  this->expire_ = start + interval_;
  // 将重置的定时器加入定时堆中
  this->manager_->addTimer(shared_from_this());
  return true;
}


TimerManager::TimerManager() {
  // 记录定时器管理器创建时的系统时间点(方便检测系统时间是否被调整)
  previous_time_ = moka::GetCurrentMs();
}

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t interval, std::function<void()> cb, bool recur) {
  Timer::ptr timer(new Timer(interval, cb, recur, this));
  RWmutex::WriteLock lock(mutex_);
  addTimer(timer);
  return timer;
}

void TimerManager::addTimer(Timer::ptr timer) {
  // insert::first获取到插入位置的迭代器
  auto it = timers_.insert(timer).first;
  // 插入到最前面说明时间是最早的定时器
  bool at_front = (it == timers_.begin()) && !ticked_;
  if (at_front) {
    ticked_ = true;
  }

  if (at_front) {
    // 如果插入的定时器时间是最早的，需要更新epoll_wait上等待的时间(不需要等待原来那么久)
    onTimerInsertedAtFront();  // 唤醒epoll_wait
  }
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
  // 将weak_ptr转换shared_ptr，并增加引用计数
  std::shared_ptr<void> tmp = weak_cond.lock();
  if (tmp) {
    // 如果条件存在，则调用回调函数
    cb();
  }
}

Timer::ptr TimerManager::addConditionalTimer(uint64_t interval, std::function<void()> cb,
                                             std::weak_ptr<void> weak_cond,
                                             bool recur) {
  return addTimer(interval, std::bind(&OnTimer, weak_cond, cb), recur);
}

uint64_t TimerManager::get_expire() {
  RWmutex::ReadLock lock(mutex_);
  // 即将更新epoll_wait等待的时间时，即可以继续触发onTimerIntertedAtFront
  ticked_ = false;
  if (timers_.empty()) {
    return UINT64_MAX;  // 返回一个最大值(表示没有定时器)
  }
  
  // 获取最近一个定时器(定时器堆是有序的按绝对到期时间由小到大排序)
  const Timer::ptr& cur = *timers_.begin();
  uint64_t now_ms = moka::GetCurrentMs();
  if (now_ms >= cur->expire_) {
    // 定时器已经超时，说明该定时器未执行(在epoll事件循环中epoll_wait会立刻返回)
    return 0;
  } else {
    // 返回当前时间到最近一个定时器的到期时间的间隔
    return cur->expire_ - now_ms;
  }
}


void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
  uint64_t now_ms = moka::GetCurrentMs();
  std::vector<Timer::ptr> expired;
  {
    RWmutex::ReadLock lock(mutex_);
    if (timers_.empty()) {
      // 定时器堆为空直接返回
      return;
    }
  }
  RWmutex::WriteLock lock(mutex_);
  bool rollover = detectClockRollover(now_ms);
  // ->优先级比*高
  if (!rollover && (*(timers_.begin()))->expire_ > now_ms) {
    // 如果没有超时的定时器，且没有计算机本地时间没有发生变动
    return;
  }

  // 使用当前时间初始化一个定时器，用于lower_bound的比较
  Timer::ptr now_timer(new Timer(now_ms));
  // 使用Timer的比较器进行比较(因为在set中已经指定了比较器)
  // 比较expire(即找到第一个到期的定时器)
  // 找到第一个小于等于now_timer的Timer对象的迭代器

  // 如果系统时间发生了调整到一个小时之前，则触发全部定时器
  auto it = rollover? timers_.end(): timers_.lower_bound(now_timer);
  while (it != timers_.end() && (*it)->expire_ == now_ms) {
    // 继续向后移动迭代器，直到找到一个定时器的时间大于now_timer
    ++it;
  }
  // 将timers_数组中[begin, it)插入到expire中
  // 将小于等于当前到期时间的定时器都加入到返回的定时器列表中(范围插入)
  expired.insert(expired.begin(), timers_.begin(), it);
  // 更新定时器堆(范围删除)
  timers_.erase(timers_.begin(), it);

  cbs.reserve(expired.size());
  // 将超时定时器的回调函数放入传出参数中
  for (auto& timer : expired) {
    cbs.push_back(timer->cb_);
    if (timer->recur_) {
      // 如果是循环定时，再将其插入到定时器堆中
      timer->expire_ = now_ms + timer->interval_;
      timers_.insert(timer);
    } else {
      // 因为function可能使用智能指针来进行管理，置为nullptr可以减少其引用计数
      timer->cb_ = nullptr;
    }
  }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
  // 是否需要回滚时间
  bool rollover = false;  
  uint64_t one_hour = 60 * 60 * 1000;  // 毫秒为单位
  if (now_ms < previous_time_ && now_ms < (previous_time_ - one_hour)) {
    // 如果当前时间在最开始创建定时管理器的时间点之前(说明系统时间被调整了)
    // 且被调整了在了一个小时之前
    rollover = true;
  }
  // 以现在的时间为起始时间点
  previous_time_ = now_ms;
  return rollover;
}

bool TimerManager::hasTimer() {
  RWmutex::ReadLock lock(mutex_);
  return !timers_.empty();
}

}