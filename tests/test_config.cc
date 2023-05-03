#include "../moka/config.h"
#include <yaml-cpp/yaml.h>

moka::ConfigVar<int>::ptr g_int_value_config = 
  moka::Config::Lookup("system.port", (int)8080, "system port");

moka::ConfigVar<float>::ptr g_float_value_config = 
  moka::Config::Lookup("system.value", (float)10.2f, "system port2");

// 错误类型转换测试
// datas_中已经有"system.port"到int的映射，但此lookup查找"system.port"，找到后类型转换会出错
// dynamic_pointer_cast返回nullptr
// moka::ConfigVar<float>::ptr g_float_valuex_config = 
//   moka::Config::Lookup("system.port", (float)8080, "system port2");

moka::ConfigVar<std::vector<int>>::ptr g_int_vec_value_config = 
  moka::Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");

moka::ConfigVar<std::list<int>>::ptr g_int_list_value_config = 
  moka::Config::Lookup("system.int_list", std::list<int>{10,20}, "system int list");

moka::ConfigVar<std::set<int>>::ptr g_int_set_value_config = 
  moka::Config::Lookup("system.int_set", std::set<int>{20,10}, "system int set");
  
moka::ConfigVar<std::unordered_set<int>>::ptr g_int_uset_value_config = 
  moka::Config::Lookup("system.int_uset", std::unordered_set<int>{10,20}, "system int u_set");
  
moka::ConfigVar<std::map<std::string, int>>::ptr g_str_int_map_value_config = 
  moka::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"3",20}}, "system str int map");

moka::ConfigVar<std::unordered_map<std::string, int>>::ptr g_str_int_umap_value_config = 
  moka::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k",20}}, "system str int umap");

// 线性容器测试宏，toString()将其转换为字符串输出
#define XX(g_var, name, prefix) \
  { \
    auto vals = g_var->get_value(); \
    for (auto val : vals) { \
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": " << val; \
    } \
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
  }

// MAP容器测试宏
#define XX_M(g_var, name, prefix) \
{ \
  auto vals = g_var->get_value(); \
  for (auto val : vals) { \
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": {" << val.first << \
      ", " << val.second << "}"; \
  } \
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
}

// 解析yaml
void parser_yaml(const YAML::Node& node, int level) {
  if (node.IsScalar()) {
    // 标量类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << node.Scalar() << " - " << node.Type()
                                   << " - " << level;
  } else if (node.IsNull()) {
    // 空类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << "NULL - " << node.Type() << " - " << level;
  } else if (node.IsMap()) {
    // 字典类型
    for (auto it = node.begin(); it != node.end(); ++it) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << it->first << " - " << it->second.Type()
                                     << " - " << level;
      parser_yaml(it->second, level+1);
    }
  } else if (node.IsSequence()) {
    // 数组类型
    for (size_t i = 0; i < node.size(); ++i) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << i << " - " << node[i].Type()
                                     << " - " << level;
      parser_yaml(node[i], level+1);
    }
  }
}

class Person {
 public:
  Person operator=(const Person& p) {
    name_ = p.name_;
    age_ = p.age_;
    sex_ = p.sex_;
    return p;
  }
  bool operator==(const Person& oth) const {
    return name_ == oth.name_ && age_ == oth.age_ && sex_ == oth.sex_;
  }

  std::string toString() const {
    std::stringstream ss;
    ss << "{Person name=" << name_
      << " age=" << age_
      << " sex=" << sex_
      << "}";
    return ss.str();
  }
  std::string name_;
  int age_ = 0;
  bool sex_ = 0;
};

namespace moka {

// 复杂类型测试宏
#define XX_CM(g_var, name, prefix) \
  { \
    auto m = g_var->get_value(); \
    for (auto i : m) { \
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": " << i.first << " - " \
                                     << i.second.toString(); \
    } \
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": size=" << m.size(); \
  }

#define XX_CM_VEC(g_var, name, prefix) \
  { \
    auto m = g_var->get_value(); \
    for (auto i : m) { \
      std::stringstream s; \
      for (auto v : i.second) { \
        s << v.toString() << " "; \
      } \
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": " << i.first << " - " << s.str(); \
    } \
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": size=" << m.size(); \
  }

template<>
class LexicalCast<std::string, Person> {
 public:
  Person operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);  
    Person p;      
    p.name_ = node["name"].as<std::string>();
    p.age_ = node["age"].as<int>();
    p.sex_ = node["sex"].as<bool>();
    return p;
  }
};

