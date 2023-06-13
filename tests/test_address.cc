#include <iostream>
#include <vector>

#include "../moka/address.h"
#include "../moka/macro.h"
#include "../moka/log.h"

moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

void test_ip() {
  // 测试IPv4地址
  auto pt4 = CREATE_IPV4("192.168.1.1", 32);
  MOKA_LOG_INFO(g_logger) << "ipv4 address : " << pt4->toString();
  MOKA_LOG_INFO(g_logger) << "ipv4 address port : "<< pt4->get_port();
  MOKA_LOG_INFO(g_logger) << "ipv4 broadcast address : " << pt4->get_broadcast_addr(17)->toString();
  MOKA_LOG_INFO(g_logger) << "ipv4 network address : " <<  pt4->get_network_addr(16)->toString();
  MOKA_LOG_INFO(g_logger) << "ipv4 netmask : " <<  pt4->get_netmask(7)->toString();
  
  // 测试域名和ip地址之间的转换
  std::vector<moka::IPAddress::ptr> addrs;
  // addr为传入参数
  MOKA_ASSERT(moka::IPAddress::DnsToIPAddr("www.baidu.com", "http", addrs));
  for (auto addr : addrs) {
    MOKA_LOG_INFO(g_logger) << "domain: www.baidu.com, ip : " << addr->toString();
  }

  auto pt6 = CREATE_IPV6("2001:0db8:85a3:0000:0000:8a2e:0370:7334", 16);
  MOKA_LOG_INFO(g_logger) << "ipv6 address : "<< pt6->toString();
  MOKA_LOG_INFO(g_logger) << "ipv6 address port : "<< pt6->get_port();
  MOKA_LOG_INFO(g_logger) << "ipv6 network address : " <<  pt6->get_network_addr(15)->toString();
  MOKA_LOG_INFO(g_logger) << "ipv6 netmask : " <<  pt6->get_netmask(15)->toString();
  MOKA_LOG_INFO(g_logger) << "ipv6 multicast : " <<  pt6->get_broadcast_addr(15)->toString();
}

void test_iface() {
  // 输出网卡和本地回环地址的ipv4和ipv6
  std::multimap<std::string, moka::Address::ptr> result;
  MOKA_ASSERT(moka::Address::GetInterfaceAddress(result));
  for (auto& i : result) {
    MOKA_LOG_INFO(g_logger) << i.first << " - " << i.second->toString();
  }

  // 测试获取获取到指定设备名的Ipv4和Ipv6的地址的接口
  std::vector<moka::Address::ptr> result2;
  std::string device_name = "lo";
  MOKA_ASSERT(moka::Address::GetInterfaceAddress(result2, device_name));
  for (auto& i : result2) {
    MOKA_LOG_INFO(g_logger) << device_name << " - " << i->toString();
  }
}

void test_unix_addr() {
  moka::UnixAddress::ptr ptu(new moka::UnixAddress("~/"));
  MOKA_LOG_INFO(g_logger) << "unix addr : " <<  ptu->toString();
  MOKA_LOG_INFO(g_logger) << "unix addr len : " <<  ptu->get_addrlen();
}

int main(int argc, char** agrv) {
  test_ip();
  test_iface();
  test_unix_addr();
  return 0;
}