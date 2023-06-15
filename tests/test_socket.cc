#include "../moka/socket.h"
#include "../moka/log.h"
#include "../moka/iomanager.h"
#include "../moka/macro.h"

static moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test_socket() {
  moka::IPAddress::ptr pt4 = LOOKUP_IPV4_ADDR("www.baidu.com", "http");
  if (pt4 != nullptr) {
    MOKA_LOG_INFO(g_logger) << "get address " << pt4->toString();
  } else {
    MOKA_LOG_ERROR(g_logger) << "get address failed";
    return;
  }
  // Socket类中保存了socket API操作所需的属性
  moka::Socket::ptr sock = moka::Socket::CreateTCP(pt4);
  if (!(sock->connect(pt4))) {
    MOKA_LOG_ERROR(g_logger) << "connect " << pt4->toString() << " failed";
    return;
  } else {
    MOKA_LOG_INFO(g_logger) << "connect to " << pt4->toString();
  }
  const char buf[] = "GET / HTTP/1.0\r\n\r\n";
  int ret = sock->send(buf, sizeof(buf));
  if (ret <= 0) {
    MOKA_LOG_INFO(g_logger) << "send fail ret=" << ret;
    return;
  }
  std::string bufs;
  bufs.resize(4096);
  ret = sock->recv(&bufs[0], bufs.size());
  if (ret <= 0) {
    MOKA_LOG_INFO(g_logger) << "recv fail ret=" << ret;
    return;
  }
  // 重设接收缓冲区的大小
  bufs.resize(ret);
  MOKA_LOG_INFO(g_logger) << bufs;
}

int main(int argc, char** argv) {
  moka::IOManager iom;
  iom.schedule(&test_socket);
  return 0;
}