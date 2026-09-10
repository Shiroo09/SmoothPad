#include "smoothpad/EngineHost.h"

#include "smoothpad/Logging.h"
#include "smoothpad/Math.h"
#include "smoothpad/platform/Win32.h"

#include <algorithm>
#include <cmath>

namespace smoothpad {

EngineHost::EngineHost() { settings_ = DefaultSettings(); }

EngineHost::~EngineHost() { Stop(); }

void EngineHost::LoadOrDefaultSettings() {
  std::string err;
  Settings s;
  if (!LoadSettings(DefaultSettingsPath(), s, &err)) {
    s = DefaultSettings();
  }
  settings_ = s;
}

bool EngineHost::Start(void* hwnd, std::string* error) {
  LoadOrDefaultSettings();
  std::string err;
  const bool rawOk = raw_.Initialize(hwnd, &err);
  if (!rawOk) {
    mode_ = EngineMode::Unsupported;
    error_ = platform::Utf8ToWide(err);
    if (error) *error = err;
    SP_LOG_ERROR(err);
    // Still allow UI / diagnostics.
  } else {
    if (const auto* sel = raw_.Selected()) {
      caps_ = *sel;
    }
    raw_.SetCallback([this](const RawContactFrame& f, const DeviceCaps& c) { HandleFrame(f, c); });
  }

  std::string filterErr;
  filterOk_ = filter_.Connect(&filterErr);
  if (filterOk_) {
    filter_.StartHeartbeatThread();
    suppressionOk_ = true;
    SP_LOG_INFO("Filter connected; fail-open pass-through until Mac Mode is enabled");
  } else {
    // Documented user-mode APIs do not suppress native PTP pointer promotion.
    suppressionOk_ = false;
    SP_LOG_WARN("Filter not present. Mac Mode would double-move; remaining in Native/Degraded.");
  }

  std::string outErr;
  output_ = std::make_unique<output::SyntheticTouchpadOutput>();
  DeviceCaps outCaps = caps_.physicalSizeKnown ? caps_ : motion::SyntheticCaps();
  syntheticOk_ = output_->Initialize(outCaps, &outErr);
  if (!syntheticOk_) {
    SP_LOG_WARN(outErr);
    output_.reset();
  }

  if (rawOk && !motionConfigured_) {
    motion::EngineConfig cfg;
    cfg.caps = caps_.identity.usage == 0 ? motion::SyntheticCaps() : caps_;
    if (cfg.caps.x.logicalMax == 0) cfg.caps = motion::SyntheticCaps();
    cfg.settings = settings_;
    motion_.Configure(cfg);
    motionConfigured_ = true;
    caps_ = cfg.caps;
  }

  started_ = true;
  RecalcMode();
  if (mode_ == EngineMode::Unsupported && error) *error = err;
  return rawOk || true;
}

void EngineHost::Stop() {
  if (filterOk_) {
    std::string e;
    filter_.SetMode(driver::FilterMode::PassThrough, &e);
    filter_.Disconnect();
    filterOk_ = false;
  }
  raw_.Shutdown();
  if (output_) output_->Shutdown();
  if (tracing_) {
    tracer_.Close();
    tracing_ = false;
  }
  started_ = false;
}

void EngineHost::RecalcMode() {
  if (raw_.Enumerate().empty() && caps_.identity.usage == 0 && !started_) {
    mode_ = EngineMode::Unsupported;
    return;
  }
  if (!settings_.enabled || settings_.profile == ProfileId::WindowsNative) {
    if (filterOk_) {
      std::string e;
      filter_.SetMode(driver::FilterMode::PassThrough, &e);
    }
    mode_ = EngineMode::Native;
    return;
  }
  if (!suppressionOk_ || !syntheticOk_) {
    // Never advertise Mac Mode if native + injected motion would combine.
    mode_ = EngineMode::Degraded;
    if (filterOk_) {
      std::string e;
      filter_.SetMode(driver::FilterMode::PassThrough, &e);
    }
    return;
  }
  std::string e;
  if (!filter_.SetMode(driver::FilterMode::Custom, &e)) {
    mode_ = EngineMode::Degraded;
    return;
  }
  mode_ = EngineMode::MacMode;
}

void EngineHost::SetEnabled(bool enabled) {
  std::lock_guard lock(mutex_);
  settings_.enabled = enabled;
  RecalcMode();
  SaveSettingsAtomic(DefaultSettingsPath(), settings_, nullptr);
}

void EngineHost::SetProfile(ProfileId id) {
  std::lock_guard lock(mutex_);
  settings_.profile = id;
  const auto p = BuiltinProfile(id);
  settings_.motion = p.motion;
  settings_.acceleration = p.acceleration;
  settings_.smoothing = p.smoothing;
  settings_.noise = p.noise;
  motion_.ApplySettings(settings_);
  RecalcMode();
  SaveSettingsAtomic(DefaultSettingsPath(), settings_, nullptr);
}

void EngineHost::SetMotionUi(const MotionUiParams& ui) {
  std::lock_guard lock(mutex_);
  settings_.motion = ui;
  motion_.ApplySettings(settings_);
  SaveSettingsAtomic(DefaultSettingsPath(), settings_, nullptr);
}

void EngineHost::ApplySettings(const Settings& s) {
  std::lock_guard lock(mutex_);
  settings_ = s;
  motion_.ApplySettings(settings_);
  RecalcMode();
  SaveSettingsAtomic(DefaultSettingsPath(), settings_, nullptr);
}

Settings EngineHost::CurrentSettings() const {
  std::lock_guard lock(mutex_);
  return settings_;
}

bool EngineHost::OnRawInput(void* lParam) { return raw_.OnRawInput(lParam); }

void EngineHost::OnHotkey(int id) {
  if (id == 1) {
    SetEnabled(!settings_.enabled);
  } else if (id == 2) {
    SetEnabled(false);
    SetProfile(ProfileId::WindowsNative);
  }
}

bool EngineHost::BeginTrace(const std::filesystem::path& path, std::string* error) {
  if (!tracer_.Open(path, caps_)) {
    if (error) *error = "failed to open trace";
    return false;
  }
  tracing_ = true;
  return true;
}

bool EngineHost::EndTrace() {
  if (!tracing_) return false;
  tracing_ = false;
  return tracer_.Close();
}

void EngineHost::HandleFrame(const RawContactFrame& frame, const DeviceCaps& caps) {
  const auto t0 = platform::QpcNow();
  {
    std::lock_guard lock(mutex_);
    caps_ = caps;
    if (tracing_) tracer_.Write(frame);
    if (!motionConfigured_) {
      motion_.Configure({caps, settings_});
      motionConfigured_ = true;
    }
  }

  if (mode_ != EngineMode::MacMode) {
    return;
  }

  const auto begin = platform::QpcNow();
  MotionOutput mo = motion_.Process(frame);
  const auto end = platform::QpcNow();
  lastSpeed_ = mo.speedMmS;
  lastGain_ = mo.gain;
  contactState_ = mo.state;
  PushLatency(platform::QpcToMilliseconds(end - begin));

  if (output_ && syntheticOk_) {
    output::OutputFrame of;
    of.motion = mo;
    of.contacts = frame;
    of.caps = caps;
    std::string err;
    output_->Inject(of, &err);
  }
  lastReceiveQpc_ = t0;
}

void EngineHost::PushLatency(double ms) {
  latencyMs_.push_back(ms);
  while (latencyMs_.size() > 256) latencyMs_.pop_front();
}

DiagnosticsSnapshot EngineHost::Diagnostics() const {
  std::lock_guard lock(mutex_);
  DiagnosticsSnapshot d;
  d.mode = mode_;
  d.input = InputBackend::RawInput;
  d.output = syntheticOk_ ? OutputBackend::SyntheticTouchpad : OutputBackend::None;
  d.ptpDetected = caps_.identity.usage == kTouchPadUsage || raw_.Selected() != nullptr;
  d.driverConnected = filterOk_;
  d.driverRequired = !suppressionOk_ || true;  // user-mode suppression is not documented
  d.suppressionReliable = suppressionOk_;
  d.syntheticAvailable = syntheticOk_;
  d.caps = caps_;
  d.reportHz = raw_.MedianHz();
  d.p95IntervalMs = raw_.P95IntervalMs();
  d.jitterMs = raw_.JitterMs();
  d.lastSpeedMmS = lastSpeed_;
  d.lastGain = lastGain_;
  d.contactState = contactState_;
  if (!latencyMs_.empty()) {
    std::vector<double> s(latencyMs_.begin(), latencyMs_.end());
    std::sort(s.begin(), s.end());
    d.latencyMedianMs = Percentile(s, 0.5);
    d.latencyP95Ms = Percentile(s, 0.95);
    d.latencyP99Ms = Percentile(s, 0.99);
  }
  if (mode_ == EngineMode::Unsupported) {
    d.statusText = L"Windows Precision Touchpad not detected.";
  } else if (mode_ == EngineMode::Degraded) {
    d.statusText =
        L"Mac Mode unavailable: native pointer promotion cannot be suppressed without SmoothPadFilter.";
  } else if (mode_ == EngineMode::MacMode) {
    d.statusText = L"Mac Mode active";
  } else {
    d.statusText = L"Windows Native";
  }
  d.errorText = error_;
  return d;
}

bool EngineHost::MacModeActive() const {
  std::lock_guard lock(mutex_);
  return mode_ == EngineMode::MacMode;
}

void EngineHost::ApplyCalibration(const CalibrationResult& c) {
  std::lock_guard lock(mutex_);
  calibration_ = c;
  settings_.noise.floorMm = c.noiseFloorMm;
  settings_.noise.thresholdMm = c.noiseFloorMm * 2.5;
  settings_.acceleration.curveStartMmS = c.slowMmS;
  settings_.acceleration.curveMidMmS = c.mediumMmS;
  settings_.acceleration.curveEndMmS = c.fastMmS;
  motion_.ApplySettings(settings_);
  SaveSettingsAtomic(DefaultSettingsPath(), settings_, nullptr);
}

}  // namespace smoothpad
