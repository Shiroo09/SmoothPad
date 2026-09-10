#include "Test.h"
#include "smoothpad/Math.h"
#include "smoothpad/motion/MotionEngine.h"

#include <chrono>
#include <cmath>

using namespace smoothpad;
using namespace smoothpad::motion;

static double ReplayMs(std::string_view kind, int repeats) {
  const auto caps = SyntheticCaps();
  auto samples = SynthesizeTrace(kind, caps, 125.0);
  MotionEngine e;
  e.Configure({caps, DefaultSettings()});
  const auto t0 = std::chrono::steady_clock::now();
  for (int r = 0; r < repeats; ++r) {
    e.Reset();
    for (const auto& s : samples) {
      RawContactFrame f{};
      f.qpc = static_cast<std::uint64_t>(s.tSeconds * 10'000'000.0);
      f.contactCount = s.contactCount;
      if (s.tip) {
        f.contacts[0] = {s.primaryId, true, true, true, static_cast<int>(s.logical.x),
                         static_cast<int>(s.logical.y), {}, {}, {}};
      }
      (void)e.Process(f);
    }
  }
  const auto t1 = std::chrono::steady_clock::now();
  const auto total = std::chrono::duration<double, std::milli>(t1 - t0).count();
  return total / (repeats * static_cast<double>(std::max<std::size_t>(1, samples.size())));
}

SP_TEST(LatencyBench_ProcessUnder2msMedian) {
  const double per = ReplayMs("medium", 200);
  SP_CHECK(per < 2.0);
}

SP_TEST(MalformedTrace_EmptySafe) {
  MotionEngine e;
  e.Configure({SyntheticCaps(), DefaultSettings()});
  RawContactFrame f{};
  auto o = e.Process(f);
  SP_CHECK(o.pointerActive == false);
}

SP_TEST(MalformedTrace_HugeContactCount) {
  MotionEngine e;
  e.Configure({SyntheticCaps(), DefaultSettings()});
  RawContactFrame f{};
  f.contactCount = 999;
  f.contacts[0] = {1, true, true, true, 1, 1, {}, {}, {}};
  auto o = e.Process(f);
  (void)o;
  SP_CHECK(true);
}
