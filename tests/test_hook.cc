#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../moka/hook.h"
#include "../moka/iomanager.h"
#include "../moka/log.h"
#include "../moka/fd_manager.h"
#include "../moka/macro.h"
#include "../moka/config.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test_sleep() {
  moka::IOManager iom(1);
  // 使用一个协程来调度执行该库函数
  iom.schedule([](){
    usleep(2000000);  // 挂起2s
    MOKA_LOG_INFO(g_logger) << "usleep 2s";
  });
  iom.schedule([](){
    sleep(3);
    MOKA_LOG_INFO(g_logger) << "sleep 3s";
  });
  iom.schedule([](){
    struct timespec sleep_time = {4, 0};
    struct timespec remaining_time;
    nanosleep(&sleep_time, &remaining_time);
    MOKA_LOG_INFO(g_logger) << "nanosleep 4s";
  });
  MOKA_LOG_INFO(g_logger) << "test_sleep";
}

void mock_sock() {
  struct timeval timeout = {5, 0};  // 设置超时事件为5s
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  // 刚开始没有设置非阻塞(但其实系统内部已经设置为非阻塞)
  MOKA_ASSERT(!(fcntl(sockfd, F_GETFL) & O_NONBLOCK));
  // 设置sockfd的写超时时间
  MOKA_ASSERT(!setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)));

  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  // HTTP默认为80端口
  addr.sin_port = htons(80);
  // 连接的百度服务器的地址
  inet_pton(AF_INET, "110.242.68.4", &addr.sin_addr.s_addr);

  MOKA_LOG_INFO(g_logger) << "begin connect";
  int ret = connect(sockfd, (sockaddr*)&addr, sizeof(addr));
  MOKA_LOG_INFO(g_logger) << "connect ret=" << ret << " " << strerror(errno);

  // 模拟发送请求HTTP服务器根路径的资源
  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  ret = send(sockfd, data, sizeof(data), 0);
  MOKA_LOG_INFO(g_logger) << "send ret=" << ret << " " << strerror(errno);
  if (ret <= 0) {
    return;
  }
  // 不使用协程栈空间
  std::string buf;
  buf.resize(4096);
  
  ret = recv(sockfd, &buf[0], buf.size(), 0);
  MOKA_LOG_INFO(g_logger) << "recv ret=" << ret << " " << strerror(errno);

  if (ret <= 0) {
    return;
  }

  buf.resize(ret);
  // 打印接收到的响应报文
  MOKA_LOG_INFO(g_logger) << buf;
  close(sockfd);
}

void test_sock() {
  moka::IOManager iom;
  iom.schedule(mock_sock);
}

int main(int argc, char** argv) {
  // test_sleep();
  test_sock();
  return 0;
}