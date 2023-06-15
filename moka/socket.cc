#include <netinet/tcp.h>
#include "socket.h"
#include "fd_manager.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace moka {
static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

Socket::ptr Socket::CreateTCP(moka::Address::ptr address) {
  Socket::ptr sock(new Socket(address->get_family(), Type::TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDP(moka::Address::ptr address) {
  Socket::ptr sock(new Socket(address->get_family(), Type::UDP, 0));
  return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
  Socket::ptr sock(new Socket(Family::IPV4, Type::TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
  Socket::ptr sock(new Socket(Family::IPV4, Type::UDP, 0));
  return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
  Socket::ptr sock(new Socket(Family::IPV6, Type::TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
  Socket::ptr sock(new Socket(Family::IPV6, Type::UDP, 0));
  return sock;
}


Socket::ptr Socket::CreateUnixTCPSocket() {
  Socket::ptr sock(new Socket(Family::UNIX, Type::TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
  Socket::ptr sock(new Socket(Family::UNIX, Type::UDP, 0));
  return sock;
}

Socket::Socket(int family, int type, int protocol)
    : sockfd_(-1), family_(family), type_(type), protocol_(protocol),
      is_connected_(false) {
}

Socket::~Socket() {
  close();
}

int64_t Socket::get_send_timeout() {
  // 获取sockfd的上下文中的超时时间属性(往epoll内核事件表中添加fd的事件时就会初始化其对应的上下文)
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd_);
  if (ctx) {
    return ctx->get_timeout(SO_SNDTIMEO);
  }
  return -1;
}

void Socket::set_send_timeout(int64_t v) {
  // 毫秒级定时
  struct timeval tv{int(v/1000), int((v%1000)*1000)};
  set_option(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::get_recv_timeout() {
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd_);
  if (ctx) {
    return ctx->get_timeout(SO_RCVTIMEO);
  }
  return -1;
}

void Socket::set_recv_timeout(int64_t v) {
  struct timeval tv{int(v/1000), int(v%1000*1000)};
  set_option(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::get_option(int level, int option, void* result, size_t* len){ 
  if (getsockopt(sockfd_, level, option, result, (socklen_t*)len)) {
    MOKA_LOG_DEBUG(g_logger) << "get_option sockfd=" << sockfd_
        << " level=" << level << " option=" << option
        << " errno=" << errno << " strerr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::set_option(int level, int option, const void* result, size_t len) {
  if (setsockopt(sockfd_, level, option, result, (socklen_t)len)) {
    MOKA_LOG_DEBUG(g_logger) << "set_option sockfd=" << sockfd_
        << " level=" << level << " option=" << option
        << " errno=" << errno << " strerr=" << strerror(errno);
    return false;
  }
  return true;
}

Socket::ptr Socket::accept() {
  Socket::ptr sockfd(new Socket(family_, type_, protocol_));
  // 调用hook版本的accept，::表示全局作用域
  int con_sockfd = ::accept(sockfd_, nullptr, nullptr);
  if (con_sockfd == -1) {
    MOKA_LOG_ERROR(g_logger) << "accept(" << sockfd << ") errno="
        << errno << " strerr=" << strerror(errno);
    return nullptr; 
  }
  // 服务器端已连接客户端
  if (sockfd->init(con_sockfd)) {
    return sockfd;
  }
  return nullptr;
}

bool Socket::init(int sockfd) {
  // 初始化连接fd的信息到当前Socket的对象的属性
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd);
  if (ctx && ctx->isSocket() && !ctx->isClosed()) {
    sockfd_ = sockfd;
    is_connected_ = true;
    // 设置延时等等
    initSock();
    get_local_address();
    get_remote_address();
    return true;
  }
  return false;
}

bool Socket::bind(const Address::ptr addr) {
  if (MOKA_UNLIKELY(!is_valid())) {
    if (MOKA_UNLIKELY(!newSock())) {
      return false;
    }
  }
  if (MOKA_UNLIKELY(addr->get_family() != family_)) {
    MOKA_LOG_ERROR(g_logger) << "bind sockfd.family(" << family_
        << ") addr.family(" << addr->get_family() << ") not equal, addr="
        << addr->toString();
    return false;
  }
  // 调用hook的版本
  if (::bind(sockfd_, addr->get_addr(), addr->get_addrlen())) {
    MOKA_LOG_ERROR(g_logger) << "bind errno=" << errno
        << " strerr=" << strerror(errno);
    return false;
  }
  get_local_address();
  return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout) {
  if (MOKA_UNLIKELY(!is_valid())) {
    if (MOKA_UNLIKELY(!newSock())) {
      return false;
    }
  }
  if (MOKA_UNLIKELY(addr->get_family() != family_)) {
    MOKA_LOG_ERROR(g_logger) << "connect sockfd.family(" << family_
        << ") addr.family(" << addr->get_family() << ") not equal, addr="
        << addr->toString();
    return false;
  }

  if (timeout == (uint64_t)(-1)) {
    // 没给超时时间
    if (::connect(sockfd_, addr->get_addr(), addr->get_addrlen())) {
      MOKA_LOG_ERROR(g_logger) << "sockfd=" << sockfd_ << " connect("
          << addr->toString() << ") errnor errno=" << errno << " strerr="
          << strerror(errno);
      // 释放fd
      close();
      return false;
    }
  } else {
    if (::connect_with_timeout(sockfd_, addr->get_addr(), addr->get_addrlen(), timeout)) {
      MOKA_LOG_ERROR(g_logger) << "sockfd=" << sockfd_ << " connect("
          << addr->toString() << ") timeout=" << timeout << " errno="
          << errno << " strerr=" << strerror(errno);
      close();
      return false;
    }
  }
  // 客户端已经连接服务器端
  is_connected_ = true;
  get_remote_address();
  get_local_address();
  return true;
}

bool Socket::listen(int backlog) {
  if (!is_valid()) {
    MOKA_LOG_ERROR(g_logger) << "listen error sockfd=-1";
    return false;
  }
  if(::listen(sockfd_, backlog) != 0) {
    MOKA_LOG_ERROR(g_logger) << "listen error errno=" << errno
        << " strerr=" << strerror(errno);
    return false;
  }
  return true;
}

void Socket::close() {
  if (!is_connected_ && sockfd_ == -1) {
    return;
  }
  is_connected_ = false;
  if (sockfd_ != -1) {
    // 调用hook的close来释放socket资源
    ::close(sockfd_);
    sockfd_ = -1;
  }
}

int Socket::send(const void* buffer, size_t len, int flags) {
  if (is_connected_) {
    return ::send(sockfd_, buffer, len, flags);
  }
  return -1;
}

int Socket::send(const iovec* buffer, size_t len, int flags) {
  if (is_connected_) {
    msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = const_cast<iovec*>(buffer);
    msg.msg_iovlen = len;
    return ::sendmsg(sockfd_, &msg, flags);
  }
  return -1;
}

int Socket::sendto(const void* buffer, size_t len, const Address::ptr to, int flags) {
  if (is_connected_) {
    return ::sendto(sockfd_, buffer, len, flags, to->get_addr(), to->get_addrlen());
  }
  return -1;
}

int Socket::sendto(const iovec* buffer, size_t len, const Address::ptr to, int flags) {
  if (is_connected_) {
    msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = const_cast<iovec*>(buffer);
    msg.msg_iovlen = len;
    msg.msg_name = const_cast<sockaddr*>(to->get_addr());
    msg.msg_namelen = to->get_addrlen();
    return ::sendmsg(sockfd_, &msg, flags);
  }
  return -1;
}

int Socket::recv(void* buffer, size_t len, int flags) {
  if (is_connected_) {
    return ::recv(sockfd_, buffer, len, flags);
  }
  return -1;
}

int Socket::recv(iovec* buffer, size_t len, int flags) {
  if (is_connected_) {
    msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = const_cast<iovec*>(buffer);
    msg.msg_iovlen = len;
    return ::recvmsg(sockfd_, &msg, flags);
  }
  return -1;
}

int Socket::recvfrom(void* buffer, size_t len, Address::ptr from, int flags) {
  if (is_connected_) {
    socklen_t len = from->get_addrlen();
    return ::recvfrom(sockfd_, buffer, len, flags, const_cast<sockaddr*>(from->get_addr()), &len);
  }
  return -1;
}

int Socket::recvfrom(iovec* buffer, size_t len, Address::ptr from, int flags) {
  if (is_connected_) {
    msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = const_cast<iovec*>(buffer);
    msg.msg_iovlen = len;
    msg.msg_name = const_cast<sockaddr*>(from->get_addr());
    msg.msg_namelen = from->get_addrlen();
    return ::recvmsg(sockfd_, &msg, flags);
  }
  return -1;
}

Address::ptr Socket::get_remote_address() {
  if (remote_address_) {
    return remote_address_;
  }
  Address::ptr res;
  switch (family_) {
    // 连接的地址族
    case AF_INET: {
      res.reset(new IPv4Address());
      break;
    }
    case AF_INET6: {
      res.reset(new IPv6Address());
      break;
    }
    case AF_UNIX:
      res.reset(new UnixAddress());
      break;
    default:
      res.reset(new UnknowAddress(family_));
  }
  socklen_t addr_len = res->get_addrlen();
  // 将对端的socket地址信息保存到res的addr字段
  if (getpeername(sockfd_, const_cast<sockaddr*>(res->get_addr()), &addr_len)) {
    MOKA_LOG_ERROR(g_logger) << "getpeername error sockfd=" << sockfd_
        << " errno=" << errno << " strerr=" << strerror(errno);
    return Address::ptr(new UnknowAddress(family_));
  }
  if (family_ == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(res);
    addr->set_addrlen(addr_len);
  }
  remote_address_ = res;
  return remote_address_;
}

Address::ptr Socket::get_local_address() {
  if (local_address_) {
    return local_address_;
  }
  Address::ptr res;
  switch (family_) {
    case AF_INET: {
      res.reset(new IPv4Address());
      break;
    }
    case AF_INET6: {
      res.reset(new IPv6Address());
      break;
    }
    case AF_UNIX:
      res.reset(new UnixAddress());
      break;
    default:
      res.reset(new UnknowAddress(family_));
  }
  socklen_t addr_len = res->get_addrlen();
  // 获取本地端的socket地址信息保存到res的addr字段
  if (getsockname(sockfd_, const_cast<sockaddr*>(res->get_addr()), &addr_len)) {
    MOKA_LOG_ERROR(g_logger) << "getsockname error sockfd=" << sockfd_
        << " errno=" << errno << " strerr=" << strerror(errno);
    return Address::ptr(new UnknowAddress(family_));
  }
  if (family_ == AF_UNIX) {
    // 如果是Unix域地址，因为它的地址长度被默认初始化到最大值了
    // 因此需要更新为实际长度(根据实际路径名长度)
    // dynamic_pointer_cast用于有继承关系的智能指针之间的转化
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(res);
    addr->set_addrlen(addr_len);
  }
  local_address_ = res;
  return local_address_;
}


bool Socket::is_valid() const {
  return sockfd_ != -1;
}

int Socket::get_error() {
  int error = 0;
  size_t len = sizeof(error);
  // 获取socket的错误状态，并将错误码保存在error变量中(如果error的值为0表示没有发生错误)
  if (get_option(SOL_SOCKET, SO_ERROR, &error, &len)) {
    // 获取对应的属性
    return -1;
  }
  return error;
}

// 将socket信息输出
std::ostream& Socket::dump(std::ostream& os) const {
  os << "[Socket sockfd=" << sockfd_
     << " is_connected=" << is_connected_
     << " family=" << family_
     << " type=" << type_
     << " protocol=" << protocol_;
  if (local_address_)
    os << " local_address=" << local_address_->toString();
  if (remote_address_)
    os<< " remote_address=" << remote_address_->toString();
  os << "]";
  return os;
}

// 强制唤醒
bool Socket::cancelRead() {
  return IOManager::GetThis()->cancelEvent(sockfd_, moka::IOManager::READ);
}

bool Socket::cancelWrite() {
  return IOManager::GetThis()->cancelEvent(sockfd_, moka::IOManager::WRITE);
}

bool Socket::cancelAccept() {
  return IOManager::GetThis()->cancelEvent(sockfd_, moka::IOManager::READ);
}

bool Socket::cancelAll() {
  return IOManager::GetThis()->cancelAll(sockfd_);
}

void Socket::initSock() {
  // optval需要一个非零的整数值
  int optval = 1;
  // 设置可以在同一端口上重新绑定被使用的地址，连接关闭后不需要等待2MSL
  set_option(SOL_SOCKET, SO_REUSEADDR, optval);
  if (type_ == SOCK_STREAM) {
    // 如果是TCP连接，则启用Nagle算法，提高传输效率
    set_option(IPPROTO_TCP, TCP_NODELAY, optval);
  }
}

bool Socket::newSock() {
  sockfd_ = socket(family_, type_, protocol_);
  if (MOKA_LIKELY(sockfd_ != -1)) {
    initSock();
  } else {
    MOKA_LOG_ERROR(g_logger) << "socket(" << family_
        << ", " << type_ << ", " << protocol_ << ") errno="
        << errno << " strerr=" << strerror(errno);
    return false;
  }
  return true;
}

}