#include "smoothpad/Settings.h"

#include "smoothpad/Json.h"
#include "smoothpad/Math.h"
#include "smoothpad/platform/Win32.h"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <ShlObj.h>
#include <Windows.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace smoothpad {
namespace {

json::Value MotionToJson(const MotionUiParams& m) {
  json::Object o;
  o["pointerSpeed"] = m.pointerSpeed;
  o["acceleration"] = m.acceleration;
  o["smoothness"] = m.smoothness;
  o["precision"] = m.precision;
  return json::Value{std::move(o)};
}

MotionUiParams MotionFromJson(const json::Value* v) {
  MotionUiParams m;
  if (!v) return m;
  if (const auto* x = v->get("pointerSpeed")) m.pointerSpeed = Clamp(x->as_number(1.0), 0.1, 3.0);
  if (const auto* x = v->get("acceleration")) m.acceleration = Clamp(x->as_number(0.5), 0.0, 1.0);
  if (const auto* x = v->get("smoothness")) m.smoothness = Clamp(x->as_number(0.3), 0.0, 1.0);
  if (const auto* x = v->get("precision")) m.precision = Clamp(x->as_number(0.5), 0.0, 1.0);
  return m;
}

json::Value AccelToJson(const AccelerationParams& a) {
  json::Object o;
  o["lowGain"] = a.lowGain;
  o["midGain"] = a.midGain;
  o["maxGain"] = a.maxGain;
  o["curveStartMmS"] = a.curveStartMmS;
  o["curveMidMmS"] = a.curveMidMmS;
  o["curveEndMmS"] = a.curveEndMmS;
  o["shape"] = a.shape;
  return json::Value{std::move(o)};
}

AccelerationParams AccelFromJson(const json::Value* v, AccelerationParams a) {
  if (!v) return a;
  if (const auto* x = v->get("lowGain")) a.lowGain = Clamp(x->as_number(a.lowGain), 0.05, 4.0);
  if (const auto* x = v->get("midGain")) a.midGain = Clamp(x->as_number(a.midGain), 0.1, 8.0);
  if (const auto* x = v->get("maxGain")) a.maxGain = Clamp(x->as_number(a.maxGain), 0.2, 12.0);
  if (const auto* x = v->get("curveStartMmS")) a.curveStartMmS = Clamp(x->as_number(a.curveStartMmS), 1.0, 200.0);
  if (const auto* x = v->get("curveMidMmS")) a.curveMidMmS = Clamp(x->as_number(a.curveMidMmS), 10.0, 800.0);
  if (const auto* x = v->get("curveEndMmS")) a.curveEndMmS = Clamp(x->as_number(a.curveEndMmS), 50.0, 2000.0);
  if (const auto* x = v->get("shape")) a.shape = Clamp(x->as_number(a.shape), 0.05, 1.0);
  return a;
}

json::Value SmoothToJson(const SmoothingParams& s) {
  json::Object o;
  o["algorithm"] = s.algorithm == SmootherKind::AdaptiveEma       ? "adaptive-ema"
                   : s.algorithm == SmootherKind::VelocityEma     ? "velocity-ema"
                   : s.algorithm == SmootherKind::CriticallyDamped ? "critically-damped"
                                                                   : "one-euro";
  o["strength"] = s.strength;
  o["minCutoff"] = s.minCutoff;
  o["beta"] = s.beta;
  o["dCutoff"] = s.dCutoff;
  return json::Value{std::move(o)};
}

SmoothingParams SmoothFromJson(const json::Value* v, SmoothingParams s) {
  if (!v) return s;
  if (const auto* x = v->get("algorithm")) {
    const auto a = x->as_string();
    if (a == "adaptive-ema") s.algorithm = SmootherKind::AdaptiveEma;
    else if (a == "velocity-ema") s.algorithm = SmootherKind::VelocityEma;
    else if (a == "critically-damped") s.algorithm = SmootherKind::CriticallyDamped;
    else s.algorithm = SmootherKind::OneEuro;
  }
  if (const auto* x = v->get("strength")) s.strength = Clamp(x->as_number(s.strength), 0.0, 1.0);
  if (const auto* x = v->get("minCutoff")) s.minCutoff = Clamp(x->as_number(s.minCutoff), 0.1, 20.0);
  if (const auto* x = v->get("beta")) s.beta = Clamp(x->as_number(s.beta), 0.0, 1.0);
  if (const auto* x = v->get("dCutoff")) s.dCutoff = Clamp(x->as_number(s.dCutoff), 0.1, 20.0);
  return s;
}

json::Value NoiseToJson(const NoiseParams& n) {
  json::Object o;
  o["adaptive"] = n.adaptive;
  o["floorMm"] = n.floorMm;
  o["thresholdMm"] = n.thresholdMm;
  return json::Value{std::move(o)};
}

NoiseParams NoiseFromJson(const json::Value* v, NoiseParams n) {
  if (!v) return n;
  if (const auto* x = v->get("adaptive")) n.adaptive = x->as_bool(n.adaptive);
  if (const auto* x = v->get("floorMm")) n.floorMm = Clamp(x->as_number(n.floorMm), 0.0, 1.0);
  if (const auto* x = v->get("thresholdMm")) n.thresholdMm = Clamp(x->as_number(n.thresholdMm), 0.0, 2.0);
  return n;
}

json::Value ExpToJson(const ExperimentalParams& e) {
  json::Object o;
  o["prediction"] = e.prediction;
  o["predictionMs"] = e.predictionMs;
  o["resampling"] = e.resampling;
  return json::Value{std::move(o)};
}

ExperimentalParams ExpFromJson(const json::Value* v, ExperimentalParams e) {
  if (!v) return e;
  if (const auto* x = v->get("prediction")) e.prediction = x->as_bool(false);
  if (const auto* x = v->get("predictionMs")) e.predictionMs = Clamp(x->as_number(0.0), 0.0, 4.0);
  if (const auto* x = v->get("resampling")) e.resampling = x->as_bool(false);
  return e;
}

bool WriteFileAtomic(const std::filesystem::path& path, std::string_view contents, std::string* error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  const auto tmp = path.wstring() + L".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      if (error) *error = "failed to open temp settings file";
      return false;
    }
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!out) {
      if (error) *error = "failed to write settings";
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    std::filesystem::rename(tmp, path, ec);
  }
  if (ec) {
    if (error) *error = ec.message();
    return false;
  }
  return true;
}

}  // namespace

