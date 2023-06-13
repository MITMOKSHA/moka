#include <sstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include "../moka/address.h"
#include "../moka/macro.h"
#include "../moka/log.h"

namespace moka {
static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

// 辅助方法，用于二进制打印十进制值
std::string binaryPrint(uint32_t val) {
  std::stringstream ss;
  for (int i = 31; i >= 0; --i) {
    ss << ((val >> i) & 1);
  }
  return ss.str();
}

template<class T>
static T CreateMask(uint32_t prefix) {
  // 传入目标网络地址的长度(也是IP地址的前缀长度)
  // 通用的返回子网掩码(如0xFFFFFF00)的模板函数
  if (prefix == 0) {
    return ~UINT32_MAX;
  }
  return ~((1 << (sizeof(T) * 8 - prefix)) - 1);
}

int Address::get_family() const {
  return get_addr()->sa_family;
}

bool Address::GetInterfaceAddress(std::multimap<std::string, Address::ptr>& result) {
  struct ifaddrs *ifaddr, *ifa;
  // 子网掩码长度和地址族
  sa_family_t family;

  if (getifaddrs(&ifaddr) == -1) {
    MOKA_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress getifaddrs "
      << " err=" << errno << " errstr=" << strerror(errno);
      return false;
  }

  // 遍历网卡信息链表
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    IPAddress::ptr ptaddr = nullptr;
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    int prefix_len = 0;
    family = ifa->ifa_addr->sa_family;
    if (family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
      struct sockaddr_in* netmask = (struct sockaddr_in*)ifa->ifa_netmask;
      // __builtin_popcount计算1的个数
      prefix_len = __builtin_popcount(netmask->sin_addr.s_addr);
      ptaddr.reset(new IPv4Address(*addr, prefix_len));
    } else if (family == AF_INET6) {
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ifa->ifa_addr;
      char ip[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &(addr->sin6_addr), ip, INET6_ADDRSTRLEN);
      struct sockaddr_in6* netmask = (struct sockaddr_in6*)ifa->ifa_netmask;
      for (int i = 0; i < 4; ++i) {
        // __builin_popcount用于计算无符号整型数包含二进制1的个数，GNU提供的内建函数
        prefix_len += __builtin_popcount(netmask->sin6_addr.s6_addr32[i]);
      }
      ptaddr.reset(new IPv6Address(*addr, prefix_len));
    }
    // 插入<网卡名，<地址指针，掩码长度>>
    if (ptaddr) {
      result.insert({ifa->ifa_name, ptaddr});
    }
  }

  freeifaddrs(ifaddr);
  return true;
}

bool Address::GetInterfaceAddress(std::vector<Address::ptr>& result, 
    const std::string& iface) {
  if (iface.empty() || iface == "*") {
    // 返回通用的ipv4和ipv6地址
    result.push_back(Address::ptr(new IPv4Address()));
    result.push_back(Address::ptr(new IPv6Address()));
    return true;
  }

  std::multimap<std::string, Address::ptr> results;

  if (!GetInterfaceAddress(results)) {
    return false;
  }
  auto its = results.equal_range(iface);
  for (; its.first != its.second; ++its.first) {
    result.push_back(its.first->second);
  }
  return true;
}

std::string Address::toString() {
  // 子类的insert序列化显示
  std::stringstream ss;
  insert(ss);
  return ss.str();
}

bool Address::operator<(const Address& rhs) const {
  // min_len表示要比较的字节数
  socklen_t min_len = std::min(get_addrlen(), rhs.get_addrlen());
  int res = memcmp(get_addr(), rhs.get_addr(), min_len);
  if (res < 0) {
    return true;
  } else if (res > 0) {
    return false;
  } else if (get_addrlen() < rhs.get_addrlen()) {
    // 相等则比较长度(比如在multimap中，先输出ipv4)
    return true;
  }
  return false;
}

bool Address::operator==(const Address& rhs) const {
  return get_addrlen() == rhs.get_addrlen() &&
         memcmp(get_addr(), rhs.get_addr(), get_addrlen()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
  // 直接用实现==重载的版本
  return !(*this == rhs);
}

bool IPAddress::DnsToIPAddr(const char* host, const char* port, std::vector<IPAddress::ptr>& addrs) {
  struct addrinfo hints;
  struct addrinfo* res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &hints, &res) != 0) {
    MOKA_LOG_WARN(g_logger) << "ip based DNS server error!";
    return false;
  }
  for (auto p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) {
      char ipstr[INET_ADDRSTRLEN];
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      if (inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, INET_ADDRSTRLEN) == nullptr) {
        MOKA_LOG_ERROR(g_logger) << "domain convert to ipv4 error!";
        return false;
      }
      addrs.push_back(IPv4Address::ptr(new IPv4Address(ipstr, ntohs(ipv4->sin_port))));
    } else if (p->ai_family == AF_INET6) {
      char ipstr[INET6_ADDRSTRLEN];
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      if (inet_ntop(res->ai_family, &(ipv6->sin6_addr), ipstr, INET6_ADDRSTRLEN) == nullptr) {
        MOKA_LOG_ERROR(g_logger) << "domain convert to ipv6 error!";
        return false;
      }
      addrs.push_back(IPv6Address::ptr(new IPv6Address(ipstr, ntohs(ipv6->sin6_port))));
    }
  }
  // 释放addrinfo链表
  freeaddrinfo(res);
  return true;
}

