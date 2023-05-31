#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__

#include <memory>
#include <vector>

#include "thread.h"
#include "iomanager.h"
#include "singleton.h"

namespace moka {
// 记录fd的信息(参与hook的)
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  using ptr = std::shared_ptr<FdCtx>;
  FdCtx(int fd);
  ~FdCtx();

  bool init();   // 初始化信息
  bool isInit() const { return is_init_; }
  bool isSocket() const { return is_socket_; }
  bool isClosed() const { return is_closed_; }
  bool close();

  void set_user_nonblock(bool val) { is_user_nonblock_ = val; }
  bool get_user_nonblock() { return is_user_nonblock_; }

  void set_sys_nonblock(bool val) { is_sys_nonblock_ = val; }
  bool get_sys_nonblock() { return is_sys_nonblock_; }

  void set_timeout(int type, uint64_t val);
  uint64_t get_timeout(int type);

 private:
  // 只占用一个bit
  bool is_init_: 1;              // 是否初始化
  bool is_socket_: 1;            // socket还是文件?
  bool is_sys_nonblock_: 1;      // 是否系统设置为非阻塞(在FdCtx初始化时)
  bool is_user_nonblock_: 1;     // 是否人为设置为非阻塞
  bool is_closed_: 1;            // 是否关闭
  int fd_;                       // 文件描述符

  uint64_t recv_timeout_;        // 接收的超时时间
  uint64_t send_timeout_;        // 发送的超时时间
};

class FdManager {
 public:
  FdManager();
  FdCtx::ptr get(int fd, bool auto_create = false);  // 获取fd的信息
  void del(int fd);   // 从fd集合中删除对应的fd信息
  RWmutex& get_lock() {return mutex_;}
 private:
  RWmutex mutex_;
  std::vector<FdCtx::ptr> datas_;  // fd信息集合(fd的值作为下标)
};

// 单例模式
using FdMgr = Singleton<FdManager>;
}

#endif