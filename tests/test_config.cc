#include <yaml-cpp/yaml.h>

#include "../moka/config.h"
#include "../moka/macro.h"

// 错误类型转换测试(会因为空指针出现segfault)
// datas_中已经有"system.port"到int的映射，但此lookup查找"system.port"，找到后类型转换会出错
// dynamic_pointer_cast返回nullptr
// moka::ConfigVar<float>::ptr g_float_value_error_config = 
//   moka::Config::Lookup("system.port", (float)8080, "system port2");

// 全局供所有测试函数测试的配置项，使用Lookup新建配置项并加入配置项集合中
moka::ConfigVar<int>::ptr g_int_value_config = 
  moka::Config::Lookup("system.port", (int)8080, "system port");

moka::ConfigVar<float>::ptr g_float_value_config = 
  moka::Config::Lookup("system.value", (float)10.2f, "system value");

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

// 解析yaml
void parser_yaml(const YAML::Node& node, int level) {
  // level为缩进级别
  if (node.IsScalar()) {
    // 标量类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << node.Scalar() << " - " << node.Type();
  } else if (node.IsNull()) {
    // 空类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << "NULL - " << node.Type();
  } else if (node.IsMap()) {
    // 字典类型
    for (auto it = node.begin(); it != node.end(); ++it) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << it->first << " - " << it->second.Type();
      parser_yaml(it->second, level+1);
    }
  } else if (node.IsSequence()) {
    // 数组类型
    for (size_t i = 0; i < node.size(); ++i) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << i << " - " << node[i].Type();
      parser_yaml(node[i], level+1);
    }
  }
}

void test_loadyaml() {
  // 测试yaml文件内容的解析读取
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");  // 加载yaml文件
  // 将yaml文件信息更新到配置项中
  moka::Config::LoadFromYaml(root);
  
  // 具体测试值参见test.yml文件
  moka::ConfigVarBase::ptr sys_val = moka::Config::LookupBase("system.value");
  MOKA_ASSERT(sys_val->get_name() == "system.value");
  // int类型为标量
  MOKA_ASSERT(sys_val != nullptr);

  MOKA_ASSERT(sys_val->get_description() == "system value");
  MOKA_ASSERT(moka::Config::LookupBase("system.port") != nullptr);
  // 不存在该配置项
  MOKA_ASSERT(moka::Config::LookupBase("system.ports") == nullptr);
  // 测试解析是否正确
  parser_yaml(root, 0);
}

void test_config() {
  // 测试初始化配置项，然后从yaml文件中加载是否会更新内容
  // 顺序容器测试宏，toString()将STL容器转换为字符串输出
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

  // 可以通过ConfigVar中的接口来获取相应的类型值或者其对应的字符串值
  auto before_int_val = g_int_value_config->get_value();
  auto before_float_val = g_float_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before int_val:" << before_int_val;
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before float_val:" << before_float_val;

  // 利用宏输出复杂类型配置项的信息
  XX(g_int_vec_value_config, int_vec, before);    // 支持vector类型
  XX(g_int_list_value_config, int_list, before);  // 支持list类型
  XX(g_int_set_value_config, int_set, before);
  XX(g_int_uset_value_config, int_uset, before);
  XX_M(g_str_int_map_value_config, str_int_map, before);
  XX_M(g_str_int_umap_value_config, str_int_umap, before);


  // 从yaml文件中加载配置项参数值到现有的配置项中(即在集合中的配置项)
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");
  moka::Config::LoadFromYaml(root);

  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after int_val:" << g_int_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after float_val:" << g_float_value_config->toString();

  MOKA_ASSERT(before_int_val != g_int_value_config->get_value());
  MOKA_ASSERT(before_float_val != g_float_value_config->get_value());

  XX(g_int_vec_value_config, int_vec, after);
  XX(g_int_list_value_config, int_list, after);
  XX(g_int_set_value_config, int_set, after);
  XX(g_int_uset_value_config, int_uset, after);
  XX_M(g_str_int_map_value_config, str_int_map, after);
  XX_M(g_str_int_umap_value_config, str_int_umap, after);
}



