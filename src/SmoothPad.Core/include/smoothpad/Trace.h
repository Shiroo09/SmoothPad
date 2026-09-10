#pragma once

#include "Types.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace smoothpad {

inline constexpr char kTraceMagic[8] = {'S', 'P', 'T', 'R', 'A', 'C', 'E', '1'};
inline constexpr std::uint32_t kTraceVersion = 1;

struct TraceHeader {
  char magic[8]{};
  std::uint32_t version = kTraceVersion;
  std::uint32_t reserved = 0;
  std::uint64_t frameCount = 0;
  std::uint16_t vendorId = 0;
  std::uint16_t productId = 0;
  double widthMm = 0.0;
  double heightMm = 0.0;
  std::int32_t xLogicalMin = 0;
  std::int32_t xLogicalMax = 0;
  std::int32_t yLogicalMin = 0;
  std::int32_t yLogicalMax = 0;
};

struct TraceContact {
  std::uint32_t id = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::uint8_t tip = 0;
  std::uint8_t confidence = 1;
  std::uint8_t pad[2]{};
};

struct TraceFrame {
  std::uint64_t qpc = 0;
  std::uint32_t scanTime = 0;
  std::uint32_t contactCount = 0;
  std::uint8_t button = 0;
  std::uint8_t pad[7]{};
  std::array<TraceContact, kMaxContacts> contacts{};
};

class TraceWriter {
 public:
  bool Open(const std::filesystem::path& path, const DeviceCaps& caps);
  bool Write(const RawContactFrame& frame);
  bool Close();
  [[nodiscard]] std::uint64_t FrameCount() const noexcept { return count_; }

 private:
  std::filesystem::path path_;
  std::vector<TraceFrame> frames_;
  TraceHeader header_{};
  std::uint64_t count_ = 0;
};

class TraceReader {
 public:
  bool Open(const std::filesystem::path& path, std::string* error);
  [[nodiscard]] const TraceHeader& Header() const noexcept { return header_; }
  [[nodiscard]] const std::vector<TraceFrame>& Frames() const noexcept { return frames_; }
  [[nodiscard]] std::vector<RawContactFrame> ToRawFrames(const DeviceIdentity& device = {}) const;

 private:
  TraceHeader header_{};
  std::vector<TraceFrame> frames_;
};

[[nodiscard]] std::string TraceToJson(const TraceReader& reader);
[[nodiscard]] std::string TraceToCsv(const TraceReader& reader);

}  // namespace smoothpad
