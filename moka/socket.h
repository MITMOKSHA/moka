#ifndef __MOKA_SOCKET_H__
#define __MOKA_SOCKET_H__

#include <memory>
#include <iostream>
#include "address.h"
#include "noncopyable.h"

namespace moka {
class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
 public:
  using ptr = std::shared_ptr<Socket>;
  // socket类型
  enum Type {
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM
  };
  enum Family {
    IPV4 = AF_INET,
    IPV6 = AF_INET6,
    UNIX = AF_UNIX
  };

  // 便于创建对应socket对象的接口
  static Socket::ptr CreateTCP(moka::Address::ptr address);
  static Socket::ptr CreateUDP(moka::Address::ptr address);
  static Socket::ptr CreateTCPSocket();
  static Socket::ptr CreateUDPSocket();
  static Socket::ptr CreateTCPSocket6();
  static Socket::ptr CreateUDPSocket6();
  static Socket::ptr CreateUnixTCPSocket();
  static Socket::ptr CreateUnixUDPSocket();
  
  // protocol参数为0表示会自动根据地址族来选择具体协议
  Socket(int family, int type, int protocol = 0);
  ~Socket();

  int64_t get_send_timeout();
  void set_send_timeout(int64_t v);

  int64_t get_recv_timeout();
  void set_recv_timeout(int64_t v);

  // 封装setsockopt和getsockopt
  bool set_option(int level, int option, const void* result, size_t len);
  bool get_option(int level, int option, void* result, size_t* len);

  template<class T>
  bool get_option(int level, int option, T& result) {
    size_t len = sizeof(T);
    return get_option(level, option, &result, &len);
  }

  template<class T>
  bool set_option(int level, int option, const T& value) {
    return set_option(level, option, &value, sizeof(T));
  }

  Socket::ptr accept();
  
  bool bind(const Address::ptr addr);
  bool connect(const Address::ptr addr, uint64_t timeout = -1);
  bool listen(int backlog = SOMAXCONN);
  void close();  // 释放socfd的资源

  // send系列函数有两个版本，send和sendmsg
  int send(const void* buffer, size_t len, int flags = 0);
  int send(const iovec* buffer, size_t len, int flags = 0);
  // sendto指定一个Address对象指针
  int sendto(const void* buffer, size_t len, const Address::ptr to, int flags = 0);
  int sendto(const iovec* buffer, size_t len, const Address::ptr to, int flags = 0);

  int recv(void* buffer, size_t len, int flags = 0);
  int recv(iovec* buffer, size_t len, int flags = 0);
  // recvfrom指定一个Address对象指针
  int recvfrom(void* buffer, size_t len, Address::ptr from, int flags = 0);
  int recvfrom(iovec* buffer, size_t len, Address::ptr from, int flags = 0);

  // 封装getsockname获取sockfd对应的地址信息(sockaddr)
  Address::ptr get_remote_address();
  Address::ptr get_local_address();

  int get_family() const { return family_; }
  int get_type() const { return type_; }
  int get_protocol() const { return protocol_; }

  bool is_connected() const { return is_connected_; } // 判断连接是否建立
  bool is_valid() const;                              // 判断sockfd是否分配
  int get_error();                                    // 获取sockfd的错误信息

  std::ostream& dump(std::ostream& os) const;         // 将Socket对象相关的字段输出到控制台
  int get_socketfd() const { return sockfd_; }

  // 强制唤醒阻塞的事件(配合IOManager的接口)
  bool cancelRead();
  bool cancelWrite();
  bool cancelAccept();
  bool cancelAll();
 private:
  bool init(int sockfd);
  // 设置sockfd的地址复用属性，并打开nagle算法
  void initSock();
  // 根据当前Socket对象的属性初始化sockfd字段，再调用initSock
  bool newSock();
 
 private:
  int sockfd_;                  // 套接字
  int family_;                  // 地址族
  int type_;                    // socket类型
  int protocol_;                // 传输协议类型
  bool is_connected_;           // 是否已经建立socket连接
  Address::ptr local_address_;  // 本地地址
  Address::ptr remote_address_; // 对端地址
};

}

#endif