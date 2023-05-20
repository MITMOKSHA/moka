#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

#include "../moka/iomanager.h"
#include "../moka/log.h"
#include "../moka/macro.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test_fiber() {
  MOKA_LOG_INFO(g_logger) << "test_fiber";

  // 新建socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, "172.30.9.91", &addr.sin_addr.s_addr);
  
  // 这里会出现Operation now in progress(因为sockfd设置为非阻塞，connect实际上已经连接)
  connect(sockfd, (sockaddr*)&addr, sizeof(addr));
  MOKA_LOG_INFO(g_logger) << strerror(errno);
  // 添加调度事件(通过sockfd)
  // 如果sockfd触发读事件，则输出回调函数
  moka::IOManager::GetThis()->addEvent(sockfd, moka::IOManager::READ, [=](){
    MOKA_LOG_INFO(g_logger) << "read callback";
  });

  // 如果sockfd触发写事件，则输出回调函数
  moka::IOManager::GetThis()->addEvent(sockfd, moka::IOManager::WRITE, [=](){
    MOKA_LOG_INFO(g_logger) << "write callback";
  });
  close(sockfd);  // 主动关闭并不会触发事件
  // 因此需要手动关闭事件(关闭时会触发)
  moka::IOManager::GetThis()->cancelEvent(sockfd, moka::IOManager::WRITE);
  moka::IOManager::GetThis()->cancelEvent(sockfd, moka::IOManager::READ);
}

moka::Timer::ptr s_timer;
void test_timer() {
  moka::IOManager iom(2, false);
  s_timer = iom.addTimer(2000, []() {
    MOKA_LOG_INFO(g_logger) << "hello timer!";
    static int i = 0;
    if (++i == 3) {
      // s_timer->reset(1000, false);
      s_timer->cancel();
      // s_timer->refresh();
    }
  }, true);
}

void test1() {
  moka::IOManager iom(3, true, "t");
  iom.schedule(test_fiber);
  // iom析构时会调用stop，join阻塞等待调度协程执行任务结束
}

int main(int argc, char** argv) {
  // test1();
  test_timer();
  return 0;
}