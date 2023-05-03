#ifndef __MOKA_UTIL_H__
#define __MOKA_UTIL_H__
#include <pthread.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <string>

namespace moka {
pid_t GetThreadId();
uint64_t GetFiberId();
std::string GetThreadName();

void Backtrace(std::vector<std::string>& bt, int size, int skip);  // skip跳过不需要打印出来的bt
std::string BacktraceToString(int size, int skip = 2, const std::string& prefix = "");
}

#endif