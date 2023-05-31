#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

#include "../moka/iomanager.h"
#include "../moka/log.h"
#include "../moka/macro.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void func() {
  // 新建socket，模拟socket读写通信
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  // 连接百度服务器
  inet_pton(AF_INET, "110.242.68.3", &addr.sin_addr.s_addr);
  
  // 这里会出现Operation now in progress(因为sockfd设置为非阻塞，connect实际上已经连接)
  connect(sockfd, (sockaddr*)&addr, sizeof(addr));
  // 这里的connect客户端会主动发起连接，服务器端会回复一个ACK(因此会发生一次读一次写)
  MOKA_LOG_INFO(g_logger) << strerror(errno);
  // 添加调度事件(通过sockfd)
  // 如果sockfd触发读事件，调度执行回调函数
  moka::IOManager::GetThis()->addEvent(sockfd, moka::IOManager::READ, [=](){
    MOKA_LOG_INFO(g_logger) << "read callback";
  });

  // 如果sockfd触发写事件，调度执行回调函数
  moka::IOManager::GetThis()->addEvent(sockfd, moka::IOManager::WRITE, [=](){
    MOKA_LOG_INFO(g_logger) << "write callback";
    // WRITE事件已经执行起来了，这时候删除事件会报错，因为事件已经在trigger的时候从注册的事件集合中删除了
    MOKA_ASSERT(moka::IOManager::GetThis()->delEvent(sockfd, moka::IOManager::WRITE) == -1);

    MOKA_ASSERT(moka::IOManager::GetThis()->cancelEvent(sockfd, moka::IOManager::READ) == 0);
    close(sockfd);
  });
}

// 这里定义一个全局的定时器，否则在定时器未返回时在lambda表达式中使用会出现问题
moka::Timer::ptr g_timer;

void test_timer() {
  // 创建调度器
  MOKA_LOG_DEBUG(g_logger) << "test timer";
  moka::IOManager iom(2, true);
  // 创建定时器
  // 循环添加定时器事件，2s触发回调函数
  g_timer = iom.addTimer(2000, []() {
    MOKA_LOG_INFO(g_logger) << "hello timer!";
    static int i = 0;
    ++i;
    if (i == 3) {
      // 循环执行3次test_timer之后改变执行周期为1s触发定时事件，这里true/false都无所谓
      g_timer->resetIntervalAndExpire(1000, true);
    }
    if (i == 5) {
      // 循环执行4次该定时事件后，cancel退出
      g_timer->cancel();
    }
  }, true);
  // 在sleep 1s结束之后调用resetExpire，也就相当于定时事件在3s后执行，在sleep2s后执行
  // 也就是test timer过后3s才执行该定时事件
  sleep(1);
  MOKA_LOG_DEBUG(g_logger) << "sleep end";
  g_timer->resetExpire();
}

void test_IOManager() {
  // 测试IO调度器的使用
  moka::IOManager iom(2, true);
  iom.schedule(func);
  // iom析构时会调用stop，join阻塞等待调度协程执行任务结束
}

int main(int argc, char** argv) {
  test_IOManager();
  // test_timer();
  return 0;
}