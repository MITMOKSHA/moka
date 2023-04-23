#include "util.h"

namespace moka {

pid_t GetThreadId() {
  return pthread_self();
}
uint32_t GetFiberId() {
  return 0;
}

}
