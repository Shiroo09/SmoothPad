#include "smoothpad/Trace.h"

#include "smoothpad/Json.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace smoothpad {
namespace {

RawContactFrame ToRaw(const TraceFrame& f, const DeviceIdentity& device) {
  RawContactFrame out;
  out.qpc = f.qpc;
  out.scanTime = f.scanTime;
  out.contactCount = f.contactCount;
  out.button = f.button != 0;
  out.buttonValid = true;
  out.device = device;
  const auto n = std::min<std::uint32_t>(f.contactCount, kMaxContacts);
  for (std::uint32_t i = 0; i < n; ++i) {
    out.contacts[i].id = f.contacts[i].id;
    out.contacts[i].xLogical = f.contacts[i].x;
    out.contacts[i].yLogical = f.contacts[i].y;
    out.contacts[i].tip = f.contacts[i].tip != 0;
    out.contacts[i].confidence = f.contacts[i].confidence != 0;
    out.contacts[i].valid = true;
  }
  return out;
}

}  // namespace

bool TraceWriter::Open(const std::filesystem::path& path, const DeviceCaps& caps) {
  path_ = path;
  frames_.clear();
  count_ = 0;
  header_ = {};
  std::memcpy(header_.magic, kTraceMagic, sizeof(header_.magic));
  header_.version = kTraceVersion;
  header_.vendorId = caps.identity.vendorId;
  header_.productId = caps.identity.productId;
  header_.widthMm = caps.widthMm;
  header_.heightMm = caps.heightMm;
  header_.xLogicalMin = caps.x.logicalMin;
  header_.xLogicalMax = caps.x.logicalMax;
  header_.yLogicalMin = caps.y.logicalMin;
  header_.yLogicalMax = caps.y.logicalMax;
  return true;
}

bool TraceWriter::Write(const RawContactFrame& frame) {
  TraceFrame f{};
  f.qpc = frame.qpc;
  f.scanTime = frame.scanTime;
  f.contactCount = frame.contactCount;
  f.button = frame.button ? 1 : 0;
  const auto n = std::min<std::uint32_t>(frame.contactCount, kMaxContacts);
  for (std::uint32_t i = 0; i < n; ++i) {
    f.contacts[i].id = frame.contacts[i].id;
    f.contacts[i].x = frame.contacts[i].xLogical;
    f.contacts[i].y = frame.contacts[i].yLogical;
    f.contacts[i].tip = frame.contacts[i].tip ? 1 : 0;
    f.contacts[i].confidence = frame.contacts[i].confidence ? 1 : 0;
  }
  frames_.push_back(f);
  ++count_;
  return true;
}

bool TraceWriter::Close() {
  header_.frameCount = count_;
  std::ofstream out(path_, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
  if (!frames_.empty()) {
    out.write(reinterpret_cast<const char*>(frames_.data()),
              static_cast<std::streamsize>(frames_.size() * sizeof(TraceFrame)));
  }
  return static_cast<bool>(out);
}

bool TraceReader::Open(const std::filesystem::path& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "failed to open trace";
    return false;
  }
  in.read(reinterpret_cast<char*>(&header_), sizeof(header_));
  if (!in) {
    if (error) *error = "truncated trace header";
    return false;
  }
  if (std::memcmp(header_.magic, kTraceMagic, sizeof(header_.magic)) != 0) {
    if (error) *error = "invalid trace magic";
    return false;
  }
  if (header_.version != kTraceVersion) {
    if (error) *error = "unsupported trace version";
    return false;
  }
  frames_.resize(static_cast<std::size_t>(header_.frameCount));
  if (header_.frameCount > 0) {
    in.read(reinterpret_cast<char*>(frames_.data()),
            static_cast<std::streamsize>(frames_.size() * sizeof(TraceFrame)));
    if (!in) {
      if (error) *error = "truncated trace frames";
      return false;
    }
  }
  return true;
}

std::vector<RawContactFrame> TraceReader::ToRawFrames(const DeviceIdentity& device) const {
  std::vector<RawContactFrame> out;
  out.reserve(frames_.size());
  for (const auto& f : frames_) {
    out.push_back(ToRaw(f, device));
  }
  return out;
}

std::string TraceToJson(const TraceReader& reader) {
  json::Object root;
  json::Object header;
  header["version"] = static_cast<double>(reader.Header().version);
  header["frameCount"] = static_cast<double>(reader.Header().frameCount);
  header["vendorId"] = static_cast<double>(reader.Header().vendorId);
  header["productId"] = static_cast<double>(reader.Header().productId);
  header["widthMm"] = reader.Header().widthMm;
  header["heightMm"] = reader.Header().heightMm;
  root["header"] = json::Value{std::move(header)};
  json::Array frames;
  for (const auto& f : reader.Frames()) {
    json::Object fo;
    fo["qpc"] = static_cast<double>(f.qpc);
    fo["scanTime"] = static_cast<double>(f.scanTime);
    fo["contactCount"] = static_cast<double>(f.contactCount);
    fo["button"] = f.button != 0;
    json::Array contacts;
    const auto n = std::min<std::uint32_t>(f.contactCount, kMaxContacts);
    for (std::uint32_t i = 0; i < n; ++i) {
      json::Object c;
      c["id"] = static_cast<double>(f.contacts[i].id);
      c["x"] = static_cast<double>(f.contacts[i].x);
      c["y"] = static_cast<double>(f.contacts[i].y);
      c["tip"] = f.contacts[i].tip != 0;
      c["confidence"] = f.contacts[i].confidence != 0;
      contacts.push_back(json::Value{std::move(c)});
    }
    fo["contacts"] = json::Value{std::move(contacts)};
    frames.push_back(json::Value{std::move(fo)});
  }
  root["frames"] = json::Value{std::move(frames)};
  return json::Stringify(json::Value{std::move(root)}, true);
}

std::string TraceToCsv(const TraceReader& reader) {
  std::ostringstream os;
  os << "qpc,scanTime,contactCount,button,id,x,y,tip,confidence\n";
  for (const auto& f : reader.Frames()) {
    const auto n = std::min<std::uint32_t>(f.contactCount, kMaxContacts);
    if (n == 0) {
      os << f.qpc << ',' << f.scanTime << ',' << f.contactCount << ',' << static_cast<int>(f.button)
         << ",,,,,\n";
      continue;
    }
    for (std::uint32_t i = 0; i < n; ++i) {
      os << f.qpc << ',' << f.scanTime << ',' << f.contactCount << ',' << static_cast<int>(f.button)
         << ',' << f.contacts[i].id << ',' << f.contacts[i].x << ',' << f.contacts[i].y << ','
         << static_cast<int>(f.contacts[i].tip) << ',' << static_cast<int>(f.contacts[i].confidence)
         << '\n';
    }
  }
  return os.str();
}

}  // namespace smoothpad
