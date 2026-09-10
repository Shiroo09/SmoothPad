#pragma once

#include "smoothpad/Types.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace smoothpad::driver {

inline constexpr const wchar_t* kDevicePath = L"\\\\.\\SmoothPadFilter";
inline constexpr std::uint32_t kIoctlHeartbeat = 0x800;
inline constexpr std::uint32_t kIoctlSetMode = 0x801;
inline constexpr std::uint32_t kIoctlGetStatus = 0x802;
inline constexpr std::uint32_t kIoctlReadReports = 0x803;
inline constexpr std::uint32_t kHeartbeatMs = 75;
inline constexpr std::uint32_t kWatchdogMs = 250;

enum class FilterMode : std::uint32_t {
  PassThrough = 0,
  Observe = 1,
  Custom = 2,
};

struct FilterStatus {
  FilterMode mode = FilterMode::PassThrough;
  bool connected = false;
  bool deviceMatched = false;
  std::uint32_t droppedReports = 0;
  std::uint32_t lastHeartbeatMs = 0;
};

class FilterClient {
 public:
  using ReportCallback = std::function<void(std::span<const std::uint8_t>, std::uint64_t qpc)>;

  bool Connect(std::string* error);
  void Disconnect();
  bool Heartbeat();
  bool SetMode(FilterMode mode, std::string* error);
  bool QueryStatus(FilterStatus& status, std::string* error);

  void StartHeartbeatThread();
  void StopHeartbeatThread();

  [[nodiscard]] bool Connected() const noexcept { return connected_; }
  [[nodiscard]] FilterMode Mode() const noexcept { return mode_; }

 private:
  void HeartbeatLoop();

  void* handle_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> runHeartbeat_{false};
  FilterMode mode_ = FilterMode::PassThrough;
  std::thread heartbeat_;
};

[[nodiscard]] std::uint32_t CtlCode(std::uint32_t function);

}  // namespace smoothpad::driver
