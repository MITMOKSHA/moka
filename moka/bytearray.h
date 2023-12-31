#ifndef __MOKA_BYTEARRAY_H__
#define __MOKA_BYTEARRAY_H__

#include <memory>
#include <string>
#include <vector>
#include <stdint.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace moka {

class ByteArray {
 public:
  using ptr = std::shared_ptr<ByteArray>;
  struct Node {
    // 结构体为链表
    Node();
    Node(size_t s);
    ~Node();

    char* ptr;
    size_t size;  // 当前节点所使用的空间(以字节为单位)
    Node* next;
  };
  ByteArray(size_t node_size = 4096);
  ~ByteArray();

  // write
  // F代表fix，V代表var
  // fix
  void writeInt8F  (int8_t value);
  void writeUint8F (uint8_t value);
  void writeInt16F (int16_t value);
  void writeUint16F(uint16_t value);
  void writeInt32F (int32_t value);
  void writeUint32F(uint32_t value);
  void writeInt64F (int64_t value);
  void writeUint64F(uint64_t value);
  // var(用protobuf的方式进行压缩)
  void writeInt32V (int32_t value);
  void writeUint32V(uint32_t value);
  void writeInt64V (int64_t value);
  void writeUint64V(uint64_t value);

  void writeFloat(float value);
  void writeDouble(double value);

  void writeStringF16(const std::string& value);
  void writeStringF32(const std::string& value);
  void writeStringF64(const std::string& value);
  void writeStringIntV(const std::string& value);
  // without length
  void writeString(const std::string& value);

  // read
  int8_t   readInt8F();
  uint8_t  readUint8F();
  int16_t  readInt16F();
  uint16_t readUint16F();
  int32_t  readInt32F();
  uint32_t readUint32F();
  int64_t  readInt64F();
  uint64_t readUint64F();

  int32_t  readInt32V();
  uint32_t readUint32V();
  int64_t  readInt64V();
  uint64_t readUint64V();

  float    readFloat();
  double   readDouble();

  std::string readString16F();
  std::string readString32F();
  std::string readString64F();
  std::string readStringIntV();

  void clear();  // 清理数据

  void write(const void* buf, size_t size);
  void read (void* buf, size_t size);
  void read (void* buf, size_t size, size_t rw_pos) const;  // 无作用版本
  
  // 读写的位置
  size_t get_rw_position() const { return rw_pos_; }
  void set_rw_position(size_t v);

  // 操作文件
  // TODO:这里是不是可以调整一下返回值？学一下系统API返回写入或者读出字节数？
  bool writeToFile(const std::string& filename) const;
  bool readFromFile(const std::string& filename);

  size_t get_base_size() const { return node_base_size_; }
  size_t get_readable_size() const { return size_ - rw_pos_; } 

  bool isLitteEndian() const;
  void set_little_endian();
  void set_big_endian();

  std::string toString() const;
  std::string toHexString() const;

  // 适配socket的iovec结构
  uint64_t get_read_buffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
  uint64_t get_read_buffers(std::vector<iovec>& buffers, uint64_t len, uint64_t rw_pos) const;

  // 增加容量，不修改rw_pos_
  uint64_t get_write_buffers(std::vector<iovec>& buffers, uint64_t len);
  size_t get_size() const { return size_; }
 private:
  void addCapacity(size_t size);
  size_t get_remain_capacity() const { return capacity_ - rw_pos_; }
 private:
  size_t node_base_size_;          // 链表节点的标准大小
  size_t rw_pos_;                  // 读写指针
  size_t capacity_;                // byteArray容量(总空间)
  size_t size_;                    // byteArray大小(当前占用的空间)
  int32_t endian_;                 // 大小端模式

  Node* root_;        
  Node* cur_node_;                 // 当前节点指针
};

}

#endif