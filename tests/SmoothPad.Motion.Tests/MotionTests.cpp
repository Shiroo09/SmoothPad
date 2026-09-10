#include "Test.h"
#include "smoothpad/Math.h"
#include "smoothpad/motion/MotionEngine.h"

#include <cmath>

using namespace smoothpad;
using namespace smoothpad::motion;

static MotionEngine MakeEngine() {
  MotionEngine e;
  EngineConfig cfg;
  cfg.caps = SyntheticCaps();
  cfg.settings = DefaultSettings();
  e.Configure(cfg);
  return e;
}

static std::vector<MotionOutput> RunKind(std::string_view kind) {
  auto e = MakeEngine();
  const auto caps = SyntheticCaps();
  auto samples = SynthesizeTrace(kind, caps, 125.0);
  std::vector<MotionOutput> out;
  for (auto& s : samples) {
    RawContactFrame f{};
    f.qpc = static_cast<std::uint64_t>(s.tSeconds * 10'000'000.0);
    f.contactCount = s.contactCount;
    if (s.tip && s.contactCount >= 1) {
      f.contacts[0].valid = true;
      f.contacts[0].tip = true;
      f.contacts[0].id = s.primaryId;
      f.contacts[0].xLogical = static_cast<int>(s.logical.x);
      f.contacts[0].yLogical = static_cast<int>(s.logical.y);
    }
    if (s.contactCount >= 2) {
      f.contacts[1] = f.contacts[0];
      f.contacts[1].id = 2;
      f.contacts[1].xLogical += 200;
    }
    out.push_back(e.Process(f));
  }
  return out;
}

SP_TEST(CoordinateNormalizerTests_Range) {
  CoordinateNormalizer n;
  n.Configure(SyntheticCaps());
  const auto a = n.ToNormalized(0, 0);
  const auto b = n.ToNormalized(4095, 2730);
  SP_CHECK(a.x >= 0.0 && a.x <= 0.001);
  SP_CHECK(b.x > 0.99);
  const auto mm = n.ToMm(4095, 0);
  SP_CHECK(mm.x > 90.0);
}

SP_TEST(ContactTrackerTests_NewContactNoJump) {
  auto e = MakeEngine();
  RawContactFrame a{};
  a.qpc = 10'000'000;
  a.contactCount = 1;
  a.contacts[0] = {1, true, true, true, 1000, 1000, {}, {}, {}};
  auto o1 = e.Process(a);
  SP_CHECK(!o1.emit || (o1.deltaMm.x == 0 && o1.deltaMm.y == 0));
  RawContactFrame b = a;
  b.qpc = 10'080'000;
  b.contacts[0].id = 2;
  b.contacts[0].xLogical = 1800;
  auto o2 = e.Process(b);
  SP_CHECK(o2.deltaMm.length() < 0.001);
}

SP_TEST(VelocityEstimatorTests_Constant) {
  VelocityEstimator v;
  for (int i = 0; i < 10; ++i) {
    v.Push(i * 0.008, {i * 0.4, 0.0});
  }
  SP_CHECK(std::abs(v.SpeedMmS() - 50.0) < 8.0);
}

SP_TEST(AccelerationCurveTests_Monotonic) {
  AccelerationCurve c;
  c.Configure(BuiltinProfile(ProfileId::MacBalanced).acceleration);
  double prev = 0.0;
  for (double s = 0; s <= 600; s += 10) {
    const double g = c.Gain(s);
    SP_CHECK(g + 1e-9 >= prev);
    prev = g;
  }
  SP_CHECK(c.Gain(0) < c.Gain(200));
  SP_CHECK(c.Gain(2000) <= BuiltinProfile(ProfileId::MacBalanced).acceleration.maxGain + 1e-9);
}

SP_TEST(AccelerationCurveTests_NoDiscontinuity) {
  AccelerationCurve c;
  auto p = BuiltinProfile(ProfileId::MacBalanced).acceleration;
  c.Configure(p);
  const double g0 = c.Gain(p.curveMidMmS - 0.5);
  const double g1 = c.Gain(p.curveMidMmS + 0.5);
  SP_CHECK(std::abs(g1 - g0) < 0.15);
}

SP_TEST(AdaptiveSmootherTests_FastReducesLag) {
  AdaptiveSmoother s;
  s.Configure(BuiltinProfile(ProfileId::MacBalanced).smoothing);
  Vec2 slow = s.Filter({1, 0}, 0.008, 10.0);
  s.Reset();
  s.Configure(BuiltinProfile(ProfileId::MacBalanced).smoothing);
  Vec2 fast = s.Filter({1, 0}, 0.008, 500.0);
  SP_CHECK(fast.x + 1e-9 >= slow.x);
}

