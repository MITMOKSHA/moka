#ifndef __MOKA_TIMER_H__
#define __MOKA_TIMER_H__

#include <memory>
#include <set>
#include <functional>
#include <vector>

#include "thread.h"

namespace moka {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;
 public:
  using ptr = std::shared_ptr<Timer>;
  bool cancel();                                  // 从定时器堆中移除当前定时器
  bool refresh();                                 // 更新当前定时器的超时时间点
  bool reset(uint64_t interval, bool from_now);   // 重置当前定时器时间(from_now即是否要从现在开始重置)

 private:
  Timer(uint64_t interval, std::function<void()> cb, bool recur, TimerManager* manager);
  Timer(uint64_t expire);

 private:
  bool recur_ = false;                // 是否循环定时
  uint64_t interval_ = 0;             // 执行周期
  uint64_t expire_ = 0;               // 超时时间点(绝对时间)
  std::function<void()> cb_;          // 回调函数
  TimerManager* manager_ = nullptr;   // 定时器管理器
  // set比较器(根据绝对超时时间点)
  struct Comparator {
    bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
  };
};

class TimerManager {
 friend class Timer;
 public:
  TimerManager();
  virtual ~TimerManager();

  // 添加定时器到定时器堆
  Timer::ptr addTimer(uint64_t interval, std::function<void()> cb, bool recur = false);
  // 当条件存在时才触发
  Timer::ptr addConditionalTimer(uint64_t interval, std::function<void()> cb,
          std::weak_ptr<void> weak_cond, bool recur = false);
  uint64_t get_expire();                                        // 获取当前时间到最近一个定时器的超时时间点的间隔
  void listExpiredCb(std::vector<std::function<void()>>& cbs);  // 获取已经超时的定时器的回调函数列表
  
 protected:
  virtual void onTimerInsertedAtFront() = 0;                  // 当有新的定时器插入到定时器首部，执行该函数
  void addTimer(Timer::ptr timer);                            // 往定时器堆中加入定时器
  bool hasTimer();                                            // 定时器堆中是否存在定时器
 private:
  // 检测电脑的时间改变，并适应
  bool detectClockRollover(uint64_t now_ms);
 private:
  RWmutex mutex_;
  // set自定义比较器类
  std::set<Timer::ptr, Timer::Comparator> timers_;  // 定时器最小堆
  bool ticked_ = false;                             // 避免频繁触发onTimerInsertedAtFront
  uint64_t previous_time_ = 0;                      // 记录创建定时管理器时的系统时间
};

}
#endif