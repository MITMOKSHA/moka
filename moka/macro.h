#ifndef __MOKA_MACRO_H__
#define __MOKA_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"
#include "log.h"

#if defined __GNUC__ || defined __llvm__
  #define MOKA_LIKELY(x) __builtin_expect(!!(x), 1)
  #define MOKA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define MOKA_LIKELY(x) (x)
  #define MOKA_UNLIKELY() (x)
#endif

// 封装assert的宏
#define MOKA_ASSERT(x) \
  if (MOKA_UNLIKELY(!(x))) { \
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ASSERTION: " #x \
      << "\nbacktrace:\n" \
      << moka::BacktraceToString(100, 2, "    "); \
      assert(x); \
  }

#define MOKA_ASSERT_2(x, w) \
  if (MOKA_UNLIKELY(!(x))) { \
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ASSERTION: " #x \
      << "\n" << #w \
      << "\nbacktrace:\n" \
      << moka::BacktraceToString(100, 2, "    "); \
      assert(x); \
  }

#endif