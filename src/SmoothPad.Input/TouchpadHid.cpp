#include "smoothpad/input/TouchpadHid.h"

#include "smoothpad/Math.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#pragma comment(lib, "hid.lib")
#endif

#include <algorithm>
#include <cwctype>

namespace smoothpad::input {

bool IsPrecisionTouchpad(std::uint16_t usagePage, std::uint16_t usage) noexcept {
  return usagePage == kDigitizerUsagePage && usage == kTouchPadUsage;
}

Transport GuessTransport(std::wstring_view path) {
  std::wstring lower(path);
  for (auto& c : lower) c = static_cast<wchar_t>(std::towlower(c));
  if (lower.find(L"bth") != std::wstring::npos || lower.find(L"bluetooth") != std::wstring::npos) {
    return Transport::Bluetooth;
  }
  if (lower.find(L"usb") != std::wstring::npos) {
    return Transport::Usb;
  }
  if (lower.find(L"i2c") != std::wstring::npos || lower.find(L"spi") != std::wstring::npos ||
      lower.find(L"acpi") != std::wstring::npos) {
    return Transport::I2c;
  }
  // Built-in laptop pads often appear as HID#<vendor> without a bus token.
  if (lower.find(L"hid#") != std::wstring::npos && lower.find(L"col") != std::wstring::npos) {
    return Transport::I2c;
  }
  return Transport::Unknown;
}

void HidReportParser::SetSyntheticCaps(const DeviceCaps& caps, const std::vector<HidValueCap>& values,
                                       const std::vector<HidButtonCap>& buttons) {
  caps_ = caps;
  values_ = values;
  buttons_ = buttons;
  ready_ = true;
}

#ifdef _WIN32
bool HidReportParser::Configure(std::span<const std::uint8_t> preparsed, std::string* error) {
  if (preparsed.empty()) {
    if (error) *error = "empty preparsed data";
    return false;
  }
  preparsed_.assign(preparsed.begin(), preparsed.end());
  preparsedPtr_ = preparsed_.data();
  PHIDP_PREPARSED_DATA pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedPtr_);
  HIDP_CAPS caps{};
  if (HidP_GetCaps(pp, &caps) != HIDP_STATUS_SUCCESS) {
    if (error) *error = "HidP_GetCaps failed";
    return false;
  }
  caps_.identity.usagePage = caps.UsagePage;
  caps_.identity.usage = caps.Usage;
  caps_.inputReportByteLength = caps.InputReportByteLength;
  caps_.maxContacts = 5;

  values_.clear();
  buttons_.clear();

  if (caps.NumberInputValueCaps > 0) {
    std::vector<HIDP_VALUE_CAPS> vc(caps.NumberInputValueCaps);
    USHORT n = caps.NumberInputValueCaps;
    if (HidP_GetValueCaps(HidP_Input, vc.data(), &n, pp) == HIDP_STATUS_SUCCESS) {
      vc.resize(n);
      for (const auto& c : vc) {
        HidValueCap o;
        o.usagePage = c.UsagePage;
        o.linkCollection = c.LinkCollection;
        o.bitSize = c.BitSize;
        o.logicalMin = c.LogicalMin;
        o.logicalMax = c.LogicalMax;
        o.physicalMin = c.PhysicalMin;
        o.physicalMax = c.PhysicalMax;
        o.units = c.Units;
        o.unitsExp = static_cast<std::int32_t>(c.UnitsExp);
        o.isRange = c.IsRange ? true : false;
        if (c.IsRange) {
          o.usageMin = c.Range.UsageMin;
          o.usageMax = c.Range.UsageMax;
          o.usage = c.Range.UsageMin;
        } else {
          o.usage = c.NotRange.Usage;
          o.usageMin = o.usage;
          o.usageMax = o.usage;
        }
        values_.push_back(o);
        if (c.LinkCollection != 0 && o.usagePage == kUsageGenericDesktop && o.usage == kUsageX) {
          caps_.x.logicalMin = o.logicalMin;
          caps_.x.logicalMax = o.logicalMax;
          caps_.x.physicalMin = o.physicalMin;
          caps_.x.physicalMax = o.physicalMax;
          caps_.x.units = o.units;
          caps_.x.unitsExp = o.unitsExp;
          caps_.x.hasPhysical = o.physicalMin != o.physicalMax;
        }
        if (c.LinkCollection != 0 && o.usagePage == kUsageGenericDesktop && o.usage == kUsageY) {
          caps_.y.logicalMin = o.logicalMin;
          caps_.y.logicalMax = o.logicalMax;
          caps_.y.physicalMin = o.physicalMin;
          caps_.y.physicalMax = o.physicalMax;
          caps_.y.units = o.units;
          caps_.y.unitsExp = o.unitsExp;
          caps_.y.hasPhysical = o.physicalMin != o.physicalMax;
        }
      }
    }
  }

