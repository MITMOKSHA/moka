#include "../moka/macro.h"
#include "../moka/log.h"
#include "../moka/util.h"

moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test_assert() {
  MOKA_LOG_INFO(g_logger) << moka::BacktraceToString(10);
  MOKA_ASSERT(false);
}

int main(int agrc, char** agrv) {
  test_assert();
  return 0;
}