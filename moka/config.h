#ifndef __MOKA_CONFIG_H__
#define __MOKA_CONFIG_H__

#include <memory>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include "log.h"

namespace moka {

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
  virtual bool fromString(const std::string& val) = 0;   // 解析字符串

 private:
  std::string name_;
  std::string description_;
};

// 具体配置项类
template<class T>
class ConfigVar : public ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVar>;
  ConfigVar(const std::string& name, const T& defualt_value, const std::string& description = "") 
      : ConfigVarBase(name, description), val_(defualt_value) {
      // 子类显示调用直接父类的构造函数
  }

  // 将val_转换为字符串类型
  virtual std::string toString() override;
  // 将字符串类型转换为T
  virtual bool fromString(const std::string& val) override;

  const T get_value() const { return val_; }
  void set_value(const T& val) { val_ = val; }

 private:
  T val_;   // 配置项参数值
};

// ConfigVarBase管理类(单例模式)
class Config {
 public:
  using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;  // 配置名是唯一的

  // 根据配置名称查询配置项(i.e. ConfigVar)
  template<class T>
  static typename ConfigVar<T>::ptr lookup(const std::string& name,  // 查找配置名是否在集合中，不在则新建该配置项(配置名+参数值)
      const T& default_val, const std::string& description = "");

  static void loadFromYaml(const YAML::Node& root);                  // 从yaml文件中加载配置参数信息
  static ConfigVarBase::ptr lookupBase(const std::string& name);     // 查找集合是否有当前配置名对应的配置项
 private:
  template<class T>
  static typename ConfigVar<T>::ptr lookup(const std::string& name);

  static ConfigVarMap datas_;  // 对象间共享，目前系统运行包含的配置项，存储配置名到具体配置项的映射集合
};

template<class T>
std::string ConfigVar<T>::toString() {
  try {
    return std::to_string(val_);
  } catch (std::exception& e) {
    // 异常则输出log
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ConfigVar::toString exception" <<
      e.what() << " convert: " << typeid(val_).name() << " to string";  // typeid.name()可以输出具体的类型名称
  }
  return "";
} 

template<class T>
bool ConfigVar<T>::fromString(const std::string& val) {
  try {
    // c_str转换为const_char*
    val_ = boost::lexical_cast<T>(val);
    return true;
  } catch (std::exception& e) {
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ConfigVar::toString exception" <<
      e.what() << " convert: string to" << typeid(val_).name();
  }
  return false;
}


template<class T>
typename ConfigVar<T>::ptr Config::lookup(const std::string& name,
    const T& default_val, const std::string& description) {
  auto tmp = lookup<T>(name);  // 查找配置名对应的配置项是否在集合中
  if (tmp) {
    // 存在
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "Lookup name=" << name << " exists";
    return tmp;
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
  datas_[name] = v;  // 以数组的方式插入哈希表中
  return v;
}

template<class T>
typename ConfigVar<T>::ptr Config::lookup(const std::string& name) {
  auto it = datas_.find(name);
  if (it == datas_.end()) {
    return nullptr;
  }
  return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
}

}

#endif