Settings DefaultSettings() {
  Settings s;
  const auto p = BuiltinProfile(ProfileId::MacBalanced);
  s.motion = p.motion;
  s.acceleration = p.acceleration;
  s.smoothing = p.smoothing;
  s.noise = p.noise;
  s.experimental = p.experimental;
  return s;
}

NamedProfile BuiltinProfile(ProfileId id) {
  NamedProfile p;
  p.builtin = true;
  p.motion = {};
  p.experimental = {};
  switch (id) {
    case ProfileId::MacSmooth:
      p.id = "mac-smooth";
      p.name = "Mac Smooth";
      p.motion.smoothness = 0.48;
      p.motion.precision = 0.62;
      p.motion.acceleration = 0.45;
      p.acceleration = {0.50, 0.95, 2.8, 16.0, 130.0, 500.0, 0.50};
      p.smoothing = {SmootherKind::OneEuro, 0.48, 0.95, 0.006, 1.0};
      p.noise = {true, 0.016, 0.042};
      break;
    case ProfileId::MacFast:
      p.id = "mac-fast";
      p.name = "Mac Fast";
      p.motion.acceleration = 0.72;
      p.motion.pointerSpeed = 1.08;
      p.motion.smoothness = 0.22;
      p.acceleration = {0.58, 1.15, 4.2, 20.0, 120.0, 480.0, 0.62};
      p.smoothing = {SmootherKind::OneEuro, 0.22, 1.6, 0.010, 1.0};
      p.noise = {true, 0.010, 0.028};
      break;
    case ProfileId::WindowsNative:
      p.id = "windows-native";
      p.name = "Windows Native";
      p.motion = {1.0, 0.0, 0.0, 0.0};
      p.acceleration = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5};
      p.smoothing = {SmootherKind::OneEuro, 0.0, 10.0, 0.0, 10.0};
      p.noise = {false, 0.0, 0.0};
      break;
    case ProfileId::Custom:
      p.id = "custom";
      p.name = "Custom";
      p.builtin = false;
      p.acceleration = {0.55, 1.0, 3.2, 18.0, 140.0, 520.0, 0.55};
      p.smoothing = {SmootherKind::OneEuro, 0.35, 1.2, 0.007, 1.0};
      p.noise = {true, 0.012, 0.035};
      break;
    case ProfileId::MacBalanced:
    default:
      p.id = "mac-balanced";
      p.name = "Mac Balanced";
      p.acceleration = {0.55, 1.0, 3.2, 18.0, 140.0, 520.0, 0.55};
      p.smoothing = {SmootherKind::OneEuro, 0.35, 1.2, 0.007, 1.0};
      p.noise = {true, 0.012, 0.035};
      break;
  }
  return p;
}

