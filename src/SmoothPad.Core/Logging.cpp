#include "smoothpad/Logging.h"

#include <iostream>

namespace smoothpad {

Logger& Logger::Instance() {
  static Logger g;
  return g;
}

void Logger::SetSink(Sink sink) {
  std::lock_guard lock(mutex_);
  sink_ = std::move(sink);
}

void Logger::Write(LogLevel level, std::string_view message) {
  if (static_cast<int>(level) > static_cast<int>(Level())) {
    return;
  }
  Sink sink;
  {
    std::lock_guard lock(mutex_);
    sink = sink_;
  }
  if (sink) {
    sink(level, message);
    return;
  }
  const char* tag = "I";
  switch (level) {
    case LogLevel::Error:
      tag = "E";
      break;
    case LogLevel::Warning:
      tag = "W";
      break;
    case LogLevel::Info:
      tag = "I";
      break;
    case LogLevel::Debug:
      tag = "D";
      break;
    case LogLevel::Trace:
      tag = "T";
      break;
  }
  std::clog << '[' << tag << "] " << message << '\n';
}

void Log(LogLevel level, std::string_view message) {
  Logger::Instance().Write(level, message);
}

}  // namespace smoothpad
