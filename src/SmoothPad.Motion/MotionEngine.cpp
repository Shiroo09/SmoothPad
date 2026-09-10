#include "smoothpad/motion/MotionEngine.h"

#include "smoothpad/Math.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace smoothpad::motion {
namespace {

double LogicalSpan(const AxisCaps& a) {
  const double span = static_cast<double>(a.logicalMax - a.logicalMin);
  return span == 0.0 ? 1.0 : span;
}

}  // namespace

void CoordinateNormalizer::Configure(const DeviceCaps& caps) {
  x = caps.x;
  y = caps.y;
  widthMm = caps.widthMm;
  heightMm = caps.heightMm;
  physicalKnown = caps.physicalSizeKnown && PlausibleTouchpadMm(widthMm, heightMm);
  if (!physicalKnown) {
    if (caps.x.hasPhysical && caps.y.hasPhysical) {
      widthMm = HidPhysicalToMm(caps.x.physicalMax - caps.x.physicalMin, caps.x.units, caps.x.unitsExp);
      heightMm = HidPhysicalToMm(caps.y.physicalMax - caps.y.physicalMin, caps.y.units, caps.y.unitsExp);
      physicalKnown = PlausibleTouchpadMm(widthMm, heightMm);
    }
  }
  if (!physicalKnown) {
    // Conservative laptop PTP fallback used only when HID physical size is absent.
    widthMm = 105.0;
    heightMm = 70.0;
  }
}

Vec2 CoordinateNormalizer::ToNormalized(int lx, int ly) const {
  return {
      (static_cast<double>(lx) - x.logicalMin) / LogicalSpan(x),
      (static_cast<double>(ly) - y.logicalMin) / LogicalSpan(y),
  };
}

Vec2 CoordinateNormalizer::ToMm(int lx, int ly) const {
  const auto n = ToNormalized(lx, ly);
  return {n.x * widthMm, n.y * heightMm};
}

Vec2 CoordinateNormalizer::DeltaMm(int x0, int y0, int x1, int y1) const {
  return ToMm(x1, y1) - ToMm(x0, y0);
}

void ContactTracker::Reset() {
  slots_ = {};
  sample_ = {};
  state_ = ContactState::Idle;
  activeCount_ = 0;
  lastT_ = 0.0;
  haveTime_ = false;
  primaryId_.reset();
  button_ = false;
}

void ContactTracker::Update(const RawContactFrame& frame, const CoordinateNormalizer& norm, double tSeconds) {
  std::array<Slot, kMaxContacts> next{};
  int nextCount = 0;
  const auto n = std::min<std::uint32_t>(frame.contactCount, kMaxContacts);
  for (std::uint32_t i = 0; i < n; ++i) {
    const auto& c = frame.contacts[i];
    if (!c.valid || !c.tip) {
      continue;
    }
    Slot s;
    s.id = c.id;
    s.used = true;
    s.tip = true;
    s.x = c.xLogical;
    s.y = c.yLogical;
    s.mm = norm.ToMm(c.xLogical, c.yLogical);
    if (nextCount < kMaxContacts) {
      next[static_cast<std::size_t>(nextCount++)] = s;
    }
  }

  double dt = 0.0;
  if (haveTime_) {
    dt = tSeconds - lastT_;
    if (dt < 0.0) dt = 0.0;
    if (dt > 0.25) dt = 0.25;  // large-dt clamp, no frame-rate assumption
  }
  lastT_ = tSeconds;
  haveTime_ = true;

  bool newContact = false;
  bool lostPrimary = false;
  std::optional<Slot> primary;
  if (primaryId_) {
    for (int i = 0; i < nextCount; ++i) {
      if (next[static_cast<std::size_t>(i)].id == *primaryId_) {
        primary = next[static_cast<std::size_t>(i)];
        break;
      }
    }
    if (!primary) lostPrimary = true;
  }
  if (!primary && nextCount > 0) {
    primary = next[0];
    newContact = true;
    primaryId_ = primary->id;
  }
  if (nextCount == 0) {
    primaryId_.reset();
  }

  Vec2 delta{};
  bool canDelta = false;
  if (primary && !newContact && !lostPrimary) {
    for (const auto& old : slots_) {
      if (old.used && old.id == primary->id) {
        delta = primary->mm - old.mm;
        canDelta = true;
        break;
      }
    }
  }

  sample_ = {};
  sample_.tSeconds = tSeconds;
  sample_.dt = dt;
  sample_.contactCount = static_cast<std::uint32_t>(nextCount);
  sample_.button = frame.button;
  sample_.newContact = newContact || lostPrimary;
  sample_.contactEnd = nextCount == 0 && activeCount_ > 0;
  if (primary) {
    sample_.primaryId = primary->id;
    sample_.tip = true;
    sample_.mm = primary->mm;
    sample_.normalized = norm.ToNormalized(primary->x, primary->y);
    sample_.logical = {static_cast<double>(primary->x), static_cast<double>(primary->y)};
  }

  const int prevCount = activeCount_;
  activeCount_ = nextCount;
  slots_ = next;
  button_ = frame.button;

  if (nextCount == 0) {
    state_ = (prevCount > 0) ? ContactState::ContactEnd : ContactState::Idle;
  } else if (prevCount == 0) {
    state_ = ContactState::ContactBegin;
  } else if (nextCount == 1) {
    state_ = frame.button ? ContactState::PhysicalClick : ContactState::SinglePointer;
  } else if (prevCount == 1 && nextCount >= 2) {
    state_ = ContactState::MultiTouchPending;
  } else {
    state_ = ContactState::MultiTouch;
  }

  if (!canDelta) {
    sample_.newContact = true;
  }
  (void)delta;
}

