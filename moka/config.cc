#include "config.h"

namespace moka {

ConfigVarBase::ptr Config::lookupBase(const std::string& name) {
  RWmutex::ReadLock lock(get_mutex());
  auto it = get_datas().find(name);
  return it == get_datas().end()? nullptr: it->second;
}

// A:
//   B:10
// 展开为"A.B", 10
static void listAllMember(const std::string& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node>>& output) {
  if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
  }
  output.push_back(std::make_pair(prefix, node));
  if (node.IsMap()) {  // 解析yaml中的字典类型
    for (auto it = node.begin(); it != node.end(); ++it) {
      listAllMember(prefix.empty()? it->first.Scalar():
                    prefix + "." + it->first.Scalar(), it->second, output);
    }
  }
}
void Config::loadFromYaml(const YAML::Node& root) {
  std::list<std::pair<std::string, const YAML::Node>> all_nodes;
  listAllMember("", root, all_nodes);

  for (auto& i : all_nodes) {
    std::string key = i.first;
    if (key.empty()) {
      continue;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);  // 大小写不敏感
    // 约定优于配置，即便配置件里有相应的配置项，也不会去解析，只有找到约定的内容才会进行覆盖/更新
    ConfigVarBase::ptr var = lookupBase(key);  // 在配置名和配置项的映射中查找key名称对应的配置项，返回指针
    
    // 若找到，则对configvarbase对应的配置项的值进行覆盖
    if (var) {
      // 若找到，则具体配置项的参数(val属性)改变为yml文件中配置项的参数
      if (i.second.IsScalar()) {
        var->fromString(i.second.Scalar());
      } else {
        std::stringstream ss;
        ss << i.second;
        var->fromString(ss.str());
      }
    }
  }
}

void Config::visit(std::function<void(ConfigVarBase::ptr)> cb) {
  RWmutex::ReadLock lock(get_mutex());
  ConfigVarMap& m = get_datas();
  for (auto it = m.begin(); it != m.end(); ++it) {
    cb(it->second);
  }
}

}