IPv4Address::IPv4Address() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  prefix_len_ = 32;
}

IPv4Address::IPv4Address(const sockaddr_in& address, uint32_t prefix_len) {
  addr_ = address;
  prefix_len_ = prefix_len;
}

IPv4Address::IPv4Address(const char* address, uint16_t port, uint32_t prefix_len) {
  memset(&addr_, 0, sizeof(addr_));
  prefix_len_ = prefix_len;
  addr_.sin_family = AF_INET;
  // 需要转换成网络字节序
  addr_.sin_port = htons(port);
  // inet_pton将字符串的IP地址转换为网络字节序的二进制形式
  int ret = inet_pton(AF_INET, address, &(addr_.sin_addr));
  if (ret == 0) {
    // IP地址错误，会自动设置为全0的通用地址
    MOKA_LOG_WARN(g_logger) << "Invalid IPv4Address.";
  } else if (ret == -1) {
    // 地址族错误
    MOKA_LOG_ERROR(g_logger) << "IPv4Address family error! errno=" << strerror(errno);
  }
}

const sockaddr* IPv4Address::get_addr() const {
  return (struct sockaddr*)&addr_;
}

socklen_t IPv4Address::get_addrlen() const {
  return sizeof(addr_);
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
  // +1表示字符串结束符
  char buf[INET_ADDRSTRLEN];
  // 将地址转换为点分十六进制字符表示
  inet_ntop(AF_INET, &(addr_.sin_addr), buf, INET_ADDRSTRLEN);
  os << buf << "/" << prefix_len_ << ":" << ntohs(addr_.sin_port);
  return os;
}

IPAddress::ptr IPv4Address::get_broadcast_addr(uint32_t prefix_len) {
  // 广播地址即主机地址全为1
  if (prefix_len > 32) {
    // error
    return nullptr;
  }
  sockaddr_in b_addr(addr_);
  // MOKA_LOG_DEBUG(g_logger) << binaryPrint(((~CreateMask<uint32_t>(prefix_len))));
  // 将主机地址置为全1
  b_addr.sin_addr.s_addr |= htonl(~CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(b_addr, prefix_len));
}

IPAddress::ptr IPv4Address::get_network_addr(uint32_t prefix_len) {
  if (prefix_len > 32) {
    // error
    return nullptr;
  }
  sockaddr_in b_addr(addr_);
  // 这里和子网掩码与了一下
  b_addr.sin_addr.s_addr &= htonl(CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(b_addr, prefix_len));
}

IPAddress::ptr IPv4Address::get_netmask(uint32_t prefix_len) {
  // 以ipv4地址的形式返回子网掩码
  sockaddr_in mask;
  memset(&mask, 0, sizeof(mask));
  mask.sin_family = AF_INET;
  mask.sin_addr.s_addr = htonl(CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(mask, prefix_len));
}

uint16_t IPv4Address::get_port() const {
  return ntohs(addr_.sin_port);
}

void IPv4Address::set_port(uint16_t v) {
  addr_.sin_port = htons(v);
}

IPv6Address::IPv6Address() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin6_family = AF_INET6;
  prefix_len_ = 128;
}

IPv6Address::IPv6Address(const sockaddr_in6& address, uint32_t prefix_len) {
  addr_ = address;
  prefix_len_ = prefix_len;
}
  
IPv6Address::IPv6Address(const char* address, uint16_t port, uint32_t prefix_len) {
  memset(&addr_, 0, sizeof(addr_));
  prefix_len_ = prefix_len;
  addr_.sin6_family = AF_INET6;
  // 需要转换成网络字节序
  addr_.sin6_port = htons(port);
  // 保证输入地址的正确性
  int ret = inet_pton(AF_INET6, address, &(addr_.sin6_addr));
  if (ret == 0) {
    // IP地址错误，会自动设置为全0的通用地址
    MOKA_LOG_WARN(g_logger) << "Invalid IPv6Address.";
  } else if (ret == -1) {
    // 地址族错误
    MOKA_LOG_ERROR(g_logger) << "IPv6Address family error! errno=" << strerror(errno);
  }
}

