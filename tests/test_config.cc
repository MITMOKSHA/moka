#include "../moka/config.h"
#include <yaml-cpp/yaml.h>

moka::ConfigVar<int>::ptr g_int_value_config = 
  moka::Config::lookup("system.port", (int)8080, "system port");

moka::ConfigVar<float>::ptr g_float_value_config = 
  moka::Config::lookup("system.value", (float)10.2f, "system port2");

// 解析yaml
void parser_yaml(const YAML::Node& node, int level) {
  if (node.IsScalar()) {
    // 标量类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << node.Scalar() << " - " << node.Type() <<
      " - " << level;
  } else if (node.IsNull()) {
    // 空类型
    MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << "NULL - " << node.Type() << " - " << level;
  } else if (node.IsMap()) {
    // 字典类型
    for (auto it = node.begin(); it != node.end(); ++it) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << it->first << " - " << it->second.Type() <<
      " - " << level;
      parser_yaml(it->second, level+1);
    }
  } else if (node.IsSequence()) {
    // 数组类型
    for (size_t i = 0; i < node.size(); ++i) {
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << std::string(level*4, ' ') << i << " - " << node[i].Type() <<
      " - " << level;
      parser_yaml(node[i], level+1);
    }
  }
}

void test_yaml() {
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/bin/conf/log.yml");  // 加载yaml文件
  parser_yaml(root, 0);  // 解析yaml文件
  // MOKA_LOG_INFO(MOKA_LOG_ROOT()) << root;
}

void test_config() {
  // 可以通过ConfigVar中的接口来获取相应的类型值或者其对应的字符串值
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before:" << g_int_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "before:" << g_float_value_config->toString();

  // 从yaml文件中加载配置项参数值到现有的配置项中(即在集合中的配置项)
  YAML::Node root = YAML::LoadFile("/home/moksha/moka/bin/conf/log.yml");
  moka::Config::loadFromYaml(root);

  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after:" << g_int_value_config->get_value();
  MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "after:" << g_float_value_config->toString();
}

int main(int agrc, char** argv) {
  // test_yaml();
  test_config();
  return 0;
}