SP_TEST(FractionalAccumulatorTests_Remainder) {
  FractionalAccumulator a;
  auto o1 = a.Consume({0.36, 0.21});
  SP_CHECK(o1.x == 0 && o1.y == 0);
  auto o2 = a.Consume({0.70, 0.90});
  SP_CHECK(o2.x == 1);
  SP_CHECK(o2.y == 1);
  SP_CHECK(std::abs(a.Remainder().x - 0.06) < 1e-9);
}

SP_TEST(StateMachineTests_IdleToSingle) {
  const auto out = RunKind("slow");
  SP_CHECK(!out.empty());
  bool sawPointer = false;
  for (const auto& o : out) {
    if (o.state == ContactState::SinglePointer || o.pointerActive) sawPointer = true;
  }
  SP_CHECK(sawPointer);
}

SP_TEST(Motion_StationaryNoDrift) {
  const auto out = RunKind("stationary");
  double acc = 0;
  for (const auto& o : out) acc += std::abs(o.deltaMm.x) + std::abs(o.deltaMm.y);
  SP_CHECK(acc < 0.5);
}

SP_TEST(Motion_MicroRetained) {
  const auto out = RunKind("micro");
  double acc = 0;
  for (const auto& o : out) acc += o.deltaMm.length();
  SP_CHECK(acc > 0.0);
}

SP_TEST(Motion_FastHasHigherGainThanSlow) {
  AccelerationCurve c;
  c.Configure(DefaultSettings().acceleration);
  SP_CHECK(c.Gain(400) > c.Gain(20) * 1.2);
}

SP_TEST(Motion_FingerLiftStops) {
  const auto out = RunKind("lift");
  SP_CHECK(out.back().pointerActive == false);
  SP_CHECK(out.back().deltaMm.length() < 1e-9);
}

SP_TEST(Motion_NoInertia) {
  auto e = MakeEngine();
  RawContactFrame f{};
  f.qpc = 10'000'000;
  f.contactCount = 1;
  f.contacts[0] = {1, true, true, true, 500, 500, {}, {}, {}};
  (void)e.Process(f);
  f.qpc += 80'000;
  f.contacts[0].xLogical = 900;
  auto moving = e.Process(f);
  SP_CHECK(moving.deltaMm.length() > 0.0 || moving.gain > 0.0);
  f.qpc += 80'000;
  auto still = e.Process(f);
  SP_CHECK(still.deltaMm.length() < 1e-6);
}

SP_TEST(Motion_DiagonalNoAxisSplit) {
  AccelerationCurve c;
  c.Configure(DefaultSettings().acceleration);
  const double g = c.Gain(200);
  Vec2 in{3, 4};
  Vec2 out = in * g;
  const double inA = std::atan2(in.y, in.x);
  const double outA = std::atan2(out.y, out.x);
  SP_CHECK(std::abs(inA - outA) < 1e-9);
}

SP_TEST(Motion_1to2StopsPointer) {
  const auto out = RunKind("1to2");
  SP_CHECK(out.back().pointerActive == false);
}

SP_TEST(Motion_2to1NoJump) {
  const auto out = RunKind("2to1");
  bool firstSingle = true;
  for (const auto& o : out) {
    if (o.pointerActive && firstSingle) {
      SP_CHECK(o.deltaMm.length() < 0.001 || !o.emit);
      firstSingle = false;
    }
  }
}

SP_TEST(Motion_ZeroDtSafe) {
  const auto out = RunKind("zerodt");
  SP_CHECK(!out.empty());
}

SP_TEST(Motion_LargeDtSafe) {
  const auto out = RunKind("largedt");
  SP_CHECK(!out.empty());
}

SP_TEST(Motion_ReversalFinite) {
  const auto out = RunKind("reversal");
  for (const auto& o : out) {
    SP_CHECK(std::isfinite(o.deltaMm.x));
    SP_CHECK(std::isfinite(o.gain));
  }
}

SP_TEST(Motion_CircleBounded) {
  const auto out = RunKind("circle");
  double maxG = 0;
  for (const auto& o : out) maxG = std::max(maxG, o.gain);
  SP_CHECK(maxG < 20.0);
}

SP_TEST(TraceReplayTests_Deterministic) {
  auto a = RunKind("medium");
  auto b = RunKind("medium");
  SP_CHECK(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    SP_CHECK(std::abs(a[i].gain - b[i].gain) < 1e-12);
    SP_CHECK(std::abs(a[i].deltaMm.x - b[i].deltaMm.x) < 1e-12);
  }
}

SP_TEST(WindowsNative_IsBypassProfile) {
  auto p = BuiltinProfile(ProfileId::WindowsNative);
  SP_CHECK(p.smoothing.strength == 0.0);
}
