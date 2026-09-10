#pragma once

#include "smoothpad/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace smoothpad::input {

inline constexpr std::uint16_t kUsageContactId = 0x51;
inline constexpr std::uint16_t kUsageTipSwitch = 0x42;
inline constexpr std::uint16_t kUsageConfidence = 0x47;
inline constexpr std::uint16_t kUsageContactCount = 0x54;
inline constexpr std::uint16_t kUsageScanTime = 0x56;
inline constexpr std::uint16_t kUsageX = 0x30;
inline constexpr std::uint16_t kUsageY = 0x31;
inline constexpr std::uint16_t kUsageGenericDesktop = 0x01;
inline constexpr std::uint16_t kUsageButtonPage = 0x09;

struct HidValueCap {
  std::uint16_t usagePage = 0;
  std::uint16_t usage = 0;
  std::uint16_t linkCollection = 0;
  std::uint16_t bitSize = 0;
  int logicalMin = 0;
  int logicalMax = 0;
  int physicalMin = 0;
  int physicalMax = 0;
  std::uint32_t units = 0;
  std::int32_t unitsExp = 0;
  bool isRange = false;
  std::uint16_t usageMin = 0;
  std::uint16_t usageMax = 0;
};

struct HidButtonCap {
  std::uint16_t usagePage = 0;
  std::uint16_t usage = 0;
  std::uint16_t linkCollection = 0;
};

class HidReportParser {
 public:
  bool Configure(std::span<const std::uint8_t> preparsed, std::string* error);
  bool Parse(std::span<const std::uint8_t> report, RawContactFrame& frame, DeviceCaps* capsUpdate);

  [[nodiscard]] const DeviceCaps& Caps() const noexcept { return caps_; }
  [[nodiscard]] bool Ready() const noexcept { return ready_; }

  // Exposed for tests: inject caps without a real HID preparsed blob.
  void SetSyntheticCaps(const DeviceCaps& caps, const std::vector<HidValueCap>& values,
                        const std::vector<HidButtonCap>& buttons);

 private:
  bool ready_ = false;
  DeviceCaps caps_{};
  std::vector<HidValueCap> values_;
  std::vector<HidButtonCap> buttons_;
  std::vector<std::uint8_t> preparsed_;
  void* preparsedPtr_ = nullptr;
};

[[nodiscard]] Transport GuessTransport(std::wstring_view path);
[[nodiscard]] bool IsPrecisionTouchpad(std::uint16_t usagePage, std::uint16_t usage) noexcept;

}  // namespace smoothpad::input
