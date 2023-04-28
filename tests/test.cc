#include <iostream>
#include <pthread.h>
#include "../moka/log.h"
#include "../moka/util.h"

int main() {
  moka::Logger::ptr logger(new moka::Logger);  // 调用无参构造函数新建日志器，并指定初始化格式串
  // 测试控制台输出
  logger->addAppender(moka::LogAppender::ptr(new moka::StdoutLogAppender));  // 将标准输出流作为日志输出的目的地

  // 测试文件流输出
  moka::FileLogAppender::ptr file_appender(new moka::FileLogAppender("../tests/log.txt"));
  logger->addAppender(file_appender);

  moka::LogFormatter::ptr fmt(new moka::LogFormatter("%d%T%p%T%m%n"));  // 自定义格式输出(不使用默认格式器)
  file_appender->set_formatter(fmt, true);
  file_appender->set_level(moka::LogLevel::ERROR);

  // moka::LogEvent::ptr event(new moka::LogEvent(__FILE__, 0, __LINE__, moka::GetThreadId(), moka::GetFiberId(), time(0), logger));  // 初始化日志事件
  // event->get_ss() << "hello moka log";  // 将这个字符串放入缓冲区中，%m的getcontent会获取缓冲区中的内容.str()
  // logger->log(moka::LogLevel::DEBUG, event);
  
  // 测试宏
  // MOKA_LOG_INFO(logger) << "test macro";
  // MOKA_LOG_DEBUG(logger) << "test macro";
  // MOKA_LOG_ERROR(logger) << "test macro";
  // MOKA_LOG_FATAL(logger) << "test macro";
  // MOKA_LOG_WARN(logger) << "test macro";

  // MOKA_LOG_FMT_ERROR(logger, "test macro fmt debug %s", "aa");
  
  auto l = moka::LoggerMgr::get_instance()->get_logger("xx");
  MOKA_LOG_INFO(l) << "xxx";
  return 0;
}