#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace smoothpad::platform {

class ScopedHandle {
 public:
  ScopedHandle() = default;
#ifdef _WIN32
  explicit ScopedHandle(HANDLE h) : h_(h) {}
  ~ScopedHandle() { reset(); }
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  ScopedHandle(ScopedHandle&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
  ScopedHandle& operator=(ScopedHandle&& o) noexcept {
    if (this != &o) {
      reset();
      h_ = o.h_;
      o.h_ = nullptr;
    }
    return *this;
  }
  [[nodiscard]] HANDLE get() const noexcept { return h_; }
  [[nodiscard]] explicit operator bool() const noexcept { return h_ && h_ != INVALID_HANDLE_VALUE; }
  HANDLE release() noexcept {
    HANDLE t = h_;
    h_ = nullptr;
    return t;
  }
  void reset(HANDLE h = nullptr) {
    if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
    h_ = h;
  }

 private:
  HANDLE h_ = nullptr;
#else
  ~ScopedHandle() = default;
#endif
};

[[nodiscard]] std::uint64_t QpcNow();
[[nodiscard]] double QpcToSeconds(std::uint64_t qpc);
[[nodiscard]] double QpcToMilliseconds(std::uint64_t delta);

bool QueryTouchpadParameters(bool* present, bool* enabled, bool* externalMouse, std::string* error);
bool SetTouchpadEnabled(bool enabled, std::string* error);

bool SetStartWithWindows(bool enable, std::string* error);
[[nodiscard]] bool IsStartWithWindows();

bool RegisterToggleHotkey(void* hwnd, unsigned modifiers, unsigned vk, int id);
void UnregisterHotkey(void* hwnd, int id);

[[nodiscard]] std::wstring Utf8ToWide(std::string_view s);
[[nodiscard]] std::string WideToUtf8(std::wstring_view s);

}  // namespace smoothpad::platform