template<>
class LexicalCast<Person, std::string> {
 public:
  std::string operator()(const Person& p) {
    YAML::Node node;
    node["name"] = p.name_;
    node["age"] = p.age_;
    node["sex"] = p.sex_;
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

}

moka::ConfigVar<Person>::ptr g_person = 
  moka::Config::Lookup("class.person", Person(), "system person");
  
moka::ConfigVar<std::map<std::string, Person>>::ptr g_person_map = 
  moka::Config::Lookup("class.map", std::map<std::string, Person>(), "system person map");

moka::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map = 
  moka::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person>>(), "system person vec_map");

// 测试自定义类
void test_class() {
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << g_person->get_name() <<" before " << g_person->get_value().toString() << "-" << g_person->toString();

  g_person->addListener(11, [](const Person& old_val, const Person& new_val){
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "old_val=" << old_val.toString()
                                   << " new val=" << new_val.toString();
  });

  XX_CM(g_person_map, map, before);
  XX_CM_VEC(g_person_vec_map, vec_map, before);
  // MOKA_LOG_INFO(MOKA_LOG_ROOT()) << g_person_vec_map->get_name() <<" before " << g_person_vec_map->toString();

  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");  // 加载yaml文件
  moka::Config::LoadFromYaml(root);

  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << g_person->get_name() << " after " << g_person->get_value().toString() << "-" << g_person->toString();
  XX_CM(g_person_map, map, after);
  XX_CM_VEC(g_person_vec_map, vec_map, after);
  // MOKA_LOG_INFO(MOKA_LOG_ROOT()) << g_person_vec_map->get_name() <<" after " << g_person_vec_map->toString();

}

// 测试yaml的解析读取
void test_yaml() {
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");  // 加载yaml文件
  parser_yaml(root, 0);  // 解析yaml文件
  // MOKA_LOG_INFO(MOKA_LOG_ROOT()) << root;
}

void test_config() {
  // 可以通过ConfigVar中的接口来获取相应的类型值或者其对应的字符串值
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before:" << g_int_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before:" << g_float_value_config->toString();

  XX(g_int_vec_value_config, int_vec, before);    // 支持vector类型
  XX(g_int_list_value_config, int_list, before);  // 支持list类型
  XX(g_int_set_value_config, int_set, before);
  XX(g_int_uset_value_config, int_uset, before);
  XX_M(g_str_int_map_value_config, str_int_map, before);
  XX_M(g_str_int_umap_value_config, str_int_umap, before);


  // 从yaml文件中加载配置项参数值到现有的配置项中(即在集合中的配置项)
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");
  moka::Config::LoadFromYaml(root);

  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after:" << g_int_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after:" << g_float_value_config->toString();

  XX(g_int_vec_value_config, int_vec, after);
  XX(g_int_list_value_config, int_list, after);
  XX(g_int_set_value_config, int_set, after);
  XX(g_int_uset_value_config, int_uset, after);
  XX_M(g_str_int_map_value_config, str_int_map, after);
  XX_M(g_str_int_umap_value_config, str_int_umap, after);
}

void test_log() {
  // 往日志管理器集合中增加system日志器，目前存在{system, root}，日志器管理器类的构造函数会初始化root日志器
  static moka::Logger::ptr system_log = MOKA_LOG_NAME("system");
  // 输出system的日志信息
  MOKA_LOG_INFO(system_log) << "hello system" << std::endl;

  // 输出当前日志管理器中存在的日志器的配置信息，输出{system, root}
  std::cout << moka::LoggerMgr::GetInstance()->toYamlString() << std::endl;

  // 加载log.yaml中的日志配置信息到LogDefine结构体中(即替换name相同的configVar的val属性息)
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/bin/conf/log.yml");
  moka::Config::LoadFromYaml(root);
  std::cout << "========================" << std::endl;

  // 测试输出更新后日志管理器中的日志器信息
  std::cout << moka::LoggerMgr::GetInstance()->toYamlString() << std::endl;
  std::cout << "========================" << std::endl;
  std::cout << root << std::endl;  // yml文件信息
  MOKA_LOG_INFO(system_log) << "hello system" << std::endl;

  // 测试更改root formatter会更改appender fmt
  system_log->set_formatter("%d - %m%n");
  MOKA_LOG_INFO(system_log) << "hello system" << std::endl;
}

int main(int agrc, char** argv) {
  // test_yaml();
  // test_config();
  // test_class();
  // test_log();

  // 测试visit
  // moka::Config::visit([](moka::ConfigVarBase::ptr var) {
  //   MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "name=" << var->get_name()
  //                           << "description=" << var->get_description()
  //                           << "typename=" << var->get_typename()
  //                           << "value=" << var->toString();
  // });
  return 0;
}