const sockaddr* IPv6Address::get_addr() const {
  // 转换为通用socket地址
  return (struct sockaddr*)&addr_;
}

socklen_t IPv6Address::get_addrlen() const {
  return sizeof(addr_);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
  char buf[INET6_ADDRSTRLEN];
  // 将地址转换为点分十六进制字符表示
  inet_ntop(AF_INET6, &(addr_.sin6_addr), buf, INET6_ADDRSTRLEN);
  os << buf << "/" << prefix_len_ << ":" << ntohs(addr_.sin6_port);
  return os;
}

IPAddress::ptr IPv6Address::get_broadcast_addr(uint32_t prefix_len) {
  // IPv6没有广播地址，使用多播来替代
  // ff02::1所有节点多播地址，用于向同一链路上的所有节点发送数据包。
  // ff02::2所有路由器多播地址，用于向同一链路上的所有路由器发送数据包。
  // ff02::9Rapid Commit多播地址，用于 DHCPv6 协议中的快速配置。
  // ff02::c移动 IPv6（Mobile IPv6）多播地址，用于移动节点的路由更新。
  return IPv6Address::ptr(new IPv6Address("ff02::1"));
}

IPAddress::ptr IPv6Address::get_network_addr(uint32_t prefix_len) {
  if (prefix_len > 128) {
    return nullptr;
  }
  sockaddr_in6 b_addr(addr_);
  // 将余数部分的掩码处理一下即可，其余高位不变
  b_addr.sin6_addr.s6_addr[prefix_len/8] &= (CreateMask<uint8_t>(prefix_len%8));
  for(int i = prefix_len / 8 + 1; i < 16; ++i) {
    // 将主机地址全部置为0 
    b_addr.sin6_addr.s6_addr[i] = 0x00;
  }
  return IPv6Address::ptr(new IPv6Address(b_addr, prefix_len));
}

IPAddress::ptr IPv6Address::get_netmask(uint32_t prefix_len) {
  sockaddr_in6 mask; 
  memset(&mask, 0, sizeof(mask));
  mask.sin6_family = AF_INET6;
  for (uint32_t i = 0; i < prefix_len/8; ++i) {
    mask.sin6_addr.s6_addr[i] = 0xFF;
  }
  mask.sin6_addr.s6_addr[prefix_len/8] |= (CreateMask<uint8_t>(prefix_len%8));
  for(int i = prefix_len/8 + 1; i < 16; ++i) {
    // 将主机地址全部置为0 
    mask.sin6_addr.s6_addr[i] = 0x00;
  }
  return IPv6Address::ptr(new IPv6Address(mask, prefix_len));
}

uint16_t IPv6Address::get_port() const {
  return ntohs(addr_.sin6_port);
}

void IPv6Address::set_port(uint16_t v) {
  addr_.sin6_port = htons(v);
}

UnixAddress::UnixAddress() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sun_family = AF_UNIX;
  addr_len_ = sizeof(sa_family_t) + sizeof(addr_.sun_path)-1;
}

UnixAddress::UnixAddress(const std::string& path) {
  // sun_path为套接字文件路径名
  memset(&addr_, 0, sizeof(addr_));
  addr_.sun_family = AF_UNIX;
  addr_len_ = path.size() + 1;  // 包括结束符
  if (!path.empty() && path[0] == '\0') {
    // 处理特殊的地址
    --addr_len_;
  }
  if (addr_len_ > sizeof(addr_.sun_path)) {  // sun_path的长度默认为108个字节
    throw std::logic_error("path too long");
  }
  // 初始化sun_path
  memcpy(addr_.sun_path, path.c_str(), addr_len_);
  addr_len_ += sizeof(sa_family_t);
}

const sockaddr* UnixAddress::get_addr() const {
  return (struct sockaddr*)&addr_;
}

socklen_t UnixAddress::get_addrlen() const {
  return addr_len_;
}
// 用于序列化输出
std::ostream& UnixAddress::insert(std::ostream& os) const {
  if (addr_len_ > sizeof(sa_family_t) && addr_.sun_path[0] == '\0') {
    return os << "\\0" << std::string(addr_.sun_path + 1, addr_len_ - sizeof(sa_family_t) - 1);
  }
  return os << addr_.sun_path;
}

UnknowAddress::UnknowAddress(int family) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sa_family = family;
}

const sockaddr* UnknowAddress::get_addr() const {
  return &addr_;
}

socklen_t UnknowAddress::get_addrlen() const {
  return sizeof(addr_);
}
// 用于序列化输出
std::ostream& UnknowAddress::insert(std::ostream& os) const {
  os << "[UnknownAddreess family=]" << addr_.sa_family << "]";
  return os;
}

}