#include "Test.h"
#include "smoothpad/Json.h"
#include "smoothpad/Settings.h"
#include "smoothpad/Trace.h"
#include "smoothpad/motion/MotionEngine.h"

#include <cmath>
#include <filesystem>

using namespace smoothpad;

SP_TEST(SettingsTests_RoundTrip) {
  Settings s = DefaultSettings();
  s.enabled = true;
  s.profile = ProfileId::MacFast;
  s.motion.pointerSpeed = 1.25;
  s.motion.acceleration = 0.7;
  const auto json = SettingsToJson(s);
  Settings out;
  std::string err;
  SP_CHECK(SettingsFromJson(json, out, &err));
  SP_CHECK(out.profile == ProfileId::MacFast);
  SP_CHECK(std::abs(out.motion.pointerSpeed - 1.25) < 1e-9);
  SP_CHECK(std::abs(out.motion.acceleration - 0.7) < 1e-9);
}

SP_TEST(SettingsTests_CorruptFallsBack) {
  Settings out;
  std::string err;
  SP_CHECK(!SettingsFromJson("{not json", out, &err));
  SP_CHECK(!err.empty());
}

SP_TEST(SettingsTests_AtomicWrite) {
  const auto dir = std::filesystem::temp_directory_path() / "smoothpad-test";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto path = dir / "settings.json";
  Settings s = DefaultSettings();
  s.motion.smoothness = 0.42;
  std::string err;
  SP_CHECK(SaveSettingsAtomic(path, s, &err));
  Settings loaded;
  SP_CHECK(LoadSettings(path, loaded, &err));
  SP_CHECK(std::abs(loaded.motion.smoothness - 0.42) < 1e-9);
}

SP_TEST(ProfileSerializationTests_Custom) {
  NamedProfile p = BuiltinProfile(ProfileId::Custom);
  p.id = "mine";
  p.name = "Mine";
  p.motion.pointerSpeed = 1.1;
  const auto json = ProfileToJson(p);
  NamedProfile out;
  std::string err;
  SP_CHECK(ProfileFromJson(json, out, &err));
  SP_CHECK(out.id == "mine");
  SP_CHECK(out.name == "Mine");
  SP_CHECK(!out.builtin);
}

SP_TEST(ProfileSerializationTests_RejectsEmpty) {
  NamedProfile out;
  std::string err;
  SP_CHECK(!ProfileFromJson("{\"id\":\"\"}", out, &err));
}

SP_TEST(JsonTests_Nested) {
  std::string err;
  auto v = json::Parse("{\"a\":[1,true,null],\"b\":\"x\"}", &err);
  SP_CHECK(v.has_value());
  SP_CHECK(v->get("b")->as_string() == "x");
}

SP_TEST(TraceRoundTrip) {
  const auto caps = motion::SyntheticCaps();
  TraceWriter w;
  const auto path = std::filesystem::temp_directory_path() / "smoothpad-test" / "t.sptrace";
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  SP_CHECK(w.Open(path, caps));
  RawContactFrame f{};
  f.qpc = 123;
  f.contactCount = 1;
  f.contacts[0] = Contact{1, true, true, true, 100, 200, std::nullopt, std::nullopt, std::nullopt};
  SP_CHECK(w.Write(f));
  SP_CHECK(w.Close());
  TraceReader r;
  std::string err;
  SP_CHECK(r.Open(path, &err));
  SP_CHECK(r.Frames().size() == 1);
  SP_CHECK(r.Frames()[0].contacts[0].x == 100);
  const auto json = TraceToJson(r);
  SP_CHECK(json.find("frameCount") != std::string::npos);
  const auto csv = TraceToCsv(r);
  SP_CHECK(csv.find("qpc") != std::string::npos);
}