std::vector<NamedProfile> BuiltinProfiles() {
  return {
      BuiltinProfile(ProfileId::MacBalanced),
      BuiltinProfile(ProfileId::MacSmooth),
      BuiltinProfile(ProfileId::MacFast),
      BuiltinProfile(ProfileId::WindowsNative),
  };
}

std::string SettingsToJson(const Settings& s) {
  json::Object root;
  root["schemaVersion"] = s.schemaVersion;
  root["enabled"] = s.enabled;
  root["profile"] = ProfileIdToString(s.profile);
  root["startWithWindows"] = s.startWithWindows;
  root["showTray"] = s.showTray;
  json::Object device;
  device["id"] = platform::WideToUtf8(s.device.id);
  root["device"] = json::Value{std::move(device)};
  root["motion"] = MotionToJson(s.motion);
  json::Object advanced;
  advanced["acceleration"] = AccelToJson(s.acceleration);
  advanced["smoothing"] = SmoothToJson(s.smoothing);
  advanced["noise"] = NoiseToJson(s.noise);
  advanced["experimental"] = ExpToJson(s.experimental);
  json::Object hotkey;
  hotkey["enabled"] = s.hotkey.enabled;
  hotkey["modifiers"] = static_cast<double>(s.hotkey.modifiers);
  hotkey["vk"] = static_cast<double>(s.hotkey.vk);
  advanced["hotkey"] = json::Value{std::move(hotkey)};
  const char* mouse = "do-nothing";
  if (s.externalMouse == ExternalMousePolicy::KeepSmoothPad) mouse = "keep";
  else if (s.externalMouse == ExternalMousePolicy::UseNativeTouchpad) mouse = "native";
  else if (s.externalMouse == ExternalMousePolicy::DisableTouchpad) mouse = "disable-touchpad";
  advanced["externalMouse"] = mouse;
  root["advanced"] = json::Value{std::move(advanced)};
  return json::Stringify(json::Value{std::move(root)}, true);
}

bool SettingsFromJson(std::string_view jsonText, Settings& out, std::string* error) {
  auto parsed = json::Parse(jsonText, error);
  if (!parsed || !parsed->is_object()) {
    if (error && error->empty()) *error = "settings JSON is not an object";
    return false;
  }
  Settings s = DefaultSettings();
  if (const auto* v = parsed->get("schemaVersion")) s.schemaVersion = v->as_int(1);
  if (s.schemaVersion < 1 || s.schemaVersion > 1) {
    // Forward-compatible: accept unknown future keys, keep v1 fields.
  }
  if (const auto* v = parsed->get("enabled")) s.enabled = v->as_bool(true);
  if (const auto* v = parsed->get("profile")) s.profile = ProfileIdFromString(v->as_string());
  if (const auto* v = parsed->get("startWithWindows")) s.startWithWindows = v->as_bool(false);
  if (const auto* v = parsed->get("showTray")) s.showTray = v->as_bool(true);
  if (const auto* d = parsed->get("device")) {
    if (const auto* id = d->get("id")) {
      s.device.id = platform::Utf8ToWide(id->as_string());
    }
  }
  s.motion = MotionFromJson(parsed->get("motion"));
  const auto builtin = BuiltinProfile(s.profile);
  s.acceleration = builtin.acceleration;
  s.smoothing = builtin.smoothing;
  s.noise = builtin.noise;
  if (const auto* adv = parsed->get("advanced")) {
    s.acceleration = AccelFromJson(adv->get("acceleration"), s.acceleration);
    s.smoothing = SmoothFromJson(adv->get("smoothing"), s.smoothing);
    s.noise = NoiseFromJson(adv->get("noise"), s.noise);
    s.experimental = ExpFromJson(adv->get("experimental"), s.experimental);
    if (const auto* h = adv->get("hotkey")) {
      if (const auto* x = h->get("enabled")) s.hotkey.enabled = x->as_bool(true);
      if (const auto* x = h->get("modifiers")) s.hotkey.modifiers = static_cast<unsigned>(x->as_number(3));
      if (const auto* x = h->get("vk")) s.hotkey.vk = static_cast<unsigned>(x->as_number(0x4D));
    }
    if (const auto* m = adv->get("externalMouse")) {
      const auto pol = m->as_string();
      if (pol == "keep") s.externalMouse = ExternalMousePolicy::KeepSmoothPad;
      else if (pol == "native") s.externalMouse = ExternalMousePolicy::UseNativeTouchpad;
      else if (pol == "disable-touchpad") s.externalMouse = ExternalMousePolicy::DisableTouchpad;
      else s.externalMouse = ExternalMousePolicy::DoNothing;
    }
  }
  out = std::move(s);
  return true;
}

