#ifndef __MOKA_CONFIG_H__
#define __MOKA_CONFIG_H__

#include <memory>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "log.h"
#include "thread.h"

namespace moka {

// F from_type, T to_type
// 仿函数类
template<class F, class T>
class LexicalCast {
 public:
  T operator()(const F& v) {
    return boost::lexical_cast<T>(v);  // 基本类型的转换
  }
};

// LexicalCast模板类的偏特化
// vector
template<class T>
class LexicalCast<std::string, std::vector<T>> {
 public:
  std::vector<T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  // 将string类型转为yaml
    typename std::vector<T> res;      // typename告诉编译器std::vector<T>为类型名而不是成员变量/函数名
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");  // 设置一个新的string buffer
      ss << node[i];
      // 将字符串都转换成类型T加入T类型数组中
      res.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::vector<T>, std::string> {
 public:
  std::string operator()(const std::vector<T>& v) {
    // 将vector转换为YAML类型，再输出为字符串
    YAML::Node node;
    for (auto i : v) {
      // 用到基本类型的转换
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;     // 输出yaml类型字符串("- ")
    return ss.str();
  }
};

// list
template<class T>
class LexicalCast<std::string, std::list<T>> {
 public:
  std::list<T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    typename std::list<T> res;      
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");  
      ss << node[i];
      res.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::list<T>, std::string> {
 public:
  std::string operator()(const std::list<T>& v) {
    YAML::Node node;
    for (auto i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

// set
template<class T>
class LexicalCast<std::string, std::set<T>> {
 public:
  std::set<T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    typename std::set<T> res;      
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");  
      ss << node[i];
      res.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::set<T>, std::string> {
 public:
  std::string operator()(const std::set<T>& v) {
    YAML::Node node;
    for (auto i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

// unordered_set
template<class T>
class LexicalCast<std::string, std::unordered_set<T>> {
 public:
  std::unordered_set<T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    typename std::unordered_set<T> res;      
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");  
      ss << node[i];
      res.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string> {
 public:
  std::string operator()(const std::unordered_set<T>& v) {
    YAML::Node node;
    for (auto i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

// map
template<class T>
class LexicalCast<std::string, std::map<std::string, T>> {
 public:
  std::map<std::string, T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    typename std::map<std::string, T> res;      
    std::stringstream ss;
    for (auto it : node) {
      ss.str("");  
      ss << it.second;
      res.insert(std::make_pair(it.first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::map<std::string, T>& v) {
    YAML::Node node;
    for (auto i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

// unordered_map
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
 public:
  std::unordered_map<std::string, T> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    typename std::unordered_map<std::string, T> res;      
    std::stringstream ss;
    for (auto it : node) {
      ss.str("");  
      ss << it.second;
      res.insert(std::make_pair(it.first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return res;
  }
};

template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::unordered_map<std::string, T>& v) {
    YAML::Node node;
    for (auto i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

// 配置项基类
class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;
  ConfigVarBase(const std::string& name, const std::string& description = "")
      : name_(name), description_(description) {
    std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);  // 将name_都转换成小写(大小写不敏感)
  }
  virtual ~ConfigVarBase() {}

  const std::string& get_name() const { return name_; }
  const std::string& get_description() const { return description_; }

  virtual std::string toString() = 0;
  virtual bool fromString(const std::string& val) = 0;   // 解析成字符串
  virtual const std::string get_typename() const = 0;

 private:
  std::string name_;
  std::string description_;
};


// 具体配置项类
//FromStr T opeator()(const std::string&)
//ToStr std::string operator()(const T&)
template<class T, class FromStr = LexicalCast<std::string, T>,
                  class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVar>;
  using chan_cb = std::function<void(const T& old_val, const T& new_val)>;

  ConfigVar(const std::string& name, const T& defualt_value, const std::string& description = "") 
      : ConfigVarBase(name, description), val_(defualt_value) {
      // 子类显示调用直接父类的构造函数
  }

  // 在toString和fromString操纵的都是val_属性
  // 将val_转换为字符串类型
  virtual std::string toString() override;
  // 将字符串类型转换为T
  virtual bool fromString(const std::string& val) override;
  virtual const std::string get_typename() const override { return typeid(T).name(); }

  const T get_value() {
    RWmutex::ReadLock lock(mutex_);
    return val_;
  }

  // 在fromString中调用设置值，此时调用回调函数通知变更
  void set_value(const T& val) {
    {
      RWmutex::ReadLock lock(mutex_);
      if (val == val_) {
        // 没有发生变化
        return;
      }
      for (auto i : cbs_) {
        i.second(val_, val);  // 调用变更回调函数
      }
    }
    RWmutex::WriteLock lock(mutex_);
    val_ = val;
  }

  // 观察者模式
  void addListener(uint64_t key, chan_cb cb) {
    RWmutex::WriteLock lock(mutex_);
    cbs_[key] = cb;
  }

  void delListener(uint64_t key) {
    RWmutex::WriteLock lock(mutex_);
    cbs_.erase(key);
  }

  void clearListener() {
    RWmutex::WriteLock lock(mutex_);
    cbs_.clear();
  }

  chan_cb get_listener(uint64_t key) {
    RWmutex::ReadLock lock(mutex_);
    return cbs_.find(key) == cbs_.end()? nullptr: cbs_[key];
  }

 private:
  RWmutex mutex_;
  T val_;   // 配置项参数值
  std::map<uint64_t, chan_cb> cbs_;  // 变更回调函数集合(使用map类型可以通过key来操作)
};

// 配置项管理类(单例模式)
class Config {
 public:
  using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;  // 配置名是唯一的

  // 根据配置名称查询配置项(i.e. ConfigVar)
  template<class T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name,  // 查找配置名是否在集合中，不在则新建该配置项(配置名+参数值)
      const T& default_val, const std::string& description = "");

  static void LoadFromYaml(const YAML::Node& root);                  // 从yaml文件中加载配置参数信息
  static ConfigVarBase::ptr LookupBase(const std::string& name);     // 查找集合是否有当前配置名对应的配置项

  static void Visit(std::function<void(ConfigVarBase::ptr)> cb);     // 用户自定义测试(测试配置项集合所有配置项的相关信息)

 private:
  static ConfigVarMap& GetDatas() {
    // 定义局部静态变量保证声明顺序
    static ConfigVarMap datas;  // 对象间共享，目前系统运行包含的配置项，存储配置名到具体配置项的映射集合
    return datas;
  }
  static RWmutex& GetMutex() {
    static RWmutex s_mutex;
    return s_mutex;
  }
};



template<class T, class FromStr, class ToStr>
std::string ConfigVar<T, FromStr, ToStr>::toString() {
  try {
    RWmutex::ReadLock lock(mutex_);
    // 调用仿函数
    return ToStr()(val_);
  } catch (std::exception& e) {
    // 异常则输出log
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ConfigVar::toString exception"
                                    << e.what() << " convert: "
                                    << typeid(val_).name() << " to string";  // typeid.name()可以输出具体的类型名称
  }
  return "";
} 

template<class T, class FromStr, class ToStr>
bool ConfigVar<T, FromStr, ToStr>::fromString(const std::string& val) {
  try {
    set_value(FromStr()(val));  // FromStr将string转换成T类型
    return true;
  } catch (std::exception& e) {
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ConfigVar::toString exception"
                                    << e.what() << " convert: string to"
                                    << typeid(val_).name();
  }
  return false;
}

template<class T>
typename ConfigVar<T>::ptr Config::Lookup(const std::string& name,
    const T& default_val, const std::string& description) {
  RWmutex::WriteLock lock(GetMutex());
  auto it = GetDatas().find(name);
  if (it != GetDatas().end()) {
    // 存在
    auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    if (tmp) {
      // 强转成功
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "Lookup name=" << name << " exists";
      return tmp;
    } else {
      // 强转失败(同名key对应的值类型不同则会强转失败)
      MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "lookup name=" << name
                                      << " exist but type is not " 
                                      << typeid(T).name() << ", real type is " 
                                      << it->second->get_typename() << " and value is"
                                      << it->second->toString();
      return nullptr;
    }
  }
  if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") !=
      std::string::npos) {
    // 处理配置名命名错误
    // 如果find_first_not_of没找到则返回string::npos
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "Lookup name invalid " << name;
    throw std::invalid_argument(name);
  }
  // 不存在，新建配置名和配置项指针的映射并放入datas_集合中。保证配置模块定义即可用的特性
  typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_val, description));
  GetDatas()[name] = v;  // 以数组的方式插入哈希表中
  return v;
}

}

#endif