  if (caps.NumberInputButtonCaps > 0) {
    std::vector<HIDP_BUTTON_CAPS> bc(caps.NumberInputButtonCaps);
    USHORT n = caps.NumberInputButtonCaps;
    if (HidP_GetButtonCaps(HidP_Input, bc.data(), &n, pp) == HIDP_STATUS_SUCCESS) {
      bc.resize(n);
      for (const auto& c : bc) {
        HidButtonCap o;
        o.usagePage = c.UsagePage;
        o.linkCollection = c.LinkCollection;
        o.usage = c.IsRange ? c.Range.UsageMin : c.NotRange.Usage;
        buttons_.push_back(o);
      }
    }
  }

  if (caps_.x.hasPhysical && caps_.y.hasPhysical) {
    const double xmm = HidPhysicalToMm(caps_.x.physicalMax - caps_.x.physicalMin, caps_.x.units, caps_.x.unitsExp);
    const double ymm = HidPhysicalToMm(caps_.y.physicalMax - caps_.y.physicalMin, caps_.y.units, caps_.y.unitsExp);
    if (PlausibleTouchpadMm(xmm, ymm)) {
      caps_.widthMm = xmm;
      caps_.heightMm = ymm;
      caps_.physicalSizeKnown = true;
    }
  }

  ready_ = IsPrecisionTouchpad(caps_.identity.usagePage, caps_.identity.usage) || !values_.empty();
  if (!ready_ && error) *error = "not a precision touchpad collection";
  return ready_;
}

