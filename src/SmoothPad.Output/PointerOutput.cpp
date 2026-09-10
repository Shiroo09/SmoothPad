#include "smoothpad/output/PointerOutput.h"

#include "smoothpad/Logging.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <cmath>

namespace smoothpad::output {
namespace {

#ifdef _WIN32
using CreateSynthetic2Fn = HSYNTHETICPOINTERDEVICE(WINAPI*)(const void*);

struct SyntheticCreationParams {
  POINTER_INPUT_TYPE pointerType;
  ULONG maxCount;
  POINTER_FEEDBACK_MODE feedbackMode;
  HMONITOR hMonitor;
  ULONG deviceWidth;
  ULONG deviceHeight;
  ULONG options;
};

constexpr ULONG kSdcoPhysicalSize = 0x1;
constexpr ULONG kSdcoTouchpadGestureOnly = 0x2;

HMODULE User32() {
  return GetModuleHandleW(L"user32.dll");
}

CreateSynthetic2Fn LoadCreate2() {
  const auto mod = User32();
  if (!mod) return nullptr;
  auto fn = reinterpret_cast<CreateSynthetic2Fn>(GetProcAddress(mod, "CreateSyntheticPointerDevice2"));
  if (!fn) {
    // Current Learn docs list User32 ordinal 2690 for this export.
    fn = reinterpret_cast<CreateSynthetic2Fn>(GetProcAddress(mod, MAKEINTRESOURCEA(2690)));
  }
  return fn;
}

ULONG HimetricFromMm(double mm, double fallback) {
  if (!(mm >= 20.0 && mm <= 250.0)) {
    mm = fallback;
  }
  return static_cast<ULONG>(std::lround(mm * 100.0));  // 1 himetric = 0.01 mm
}

ULONG HimetricWidth(const DeviceCaps& caps) { return HimetricFromMm(caps.widthMm, 105.0); }
ULONG HimetricHeight(const DeviceCaps& caps) { return HimetricFromMm(caps.heightMm, 70.0); }
#endif

}  // namespace

bool DiagnosticMouseOutput::Initialize(const DeviceCaps&, std::string*) {
  ready_ = true;
  SP_LOG_WARN("DiagnosticMouseOutput is for development only; it is not the production output path.");
  return true;
}

void DiagnosticMouseOutput::Shutdown() { ready_ = false; }

bool DiagnosticMouseOutput::Inject(const OutputFrame& frame, std::string* error) {
#ifdef _WIN32
  if (!ready_) {
    if (error) *error = "diagnostic output not initialized";
    return false;
  }
  if (!frame.motion.emit) return true;
  INPUT in{};
  in.type = INPUT_MOUSE;
  in.mi.dwFlags = MOUSEEVENTF_MOVE;
  in.mi.dx = static_cast<LONG>(frame.motion.deltaPixels.x);
  in.mi.dy = static_cast<LONG>(frame.motion.deltaPixels.y);
  if (SendInput(1, &in, sizeof(INPUT)) != 1) {
    if (error) *error = "SendInput failed";
    return false;
  }
  return true;
#else
  (void)frame;
  if (error) *error = "SendInput requires Windows";
  return false;
#endif
}

bool SyntheticTouchpadOutput::Initialize(const DeviceCaps& caps, std::string* error) {
#ifdef _WIN32
  caps_ = caps;
  Shutdown();
  const auto create2 = LoadCreate2();
  if (create2) {
    SyntheticCreationParams p{};
    p.pointerType = static_cast<POINTER_INPUT_TYPE>(PT_TOUCHPAD);
    p.maxCount = 5;
    p.feedbackMode = POINTER_FEEDBACK_NONE;
    p.hMonitor = nullptr;
    p.deviceWidth = HimetricWidth(caps);
    p.deviceHeight = HimetricHeight(caps);
    p.options = kSdcoPhysicalSize;
    device_ = create2(&p);
    if (device_) {
      usesCreate2_ = true;
      available_ = true;
      SP_LOG_INFO("CreateSyntheticPointerDevice2(PT_TOUCHPAD) succeeded");
      return true;
    }
    SP_LOG_WARN("CreateSyntheticPointerDevice2 returned null; falling back to runtime probe");
  } else {
    SP_LOG_WARN("CreateSyntheticPointerDevice2 is not exported by this OS/SDK");
  }
  // Documented fallback: PT_TOUCH exists on this SDK. Production Mac Mode still
  // requires PT_TOUCHPAD; this path is used only so the output module can compile
  // and report availability honestly.
  if (error) {
    *error =
        "Synthetic PT_TOUCHPAD requires CreateSyntheticPointerDevice2 (Windows 11 current SDK). "
        "This build of the OS/SDK does not expose the API; Mac Mode output is unavailable.";
  }
  available_ = false;
  return false;
#else
  (void)caps;
  if (error) *error = "synthetic pointer requires Windows";
  return false;
#endif
}

void SyntheticTouchpadOutput::Shutdown() {
#ifdef _WIN32
  if (device_) {
    DestroySyntheticPointerDevice(static_cast<HSYNTHETICPOINTERDEVICE>(device_));
    device_ = nullptr;
  }
#endif
  available_ = false;
  usesCreate2_ = false;
  contactDown_ = false;
}

bool SyntheticTouchpadOutput::Inject(const OutputFrame& frame, std::string* error) {
#ifdef _WIN32
  if (!device_ || !available_) {
    if (error) *error = "synthetic touchpad not available";
    return false;
  }
  POINTER_TYPE_INFO info{};
  info.type = static_cast<POINTER_INPUT_TYPE>(PT_TOUCHPAD);
  auto& t = info.touchInfo;
  t.pointerInfo.pointerType = static_cast<POINTER_INPUT_TYPE>(PT_TOUCHPAD);
  t.pointerInfo.pointerId = 1;
  t.pointerInfo.ptHimetricLocation.x =
      static_cast<LONG>(std::lround(frame.contacts.contacts[0].xLogical));
  t.pointerInfo.ptHimetricLocation.y =
      static_cast<LONG>(std::lround(frame.contacts.contacts[0].yLogical));
  if (frame.contacts.contactCount > 0 && frame.contacts.contacts[0].tip) {
    t.pointerInfo.pointerFlags = POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;
    if (!contactDown_) {
      t.pointerInfo.pointerFlags = POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
      contactDown_ = true;
    }
  } else {
    t.pointerInfo.pointerFlags = contactDown_ ? POINTER_FLAG_UP : POINTER_FLAG_UPDATE;
    contactDown_ = false;
  }
  t.touchMask = TOUCH_MASK_CONTACTAREA;
  if (!InjectSyntheticPointerInput(static_cast<HSYNTHETICPOINTERDEVICE>(device_), &info, 1)) {
    if (error) *error = "InjectSyntheticPointerInput failed";
    return false;
  }
  return true;
#else
  (void)frame;
  if (error) *error = "synthetic pointer requires Windows";
  return false;
#endif
}

std::unique_ptr<IPointerOutput> CreatePreferredOutput(bool allowDiagnosticFallback) {
  auto syn = std::make_unique<SyntheticTouchpadOutput>();
  std::string err;
  DeviceCaps caps = motion::SyntheticCaps();
  if (syn->Initialize(caps, &err)) {
    return syn;
  }
  if (allowDiagnosticFallback) {
    auto diag = std::make_unique<DiagnosticMouseOutput>();
    diag->Initialize(caps, &err);
    return diag;
  }
  return syn;
}

}  // namespace smoothpad::output
