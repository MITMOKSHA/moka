#ifndef __MOKA_LOG_H__
#define __MOKA_LOG_H__

#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <memory>
#include <string>
#include <list>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <functional>
#include <unordered_map>
#include "util.h"
#include "singleton.h"

// 测试宏
#define MOKA_LOG_LEVEL(logger, level) \
  if(logger->get_level() <= level) \
    moka::LogEventWrap(moka::LogEvent::ptr(new moka::LogEvent(__FILE__, 0, __LINE__, moka::GetThreadId(), \
                 moka::GetFiberId(), time(0), logger, level))).get_ss()
          
#define MOKA_LOG_DEBUG(logger) MOKA_LOG_LEVEL(logger, moka::LogLevel::DEBUG)
#define MOKA_LOG_INFO(logger) MOKA_LOG_LEVEL(logger, moka::LogLevel::INFO)
#define MOKA_LOG_WARN(logger) MOKA_LOG_LEVEL(logger, moka::LogLevel::WARN)
#define MOKA_LOG_ERROR(logger) MOKA_LOG_LEVEL(logger, moka::LogLevel::ERROR)
#define MOKA_LOG_FATAL(logger) MOKA_LOG_LEVEL(logger, moka::LogLevel::FATAL)

// 支持格式符打印输出
#define MOKA_LOG_FMT_LEVEL(logger, level, fmt, ...) \
  if (logger->get_level() <= level) \
    moka::LogEventWrap(moka::LogEvent::ptr(new moka::LogEvent(__FILE__, 0, __LINE__, moka::GetThreadId(), \
                 moka::GetFiberId(), time(0), logger, level))).get_event()->format(fmt, __VA_ARGS__)

#define MOKA_LOG_FMT_DEBUG(logger, fmt, ...) MOKA_LOG_FMT_LEVEL(logger, moka::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define MOKA_LOG_FMT_INFO(logger, fmt, ...) MOKA_LOG_FMT_LEVEL(logger, moka::LogLevel::INFO, fmt, __VA_ARGS__)
#define MOKA_LOG_FMT_WARN(logger, fmt, ...) MOKA_LOG_FMT_LEVEL(logger, moka::LogLevel::WARN, fmt, __VA_ARGS__)
#define MOKA_LOG_FMT_ERROR(logger, fmt, ...) MOKA_LOG_FMT_LEVEL(logger, moka::LogLevel::ERROR, fmt, __VA_ARGS__)
#define MOKA_LOG_FMT_FATAL(logger, fmt, ...) MOKA_LOG_FMT_LEVEL(logger, moka::LogLevel::FATAL, fmt, __VA_ARGS__)

namespace moka {


class Logger;

// 日志级别
class LogLevel {
 public:
  enum level {
    UNKNOW = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
  };
  static const char* toString(LogLevel::level);
};

// 日志事件，用于记录日志现场
class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;
  LogEvent(const char* file, uint32_t elapse, uint32_t line, uint32_t thread_id,
           uint32_t fiber_id, uint32_t timestamp,
           std::shared_ptr<Logger> logger, LogLevel::level level = LogLevel::DEBUG);
  ~LogEvent();
  
  const char* get_filename() { return filename_; }
  uint32_t get_elapse() { return elapse_; }
  uint32_t get_line_num() { return line_num_; }
  uint32_t get_thread_id() { return thread_id_; }
  uint32_t get_fiber_id() { return fiber_id_; }
  uint32_t get_timestamp_() { return timestamp_; }
  const std::string get_content() const { return ss_.str(); }
  LogLevel::level get_level() { return level_; }
  std::shared_ptr<Logger> get_logger() { return logger_; }
  std::stringstream& get_ss() {return ss_; }
  void format(const char* fmt, ...);
  void format(const char* fmt, va_list al);

 private:
  const char* filename_ = nullptr;             // 文件名
  uint32_t elapse_ = 0;                        // 程序启动开始到现在的毫秒数
  uint32_t line_num_ = 0;                      // 行号
  uint32_t thread_id_ = 0;                     // 线程id
  uint32_t fiber_id_ = 0;                      // 协程id
  uint64_t timestamp_ = 0;                     // 时间戳
  std::stringstream ss_;                       // 文件内容
  std::shared_ptr<Logger> logger_;             // 日志器
  LogLevel::level level_;                      // 日志级别(这里不能直接用日志器的level，前向声明不支持)
};

// 日志事件包装类(将日志事件和日志器包装起来)
class LogEventWrap {
 public:
  LogEventWrap(LogEvent::ptr e);
  ~LogEventWrap();
  std::stringstream& get_ss();
  LogEvent::ptr get_event() { return event_; }
 private:
  LogEvent::ptr event_;
};

// 日志格式器
class LogFormatter {
 public:
  using ptr = std::shared_ptr<LogFormatter>;
  LogFormatter(const std::string& pattern);  // 根据pattern的格式来解析信息
  std::string format(LogEvent::ptr event);   // 将日志事件格式化为字符串

 public:
  class FormatterItem {                      // 工厂模式，实现对针对各个格式符进行不同的输出
   public:
    using ptr = std::shared_ptr<FormatterItem>;
    virtual ~FormatterItem() {}
    virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
  };

