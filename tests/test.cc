#include <iostream>
#include <string>
#include <pthread.h>
#include "../moka/log.h"
#include "../moka/util.h"
#include "../moka/macro.h"
#include "../moka/singleton.h"

void test_macro() {
  moka::Logger::ptr logger = MOKA_LOG_NAME("system");
  
  // 测试宏
  MOKA_LOG_DEBUG(logger) << "test macro debug";
  MOKA_LOG_INFO(logger) << "test macro info";
  MOKA_LOG_ERROR(logger) << "test macro error";
  MOKA_LOG_FATAL(logger) << "test macro fatal";
  MOKA_LOG_WARN(logger) << "test macro warn";

  // 测试支持C语言格式符解析的版本
  MOKA_LOG_FMT_DEBUG(logger, "test macro fmt debug %d", 1);
  MOKA_LOG_FMT_INFO(logger, "test macro fmt %s %d", "info", 2);
  MOKA_LOG_FMT_ERROR(logger, "test macro fmt %s %f", "error", 2.5);
}

void test_logger() {
  // 新建日志器，日志级别默认DEBUG(最后项目发布可以更改其日志级别，过滤DEBUG日志), 并使用默认格式串
  moka::Logger::ptr logger(new moka::Logger);

  // 测试控制台输出
  moka::StdoutLogAppender::ptr std_appender(new moka::StdoutLogAppender);
  // 测试文件流输出，指定输出文件(如果没有文件则新建文件，如果有文件则追加写入)
  moka::FileLogAppender::ptr file_appender(new moka::FileLogAppender("../build/log.txt"));

  // 输出器的日志级别默认为DEBUG
  MOKA_ASSERT(file_appender->get_level() == moka::LogLevel::DEBUG);

  // 加入到日志器的输出器集合中
  logger->addAppender(std_appender);
  logger->addAppender(file_appender);

  // 如果输出器未指定格式串，则默认使用的是日志器的格式串
  MOKA_ASSERT(std_appender->get_formatter()->get_pattern() ==
      logger->get_formatter()->get_pattern());

  // 新建日志格式器
  const std::string str ="[%d]%T%p%T%m%n";
  moka::LogFormatter::ptr fmt(new moka::LogFormatter(str));

  MOKA_ASSERT(!fmt->isError());

  // 设置输出器的日志格式，自定义输出格式(不使用日志器的默认格式)
  std_appender->set_formatter(fmt, true);

  MOKA_ASSERT(std_appender->get_formatter()->get_pattern() !=
      logger->get_formatter()->get_pattern());
  MOKA_ASSERT(std_appender->get_formatter()->get_pattern() ==
      str);

  // 只有日志事件的级别高于日志输出地的级别才会输出
  // 这里不会输出到文件中(因为INFO的级别大于DEBUG)
  file_appender->set_level(moka::LogLevel::INFO);

  // 新建日志事件
  moka::LogEvent::ptr event(new moka::LogEvent(__FILE__, 0, __LINE__, moka::GetThreadId(),
    moka::GetFiberId(), "test", time(0), logger, moka::LogLevel::DEBUG));

  // 使用字符串流，流式输出信息，并在解析格式符%m时将其输出
  event->get_ss() << "test logger";

  // 打印日志
  logger->log(event->get_level(), event);

  // 测试日志包装器
  {
    // 设置回默认格式(即日志器格式串)打印
    moka::LogFormatter::ptr fmt(new moka::LogFormatter(logger->get_formatter()->get_pattern()));
    std_appender->set_formatter(fmt, true);
    // 流式输出信息
    auto t = moka::LogEventWrap(event);
  }

  // 删除终端输出地
  logger->delAppender(std_appender);

  // 不会再次打印，因为终端输出器删除了
  logger->log(event->get_level(), event);

  // 测试日志管理器类，若system日志器不存在则新建该日志器
  moka::Logger::ptr system = moka::LoggerMgr::GetInstance()->get_logger("system");
  MOKA_ASSERT(system->get_name() == "system");

  // 日志管理器类对象在初始化时会新建一个根日志
  MOKA_ASSERT(moka::LoggerMgr::GetInstance()->get_root()->get_name() == "root");
}

int main() {
  test_logger();
  test_macro();
  return 0;
}