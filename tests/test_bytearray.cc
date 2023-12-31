#include "../moka/bytearray.h"
#include "../moka/macro.h"
#include "../moka/log.h"

static moka::Logger::ptr g_logger = MOKA_LOG_ROOT();

void test() {
// base_len表示节点的默认容量
#define XX(type, len, write_fun, read_fun, base_len) {\
  std::vector<type> vec; \
  for (int i = 0; i < len; ++i) { \
    vec.push_back(rand()); \
  } \
  moka::ByteArray::ptr ba(new moka::ByteArray(base_len)); \
  for (auto i : vec) { \
    ba->write_fun(i); \
  } \
  ba->set_rw_position(0); \
  for (size_t i = 0; i < vec.size(); ++i) { \
    type v = ba->read_fun(); \
    MOKA_ASSERT(v == vec[i]); \
  } \
  MOKA_ASSERT(ba->get_readable_size() == 0); \
  MOKA_LOG_INFO(g_logger) << #write_fun "/" #read_fun " (" #type ") len="<< len \
      << " base_len = " << base_len \
      << " size = " << ba->get_size(); \
  ba->set_rw_position(0); \
  MOKA_ASSERT(ba->writeToFile("/tmp/" #type "_" #len "-" #read_fun ".dat")); \
  moka::ByteArray::ptr ba2(new moka::ByteArray(base_len * 2)); \
  MOKA_ASSERT(ba2->readFromFile("/tmp/" #type "_" #len "-" #read_fun ".dat")); \
  ba2->set_rw_position(0); \
  MOKA_ASSERT(ba->toString() == ba2->toString()); \
  MOKA_ASSERT(ba->get_rw_position() == 0); \
  MOKA_ASSERT(ba2->get_rw_position() == 0); \
}
  for (int i = 1; i <= 5; ++i) {
    XX(int8_t, 100, writeInt8F, readInt8F, i);
    XX(uint8_t, 100, writeUint8F, readUint8F, i);
    XX(int16_t, 100, writeInt16F, readInt16F, i);
    XX(uint16_t, 100, writeUint16F, readUint16F, i);
    XX(int32_t, 100, writeInt32F, readInt32F, i);
    XX(uint32_t, 100, writeUint32F, readUint32F, i);
    XX(int64_t, 100, writeInt64F, readInt64F, i);
    XX(uint64_t, 100, writeUint64F, readUint64F, i);

    XX(int32_t, 100, writeInt32V, readInt32V, i);
    XX(uint32_t, 100, writeUint32V, readUint32V, i);
    XX(int64_t, 100, writeInt64V, readInt64V, i);
    XX(uint64_t, 100, writeUint64V, readUint64V, i);
  }
#undef XX
}

int main(int argc, char** argv) {
  test();
  return 0;
}