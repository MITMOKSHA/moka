#ifndef __MOKA_ADDRESS_H__
#define __MOKA_ADDRESS_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

#define CREATE_IPV4(addr_str, prefix) \
  moka::IPv4Address::ptr(new moka::IPv4Address(addr_str, prefix));

#define CREATE_IPV6(addr_str, prefix) \
  moka::IPv6Address::ptr(new moka::IPv6Address(addr_str, prefix));

#define LOOKUP_IPV4_ADDR(domain, port) \
  moka::IPAddress::LookupIPv4Addr(domain, port);

namespace moka {

class Address {
 public:
  using ptr = std::shared_ptr<Address>;
  virtual ~Address() {}
  int get_family() const;

  // 获取网卡和本地回环的IP地址
  static bool GetInterfaceAddress(std::multimap<std::string, Address::ptr>& result);
  // 获取指定网卡名称的IP地址
  static bool GetInterfaceAddress(std::vector<Address::ptr>& result, 
      const std::string& iface);

  virtual const sockaddr* get_addr() const = 0;
  virtual socklen_t get_addrlen() const = 0;

  // 用于序列化输出称字符串形式
  virtual std::ostream& insert(std::ostream& os) const = 0;
  std::string toString();
  bool operator<(const Address& rhs) const;
  bool operator==(const Address& rhs) const;
  bool operator!=(const Address& rhs) const;
 
 protected:
 private:
};

class IPAddress : public Address {
 public:
  using ptr = std::shared_ptr<IPAddress>;
  virtual ~IPAddress() {}

  virtual IPAddress::ptr get_broadcast_addr(uint32_t prefix_len) = 0;
  virtual IPAddress::ptr get_network_addr(uint32_t prefix_len) = 0;
  // 获取子网掩码
  virtual IPAddress::ptr get_netmask(uint32_t prefix_len) = 0;

  // 域名转换为IP地址的通用函数，获取域名对应的所有ip地址
  static bool DnsToIPAddr(const char* host, const char* port, std::vector<IPAddress::ptr>& addrs);
  static moka::IPAddress::ptr LookupIPv4Addr(const char* host, const char* port) {
    std::vector<moka::IPAddress::ptr> addrs;
    moka::IPAddress::DnsToIPAddr(host, port, addrs);
    if (addrs.empty()) {
      // 不存在则返回nullptr
      return nullptr;
    }
    return addrs[0];
  }


  virtual uint16_t get_port() const = 0;
  virtual void set_port(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
 public:
  using ptr = std::shared_ptr<IPv4Address>;
  IPv4Address();
  IPv4Address(const sockaddr_in& address, uint32_t prefix_len = 32);
  IPv4Address(const char* address, uint16_t port = 0, uint32_t prefix_len = 32);

  virtual const sockaddr* get_addr() const override;
  virtual socklen_t get_addrlen() const override;
  virtual std::ostream& insert(std::ostream& os) const override;

  virtual IPAddress::ptr get_broadcast_addr(uint32_t prefix_len) override;
  virtual IPAddress::ptr get_network_addr(uint32_t prefix_len) override;
  virtual IPAddress::ptr get_netmask(uint32_t prefix_len) override;

  virtual uint16_t get_port() const override;
  virtual void set_port(uint16_t v) override;
 private:
  sockaddr_in addr_;
  uint32_t prefix_len_;
};

class IPv6Address : public IPAddress {
 public:
  using ptr = std::shared_ptr<IPv6Address>;
  IPv6Address();
  IPv6Address(const sockaddr_in6& address, uint32_t prefix_len = 128);
  IPv6Address(const char* address, uint16_t port = 0, uint32_t prefix_len = 128);

  virtual const sockaddr* get_addr() const override;
  virtual socklen_t get_addrlen() const override;
  virtual std::ostream& insert(std::ostream& os) const override;

  virtual IPAddress::ptr get_broadcast_addr(uint32_t prefix_len) override;
  virtual IPAddress::ptr get_network_addr(uint32_t prefix_len) override;
  virtual IPAddress::ptr get_netmask(uint32_t prefix_len) override;

  virtual uint16_t get_port() const override;
  virtual void set_port(uint16_t v) override;
 private:
  sockaddr_in6 addr_;
  uint32_t prefix_len_;      // CIDR表示的网络地址长度(即默认前缀长度)
};

// 用于在同一台机器上不同进程之间的通信
class UnixAddress : public Address {
 public:
  using ptr = std::shared_ptr<UnixAddress>;
  UnixAddress();
  UnixAddress(const std::string& path);

  virtual const sockaddr* get_addr() const override;
  virtual socklen_t get_addrlen() const override;
  void set_addrlen(socklen_t len) { addr_len_ = len; }
  // 用于序列化输出
  virtual std::ostream& insert(std::ostream& os) const override;
 private:
  sockaddr_un addr_;
  socklen_t addr_len_;  // 整个地址结构体的长度(包括地址族和路径名)
};

class UnknowAddress : public Address {
 public:
  using ptr = std::shared_ptr<UnknowAddress>;
  UnknowAddress(int family);

  virtual const sockaddr* get_addr() const override;
  virtual socklen_t get_addrlen() const override;
  // 用于序列化输出
  virtual std::ostream& insert(std::ostream& os) const override;
 private:
  sockaddr addr_;
};

}

#endif