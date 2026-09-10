#pragma once

#include "smoothpad/Settings.h"
#include "smoothpad/Types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace smoothpad::motion {

struct CoordinateNormalizer {
  AxisCaps x;
  AxisCaps y;
  double widthMm = 0.0;
  double heightMm = 0.0;
  bool physicalKnown = false;

  void Configure(const DeviceCaps& caps);
  [[nodiscard]] Vec2 ToNormalized(int lx, int ly) const;
  [[nodiscard]] Vec2 ToMm(int lx, int ly) const;
  [[nodiscard]] Vec2 DeltaMm(int x0, int y0, int x1, int y1) const;
};

class ContactTracker {
 public:
  void Reset();
  void Update(const RawContactFrame& frame, const CoordinateNormalizer& norm, double tSeconds);
  [[nodiscard]] MotionSample Sample() const noexcept { return sample_; }
  [[nodiscard]] ContactState State() const noexcept { return state_; }
  [[nodiscard]] int ActiveCount() const noexcept { return activeCount_; }

 private:
  struct Slot {
    std::uint32_t id = 0;
    bool used = false;
    bool tip = false;
    int x = 0;
    int y = 0;
    Vec2 mm{};
  };

  std::array<Slot, kMaxContacts> slots_{};
  MotionSample sample_{};
  ContactState state_ = ContactState::Idle;
  int activeCount_ = 0;
  double lastT_ = 0.0;
  bool haveTime_ = false;
  std::optional<std::uint32_t> primaryId_;
  bool button_ = false;
};

class NoiseEstimator {
 public:
  void Reset();
  void Configure(const NoiseParams& p);
  [[nodiscard]] Vec2 Filter(Vec2 deltaMm, bool stationaryHint);
  [[nodiscard]] double FloorMm() const noexcept { return floorMm_; }

 private:
  NoiseParams params_{};
  double floorMm_ = 0.012;
  double ema_ = 0.0;
  bool haveEma_ = false;
};

class VelocityEstimator {
 public:
  void Reset();
  void Push(double t, Vec2 posMm);
  [[nodiscard]] Vec2 VelocityMmS() const noexcept { return vel_; }
  [[nodiscard]] double SpeedMmS() const noexcept { return vel_.length(); }

 private:
  struct Sample {
    double t = 0.0;
    Vec2 p{};
  };
  static constexpr int kMax = 8;
  std::array<Sample, kMax> hist_{};
  int count_ = 0;
  int head_ = 0;
  Vec2 vel_{};
  double windowS_ = 0.016;
};

class AccelerationCurve {
 public:
  void Configure(const AccelerationParams& p);
  [[nodiscard]] double Gain(double speedMmS) const;
  [[nodiscard]] const AccelerationParams& Params() const noexcept { return params_; }

 private:
  AccelerationParams params_{};
};

class OneEuroFilter {
 public:
  void Reset();
  void Configure(double minCutoff, double beta, double dCutoff);
  [[nodiscard]] Vec2 Filter(Vec2 value, double dt);

 private:
  struct Axis {
    double xHat = 0.0;
    double dxHat = 0.0;
    bool initialized = false;
  };
  Axis x_{};
  Axis y_{};
  double minCutoff_ = 1.2;
  double beta_ = 0.007;
  double dCutoff_ = 1.0;

  static double Alpha(double cutoff, double dt);
  static double FilterAxis(Axis& a, double value, double dt, double minCutoff, double beta, double dCutoff);
};

class AdaptiveSmoother {
 public:
  void Reset();
  void Configure(const SmoothingParams& p);
  [[nodiscard]] Vec2 Filter(Vec2 value, double dt, double speedMmS);

 private:
  SmoothingParams params_{};
  OneEuroFilter oneEuro_;
  Vec2 ema_{};
  bool haveEma_ = false;
};

class FractionalAccumulator {
 public:
  void Reset();
  [[nodiscard]] Vec2 Consume(Vec2 delta);
  [[nodiscard]] Vec2 Remainder() const noexcept { return rem_; }

 private:
  Vec2 rem_{};
};

struct EngineConfig {
  DeviceCaps caps;
  Settings settings;
};

class MotionEngine {
 public:
  void Configure(const EngineConfig& cfg);
  void ApplySettings(const Settings& settings);
  void Reset();

  [[nodiscard]] MotionOutput Process(const RawContactFrame& frame);
  [[nodiscard]] MotionOutput ProcessSample(const MotionSample& sample);

  [[nodiscard]] ContactState State() const noexcept { return tracker_.State(); }
  [[nodiscard]] double LastSpeedMmS() const noexcept { return lastSpeed_; }
  [[nodiscard]] double LastGain() const noexcept { return lastGain_; }
  [[nodiscard]] const AccelerationParams& Curve() const noexcept { return curve_.Params(); }
  [[nodiscard]] Vec2 Remainder() const noexcept { return frac_.Remainder(); }

 private:
  CoordinateNormalizer normalizer_;
  ContactTracker tracker_;
  NoiseEstimator noise_;
  VelocityEstimator velocity_;
  AccelerationCurve curve_;
  AdaptiveSmoother smoother_;
  FractionalAccumulator frac_;
  Settings settings_{};
  bool configured_ = false;
  double lastSpeed_ = 0.0;
  double lastGain_ = 1.0;
  Vec2 lastPosMm_{};
  bool havePos_ = false;
  double qpcFreq_ = 0.0;
};

[[nodiscard]] std::vector<MotionSample> SynthesizeTrace(
    std::string_view kind,
    const DeviceCaps& caps,
    double hz = 125.0);

[[nodiscard]] DeviceCaps SyntheticCaps();

}  // namespace smoothpad::motion