class Person {
 public:
  // 因为在set_value时需要用到赋值运算符和比较运算符，因此需要重载这两个运算符
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

template<>
class moka::LexicalCast<std::string, Person> {
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

// 模板全特化
template<>
class moka::LexicalCast<Person, std::string> {
public:
  std::string operator()(const Person& p) {
    YAML::Node node;
    node["name"] = p.name_;
    node["age"] = p.age_;
    node["sex"] = p.sex_;
    std::stringstream ss;
    // 将node转换为string(<<)
    ss << node;
    return ss.str();
  }
};

void test_class() {
  // 测试自定义类作为配置项

  // 复杂类型测试宏，用于打印变更前后信息
  #define XX_PERSON(g_var, name, prefix) \
    { \
      auto m = g_var->get_value(); \
      for (auto i : m) { \
        MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": " << i.first << " - " \
                                      << i.second.toString(); \
      } \
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << #prefix " " #name << ": size=" << m.size(); \
    }
  #define XX_PERSON_VEC(g_var, name, prefix) \
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

  // 每一个配置项都需要保证两个方向的仿函数存在。
  moka::ConfigVar<Person>::ptr g_person = 
    moka::Config::Lookup("class.person", Person(), "system person");
    
  moka::ConfigVar<std::map<std::string, Person>>::ptr g_person_map = 
    moka::Config::Lookup("class.map", std::map<std::string, Person>(), "system person map");

  moka::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map = 
    moka::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person>>(), "system person vec_map");
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << g_person->get_name() <<" before " << g_person->get_value().toString() << "-" << g_person->toString();

  // 配置项变更事件，输出更改之后的值对比
  g_person->addListener(11, [](const Person& old_val, const Person& new_val){
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "old_val=" << old_val.toString();
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "new_val=" << new_val.toString();
  });
  g_person_map->addListener(22, [](const std::map<std::string, Person>& old_val,
          const std::map<std::string, Person>& new_val){
    std::stringstream ss;
    for (auto val : old_val) {
      ss << val.second.toString();
    }
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "old_map_val=" << ss.str().size()? ss.str():"empty";
    ss.clear();
    for (auto val : new_val) {
      ss << val.second.toString();
    }
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "new_map_val=" << ss.str();
  });
  XX_PERSON(g_person_map, map, before);
  XX_PERSON_VEC(g_person_vec_map, vec_map, before);

  YAML::Node root = YAML::LoadFile("/home/moksha/moka/tests/test.yml");  // 加载yaml文件
  moka::Config::LoadFromYaml(root);

  XX_PERSON(g_person_map, map, after);
  XX_PERSON_VEC(g_person_vec_map, vec_map, after);
  
  // 测试操作事件的API
  // 刚添加的事件还存在
  MOKA_ASSERT(g_person->get_listener(11) != nullptr);
  // 删除该事件
  g_person->delListener(11);
  // 已经获取不到该事件了
  MOKA_ASSERT(g_person->get_listener(11) == nullptr);
  g_person_map->clearListener();
  MOKA_ASSERT(g_person_map->get_listener(22) == nullptr);
}


void test_log() {
  // 往日志管理器日志集合中增加名为system的日志器
  // 目前存在{system, root}，日志器管理器类的构造函数会初始化root日志器
  static moka::Logger::ptr system_log = MOKA_LOG_NAME("system");

  // 输出当前日志管理器中存在的日志器的配置信息，即输出{system, root}
  std::cout << moka::LoggerMgr::GetInstance()->toYamlString() << std::endl;

  // 加载log.yaml中的日志配置信息到LogDefine结构体中
  // 同时响应事件将LogDefine结构更新到log中的g_log_defines配置项集合中
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/bin/conf/log.yml");
  moka::Config::LoadFromYaml(root);
  std::cout << "========================" << std::endl;

  // 测试输出更新后日志管理器中的日志器集合中所有日志器的信息
  std::cout << moka::LoggerMgr::GetInstance()->toYamlString() << std::endl;
  std::cout << "========================" << std::endl;
  std::cout << root << std::endl;  // yml文件信息
  MOKA_LOG_INFO(system_log) << "hello system" << std::endl;

  // 测试更改root formatter会更改appender fmt
  system_log->set_formatter("%d - %m%n");
  MOKA_LOG_INFO(system_log) << "hello system" << std::endl;
}

void test_visit() {
  // 测试配置管理器中配置项集合的相关信息，自定义函数测试
  moka::Config::Visit([](moka::ConfigVarBase::ptr logger) {
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "name=" << logger->get_name()
                            << " description=" << logger->get_description()
                            << " typename=" << logger->get_typename()
                            << " value=" << logger->toString();
  });
}

int main(int agrc, char** argv) {
  // test_loadyaml();
  // test_config();
  // test_class();
  // test_log();
  test_visit();
  return 0;
}