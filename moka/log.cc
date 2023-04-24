#include "log.h"

namespace moka {

const char* LogLevel::toString(LogLevel::level level) {
  switch (level) {
#define XX(name) \
  case LogLevel::name: \
    return #name; \
    break;
  XX(DEBUG);
  XX(INFO);
  XX(WARN);
  XX(ERROR);
  XX(FATAL);
#undef XX
  default:
    return "UNKNOW";
  }
  return "UNKNOW";
}

LogEvent::LogEvent(const char* file, uint32_t elapse, uint32_t line, uint32_t thread_id,
           uint32_t fiber_id, uint32_t timestamp, std::shared_ptr<Logger> logger, LogLevel::level level)
    : filename_(file), elapse_(elapse), line_num_(line), thread_id_(thread_id),
      fiber_id_(fiber_id), timestamp_(timestamp), logger_(logger), level_(level) {
}

LogEvent::~LogEvent() {
}

void LogEvent::format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);  // 初始化操作
  format(fmt, al);
  va_end(al);         // 将va_list宏设置为undefined
}

void LogEvent::format(const char* fmt, va_list al) {
  char* buf = nullptr;
  // 将字符串打印到buf中
  int len = vasprintf(&buf, fmt, al);  // 动态分配一段空间(以'\0'结尾)，将起始地址传递给buf参数
  // 返回-1表示内存空间不可用或错误；成功时返回打印的字节数
  if (len != -1) {
    ss_ << std::string(buf, len);  // 输出字符串到LogEvent的ss字符串缓冲区中
    free(buf);                     // 释放空间
  }
}

LogEventWrap::LogEventWrap(LogEvent::ptr e)
    : event_(e) {
}

LogEventWrap::~LogEventWrap() {
  event_->get_logger()->log(event_->get_level(), event_);  // 在析构函数中打印log
}

std::stringstream &LogEventWrap::get_ss() {
  return event_->get_ss();
}

Logger::Logger(const std::string& name)
    : name_(name), level_(LogLevel::DEBUG) {
  formatter_.reset(new LogFormatter("[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));  // 初始化日志器的默认格式器
}

void Logger::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) {    // 判断level是否有输出？若输出的日志级别大于当前的日志器级别即可输出
    for (auto& i : appenders_) {
      i->log(level, event);  // 调用appender的log输出
    }
  }
}

void Logger::debug(LogEvent::ptr event) {
  log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
  log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
  log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
  log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
  log(LogLevel::FATAL, event);
}

void Logger::addAppender(LogAppender::ptr appender) {
  if (!appender->get_formatter()) {
    // 如果没有formatter则把当前日志器的formatter给它
    appender->set_formatter(formatter_);
  }
  appenders_.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender) {
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it); // 会出现迭代器失效
      break;
    }
  }
}

void StdoutLogAppender::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) { 
    std::cout << formatter_->format(event);   // cout将该字符串输出到终端中
  }
}


FileLogAppender::FileLogAppender(const std::string filename)
    : filename_(filename) {  // 冒号前空4行(style)
    assert(reopen());
}

void FileLogAppender::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) {
    filestream_ << formatter_->format(event);  // 输出到文件流中(根据不同的item输出不同的内容)
  }
}

bool FileLogAppender::reopen() {
  if (filestream_) {
    filestream_.close();
  }
  filestream_.open(filename_, std::ios::app);   // 如果没有文件生成文件；如果有文件追加写入
  return !!filestream_;
}

LogFormatter::LogFormatter(const std::string &pattern) : pattern_(pattern) {
  init();  // 初始化items
}

std::string LogFormatter::format(LogEvent::ptr event) {
  std::stringstream ss;     // 用于存储string流缓冲
  for (auto& i : items_) {  // 遍历日志格式items
    i->format(ss, event);   // 多态，调用子类重写了的具体的虚函数format，并信息将放入string流中
  }
  return ss.str();          // 返回字符流中的内容
}

