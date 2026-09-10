#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace smoothpad {

enum class LogLevel {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
  Trace = 4,
};

class Logger {
 public:
  using Sink = std::function<void(LogLevel, std::string_view)>;

  static Logger& Instance();

  void SetLevel(LogLevel level) noexcept { level_.store(level, std::memory_order_relaxed); }
  [[nodiscard]] LogLevel Level() const noexcept { return level_.load(std::memory_order_relaxed); }
  void SetSink(Sink sink);

  void Write(LogLevel level, std::string_view message);

 private:
  std::atomic<LogLevel> level_{LogLevel::Info};
  std::mutex mutex_;
  Sink sink_;
};

void Log(LogLevel level, std::string_view message);

#define SP_LOG_ERROR(msg) ::smoothpad::Log(::smoothpad::LogLevel::Error, msg)
#define SP_LOG_WARN(msg) ::smoothpad::Log(::smoothpad::LogLevel::Warning, msg)
#define SP_LOG_INFO(msg) ::smoothpad::Log(::smoothpad::LogLevel::Info, msg)
#define SP_LOG_DEBUG(msg) ::smoothpad::Log(::smoothpad::LogLevel::Debug, msg)

}  // namespace smoothpad
