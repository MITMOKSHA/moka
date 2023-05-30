#ifndef __MOKA_NONCOPYABLE_H__
#define __MOKA_NONCOPYABLE_H__

namespace moka {

class Noncopyable {
 public:
  Noncopyable() = default;                  // 默认构造函数
  ~Noncopyable() = default;
  Noncopyable(const Noncopyable&) = delete; // 禁用拷贝构造函数
  Noncopyable& operator=(const Noncopyable&) = delete; // 禁用拷贝构造函数
};

}

#endif