bool HidReportParser::Parse(std::span<const std::uint8_t> report, RawContactFrame& frame, DeviceCaps* capsUpdate) {
  frame = {};
  if (!ready_ || report.empty()) return false;
  PHIDP_PREPARSED_DATA pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedPtr_);
  if (!pp) return false;

  PCHAR reportPtr = reinterpret_cast<PCHAR>(const_cast<std::uint8_t*>(report.data()));
  const ULONG reportLen = static_cast<ULONG>(report.size());

  ULONG contactCount = 0;
  ULONG scanTime = 0;
  std::array<Contact, kMaxContacts> contacts{};
  std::array<bool, kMaxContacts> haveId{};
  std::array<bool, kMaxContacts> haveX{};
  std::array<bool, kMaxContacts> haveY{};

  auto slotForCollection = [&](std::uint16_t link) -> int {
    if (link == 0) return -1;
    return static_cast<int>((link - 1) % kMaxContacts);
  };

  for (const auto& cap : values_) {
    ULONG value = 0;
    if (HidP_GetUsageValue(HidP_Input, cap.usagePage, cap.linkCollection, cap.usage, &value, pp, reportPtr,
                           reportLen) != HIDP_STATUS_SUCCESS) {
      continue;
    }
    if (cap.linkCollection == 0) {
      if (cap.usagePage == kDigitizerUsagePage && cap.usage == kUsageScanTime) scanTime = value;
      if (cap.usagePage == kDigitizerUsagePage && cap.usage == kUsageContactCount) contactCount = value;
      continue;
    }
    const int slot = slotForCollection(cap.linkCollection);
    if (slot < 0) continue;
    auto& c = contacts[static_cast<std::size_t>(slot)];
    c.valid = true;
    if (cap.usagePage == kDigitizerUsagePage && cap.usage == kUsageContactId) {
      c.id = value;
      haveId[static_cast<std::size_t>(slot)] = true;
    } else if (cap.usagePage == kUsageGenericDesktop && cap.usage == kUsageX) {
      c.xLogical = static_cast<int>(value);
      haveX[static_cast<std::size_t>(slot)] = true;
    } else if (cap.usagePage == kUsageGenericDesktop && cap.usage == kUsageY) {
      c.yLogical = static_cast<int>(value);
      haveY[static_cast<std::size_t>(slot)] = true;
    }
  }

  for (const auto& b : buttons_) {
    USAGE usage = b.usage;
    ULONG len = 1;
    USAGE usages[8]{};
    const NTSTATUS st =
        HidP_GetUsages(HidP_Input, b.usagePage, b.linkCollection, usages, &len, pp, reportPtr, reportLen);
    if (st != HIDP_STATUS_SUCCESS && st != HIDP_STATUS_INCOMPATIBLE_REPORT_ID) {
      continue;
    }
    bool pressed = false;
    for (ULONG i = 0; i < len; ++i) {
      if (usages[i] == usage || (b.usagePage == kDigitizerUsagePage && usages[i] == kUsageTipSwitch)) {
        pressed = true;
      }
    }
    if (b.linkCollection == 0 && b.usagePage == kUsageButtonPage) {
      frame.button = pressed || len > 0;
      frame.buttonValid = true;
    }
    if (b.usagePage == kDigitizerUsagePage && b.usage == kUsageTipSwitch) {
      const int slot = slotForCollection(b.linkCollection);
      if (slot >= 0) contacts[static_cast<std::size_t>(slot)].tip = pressed || len > 0;
    }
    if (b.usagePage == kDigitizerUsagePage && b.usage == kUsageConfidence) {
      const int slot = slotForCollection(b.linkCollection);
      if (slot >= 0) contacts[static_cast<std::size_t>(slot)].confidence = pressed || len > 0;
    }
  }

  // Tip switch may be a value instead of a button on some devices.
  for (const auto& cap : values_) {
    if (cap.usagePage != kDigitizerUsagePage || cap.usage != kUsageTipSwitch) continue;
    ULONG value = 0;
    if (HidP_GetUsageValue(HidP_Input, cap.usagePage, cap.linkCollection, cap.usage, &value, pp, reportPtr,
                           reportLen) == HIDP_STATUS_SUCCESS) {
      const int slot = slotForCollection(cap.linkCollection);
      if (slot >= 0) contacts[static_cast<std::size_t>(slot)].tip = value != 0;
    }
  }

  frame.scanTime = scanTime;
  frame.contactCount = static_cast<std::uint32_t>(std::min<ULONG>(contactCount, kMaxContacts));
  int filled = 0;
  for (int i = 0; i < kMaxContacts; ++i) {
    if (!contacts[static_cast<std::size_t>(i)].valid) continue;
    if (!haveX[static_cast<std::size_t>(i)] || !haveY[static_cast<std::size_t>(i)]) continue;
    if (filled < kMaxContacts) {
      if (!haveId[static_cast<std::size_t>(i)]) {
        contacts[static_cast<std::size_t>(i)].id = static_cast<std::uint32_t>(filled + 1);
      }
      frame.contacts[static_cast<std::size_t>(filled++)] = contacts[static_cast<std::size_t>(i)];
    }
  }
  if (frame.contactCount == 0) {
    frame.contactCount = static_cast<std::uint32_t>(filled);
  }
  if (capsUpdate) *capsUpdate = caps_;
  return true;
}
#else
bool HidReportParser::Configure(std::span<const std::uint8_t>, std::string* error) {
  if (error) *error = "HID parsing requires Windows";
  return false;
}
bool HidReportParser::Parse(std::span<const std::uint8_t>, RawContactFrame&, DeviceCaps*) { return false; }
#endif

}  // namespace smoothpad::input