std::string ProfileToJson(const NamedProfile& p) {
  json::Object o;
  o["id"] = p.id;
  o["name"] = p.name;
  o["builtin"] = p.builtin;
  o["motion"] = MotionToJson(p.motion);
  o["acceleration"] = AccelToJson(p.acceleration);
  o["smoothing"] = SmoothToJson(p.smoothing);
  o["noise"] = NoiseToJson(p.noise);
  o["experimental"] = ExpToJson(p.experimental);
  return json::Stringify(json::Value{std::move(o)}, true);
}

bool ProfileFromJson(std::string_view jsonText, NamedProfile& out, std::string* error) {
  auto parsed = json::Parse(jsonText, error);
  if (!parsed || !parsed->is_object()) {
    if (error && error->empty()) *error = "profile JSON is not an object";
    return false;
  }
  NamedProfile p = BuiltinProfile(ProfileId::Custom);
  p.builtin = false;
  if (const auto* v = parsed->get("id")) p.id = v->as_string();
  if (const auto* v = parsed->get("name")) p.name = v->as_string();
  if (p.id.empty() || p.name.empty()) {
    if (error) *error = "profile requires id and name";
    return false;
  }
  p.motion = MotionFromJson(parsed->get("motion"));
  p.acceleration = AccelFromJson(parsed->get("acceleration"), p.acceleration);
  p.smoothing = SmoothFromJson(parsed->get("smoothing"), p.smoothing);
  p.noise = NoiseFromJson(parsed->get("noise"), p.noise);
  p.experimental = ExpFromJson(parsed->get("experimental"), p.experimental);
  out = std::move(p);
  return true;
}

bool SaveSettingsAtomic(const std::filesystem::path& path, const Settings& s, std::string* error) {
  return WriteFileAtomic(path, SettingsToJson(s), error);
}

bool LoadSettings(const std::filesystem::path& path, Settings& s, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "settings file not found";
    s = DefaultSettings();
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  if (!SettingsFromJson(ss.str(), s, error)) {
    s = DefaultSettings();
    return false;
  }
  return true;
}

std::filesystem::path DefaultSettingsPath() {
#ifdef _WIN32
  PWSTR appdata = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata)) && appdata) {
    std::filesystem::path p(appdata);
    CoTaskMemFree(appdata);
    return p / L"SmoothPad" / L"settings.json";
  }
#endif
  return std::filesystem::current_path() / "settings.json";
}

std::filesystem::path DefaultProfilesDir() {
  return DefaultSettingsPath().parent_path() / L"profiles";
}

AccelerationParams BlendAcceleration(const AccelerationParams& base, const MotionUiParams& ui) {
  AccelerationParams a = base;
  const double acc = Clamp(ui.acceleration, 0.0, 1.0);
  const double speed = Clamp(ui.pointerSpeed, 0.1, 3.0);
  a.lowGain *= speed * Lerp(1.05, 0.92, ui.precision);
  a.midGain *= speed * Lerp(0.85, 1.25, acc);
  a.maxGain *= speed * Lerp(0.70, 1.45, acc);
  a.curveStartMmS *= Lerp(1.15, 0.85, acc);
  a.curveEndMmS *= Lerp(1.10, 0.90, acc);
  a.shape = Clamp(Lerp(0.40, 0.75, acc), 0.05, 1.0);
  return a;
}

SmoothingParams BlendSmoothing(const SmoothingParams& base, const MotionUiParams& ui) {
  SmoothingParams s = base;
  s.strength = Clamp(ui.smoothness, 0.0, 1.0);
  s.minCutoff = Lerp(2.4, 0.7, s.strength);
  s.beta = Lerp(0.012, 0.003, s.strength);
  return s;
}

NoiseParams BlendNoise(const NoiseParams& base, const MotionUiParams& ui) {
  NoiseParams n = base;
  n.thresholdMm = Lerp(0.018, 0.055, Clamp(ui.precision, 0.0, 1.0));
  n.floorMm = n.thresholdMm * 0.35;
  return n;
}

}  // namespace smoothpad
