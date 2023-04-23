#ifndef __MOKA_UTIL_H__
#define __MOKA_UTIL_H__
#include <pthread.h>
#include <stdint.h>

namespace moka {
pid_t GetThreadId();
uint32_t GetFiberId();
}

#endif