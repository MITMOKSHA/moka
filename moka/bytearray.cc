#include "bytearray.h"
#include "string.h"
#include "../moka/log.h"

#include <byteswap.h>
#include <iomanip>
#include <fstream>
#include <sstream>

namespace moka {

static moka::Logger::ptr g_logger = MOKA_LOG_NAME("system");

ByteArray::Node::Node() : ptr(nullptr), size(0), next(nullptr) {}

// comment: 以字节为单位
ByteArray::Node::Node(size_t s) : ptr(new char[s]), size(s), next(nullptr) {}

ByteArray::Node::~Node() {
  if (ptr) {
    delete[] ptr;
  }
}

ByteArray::ByteArray(size_t list_size)
    : node_base_size_(list_size),
      rw_pos_(0),
      capacity_(list_size),
      size_(0),
      endian_(__ORDER_BIG_ENDIAN__),
      root_(new Node(list_size)), 
      cur_node_(root_) {
}

ByteArray::~ByteArray() {
  // 释放链表的元素数据
  Node* tmp = root_; 
  while (tmp) {
    cur_node_ = tmp;
    tmp = tmp->next;
    delete cur_node_;
  }
}

void ByteArray::writeInt8F(int8_t value) {
  write(&value, sizeof(value));
}

void ByteArray::writeUint8F (uint8_t value) {
  write(&value, sizeof(value));
}

// 需要保证字节序
#define XX(value, bit) \
  if (__BYTE_ORDER__ != endian_) { \
    value = bswap_##bit(value); \
  } \
  write(&value, sizeof(value)); \

void ByteArray::writeInt16F (int16_t value) {
  XX(value, 16);
}

void ByteArray::writeUint16F(uint16_t value) {
  XX(value, 16);
}

void ByteArray::writeInt32F (int32_t value) {
  XX(value, 32);
}

void ByteArray::writeUint32F(uint32_t value) {
  XX(value, 32);
}

void ByteArray::writeInt64F (int64_t value) {
  XX(value, 64);
}

void ByteArray::writeUint64F(uint64_t value) {
  XX(value, 64);
}

#undef XX

/** Zigzag **/
static uint32_t EncodeZigzag32(const int32_t& v) {
  // 将有符号转换为无符号处理，减少存储的字节
  return (v << 1) ^ (v >> 31);
}

static uint64_t EncodeZigzag64(const int64_t& v) {
  return (v << 1) ^ (v >> 63);
}

static uint32_t DecodeZigzag32(const uint32_t& v) {
  return (v >> 1) ^ -(v & 1);
}

static uint64_t DecodeZigzag64(const uint64_t& v) {
  return (v >> 1) ^ -(v & 1);
}

void ByteArray::writeInt32V (int32_t value) {
  writeUint32V(EncodeZigzag32(value));
}

void ByteArray::writeUint32V(uint32_t value) {
  // TLV编码
  uint8_t tmp[5];  // 压缩类型的上限(5个字节)
  uint8_t i = 0;
  while (value >= 0x80) {
    // 最高位为"1"表示后续字节也属于同样的类型
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  write(tmp, i);
}

void ByteArray::writeInt64V(int64_t value) {
  writeUint64V(EncodeZigzag64(value));
}

void ByteArray::writeUint64V(uint64_t value) {
  uint8_t tmp[10];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  tmp[i++] = value;
  write(tmp, i);
}

void ByteArray::writeFloat(float value) {
  uint32_t v;
  memcpy(&v, &value, sizeof(value));
  writeUint32F(v);
}

void ByteArray::writeDouble(double value) {
  uint64_t v;
  memcpy(&v, &value, sizeof(value));
  writeUint64F(v);
}

void ByteArray::writeStringF16(const std::string& value) {
  // 先写入长度，再写入数据
  writeUint16F(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringF32(const std::string& value) {
  writeUint32F(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
  writeUint64F(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeStringIntV(const std::string& value) {
  writeUint64V(value.size());
  write(value.c_str(), value.size());
}

void ByteArray::writeString(const std::string& value) {
  write(value.c_str(), value.size());
}

int8_t ByteArray::readInt8F() {
  int8_t v;
  read(&v, sizeof(v));
  return v;
}

uint8_t ByteArray::readUint8F() {
  uint8_t v;
  read(&v, sizeof(v));
  return v;
}

// 需要保证字节序
#define XX(type, bit) \
  type v; \
  read(&v, sizeof(v)); \
  if (__BYTE_ORDER__ != endian_) { \
    v = bswap_##bit(v); \
  } \
  return v;

int16_t ByteArray::readInt16F() {
  XX(int16_t, 16);
}

uint16_t ByteArray::readUint16F() {
  XX(uint16_t, 16);
}

int32_t ByteArray::readInt32F() {
  XX(int32_t, 32);
}

uint32_t ByteArray::readUint32F() {
  XX(uint32_t, 32);
}

int64_t ByteArray::readInt64F() {
  XX(int64_t, 64);
}

uint64_t ByteArray::readUint64F() {
  XX(uint64_t, 64);
}

#undef XX

int32_t ByteArray::readInt32V() {
  return DecodeZigzag32(readUint32V());
}

uint32_t ByteArray::readUint32V() {
  uint32_t res = 0;
  for(int i = 0; i < 32; i += 7) {
    uint8_t b = readUint8F();
    if (b < 0x80) {
      // 没有下一个字节
      res |= ((uint32_t)b) << i;
      break;
    } else {
      res |= (((uint32_t)(b & 0x7f)) << i);
    }
  }
  return res;
}

int64_t ByteArray::readInt64V() {
  return DecodeZigzag64(readUint64V());
}
uint64_t ByteArray::readUint64V() {
  uint64_t res = 0;
  for(int i = 0; i < 64; i += 7) {
    uint8_t b = readUint8F();
    if (b < 0x80) {
      res |= ((uint64_t)b) << i;
      break;
    } else {
      res |= (((uint64_t)(b & 0x7f)) << i);
    }
  }
  return res;
}

float ByteArray::readFloat() {
  uint32_t v = readUint32F();
  float value;
  // comment: 位模式拷贝memcpy读float数据而不是强转会比较合理
  memcpy(&value, &v, sizeof(v));
  return value;
}

double ByteArray::readDouble() {
  uint64_t v = readUint64F();
  double value;
  memcpy(&value, &v, sizeof(v));
  return value;
}

std::string ByteArray::readString16F() {
  // 先读出长度，再读数据
  uint16_t len = readUint16F();
  std::string buf;
  buf.resize(len);
  read(&buf[0], len);
  return buf;
}

std::string ByteArray::readString32F() {
  uint32_t len = readUint32F();
  std::string buf;
  buf.resize(len);
  read(&buf[0], len);
  return buf;
}

std::string ByteArray::readString64F() {
  uint64_t len = readUint64F();
  std::string buf;
  buf.resize(len);
  read(&buf[0], len);
  return buf;
}

std::string ByteArray::readStringIntV() {
  uint64_t len = readUint64F();
  std::string buf;
  buf.resize(len);
  read(&buf[0], len);
  return buf;
}

void ByteArray::clear() {
  rw_pos_ = size_ = 0;
  capacity_ = node_base_size_;
  Node* tmp = root_->next;
  while (tmp) {
    cur_node_ = tmp;
    tmp = tmp->next;
    delete cur_node_;
  }
  cur_node_ = root_;
  // comment:只留一个节点，释放其他节点
  root_->next = nullptr;
}

void ByteArray::write(const void* buf, size_t size) {
  if (size == 0) {
    return;
  }
  addCapacity(size);
  size_t cur_node_pos = rw_pos_ % node_base_size_;         // 取余获取当前节点的pos
  size_t remain_cap = cur_node_->size - cur_node_pos;     // 当前节点剩余的容量
  size_t bpos = 0;                                        // buffer position

  while (size > 0) {
    if (remain_cap >= size) {
      // 刚好能写入一个节点
      // comment 61c: 指针+pos，实际上会将指针类型的大小作为基数
      memcpy(cur_node_->ptr + cur_node_pos, (const char*)buf+ bpos, size);
      if (cur_node_->size == (cur_node_pos + size)) { 
        // 若恰好写满一个节点，当前位置需要后移指向新的节点
        cur_node_ = cur_node_->next;
      }
      rw_pos_ += size;
      bpos += size;
      size = 0; // 需要写的size置为0
    } else {
      // 需要写入多个节点
      memcpy(cur_node_->ptr + cur_node_pos, (const char*)buf + bpos, remain_cap);
      // comment:可以细品一下这里涉及到数据的更新
      rw_pos_ += remain_cap;
      bpos += remain_cap;
      size -= remain_cap;
      cur_node_ = cur_node_->next;
      remain_cap = cur_node_->size;
      cur_node_pos = 0;
    }
  }
  if (rw_pos_ > size_) {
    // 更新当前ByteArray的占用空间
    size_ = rw_pos_;
  }
}

void ByteArray::read(void* buf, size_t size) {
  if (size > get_readable_size()) {
    throw std::out_of_range("not enough len");
  }
  size_t cur_node_pos = rw_pos_ % node_base_size_;
  size_t remain_cap = cur_node_->size - cur_node_pos;  // 当前节点剩余的容量
  size_t bpos = 0;
  while (size > 0) {
    if (remain_cap >= size) {
      // 可以容纳下
      memcpy((char*)buf + bpos, cur_node_->ptr + cur_node_pos, size);
      if (cur_node_->size == (cur_node_pos + size)) {
        // 恰好读完一个节点
        cur_node_ = cur_node_->next;
      }
      rw_pos_ += size;
      bpos += size;
      size = 0;
    } else {
      memcpy((char*)buf + bpos, cur_node_->ptr + cur_node_pos, remain_cap);
      rw_pos_ += remain_cap;
      bpos += remain_cap;
      size -= remain_cap;
      cur_node_ = cur_node_->next;
      // comment:当前节点剩余的容量为下一个新节点的容量
      remain_cap = cur_node_->size;
      cur_node_pos = 0;
    }
  }
}

void ByteArray::read(void* buf, size_t size, size_t rw_pos) const {
  // comment: 没有副作用(不会修改读写指针，同时也不会修改节点指针)
  if (size > get_readable_size()) {
    throw std::out_of_range("not enough length");
  }
  size_t cur_node_pos = rw_pos % node_base_size_;
  size_t remain_cap = cur_node_->size - cur_node_pos;
  size_t bpos = 0;
  Node* cur = cur_node_;
  while (size > 0) {
    if (remain_cap >= size) {
      memcpy((char*)buf + bpos, cur->ptr + cur_node_pos, size);
      if (cur->size == cur_node_pos + size) {
        cur = cur->next;
      }
      rw_pos += size;
      bpos += size;
      size = 0;
    } else {
      memcpy((char*)buf + bpos, cur->ptr + cur_node_pos, remain_cap);
      rw_pos += remain_cap;
      bpos += remain_cap;
      size -= remain_cap;
      cur = cur->next;
      remain_cap = cur->size;
      cur_node_pos = 0;
    }
  }
}

void ByteArray::set_rw_position(size_t v) {
  if (v > size_) {
    throw std::out_of_range("set_rw_position out of range");
  }
  // comment:更新读写指针的同时，需要更新cur_node_的指向
  rw_pos_ = v;
  cur_node_ = root_;
  // 更新节点指针的位置
  while (v > cur_node_->size) {
    v -= cur_node_->size;
    cur_node_ = cur_node_->next;
  }
  if (v == cur_node_->size) {
    // 如果恰好占满最后一个节点，移动指针
    cur_node_ = cur_node_->next;
  }
}

bool ByteArray::writeToFile(const std::string& filename) const {
  // 将数据写入到文件
  std::ofstream ofs;   
  ofs.open(filename, std::ios::trunc | std::ios::binary);
  if (!ofs) {
    MOKA_LOG_ERROR(g_logger) << "writeToFile filename=" << filename
      << " error, errno" << errno << " errstr=" << strerror(errno);
    return false;
  }
  int64_t readable_size = get_readable_size();
  int64_t new_rw_pos = rw_pos_;
  Node* cur = cur_node_;
  while (readable_size > 0) {
    int cur_node_pos = new_rw_pos % node_base_size_;
    int64_t len = (readable_size > (int64_t)node_base_size_? node_base_size_: readable_size) - cur_node_pos;
    // 将ByteArray中的数据写入文件
    ofs.write(cur->ptr + cur_node_pos, len);
    cur = cur->next;
    new_rw_pos += len;
    readable_size -= len;
  }
  // comment: 不需要更新rw_pos_，因为是从ByteArray中读数据
  return true;
}

bool ByteArray::readFromFile(const std::string& filename) {
  std::ifstream ifs;
  ifs.open(filename, std::ios::binary);
  if (!ifs) {
    MOKA_LOG_ERROR(g_logger) << "readFromFile filename=" << filename
      << " error, errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  // comment: 自定义智能指针析构器
  std::shared_ptr<char> buf(new char[node_base_size_], [](char* ptr) { delete[] ptr; });
  while (!ifs.eof()) {
    // 直到读取到文件末尾为止
    // 从文件中读取数据，写入ByteArray
    ifs.read(buf.get(), node_base_size_);
    write(buf.get(), ifs.gcount());  // gcount获取最后一次读取操作成功读取的字符数
  }
  return true;
}

bool ByteArray::isLitteEndian() const {
  return endian_ == __ORDER_LITTLE_ENDIAN__;
}

void ByteArray::set_little_endian() {
  endian_ = __ORDER_LITTLE_ENDIAN__;
}
  
void ByteArray::set_big_endian() {
  endian_ = __ORDER_BIG_ENDIAN__;
}

// 增加ByteArray的容量
void ByteArray::addCapacity(size_t size) {
  if (size == 0) {
    return;
  }
  size_t remain_capacity = get_remain_capacity();
  if (remain_capacity >= size) {
    return;
  }
  // comment: (*)当前最后一个节点的剩余空间如果比剩余容量的还要小，那么需要额外开辟一个
  // 需要开辟的节点数 + 额外分配数
  size_t count = (size / node_base_size_) + (((size % node_base_size_) > remain_capacity)? 1: 0);
  // 计算还出需要开辟的空间
  size -= remain_capacity;
  Node* tmp = root_;
  // 移动到最后一个节点
  while (tmp->next) {
    tmp = tmp->next;
  }
  Node* first = nullptr;
  for (size_t i = 0; i < count; ++i) {
    // 开辟新的空间节点
    tmp->next = new Node(node_base_size_);
    if (first == nullptr) {
      // 添加的"第一个"节点需要关联之前的节点
      first = tmp->next; 
    }
    tmp = tmp->next;
    capacity_ += node_base_size_;
  }
  if (remain_capacity == 0) {
    // comment: 如果ByteArray当前没有剩余的空间，cur_node_会指向空，需要更新它的值
    // 如果新的剩余空间为0，则更新节点指针
    cur_node_ = first;
  }
}

std::string ByteArray::toString() const {
  std::string str;
  str.resize(get_readable_size());
  if (str.empty()) {
    // 没有可以输出的数据
    return str;
  }
  read(&str[0], str.size(), rw_pos_);
  return str;
}

std::string ByteArray::toHexString() const {
  std::string str = toString();
  std::stringstream ss;
  for (size_t i = 0; i < str.size(); ++i) {
    if (i > 0 && i % 32 == 0) {
      // 保证一行显示32个字符
      ss << std::endl;
    }
    ss << std::setw(2) << std::setfill('0') << std::hex 
       << (int)(uint8_t)str[i] << " ";
  }
  return ss.str();
}

uint64_t ByteArray::get_read_buffers(std::vector<iovec>& buffers, uint64_t len) const {
  len = len > get_readable_size()? get_readable_size(): len;
  if (len == 0) {
    return 0;
  }
  uint64_t res = len;
  size_t cur_node_pos = rw_pos_ % node_base_size_;
  size_t remain_capacity = cur_node_->size - cur_node_pos;
  struct iovec iov;
  Node* cur = cur_node_;
  while (len > 0) {
    if (remain_capacity >= len) {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = remain_capacity;
      len -= remain_capacity;
      cur = cur->next;
      remain_capacity = cur->size;
      cur_node_pos = 0;
    }
    buffers.push_back(iov);
  }
  return res;
}

uint64_t ByteArray::get_read_buffers(std::vector<iovec>& buffers, uint64_t len, uint64_t rw_pos) const {
  len = len > get_readable_size()? get_readable_size(): len;
  if (len == 0) {
    return 0;
  }
  uint64_t res = len;
  size_t cur_node_pos = rw_pos % node_base_size_;
  size_t count = rw_pos / node_base_size_;  // 获取使用的节点
  Node* cur = root_;
  while (count > 0) {
    cur = cur->next;
    --count;
  }

  size_t ncap = cur->size - cur_node_pos;
  struct iovec iov;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = ncap;
      len -= ncap;
      cur = cur->next;
      ncap = cur->size;
      cur_node_pos = 0;
    }
    buffers.push_back(iov);
  }
  return res;
}

uint64_t ByteArray::get_write_buffers(std::vector<iovec>& buffers, uint64_t len) {
  if (len == 0)  {
    return 0;
  }
  addCapacity(len);
  uint64_t res = len;
  size_t cur_node_pos = rw_pos_ % node_base_size_;
  size_t ncap = cur_node_->size - cur_node_pos;
  struct iovec iov;
  Node* cur = cur_node_;
  while (len > 0) {
    if (ncap >= len) {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = len;
      len = 0;
    } else {
      iov.iov_base = cur->ptr + cur_node_pos;
      iov.iov_len = ncap;
      len -= ncap;
      cur = cur->next;  
      ncap = cur->size;
      cur_node_pos = 0;
    }
    buffers.push_back(iov);
  }
  return res;
}

}