void NoiseEstimator::Reset() {
  ema_ = 0.0;
  haveEma_ = false;
  floorMm_ = params_.floorMm;
}

void NoiseEstimator::Configure(const NoiseParams& p) {
  params_ = p;
  floorMm_ = p.floorMm;
}

Vec2 NoiseEstimator::Filter(Vec2 deltaMm, bool stationaryHint) {
  const double mag = deltaMm.length();
  if (params_.adaptive) {
    const double a = stationaryHint ? 0.08 : 0.02;
    ema_ = haveEma_ ? (a * mag + (1.0 - a) * ema_) : mag;
    haveEma_ = true;
    floorMm_ = std::max(params_.floorMm, ema_ * 0.85);
  } else {
    floorMm_ = params_.floorMm;
  }
  const double thr = std::max(params_.thresholdMm, floorMm_);
  if (mag < thr) {
    // Soft gate: retain micro-motion as a fraction rather than hard-zero.
    const double scale = mag <= kEpsilon ? 0.0 : Smoothstep(0.0, thr, mag) * (mag / thr);
    return deltaMm * scale * 0.25;
  }
  return deltaMm;
}

void VelocityEstimator::Reset() {
  hist_ = {};
  count_ = 0;
  head_ = 0;
  vel_ = {};
}

void VelocityEstimator::Push(double t, Vec2 posMm) {
  hist_[static_cast<std::size_t>(head_)] = Sample{t, posMm};
  head_ = (head_ + 1) % kMax;
  if (count_ < kMax) ++count_;

  if (count_ < 2) {
    vel_ = {};
    return;
  }

  // Weighted linear regression over ~8-20 ms.
  double tMax = t;
  int n = 0;
  std::array<Sample, kMax> pts{};
  for (int i = 0; i < count_; ++i) {
    const int idx = (head_ - 1 - i + kMax * 4) % kMax;
    const auto& s = hist_[static_cast<std::size_t>(idx)];
    if (tMax - s.t > windowS_ && n >= 2) break;
    pts[static_cast<std::size_t>(n++)] = s;
  }
  if (n < 2) {
    vel_ = {};
    return;
  }

  double wsum = 0.0;
  double tw = 0.0;
  Vec2 pw{};
  for (int i = 0; i < n; ++i) {
    const double w = static_cast<double>(i + 1);
    wsum += w;
    tw += w * pts[static_cast<std::size_t>(i)].t;
    pw = pw + pts[static_cast<std::size_t>(i)].p * w;
  }
  const double tMean = tw / wsum;
  const Vec2 pMean{pw.x / wsum, pw.y / wsum};
  double varT = 0.0;
  Vec2 cov{};
  for (int i = 0; i < n; ++i) {
    const double w = static_cast<double>(i + 1);
    const double dt = pts[static_cast<std::size_t>(i)].t - tMean;
    varT += w * dt * dt;
    cov.x += w * dt * (pts[static_cast<std::size_t>(i)].p.x - pMean.x);
    cov.y += w * dt * (pts[static_cast<std::size_t>(i)].p.y - pMean.y);
  }
  if (varT <= kEpsilon) {
    vel_ = {};
    return;
  }
  vel_ = {cov.x / varT, cov.y / varT};
}

