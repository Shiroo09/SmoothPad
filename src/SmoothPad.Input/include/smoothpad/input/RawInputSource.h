#pragma once

#include "smoothpad/Types.h"
#include "TouchpadHid.h"

#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace smoothpad::input {

class RawInputSource {
 public:
  using FrameCallback = std::function<void(const RawContactFrame&, const DeviceCaps&)>;

  bool Initialize(void* hwnd, std::string* error);
  void Shutdown();

  [[nodiscard]] std::vector<DeviceCaps> Enumerate() const;
  bool SelectDevice(std::wstring_view instanceHint);
  [[nodiscard]] const DeviceCaps* Selected() const noexcept;

  // WM_INPUT handler. Returns true if consumed.
  bool OnRawInput(void* lParam);

  void SetCallback(FrameCallback cb);

  [[nodiscard]] double MedianHz() const noexcept { return medianHz_; }
  [[nodiscard]] double P95IntervalMs() const noexcept { return p95Ms_; }
  [[nodiscard]] double JitterMs() const noexcept { return jitterMs_; }

 private:
  void UpdateTiming(std::uint64_t qpc);

#ifdef _WIN32
  HWND hwnd_ = nullptr;
#endif
  FrameCallback callback_;
  std::vector<DeviceCaps> devices_;
  int selected_ = -1;
  mutable std::mutex mutex_;
  std::array<double, 64> intervals_{};
  int intervalCount_ = 0;
  int intervalHead_ = 0;
  std::uint64_t lastQpc_ = 0;
  double medianHz_ = 0.0;
  double p95Ms_ = 0.0;
  double jitterMs_ = 0.0;
  HidReportParser parser_;
  bool registered_ = false;
};

}  // namespace smoothpad::input
