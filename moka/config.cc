#include "config.h"

namespace moka {

ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
  RWmutex::ReadLock lock(GetMutex());
  auto it = GetDatas().find(name);
  return it == GetDatas().end()? nullptr: it->second;
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
  if (node.IsMap()) {
    // 只将字典类型的yaml展开放入到output中
    for (auto it = node.begin(); it != node.end(); ++it) {
      listAllMember(prefix.empty()? it->first.Scalar():
                    prefix + "." + it->first.Scalar(), it->second, output);
    }
  }
}

void Config::LoadFromYaml(const YAML::Node& root) {
  std::list<std::pair<std::string, const YAML::Node>> all_nodes;
  listAllMember("", root, all_nodes);

  for (auto& i : all_nodes) {
    std::string key = i.first;
    if (key.empty()) {
      continue;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);  // 大小写不敏感
    // 约定优于配置，即便配置文件里有相应的配置项，也不会去解析，只有找到约定的内容才会进行覆盖/更新
    ConfigVarBase::ptr var = LookupBase(key);  // 在配置名和配置项的映射中查找key名称对应的配置项，返回指针
    
    // 若找到，则对configvarbase对应的配置项的值进行更新(当前配置集合中没有的项，即便配置文件有，也不进行覆盖)
    if (var) {
      if (i.second.IsScalar()) {
        // 如果该节点是标量
        var->fromString(i.second.Scalar());
      } else {
        // 如果不是标量类型则直接通过字符串流获取缓冲区内容输出
        std::stringstream ss;
        ss << i.second;
        var->fromString(ss.str());
      }
    }
  }
  // for (auto i : GetDatas()) {
  //   MOKA_LOG_INFO(MOKA_LOG_ROOT()) << i.first;
  // }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
  RWmutex::ReadLock lock(GetMutex());
  ConfigVarMap& m = GetDatas();
  for (auto it = m.begin(); it != m.end(); ++it) {
    cb(it->second);
  }
}

}