void AccelerationCurve::Configure(const AccelerationParams& p) {
  params_ = p;
  if (params_.curveMidMmS <= params_.curveStartMmS) {
    params_.curveMidMmS = params_.curveStartMmS + 1.0;
  }
  if (params_.curveEndMmS <= params_.curveMidMmS) {
    params_.curveEndMmS = params_.curveMidMmS + 1.0;
  }
  if (params_.maxGain < params_.midGain) {
    params_.maxGain = params_.midGain;
  }
}

double AccelerationCurve::Gain(double speedMmS) const {
  const double s = std::max(0.0, speedMmS);
  const double lo = params_.lowGain;
  const double mid = params_.midGain;
  const double hi = params_.maxGain;
  // Constant precision gain until curveStart, then C0-smooth rise. Never
  // drop at the Start/Mid boundary (that would feel like a dead zone).
  if (s <= params_.curveStartMmS) {
    return lo;
  }
  if (s <= params_.curveMidMmS) {
    const double t = Smoothstep(params_.curveStartMmS, params_.curveMidMmS, s);
    const double k = Clamp(params_.shape, 0.05, 1.0);
    const double shaped = std::pow(t, 0.5 + k);
    return Lerp(lo, mid, shaped);
  }
  if (s >= params_.curveEndMmS) {
    return hi;
  }
  const double t = Smoothstep(params_.curveMidMmS, params_.curveEndMmS, s);
  const double k = Clamp(params_.shape, 0.05, 1.0);
  return Lerp(mid, hi, std::pow(t, 1.0 - 0.4 * k));
}

double OneEuroFilter::Alpha(double cutoff, double dt) {
  if (dt <= 0.0) return 1.0;
  const double tau = 1.0 / (2.0 * kPi * std::max(cutoff, 1e-6));
  return 1.0 / (1.0 + tau / dt);
}

double OneEuroFilter::FilterAxis(Axis& a, double value, double dt, double minCutoff, double beta, double dCutoff) {
  if (!a.initialized) {
    a.xHat = value;
    a.dxHat = 0.0;
    a.initialized = true;
    return value;
  }
  const double dx = dt > 0.0 ? (value - a.xHat) / dt : 0.0;
  const double ad = Alpha(dCutoff, dt);
  a.dxHat = ad * dx + (1.0 - ad) * a.dxHat;
  const double cutoff = minCutoff + beta * std::abs(a.dxHat);
  const double a0 = Alpha(cutoff, dt);
  a.xHat = a0 * value + (1.0 - a0) * a.xHat;
  return a.xHat;
}

void OneEuroFilter::Reset() {
  x_ = {};
  y_ = {};
}

void OneEuroFilter::Configure(double minCutoff, double beta, double dCutoff) {
  minCutoff_ = minCutoff;
  beta_ = beta;
  dCutoff_ = dCutoff;
}

Vec2 OneEuroFilter::Filter(Vec2 value, double dt) {
  return {
      FilterAxis(x_, value.x, dt, minCutoff_, beta_, dCutoff_),
      FilterAxis(y_, value.y, dt, minCutoff_, beta_, dCutoff_),
  };
}

void AdaptiveSmoother::Reset() {
  oneEuro_.Reset();
  ema_ = {};
  haveEma_ = false;
}

void AdaptiveSmoother::Configure(const SmoothingParams& p) {
  params_ = p;
  oneEuro_.Configure(p.minCutoff, p.beta, p.dCutoff);
}

