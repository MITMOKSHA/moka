#include "util.h"
#include <execinfo.h>

#include "log.h"
#include "fiber.h"
#include "thread.h"

moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

namespace moka {

pid_t GetThreadId() {
  return syscall(SYS_gettid);
}

uint64_t GetFiberId() {
  return moka::Fiber::GetFiberId();
}

std::string GetThreadName() {
  return moka::Thread::GetName();
}

void Backtrace(std::vector<std::string> &bt, int size, int skip) {
  // 在堆上开辟空间
  void** array = (void**)malloc((sizeof(void*) * size));
  // 传入buffer和其大小
  size_t s = ::backtrace(array, size);  // 返回array中地址的数量

  // 将地址符号化为字符串数组，返回该字符串数组的指针
  char** strings = backtrace_symbols(array, s);
  if (strings == NULL) {
    MOKA_LOG_ERROR(g_logger) << "backtrace_symbols error";
    free(strings);
    free(array);
    return;
  }

  for (size_t i = skip; i < s; ++i) {
    bt.push_back(strings[i]);
  }
  free(strings);
  free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
  std::vector<std::string> bt;
  Backtrace(bt, size, skip);
  std::stringstream ss;
  for (size_t i = 0; i < bt.size(); ++i) {
    ss << prefix << bt[i] << std::endl;
  }
  return ss.str();
}

}
