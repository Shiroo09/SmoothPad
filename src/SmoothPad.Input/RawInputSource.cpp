#include "smoothpad/input/RawInputSource.h"

#include "smoothpad/Logging.h"
#include "smoothpad/Math.h"

#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#endif

namespace smoothpad::input {
namespace {

#ifdef _WIN32
std::wstring DevicePath(HANDLE hDevice) {
  UINT size = 0;
  GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, nullptr, &size);
  if (size == 0) return {};
  std::wstring path(size, L'\0');
  if (GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, path.data(), &size) == static_cast<UINT>(-1)) {
    return {};
  }
  while (!path.empty() && path.back() == L'\0') path.pop_back();
  return path;
}

bool FillCaps(HANDLE hDevice, DeviceCaps& caps) {
  RID_DEVICE_INFO info{};
  info.cbSize = sizeof(info);
  UINT size = sizeof(info);
  if (GetRawInputDeviceInfoW(hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1)) {
    return false;
  }
  if (info.dwType != RIM_TYPEHID) return false;
  if (!IsPrecisionTouchpad(info.hid.usUsagePage, info.hid.usUsage)) return false;
  caps.identity.vendorId = static_cast<std::uint16_t>(info.hid.dwVendorId);
  caps.identity.productId = static_cast<std::uint16_t>(info.hid.dwProductId);
  caps.identity.versionNumber = static_cast<std::uint16_t>(info.hid.dwVersionNumber);
  caps.identity.usagePage = info.hid.usUsagePage;
  caps.identity.usage = info.hid.usUsage;
  caps.identity.rawHandle = hDevice;
  caps.identity.instancePath = DevicePath(hDevice);
  caps.identity.transport = GuessTransport(caps.identity.instancePath);
  caps.identity.deviceName = caps.identity.instancePath;

  UINT ppSize = 0;
  GetRawInputDeviceInfoW(hDevice, RIDI_PREPARSEDDATA, nullptr, &ppSize);
  if (ppSize > 0) {
    std::vector<std::uint8_t> preparsed(ppSize);
    if (GetRawInputDeviceInfoW(hDevice, RIDI_PREPARSEDDATA, preparsed.data(), &ppSize) != static_cast<UINT>(-1)) {
      HidReportParser parser;
      std::string err;
      if (parser.Configure(preparsed, &err)) {
        const auto parsed = parser.Caps();
        caps.x = parsed.x;
        caps.y = parsed.y;
        caps.widthMm = parsed.widthMm;
        caps.heightMm = parsed.heightMm;
        caps.physicalSizeKnown = parsed.physicalSizeKnown;
        caps.maxContacts = parsed.maxContacts;
        caps.inputReportByteLength = parsed.inputReportByteLength;
      }
    }
  }
  return true;
}
#endif

}  // namespace

bool RawInputSource::Initialize(void* hwnd, std::string* error) {
#ifdef _WIN32
  hwnd_ = static_cast<HWND>(hwnd);
  devices_ = Enumerate();
  if (devices_.empty()) {
    if (error) *error = "Windows Precision Touchpad not detected.";
    return false;
  }
  selected_ = 0;
  RAWINPUTDEVICE rid{};
  rid.usUsagePage = kDigitizerUsagePage;
  rid.usUsage = kTouchPadUsage;
  rid.dwFlags = RIDEV_INPUTSINK;
  rid.hwndTarget = hwnd_;
  if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
    if (error) *error = "RegisterRawInputDevices failed";
    return false;
  }
  registered_ = true;
  SP_LOG_INFO("Raw input registered for Precision Touchpad");
  return true;
#else
  (void)hwnd;
  if (error) *error = "Raw Input requires Windows";
  return false;
#endif
}

void RawInputSource::Shutdown() {
#ifdef _WIN32
  if (registered_ && hwnd_) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = kDigitizerUsagePage;
    rid.usUsage = kTouchPadUsage;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
  }
  registered_ = false;
  hwnd_ = nullptr;
#endif
}

std::vector<DeviceCaps> RawInputSource::Enumerate() const {
  std::vector<DeviceCaps> out;
#ifdef _WIN32
  UINT count = 0;
  GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));
  if (count == 0) return out;
  std::vector<RAWINPUTDEVICELIST> list(count);
  if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
    return out;
  }
  for (UINT i = 0; i < count; ++i) {
    if (list[i].dwType != RIM_TYPEHID) continue;
    DeviceCaps caps{};
    if (!FillCaps(list[i].hDevice, caps)) continue;
    // Prefer built-in I2C devices when multiple PTPs exist.
    out.push_back(std::move(caps));
  }
  std::stable_sort(out.begin(), out.end(), [](const DeviceCaps& a, const DeviceCaps& b) {
    auto rank = [](Transport t) {
      if (t == Transport::I2c) return 0;
      if (t == Transport::Usb) return 1;
      if (t == Transport::Bluetooth) return 2;
      return 3;
    };
    return rank(a.identity.transport) < rank(b.identity.transport);
  });
#endif
  return out;
}

