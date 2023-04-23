#ifndef __MOKA_SINGLETON_H__
#define __MOKA_SINGLETON_H__

#include <memory>

namespace moka {

template<class T, class X = void, int N = 0>
class Singleton {
 public:
  static T* get_instance() {
    static T v;
    return &v;
  }
};

template<class T, class X = void, int N = 0>
class SingletonPtr {
  public:
    static std::shared_ptr<T> ger_instance() {
      static std::shared_ptr<T> v(new T);
      return v;
    }
};
  
}

#endif