Vec2 AdaptiveSmoother::Filter(Vec2 value, double dt, double speedMmS) {
  if (params_.strength <= 0.001) {
    return value;
  }
  // Fast movement: reduce lag.
  const double fast = Smoothstep(80.0, 400.0, speedMmS);
  const double strength = params_.strength * (1.0 - 0.85 * fast);
  if (params_.algorithm == SmootherKind::OneEuro || params_.algorithm == SmootherKind::VelocityEma) {
    const Vec2 f = oneEuro_.Filter(value, dt);
    return value * (1.0 - strength) + f * strength;
  }
  const double alpha = Clamp(Lerp(0.15, 0.85, 1.0 - strength), 0.05, 0.95);
  if (!haveEma_) {
    ema_ = value;
    haveEma_ = true;
    return value;
  }
  ema_ = value * alpha + ema_ * (1.0 - alpha);
  return ema_;
}

void FractionalAccumulator::Reset() { rem_ = {}; }

Vec2 FractionalAccumulator::Consume(Vec2 delta) {
  rem_ = rem_ + delta;
  Vec2 out{std::trunc(rem_.x), std::trunc(rem_.y)};
  rem_ = rem_ - out;
  return out;
}

void MotionEngine::Configure(const EngineConfig& cfg) {
  settings_ = cfg.settings;
  normalizer_.Configure(cfg.caps);
  ApplySettings(cfg.settings);
  Reset();
  configured_ = true;
#ifdef _WIN32
  LARGE_INTEGER freq{};
  QueryPerformanceFrequency(&freq);
  qpcFreq_ = freq.QuadPart > 0 ? static_cast<double>(freq.QuadPart) : 10'000'000.0;
#else
  qpcFreq_ = 1'000'000'000.0;
#endif
}

void MotionEngine::ApplySettings(const Settings& settings) {
  settings_ = settings;
  const auto blendedAccel = BlendAcceleration(settings.acceleration, settings.motion);
  const auto blendedSmooth = BlendSmoothing(settings.smoothing, settings.motion);
  const auto blendedNoise = BlendNoise(settings.noise, settings.motion);
  curve_.Configure(blendedAccel);
  smoother_.Configure(blendedSmooth);
  noise_.Configure(blendedNoise);
}

void MotionEngine::Reset() {
  tracker_.Reset();
  noise_.Reset();
  velocity_.Reset();
  smoother_.Reset();
  frac_.Reset();
  lastSpeed_ = 0.0;
  lastGain_ = 1.0;
  lastPosMm_ = {};
  havePos_ = false;
}

MotionOutput MotionEngine::Process(const RawContactFrame& frame) {
  double t = 0.0;
  if (frame.qpc != 0 && qpcFreq_ > 0.0) {
    t = static_cast<double>(frame.qpc) / qpcFreq_;
  }
  tracker_.Update(frame, normalizer_, t);
  return ProcessSample(tracker_.Sample());
}

MotionOutput MotionEngine::ProcessSample(const MotionSample& sample) {
  MotionOutput out;
  out.state = tracker_.State();
  out.pointerActive = sample.tip && sample.contactCount == 1;

  if (sample.contactEnd || sample.contactCount == 0) {
    velocity_.Reset();
    smoother_.Reset();
    frac_.Reset();
    havePos_ = false;
    lastSpeed_ = 0.0;
    lastGain_ = 1.0;
    out.emit = false;
    out.pointerActive = false;
    return out;
  }

  // Multi-touch: do not drive pointer (gestures preserved by output path).
  if (sample.contactCount >= 2) {
    velocity_.Reset();
    frac_.Reset();
    havePos_ = false;
    out.emit = false;
    out.pointerActive = false;
    out.state = sample.contactCount == 2 ? ContactState::MultiTouch : ContactState::MultiTouch;
    return out;
  }

  if (sample.newContact || !havePos_) {
    lastPosMm_ = sample.mm;
    havePos_ = true;
    velocity_.Reset();
    velocity_.Push(sample.tSeconds, sample.mm);
    frac_.Reset();
    lastSpeed_ = 0.0;
    lastGain_ = curve_.Gain(0.0);
    out.gain = lastGain_;
    out.emit = false;
    return out;
  }

  Vec2 delta = sample.mm - lastPosMm_;
  lastPosMm_ = sample.mm;
  havePos_ = true;

  const bool stationary = delta.length() < 0.08;
  delta = noise_.Filter(delta, stationary);

  velocity_.Push(sample.tSeconds, sample.mm);
  double speed = velocity_.SpeedMmS();
  if (sample.dt > 0.0) {
    const double inst = delta.length() / sample.dt;
    speed = Lerp(inst, speed, 0.65);
  }
  lastSpeed_ = speed;

  // No pointer inertia: zero input delta => zero output.
  if (delta.length() <= kEpsilon) {
    lastGain_ = curve_.Gain(0.0);
    out.speedMmS = 0.0;
    out.gain = lastGain_;
    out.emit = false;
    return out;
  }

  const double gain = curve_.Gain(speed) * settings_.motion.pointerSpeed;
  lastGain_ = gain;
  Vec2 mapped = delta * gain;

  if (settings_.experimental.prediction && settings_.experimental.predictionMs > 0.0) {
    const double predS = settings_.experimental.predictionMs * 0.001;
    mapped = mapped + velocity_.VelocityMmS() * predS * gain * 0.25;
  }

  mapped = smoother_.Filter(mapped, sample.dt > 0.0 ? sample.dt : 0.008, speed);

  out.deltaMm = mapped;
  out.speedMmS = speed;
  out.gain = gain;
  out.pointerActive = true;

  // Convert mm to pixels using a stable himetric-ish mapping: 1 mm ≈ 3.78 px at 96 DPI.
  // Fractional remainder is kept in pixel space for low-speed precision.
  constexpr double kMmToPx = 96.0 / 25.4;
  const Vec2 px = mapped * kMmToPx;
  const Vec2 integerPx = frac_.Consume(px);
  out.deltaPixels = integerPx;
  out.emit = integerPx.x != 0.0 || integerPx.y != 0.0 || mapped.length() > 0.0;
  return out;
}