bool RawInputSource::SelectDevice(std::wstring_view instanceHint) {
  std::lock_guard lock(mutex_);
  if (instanceHint.empty() || instanceHint == L"auto") {
    selected_ = devices_.empty() ? -1 : 0;
    return selected_ >= 0;
  }
  for (int i = 0; i < static_cast<int>(devices_.size()); ++i) {
    if (devices_[static_cast<std::size_t>(i)].identity.instancePath.find(instanceHint) != std::wstring::npos) {
      selected_ = i;
      return true;
    }
  }
  return false;
}

const DeviceCaps* RawInputSource::Selected() const noexcept {
  if (selected_ < 0 || selected_ >= static_cast<int>(devices_.size())) return nullptr;
  return &devices_[static_cast<std::size_t>(selected_)];
}

void RawInputSource::SetCallback(FrameCallback cb) { callback_ = std::move(cb); }

void RawInputSource::UpdateTiming(std::uint64_t qpc) {
#ifdef _WIN32
  LARGE_INTEGER freq{};
  QueryPerformanceFrequency(&freq);
  if (lastQpc_ != 0 && freq.QuadPart > 0) {
    const double ms = (static_cast<double>(qpc - lastQpc_) * 1000.0) / static_cast<double>(freq.QuadPart);
    intervals_[static_cast<std::size_t>(intervalHead_)] = ms;
    intervalHead_ = (intervalHead_ + 1) % static_cast<int>(intervals_.size());
    if (intervalCount_ < static_cast<int>(intervals_.size())) ++intervalCount_;
    std::vector<double> sorted(intervals_.begin(), intervals_.begin() + intervalCount_);
    std::sort(sorted.begin(), sorted.end());
    const double median = Percentile(sorted, 0.5);
    p95Ms_ = Percentile(sorted, 0.95);
    medianHz_ = median > 0.0 ? 1000.0 / median : 0.0;
    double mean = 0.0;
    for (int i = 0; i < intervalCount_; ++i) mean += intervals_[static_cast<std::size_t>(i)];
    mean /= intervalCount_;
    double var = 0.0;
    for (int i = 0; i < intervalCount_; ++i) {
      const double d = intervals_[static_cast<std::size_t>(i)] - mean;
      var += d * d;
    }
    jitterMs_ = std::sqrt(var / std::max(1, intervalCount_));
  }
  lastQpc_ = qpc;
#else
  (void)qpc;
#endif
}

bool RawInputSource::OnRawInput(void* lParam) {
#ifdef _WIN32
  const HRAWINPUT raw = static_cast<HRAWINPUT>(lParam);
  UINT size = 0;
  if (GetRawInputData(raw, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
    return false;
  }
  std::vector<std::uint8_t> buffer(size);
  if (GetRawInputData(raw, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
    return false;
  }
  const auto* ri = reinterpret_cast<RAWINPUT*>(buffer.data());
  if (ri->header.dwType != RIM_TYPEHID) return false;

  DeviceCaps selectedCaps{};
  {
    std::lock_guard lock(mutex_);
    const auto* sel = Selected();
    if (sel) selectedCaps = *sel;
  }
  if (selectedCaps.identity.rawHandle && ri->header.hDevice != selectedCaps.identity.rawHandle) {
    // Another HID digitizer — ignore so external devices cannot enter the pipeline.
    DeviceCaps other{};
    if (FillCaps(ri->header.hDevice, other)) {
      // Different PTP than the selected one.
      return false;
    }
    return false;
  }

  UINT ppSize = 0;
  GetRawInputDeviceInfoW(ri->header.hDevice, RIDI_PREPARSEDDATA, nullptr, &ppSize);
  if (ppSize == 0) return false;
  std::vector<std::uint8_t> preparsed(ppSize);
  if (GetRawInputDeviceInfoW(ri->header.hDevice, RIDI_PREPARSEDDATA, preparsed.data(), &ppSize) ==
      static_cast<UINT>(-1)) {
    return false;
  }
  std::string err;
  if (!parser_.Configure(preparsed, &err)) {
    return false;
  }

  const auto hidBytes = ri->data.hid.dwSizeHid * ri->data.hid.dwCount;
  const std::uint8_t* rawHid = ri->data.hid.bRawData;
  if (rawHid + hidBytes > buffer.data() + buffer.size()) {
    return false;
  }

  RawContactFrame frame{};
  DeviceCaps caps = parser_.Caps();
  caps.identity = selectedCaps.identity;
  if (!parser_.Parse({rawHid, hidBytes}, frame, &caps)) {
    return false;
  }
  LARGE_INTEGER qpc{};
  QueryPerformanceCounter(&qpc);
  frame.qpc = static_cast<std::uint64_t>(qpc.QuadPart);
  frame.device = caps.identity;
  UpdateTiming(frame.qpc);

  {
    std::lock_guard lock(mutex_);
    if (selected_ >= 0 && selected_ < static_cast<int>(devices_.size())) {
      devices_[static_cast<std::size_t>(selected_)].x = caps.x;
      devices_[static_cast<std::size_t>(selected_)].y = caps.y;
      devices_[static_cast<std::size_t>(selected_)].widthMm = caps.widthMm;
      devices_[static_cast<std::size_t>(selected_)].heightMm = caps.heightMm;
      devices_[static_cast<std::size_t>(selected_)].physicalSizeKnown = caps.physicalSizeKnown;
    }
  }

  if (callback_) callback_(frame, caps);
  return true;
#else
  (void)lParam;
  return false;
#endif
}

}  // namespace smoothpad::input
