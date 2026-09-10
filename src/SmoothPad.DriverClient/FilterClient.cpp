#include "smoothpad/driver/FilterClient.h"

#include "smoothpad/Logging.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winioctl.h>
#endif

#include <chrono>

namespace smoothpad::driver {

std::uint32_t CtlCode(std::uint32_t function) {
#ifdef _WIN32
  return CTL_CODE(FILE_DEVICE_UNKNOWN, function, METHOD_BUFFERED, FILE_ANY_ACCESS);
#else
  return 0x220000u | (function << 2);
#endif
}

bool FilterClient::Connect(std::string* error) {
#ifdef _WIN32
  Disconnect();
  HANDLE h = CreateFileW(kDevicePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (error) *error = "SmoothPadFilter is not installed or not loaded";
    connected_ = false;
    return false;
  }
  handle_ = h;
  connected_ = true;
  mode_ = FilterMode::PassThrough;
  SP_LOG_INFO("Connected to SmoothPadFilter");
  return true;
#else
  if (error) *error = "Windows only";
  return false;
#endif
}

void FilterClient::Disconnect() {
  StopHeartbeatThread();
#ifdef _WIN32
  if (handle_) {
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
#endif
  connected_ = false;
  mode_ = FilterMode::PassThrough;
}

bool FilterClient::Heartbeat() {
#ifdef _WIN32
  if (!handle_) return false;
  DWORD returned = 0;
  const std::uint32_t token = 1;
  return DeviceIoControl(static_cast<HANDLE>(handle_), CtlCode(kIoctlHeartbeat),
                         const_cast<std::uint32_t*>(&token), sizeof(token), nullptr, 0, &returned, nullptr) != FALSE;
#else
  return false;
#endif
}

bool FilterClient::SetMode(FilterMode mode, std::string* error) {
#ifdef _WIN32
  if (!handle_) {
    if (error) *error = "filter not connected";
    return false;
  }
  DWORD returned = 0;
  auto m = static_cast<std::uint32_t>(mode);
  if (!DeviceIoControl(static_cast<HANDLE>(handle_), CtlCode(kIoctlSetMode), &m, sizeof(m), nullptr, 0, &returned,
                       nullptr)) {
    if (error) *error = "SetMode IOCTL failed";
    return false;
  }
  mode_ = mode;
  return true;
#else
  (void)mode;
  if (error) *error = "Windows only";
  return false;
#endif
}

bool FilterClient::QueryStatus(FilterStatus& status, std::string* error) {
#ifdef _WIN32
  if (!handle_) {
    status = {};
    if (error) *error = "filter not connected";
    return false;
  }
  DWORD returned = 0;
  if (!DeviceIoControl(static_cast<HANDLE>(handle_), CtlCode(kIoctlGetStatus), nullptr, 0, &status, sizeof(status),
                       &returned, nullptr)) {
    if (error) *error = "GetStatus IOCTL failed";
    return false;
  }
  status.connected = true;
  return true;
#else
  (void)status;
  if (error) *error = "Windows only";
  return false;
#endif
}

void FilterClient::StartHeartbeatThread() {
  StopHeartbeatThread();
  runHeartbeat_ = true;
  heartbeat_ = std::thread([this] { HeartbeatLoop(); });
}

void FilterClient::StopHeartbeatThread() {
  runHeartbeat_ = false;
  if (heartbeat_.joinable()) heartbeat_.join();
}

void FilterClient::HeartbeatLoop() {
  while (runHeartbeat_) {
    if (connected_) {
      if (!Heartbeat()) {
        SP_LOG_WARN("filter heartbeat failed; driver will fail-open");
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatMs));
  }
}

}  // namespace smoothpad::driver
