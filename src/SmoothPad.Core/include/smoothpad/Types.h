#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace smoothpad {

inline constexpr std::uint16_t kDigitizerUsagePage = 0x0D;
inline constexpr std::uint16_t kTouchPadUsage = 0x05;
inline constexpr std::uint16_t kMouseUsagePage = 0x01;
inline constexpr std::uint16_t kMouseUsage = 0x02;
inline constexpr int kMaxContacts = 5;
inline constexpr int kMaxContactSlots = 10;

enum class Transport {
  Unknown,
  I2c,
  Usb,
  Bluetooth,
};

enum class EngineMode {
  Unsupported,
  Native,
  Starting,
  MacMode,
  Bypass,
  Degraded,
  DriverDisconnected,
  Error,
};

enum class InputBackend {
  None,
  RawInput,
  Filter,
};

enum class OutputBackend {
  None,
  SyntheticTouchpad,
  DiagnosticMouse,
};

enum class ProfileId {
  MacBalanced,
  MacSmooth,
  MacFast,
  WindowsNative,
  Custom,
};

enum class SmootherKind {
  OneEuro,
  AdaptiveEma,
  VelocityEma,
  CriticallyDamped,
};

enum class ExternalMousePolicy {
  DoNothing,
  KeepSmoothPad,
  UseNativeTouchpad,
  DisableTouchpad,
};

enum class ContactState {
  Idle,
  ContactBegin,
  SinglePointer,
  MultiTouchPending,
  MultiTouch,
  PhysicalClick,
  Drag,
  ContactEnd,
};

struct DeviceIdentity {
  std::wstring instancePath;
  std::wstring deviceName;
  std::uint16_t vendorId = 0;
  std::uint16_t productId = 0;
  std::uint16_t versionNumber = 0;
  std::uint16_t usagePage = 0;
  std::uint16_t usage = 0;
  Transport transport = Transport::Unknown;
  void* rawHandle = nullptr;
};

struct AxisCaps {
  int logicalMin = 0;
  int logicalMax = 0;
  int physicalMin = 0;
  int physicalMax = 0;
  std::uint32_t units = 0;
  std::int32_t unitsExp = 0;
  bool hasPhysical = false;
};

struct DeviceCaps {
  DeviceIdentity identity;
  AxisCaps x;
  AxisCaps y;
  int maxContacts = 5;
  double widthMm = 0.0;
  double heightMm = 0.0;
  bool physicalSizeKnown = false;
  std::uint16_t inputReportByteLength = 0;
};

struct Contact {
  std::uint32_t id = 0;
  bool tip = false;
  bool confidence = true;
  bool valid = false;
  int xLogical = 0;
  int yLogical = 0;
  std::optional<double> pressure;
  std::optional<double> width;
  std::optional<double> height;
};

struct RawContactFrame {
  std::uint64_t qpc = 0;
  std::uint64_t unixNs = 0;
  std::uint32_t scanTime = 0;
  std::uint32_t contactCount = 0;
  bool button = false;
  bool buttonValid = false;
  DeviceIdentity device;
  std::array<Contact, kMaxContacts> contacts{};
};

struct Vec2 {
  double x = 0.0;
  double y = 0.0;

  [[nodiscard]] double length() const noexcept { return std::hypot(x, y); }
  [[nodiscard]] Vec2 normalized() const noexcept {
    const double len = length();
    if (len <= 1e-12) {
      return {};
    }
    return {x / len, y / len};
  }
};

inline Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double s) noexcept { return {a.x * s, a.y * s}; }
inline Vec2 operator*(double s, Vec2 a) noexcept { return a * s; }

struct MotionSample {
  double tSeconds = 0.0;
  double dt = 0.0;
  std::uint32_t contactCount = 0;
  std::uint32_t primaryId = 0;
  bool tip = false;
  bool button = false;
  bool newContact = false;
  bool contactEnd = false;
  Vec2 mm{};
  Vec2 normalized{};
  Vec2 logical{};
};

struct MotionOutput {
  Vec2 deltaMm{};
  Vec2 deltaPixels{};
  double speedMmS = 0.0;
  double gain = 1.0;
  bool emit = false;
  bool pointerActive = false;
  ContactState state = ContactState::Idle;
  std::uint64_t processingBeginQpc = 0;
  std::uint64_t processingEndQpc = 0;
};

struct LatencySample {
  double receiveToProcessMs = 0.0;
  double processMs = 0.0;
  double processToInjectMs = 0.0;
};

inline constexpr const char* ProfileIdToString(ProfileId id) noexcept {
  switch (id) {
    case ProfileId::MacBalanced:
      return "mac-balanced";
    case ProfileId::MacSmooth:
      return "mac-smooth";
    case ProfileId::MacFast:
      return "mac-fast";
    case ProfileId::WindowsNative:
      return "windows-native";
    case ProfileId::Custom:
      return "custom";
  }
  return "mac-balanced";
}

// HID UnitsExponent is a 4-bit two's-complement nibble. HidP may leave it as
// 0..15 or as a sign-extended integer; both collapse correctly after masking.
[[nodiscard]] inline int HidExponentNibble(std::int32_t unitsExp) noexcept {
  const unsigned n = static_cast<unsigned>(unitsExp) & 0x0Fu;
  return (n & 0x8u) ? static_cast<int>(n) - 16 : static_cast<int>(n);
}

// Convert a HID physical extent to millimetres. Length nibble 1 = centimetre
// (typical PTP descriptor: Units 0x11, UnitsExp -2).
[[nodiscard]] inline double HidPhysicalToMm(int physicalSpan, std::uint32_t units, std::int32_t unitsExp) noexcept {
  const int exp = HidExponentNibble(unitsExp);
  double mm = std::abs(static_cast<double>(physicalSpan)) * std::pow(10.0, exp);
  if (((units >> 4) & 0xFu) == 1u) {
    mm *= 10.0;
  }
  return mm;
}

[[nodiscard]] inline bool PlausibleTouchpadMm(double widthMm, double heightMm) noexcept {
  return widthMm >= 20.0 && widthMm <= 250.0 && heightMm >= 20.0 && heightMm <= 180.0;
}

inline ProfileId ProfileIdFromString(std::string_view s) noexcept {
  if (s == "mac-smooth") return ProfileId::MacSmooth;
  if (s == "mac-fast") return ProfileId::MacFast;
  if (s == "windows-native") return ProfileId::WindowsNative;
  if (s == "custom") return ProfileId::Custom;
  return ProfileId::MacBalanced;
}

inline const wchar_t* EngineModeName(EngineMode m) noexcept {
  switch (m) {
    case EngineMode::Unsupported:
      return L"Unsupported";
    case EngineMode::Native:
      return L"Windows Native";
    case EngineMode::Starting:
      return L"Starting";
    case EngineMode::MacMode:
      return L"Mac Mode";
    case EngineMode::Bypass:
      return L"Bypass";
    case EngineMode::Degraded:
      return L"Degraded";
    case EngineMode::DriverDisconnected:
      return L"Driver disconnected";
    case EngineMode::Error:
      return L"Error";
  }
  return L"Unknown";
}

}  // namespace smoothpad