 private:
  void init();                              // 基于状态机完成日志格式的解析
  std::string pattern_;                     // formatter输出格式
  std::vector<FormatterItem::ptr> items_;   // 存储多个格式项(解析的格式符和其他字符)
};

// 日志输出器(抽象基类，工厂模式)
class LogAppender {
 public:
  using ptr = std::shared_ptr<LogAppender>;
  virtual ~LogAppender() {}
  virtual void log(LogLevel::level level, LogEvent::ptr event) = 0;               // 调用日志格式器的format方法遍历items输出字符串
  void set_formatter(LogFormatter::ptr formatter) { formatter_ = formatter; };
  LogFormatter::ptr get_formatter() { return formatter_; }
  LogLevel::level get_level() { return level_; }
  void set_level(LogLevel::level level) { level_ = level; }

 protected:   // 派生类要使用
  LogLevel::level level_ = LogLevel::DEBUG;
  LogFormatter::ptr formatter_;
};

// 日志器
class Logger {
 public:
  using ptr = std::shared_ptr<Logger>;
  Logger(const std::string& name = "root");

  void log(LogLevel::level level_, LogEvent::ptr event);  // 判断该日志事件级别高于日志器本身则调用Appender的log函数将日志输出
  void debug(LogEvent::ptr event);
  void info(LogEvent::ptr event);
  void warn(LogEvent::ptr event);
  void error(LogEvent::ptr event);
  void fatal(LogEvent::ptr event);

  void addAppender(LogAppender::ptr appender);
  void delAppender(LogAppender::ptr appender);

  LogLevel::level get_level() const { return level_; }
  void set_level(LogLevel::level level) { level_ = level; }
  const std::string& get_name() const { return name_; }
 private:
  std::string name_;                        // 日志名称
  LogLevel::level level_;                   // 日志级别(日志默认级别，若日志事件的级别大于日志器的级别则输出)
  std::list<LogAppender::ptr> appenders_;   // appender集合(一个日志可以有多个输出地，如：文件、终端)
  LogFormatter::ptr formatter_;             // 格式器(日志默认格式)
};

// 输出到控制台的日志输出器
class StdoutLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<StdoutLogAppender>;
  virtual void log(LogLevel::level level, LogEvent::ptr event) override;
 private:
};

// 输出到文件的日志输出器
class FileLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<FileLogAppender>;
  FileLogAppender(const std::string filename);
  virtual void log(LogLevel::level level, LogEvent::ptr event) override;
 private:
  bool reopen();  // 根据文件名成员重新打开文件，文件打开成功返回true
  std::string filename_;
  std::ofstream filestream_;
};

// 负责管理所有的日志器(单例模式)
class LoggerManager {
 public:
  LoggerManager();
  Logger::ptr get_logger(const std::string& name);

 private:
  // void init();
  std::unordered_map<std::string, Logger::ptr> loggers_;  // 日志集合
  Logger::ptr root_;  // 根日志
};

typedef moka::Singleton<LoggerManager> LoggerMgr;

class MessageFormatItem : public LogFormatter::FormatterItem {
 public:
  MessageFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_content();
  }

};

class LevelFormatItem : public LogFormatter::FormatterItem {
 public:
  LevelFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << LogLevel::toString(event->get_level());
  }

};

class ElapseFormatItem : public LogFormatter::FormatterItem {
 public:
  ElapseFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_elapse();
  }

};

class NameFormatItem : public LogFormatter::FormatterItem {
 public:
  NameFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_logger()->get_name();
  }

};

class ThreadIdFormatItem : public LogFormatter::FormatterItem {
 public:
  ThreadIdFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_thread_id();
  }

};

class FiberIdFormatItem : public LogFormatter::FormatterItem {
 public:
  FiberIdFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_fiber_id();
  }

};

class DateTimeFormatItem : public LogFormatter::FormatterItem {
 public:
  DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
    : format_(format) {
      if (format_.empty()) {
        format_ = "%Y-%m-%d %H:%M:%S";
      }
    }
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    struct tm* tmp;
    char buf[64];
    time_t t = event->get_timestamp_();
    tmp = localtime(&t);
    strftime(buf, sizeof(buf), format_.c_str(), tmp);
    os << buf;
  }
 private:
  std::string format_;
};

class FilenameFormatItem : public LogFormatter::FormatterItem {
 public:
  FilenameFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_filename();
  }
};

class LineFormatItem : public LogFormatter::FormatterItem {
 public:
  LineFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << event->get_line_num();
  }
};

class NewLineFormatItem : public LogFormatter::FormatterItem {
 public:
  NewLineFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << std::endl;
  }
};

class StringFormatItem : public LogFormatter::FormatterItem {
 public:
  StringFormatItem(const std::string& str)
      : string_(str) {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    // 输出构造函数传入的参数
    os << string_;
  }
 private:
  std::string string_;
};

class TabFormatItem : public LogFormatter::FormatterItem {
 public:
  TabFormatItem(const std::string& str = "") {}
  virtual void format(std::ostream& os, LogEvent::ptr event) override {
    os << "\t";
  }
 private:
};
}

#endif