// 仿造log4jcpp的日志格式解析
// %(xxx) %xxx{xxx} %%
void LogFormatter::init() {
  /*
  %m 消息
  %p 优先级
  %r 启动后的时间
  %c 日志名
  %t 线程id
  %n 回车换行
  %d 时间
  %f 文件名
  %l 行号
  */
  // param@1表示格式符
  // param@2表示解析到的字符
  // param@3标志是否为解析的格式符(1 or 0)
  std::vector<std::tuple<std::string, std::string, int>> vec;
  std::string nstr;  // 用于暂存非格式符
  for (size_t i = 0; i < pattern_.size(); ++i) {
    // 若不为格式符则加入nstr中
    if (pattern_[i] != '%') {
      nstr.append(1, pattern_[i]);
      continue;
    }
    if (i+1 < pattern_.size() && pattern_[i+1] == '%') {
      nstr.append(1, '%');
      continue;
    }
    // 此时i指向'%'
    size_t n = i + 1;
    int fmt_status = 0;  // 状态机状态号
    std::string str;     // 暂存格式符
    std::string fmt;     // 暂存解析到的格式
    size_t fmt_begin_idx = 0;
    while (n < pattern_.size()) {
      if (!fmt_status && !isalpha(pattern_[n]) && pattern_[n] != '{' &&
          pattern_[n] != '}') {
        // 不连续
        str = pattern_.substr(i+1, n-i-1);  // 存入格式符
        break;
      }
      if (fmt_status == 0) {
        if (pattern_[n] == '{') {
          str = pattern_.substr(i+1, n-i-1);
          fmt_status = 1; // 解析格式
          fmt_begin_idx = n;
          ++n;
          continue;
        }
      }
      if (fmt_status == 1) {
        if (pattern_[n] == '}') {
          fmt = pattern_.substr(fmt_begin_idx+1, n-fmt_begin_idx-1);
          fmt_status = 0; // 解析格式
          ++n;
          break;
        }
      }
      ++n;
      if (n == pattern_.size()) {
        if (str.empty()) {
          str = pattern_.substr(i+1);
        }
      }
    }
    // 处理nstr
    if (fmt_status == 0) {
      if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, std::string(), 0));
        nstr.clear();  // 放入之后清空容器
      }
      vec.push_back(std::make_tuple(str, fmt, 1));
      i = n - 1;
    } else if (fmt_status == 1) {
      std::cout << "pattern parse error: " << pattern_ << "-" << 
                    pattern_.substr(i) << std::endl;
      vec.push_back(std::make_tuple("<<pattern error>>", fmt, 0));
    }
  }
  // 处理nstr中剩余部分
  if (!nstr.empty()) {
    vec.push_back(std::make_tuple(nstr, "", 1));
  }
  static std::unordered_map<std::string, std::function<FormatterItem::ptr(const std::string& str)>> s_format_items = {
  // 宏对应lambda表达式
#define XX(str, C) \
    {#str, [](const std::string& fmt){ return FormatterItem::ptr(new C(fmt));}} 

        XX(m, MessageFormatItem),
        XX(p, LevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, NameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(n, NewLineFormatItem),
        XX(d, DateTimeFormatItem),
        XX(f, FilenameFormatItem),
        XX(l, LineFormatItem),
        XX(T, TabFormatItem),
        XX(F, FiberIdFormatItem),
#undef XX
  };//   };
  for(auto& i : vec) {  // 遍历tuple数组
    if(std::get<2>(i) == 0) {  // 若不是格式符
      items_.push_back(FormatterItem::ptr(new StringFormatItem(std::get<0>(i))));
    } else {
      // 解析对应的格式符，根据哈希表所引导具体的item执行lambda表达式
      auto it = s_format_items.find(std::get<0>(i));
      if(it == s_format_items.end()) {
        // 若未解析到对应的格式符(除格式符以外的字符)，则调用stingformat使用输出流打印出来
        items_.push_back(FormatterItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
      } else {
        items_.push_back(it->second(std::get<1>(i)));  // 调用对应的函数(传入fmt)
      }
    }
    // DEBUG
    // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
  }
  // std::cout << items_.size();
}

LoggerManager::LoggerManager() {
  root_.reset(new Logger);  // 初始化根日志指针
  root_->addAppender(LogAppender::ptr(new StdoutLogAppender));  // 根日志默认以控制台作为appender
}

Logger::ptr LoggerManager::get_logger(const std::string &name) {
  auto it = loggers_.find(name);
  // 若在日志集合中找到，则返回该日志器，否则返回根日志
  return it == loggers_.end()? root_: it->second;
}
}
