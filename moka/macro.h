#ifndef __MOKA_MACRO_H__
#define __MOKA_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"
#include "log.h"

// 封装assert的宏
#define MOKA_ASSERT(x) \
  if (!(x)) { \
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ASSERTION: " #x \
      << "\nbacktrace:\n" \
      << moka::BacktraceToString(100, 2, "    "); \
      assert(x); \
  }

#define MOKA_ASSERT_2(x, w) \
  if (!(x)) { \
    MOKA_LOG_ERROR(MOKA_LOG_ROOT()) << "ASSERTION: " #x \
      << "\n" << #w \
      << "\nbacktrace:\n" \
      << moka::BacktraceToString(100, 2, "    "); \
      assert(x); \
  }

#endif