DeviceCaps SyntheticCaps() {
  DeviceCaps c;
  c.identity.deviceName = L"Synthetic PTP";
  c.identity.usagePage = kDigitizerUsagePage;
  c.identity.usage = kTouchPadUsage;
  c.x = {0, 4095, 0, 10500, 0x11, -2, true};
  c.y = {0, 2730, 0, 7000, 0x11, -2, true};
  c.maxContacts = 5;
  c.widthMm = 105.0;
  c.heightMm = 70.0;
  c.physicalSizeKnown = true;
  return c;
}

std::vector<MotionSample> SynthesizeTrace(std::string_view kind, const DeviceCaps& caps, double hz) {
  CoordinateNormalizer n;
  n.Configure(caps);
  const double dt = hz > 0.0 ? 1.0 / hz : 0.008;
  std::vector<MotionSample> out;
  auto push = [&](double t, double nx, double ny, bool tip, std::uint32_t count, bool neu = false) {
    MotionSample s;
    s.tSeconds = t;
    s.dt = dt;
    s.contactCount = count;
    s.primaryId = 1;
    s.tip = tip;
    s.newContact = neu;
    s.contactEnd = !tip;
    s.normalized = {nx, ny};
    const int lx = static_cast<int>(std::lround(n.x.logicalMin + nx * (n.x.logicalMax - n.x.logicalMin)));
    const int ly = static_cast<int>(std::lround(n.y.logicalMin + ny * (n.y.logicalMax - n.y.logicalMin)));
    s.logical = {static_cast<double>(lx), static_cast<double>(ly)};
    s.mm = n.ToMm(lx, ly);
    out.push_back(s);
  };

  if (kind == "stationary") {
    for (int i = 0; i < 40; ++i) {
      const double jitter = ((i % 3) - 1) * 0.00015;
      push(i * dt, 0.5 + jitter, 0.5, true, 1, i == 0);
    }
  } else if (kind == "micro") {
    for (int i = 0; i < 50; ++i) {
      push(i * dt, 0.5 + i * 0.00008, 0.5, true, 1, i == 0);
    }
  } else if (kind == "slow") {
    for (int i = 0; i < 80; ++i) {
      push(i * dt, 0.2 + i * 0.002, 0.5, true, 1, i == 0);
    }
  } else if (kind == "medium") {
    for (int i = 0; i < 40; ++i) {
      push(i * dt, 0.15 + i * 0.015, 0.5, true, 1, i == 0);
    }
  } else if (kind == "fast" || kind == "veryfast") {
    const double step = kind == "veryfast" ? 0.06 : 0.035;
    for (int i = 0; i < 20; ++i) {
      push(i * dt, 0.05 + i * step, 0.5, true, 1, i == 0);
    }
  } else if (kind == "ramp") {
    for (int i = 0; i < 60; ++i) {
      const double step = 0.001 + i * 0.0008;
      push(i * dt, 0.1 + i * step, 0.5, true, 1, i == 0);
    }
  } else if (kind == "reversal") {
    for (int i = 0; i < 25; ++i) push(i * dt, 0.2 + i * 0.02, 0.5, true, 1, i == 0);
    for (int i = 0; i < 25; ++i) push((25 + i) * dt, 0.7 - i * 0.02, 0.5, true, 1, false);
  } else if (kind == "circle") {
    for (int i = 0; i < 80; ++i) {
      const double a = i / 80.0 * 2.0 * kPi;
      push(i * dt, 0.5 + 0.2 * std::cos(a), 0.5 + 0.2 * std::sin(a), true, 1, i == 0);
    }
  } else if (kind == "diagonal") {
    for (int i = 0; i < 40; ++i) {
      push(i * dt, 0.2 + i * 0.015, 0.2 + i * 0.015, true, 1, i == 0);
    }
  } else if (kind == "horizontal") {
    for (int i = 0; i < 40; ++i) push(i * dt, 0.1 + i * 0.02, 0.5, true, 1, i == 0);
  } else if (kind == "vertical") {
    for (int i = 0; i < 40; ++i) push(i * dt, 0.5, 0.1 + i * 0.02, true, 1, i == 0);
  } else if (kind == "lift") {
    for (int i = 0; i < 20; ++i) push(i * dt, 0.3 + i * 0.01, 0.5, true, 1, i == 0);
    push(20 * dt, 0.5, 0.5, false, 0, false);
    out.back().contactEnd = true;
  } else if (kind == "idswap") {
    for (int i = 0; i < 15; ++i) push(i * dt, 0.3 + i * 0.01, 0.5, true, 1, i == 0);
    for (int i = 0; i < 15; ++i) {
      auto s = out.back();
      s.tSeconds = (15 + i) * dt;
      s.primaryId = 2;
      s.newContact = i == 0;
      s.normalized.x = 0.45 + i * 0.01;
      s.mm.x += 1.0;
      out.push_back(s);
    }
  } else if (kind == "two") {
    for (int i = 0; i < 30; ++i) {
      push(i * dt, 0.3, 0.4 + i * 0.005, true, 2, i == 0);
    }
  } else if (kind == "1to2") {
    for (int i = 0; i < 15; ++i) push(i * dt, 0.3 + i * 0.01, 0.5, true, 1, i == 0);
    for (int i = 0; i < 15; ++i) push((15 + i) * dt, 0.45, 0.5, true, 2, false);
  } else if (kind == "2to1") {
    for (int i = 0; i < 15; ++i) push(i * dt, 0.4, 0.5, true, 2, i == 0);
    for (int i = 0; i < 15; ++i) push((15 + i) * dt, 0.4 + i * 0.01, 0.5, true, 1, i == 0);
  } else if (kind == "zerodt") {
    for (int i = 0; i < 10; ++i) {
      push(0.5, 0.4 + i * 0.01, 0.5, true, 1, i == 0);
      out.back().dt = 0.0;
    }
  } else if (kind == "largedt") {
    for (int i = 0; i < 8; ++i) {
      push(i * 0.2, 0.2 + i * 0.05, 0.5, true, 1, i == 0);
      out.back().dt = 0.2;
    }
  } else if (kind == "jitter") {
    for (int i = 0; i < 40; ++i) {
      const double j = ((i * 37) % 5 - 2) * 0.0004;
      push(i * dt * (1.0 + j * 10.0), 0.4 + i * 0.004, 0.5 + j, true, 1, i == 0);
    }
  } else if (kind == "edge") {
    for (int i = 0; i < 20; ++i) {
      push(i * dt, std::min(1.0, 0.9 + i * 0.01), 0.01, true, 1, i == 0);
    }
  } else {
    for (int i = 0; i < 20; ++i) push(i * dt, 0.3 + i * 0.01, 0.5, true, 1, i == 0);
  }
  return out;
}

}  // namespace smoothpad::motion
