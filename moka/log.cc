#include "log.h"
#include "config.h"

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

LogLevel::level LogLevel::fromString(const std::string& str) {
#define XX(name, v) \
  if (str == #v) \
    return LogLevel::name;

  // 支持小写
  XX(DEBUG, debug);
  XX(INFO, info);
  XX(WARN, warn);
  XX(ERROR, error);
  XX(FATAL, fatal);
  // 支持大写
  XX(DEBUG, DEBUG);
  XX(INFO, INFO);
  XX(WARN, WARN);
  XX(ERROR, ERROR);
  XX(FATAL, FATAL);
#undef XX
  return LogLevel::UNKNOW;
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

void LogAppender::set_formatter(LogFormatter::ptr formatter, bool is_own_fmt) {
  Mutex::LockGuard lock(mutex_);
  formatter_ = formatter;
  is_own_fmt_ = is_own_fmt;
}

void LogAppender::set_formatter(const std::string& val, bool is_own_fmt) {
  Mutex::LockGuard lock(mutex_);
  LogFormatter::ptr fmt(new LogFormatter(val));
  if (fmt->isError()) {
    std::cout << "log appender setformatter " << "value=" << val << " invalid formatter" << std::endl;
    return;
  }
  formatter_ = fmt;
  is_own_fmt_ = is_own_fmt;
}

LogFormatter::ptr LogAppender::get_formatter() {
  Mutex::LockGuard lock(mutex_);
  return formatter_;
}

Logger::Logger(const std::string& name)
    : name_(name), level_(LogLevel::DEBUG), root_(nullptr) {
  formatter_.reset(new LogFormatter("[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));  // 初始化日志器的默认格式器
}

void Logger::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) {    // 判断level是否有输出？若输出的日志级别大于当前的日志器级别即可输出
    Mutex::LockGuard lock(mutex_);
    for (auto& i : appenders_) {
      i->log(level, event);  // 调用appender的log输出
    }
    if (appenders_.empty() && root_) {
      // 如果logger没有对应的appender，使用的root写log
      root_->log(level, event);
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
  Mutex::LockGuard lock(mutex_);
  if (!(appender->has_fmt())) {
    // 如果没有formatter则把当前日志器的formatter给它
    appender->set_formatter(formatter_, false);  // false表示appender用的是日志器的fmt
  }
  appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  Mutex::LockGuard lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it); // 会出现迭代器失效
      break;
    }
  }
}

void Logger::clearAppenders() {
  Mutex::LockGuard lock(mutex_);
  appenders_.clear();
}

void Logger::set_formatter(LogFormatter::ptr fmt) {
  Mutex::LockGuard lock(mutex_);
  formatter_ = fmt;
  updateAppenderFmt();
}
void Logger::set_formatter(const std::string& val) {
  Mutex::LockGuard lock(mutex_);
  moka::LogFormatter::ptr fmt(new moka::LogFormatter(val));
  if (fmt->isError()) {
    std::cout << "log setformatter name" << name_ << "value=" << val << " invalid formatter" << std::endl;
    return;
  }
  formatter_ = fmt;
  updateAppenderFmt();
}

LogFormatter::ptr Logger::get_formatter() {
  Mutex::LockGuard lock(mutex_);
  return formatter_;
}

std::string Logger::toYamlString() {
  Mutex::LockGuard lock(mutex_);
  YAML::Node node;
  node["name"] = name_;
  node["level"] = LogLevel::toString(level_);
  if (formatter_) {
    node["formatter"] = formatter_->get_pattern();
  }
  for (auto i : appenders_) {
    node["appender"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void Logger::updateAppenderFmt() {
  for (auto& i : appenders_) {
    // 若该appender继承日志器的fmt，则更新它
    if (!(i->has_fmt())) {
      i->set_formatter(formatter_, false);
    }
  }
}

void StdoutLogAppender::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) { 
    Mutex::LockGuard lock(mutex_);      // 保证一条日志输出完整的信息
    std::cout << formatter_->format(event);   // cout将该字符串输出到终端中
  }
}

std::string StdoutLogAppender::toYamlString() {
  Mutex::LockGuard lock(mutex_);
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (level_ != LogLevel::UNKNOW) {
    node["level"] = LogLevel::toString(level_);
  }
  if (is_own_fmt_) {
    node["formatter"] = formatter_->get_pattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

FileLogAppender::FileLogAppender(const std::string filename)
    : filename_(filename) {  // 冒号前空4行(style)
    assert(reopen());
}

void FileLogAppender::log(LogLevel::level level, LogEvent::ptr event) {
  if (level >= level_) {
    Mutex::LockGuard lock(mutex_);
    filestream_ << formatter_->format(event);  // 输出到文件流中(根据不同的item输出不同的内容)
  }
}

std::string FileLogAppender::toYamlString() {
  Mutex::LockGuard lock(mutex_);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = filename_;
  if (level_ != LogLevel::UNKNOW) {
    node["level"] = LogLevel::toString(level_);
  }
  if (is_own_fmt_) {
    // 如果是继承的日志器的fmt则不输出(即appender没有自己的fmt)
    node["formatter"] = formatter_->get_pattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

bool FileLogAppender::reopen() {
  Mutex::LockGuard lock(mutex_);
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
      std::cout << "pattern parse error: " << pattern_ << "-"
                << pattern_.substr(i) << std::endl;
      error_ = true;
      
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
        error_ = true;
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
  // 将根日志放入Manager的集合中
  loggers_[root_->get_name()] = root_;
}

Logger::ptr LoggerManager::get_logger(const std::string &name) {
  Mutex::LockGuard lock(mutex_);
  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  // 如果不存在则新建一个logger，但是该logger没有对应的appender
  Logger::ptr logger(new Logger(name));
  logger->set_root(root_);    // 更新日志器的logger
  loggers_[name] = logger;
  return logger;
}

std::string LoggerManager::toYamlString() {
  Mutex::LockGuard lock(mutex_);
  YAML::Node node;
  for (auto i : loggers_) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

// 日志输出器配置
struct LogAppenderDefine {
  int type = 0;  // 1 File 2 Stdout
  LogLevel::level level = LogLevel::UNKNOW;
  std::string formatter;
  std::string file;

  bool operator==(const LogAppenderDefine& oth) const {
    return type == oth.type &&
           level == oth.level &&
           formatter == oth.formatter &&
           file == oth.file;
  }
};

// 日志配置类，用于读取yaml文件数据的过渡
struct LogDefine {
  std::string name;
  LogLevel::level level = LogLevel::UNKNOW;
  std::string formatter;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine& oth) const {
    return name == oth.name &&
           level == oth.level &&
           formatter == oth.formatter &&
           appenders == oth.appenders;
  }
  bool operator<(const LogDefine& oth) const {
    return name < oth.name;
  }
};

// 全特化
template<>
class LexicalCast<std::string, LogDefine> {
public:
    LogDefine operator()(const std::string& v) {
        YAML::Node n = YAML::Load(v);
        LogDefine ld;
        if(!n["name"].IsDefined()) {
            std::cout << "log config error: name is null, " << n
                      << std::endl;
            throw std::logic_error("log config name is null");
        }
        ld.name = n["name"].as<std::string>();
        // YAML的as函数只支持基本类型的转换，转换成string之后再通过LogLevel的fromString转换为LogLevel对象
        ld.level = LogLevel::fromString(n["level"].IsDefined()? n["level"].as<std::string>() : "");
        if(n["formatter"].IsDefined()) {
            ld.formatter = n["formatter"].as<std::string>();
        }

        if(n["appenders"].IsDefined()) {
            //std::cout << "==" << ld.name << " = " << n["appenders"].size() << std::endl;
            // 遍历获取yaml文件中的appender
            for(size_t i = 0; i < n["appenders"].size(); ++i) {
                auto n_a = n["appenders"][i];
                if(!n_a["type"].IsDefined()) {
                    std::cout << "log config error: appender type is null, " << n_a
                              << std::endl;
                    continue;
                }
                std::string type = n_a["type"].as<std::string>();
                LogAppenderDefine lad;
                if(type == "FileLogAppender") {
                    lad.type = 1;
                    if(!n_a["file"].IsDefined()) {
                        std::cout << "log config error: fileappender file is null, " << n_a
                              << std::endl;
                        continue;
                    }
                    lad.file = n_a["file"].as<std::string>();
                    if(n_a["formatter"].IsDefined()) {
                        lad.formatter = n_a["formatter"].as<std::string>();
                    }
                } else if(type == "StdoutLogAppender") {
                    lad.type = 2;
                    if(n_a["formatter"].IsDefined()) {
                        lad.formatter = n_a["formatter"].as<std::string>();
                    }
                } else {
                    std::cout << "log config error: appender type is invalid, " << n_a
                              << std::endl;
                    continue;
                }

                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator()(const LogDefine& i) {
        YAML::Node n;
        n["name"] = i.name;
        if(i.level != LogLevel::UNKNOW) {
            n["level"] = LogLevel::toString(i.level);
        }
        if(!i.formatter.empty()) {
            n["formatter"] = i.formatter;
        }

        for(auto& a : i.appenders) {
            YAML::Node n_a;
            if(a.type == 1) {
                n_a["type"] = "FileLogAppender";
                n_a["file"] = a.file;
            } else if(a.type == 2) {
                n_a["type"] = "StdoutLogAppender";
            }
            if(a.level != LogLevel::UNKNOW) {
                n_a["level"] = LogLevel::toString(a.level);
            }

            if(!a.formatter.empty()) {
                n_a["formatter"] = a.formatter;
            }

            n["appenders"].push_back(n_a);
        }
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
};

// 全局变量，初始化配置类
moka::ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
  moka::Config::lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
  LogIniter() {
    // 注册初始化log配置更改的回调函数，从LogDefine结构体中读出对应的信息，更改logger类属性值
    g_log_defines->addListener(0xF1E231, [](const std::set<LogDefine>& old_val,
      const std::set<LogDefine>& new_val) {
      // 这里logger转换为std::string需要
      MOKA_LOG_INFO(MOKA_LOG_ROOT()) << "on_logger_conf_changed";
      // 新增日志
      for (auto log_def : new_val) {
        auto it = old_val.find(log_def);  // 按照LogDefine中重载的<来比较的
        moka::Logger::ptr logger;
        if (it == old_val.end()) {
          // 新增logger
          // 等价于logger.reset(new moka::Logger(log_def.name));
          logger = MOKA_LOG_NAME(log_def.name);
        } else if (!(log_def == *it)) {
          // 修改logger
          logger = MOKA_LOG_NAME(log_def.name);
        }

        logger->set_level(log_def.level);
        if (!(log_def.formatter.empty())) {
          logger->set_formatter(log_def.formatter);
        }
        logger->clearAppenders();
        // 初始化logger的appender
        for (auto a : log_def.appenders) {
          moka::LogAppender::ptr ap;
          if (a.type == 1) {
            // file
            ap.reset(new FileLogAppender(a.file));
          } else if (a.type == 2) {
            // stdOut
            ap.reset(new StdoutLogAppender);
          }
          ap->set_level(a.level);
          if (!(a.formatter.empty())) {
            ap->set_formatter(a.formatter, true);
          }
          logger->addAppender(ap);
        }
      }

      for (auto i : old_val) {
        auto it = new_val.find(i);
        if (it == new_val.end()) {
          // 删除logger
          auto logger = MOKA_LOG_NAME(i.name);
          // 设置一个很高级的level，让日志写入到输出地
          logger->set_level((LogLevel::level)100);
          logger->clearAppenders();
        }
      }
    });
  }
};

// 在main函数之前注册log配置更改的回调函数(全局变量和静态变量在main函数完成初始化)
static LogIniter __log_init;

}
