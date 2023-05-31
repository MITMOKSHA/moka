#ifndef __MOKA_HOOK_H__
#define __MOKA_HOOK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

namespace moka {
  bool is_hook_enbale();
  void set_hook_enable(bool flag);
}

// extern c告诉编译器和连接器按照C语言的方式处理函数名和变量名
// 防止C++编译器对符号名称添加修饰namedangling(因为C不支持重载)，方便动态链接
extern "C" {
// sleep
typedef unsigned int (*sleep_fun)(unsigned int seconds);  // 声明函数指针
typedef int (*usleep_fun)(useconds_t usec);
typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);

// socket
typedef int (*socket_fun)(int domain, int type, int protocol);
typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*accept_fun)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// read
typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);

// write
typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*send_fun)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t (*sendto_fun)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
typedef ssize_t (*sendmsg_fun)(int sockfd, const struct msghdr *msg, int flags);

// fd op
typedef int (*close_fun)(int fd);
typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */ );
typedef int (*ioctl_fun)(int fd, unsigned long request, ...);
typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);

// 定义外部变量(函数指针变量)
extern sleep_fun sleep_f;
extern usleep_fun usleep_f;
extern nanosleep_fun nanosleep_f;
extern socket_fun socket_f;
extern connect_fun connect_f;
extern accept_fun accept_f;
extern read_fun read_f;
extern readv_fun readv_f;
extern recv_fun recv_f;
extern recvfrom_fun recvfrom_f;
extern recvmsg_fun recvmsg_f;
extern write_fun write_f;
extern writev_fun writev_f;
extern send_fun send_f;
extern sendto_fun sendto_f;
extern sendmsg_fun sendmsg_f;
extern close_fun close_f;
extern fcntl_fun fcntl_f;
extern ioctl_fun ioctl_f;
extern getsockopt_fun getsockopt_f;
extern setsockopt_fun setsockopt_f;
}

#endif