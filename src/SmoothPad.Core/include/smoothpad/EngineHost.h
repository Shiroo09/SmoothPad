#pragma once

#include "smoothpad/Settings.h"
#include "smoothpad/Trace.h"
#include "smoothpad/Types.h"
#include "smoothpad/driver/FilterClient.h"
#include "smoothpad/input/RawInputSource.h"
#include "smoothpad/motion/MotionEngine.h"
#include "smoothpad/output/PointerOutput.h"

#include <atomic>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace smoothpad {

struct DiagnosticsSnapshot {
  EngineMode mode = EngineMode::Unsupported;
  InputBackend input = InputBackend::None;
  OutputBackend output = OutputBackend::None;
  bool ptpDetected = false;
  bool driverConnected = false;
  bool driverRequired = true;
  bool suppressionReliable = false;
  bool syntheticAvailable = false;
  DeviceCaps caps;
  double reportHz = 0.0;
  double p95IntervalMs = 0.0;
  double jitterMs = 0.0;
  double latencyMedianMs = 0.0;
  double latencyP95Ms = 0.0;
  double latencyP99Ms = 0.0;
  double lastSpeedMmS = 0.0;
  double lastGain = 1.0;
  ContactState contactState = ContactState::Idle;
  std::wstring statusText;
  std::wstring errorText;
};

struct CalibrationResult {
  double noiseFloorMm = 0.012;
  double medianHz = 125.0;
  double slowMmS = 25.0;
  double mediumMmS = 140.0;
  double fastMmS = 420.0;
  double widthMm = 0.0;
  double heightMm = 0.0;
};

class EngineHost {
 public:
  EngineHost();
  ~EngineHost();

  bool Start(void* hwnd, std::string* error);
  void Stop();

  void SetEnabled(bool enabled);
  void SetProfile(ProfileId id);
  void SetMotionUi(const MotionUiParams& ui);
  void ApplySettings(const Settings& s);
  [[nodiscard]] Settings CurrentSettings() const;

  bool OnRawInput(void* lParam);
  void OnHotkey(int id);

  bool BeginTrace(const std::filesystem::path& path, std::string* error);
  bool EndTrace();

  [[nodiscard]] DiagnosticsSnapshot Diagnostics() const;
  [[nodiscard]] CalibrationResult LastCalibration() const { return calibration_; }
  void ApplyCalibration(const CalibrationResult& c);

  [[nodiscard]] bool MacModeActive() const;

 private:
  void HandleFrame(const RawContactFrame& frame, const DeviceCaps& caps);
  void RecalcMode();
  void PushLatency(double ms);
  void LoadOrDefaultSettings();

  mutable std::mutex mutex_;
  Settings settings_{};
  EngineMode mode_ = EngineMode::Unsupported;
  input::RawInputSource raw_;
  motion::MotionEngine motion_;
  std::unique_ptr<output::IPointerOutput> output_;
  driver::FilterClient filter_;
  TraceWriter tracer_;
  bool tracing_ = false;
  bool started_ = false;
  bool motionConfigured_ = false;
  bool syntheticOk_ = false;
  bool filterOk_ = false;
  bool suppressionOk_ = false;
  std::wstring error_;
  DeviceCaps caps_{};
  CalibrationResult calibration_{};
  std::deque<double> latencyMs_;
  double lastSpeed_ = 0.0;
  double lastGain_ = 1.0;
  ContactState contactState_ = ContactState::Idle;
  std::uint64_t lastReceiveQpc_ = 0;
};

}  // namespace smoothpad
