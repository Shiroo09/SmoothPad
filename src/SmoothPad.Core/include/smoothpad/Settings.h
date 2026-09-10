#pragma once

#include "Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace smoothpad {

struct MotionUiParams {
  double pointerSpeed = 1.0;
  double acceleration = 0.5;
  double smoothness = 0.3;
  double precision = 0.5;
};

struct AccelerationParams {
  double lowGain = 0.55;
  double midGain = 1.0;
  double maxGain = 3.2;
  double curveStartMmS = 18.0;
  double curveMidMmS = 140.0;
  double curveEndMmS = 520.0;
  double shape = 0.55;
};

struct SmoothingParams {
  SmootherKind algorithm = SmootherKind::OneEuro;
  double strength = 0.35;
  double minCutoff = 1.2;
  double beta = 0.007;
  double dCutoff = 1.0;
};

struct NoiseParams {
  bool adaptive = true;
  double floorMm = 0.012;
  double thresholdMm = 0.035;
};

struct ExperimentalParams {
  bool prediction = false;
  double predictionMs = 0.0;
  bool resampling = false;
};

struct HotkeyParams {
  bool enabled = true;
  unsigned modifiers = 0x0003;  // MOD_ALT | MOD_CONTROL
  unsigned vk = 0x4D;           // 'M'
  bool emergencyEnabled = true;
  unsigned emergencyModifiers = 0x0007;  // SHIFT|CTRL|ALT
  unsigned emergencyVk = 0x4D;
};

struct DeviceSettings {
  std::wstring id = L"auto";
};

struct Settings {
  int schemaVersion = 1;
  bool enabled = true;
  ProfileId profile = ProfileId::MacBalanced;
  DeviceSettings device;
  MotionUiParams motion;
  AccelerationParams acceleration;
  SmoothingParams smoothing;
  NoiseParams noise;
  ExperimentalParams experimental;
  HotkeyParams hotkey;
  ExternalMousePolicy externalMouse = ExternalMousePolicy::DoNothing;
  bool startWithWindows = false;
  bool showTray = true;
};

struct NamedProfile {
  std::string id;
  std::string name;
  bool builtin = false;
  MotionUiParams motion;
  AccelerationParams acceleration;
  SmoothingParams smoothing;
  NoiseParams noise;
  ExperimentalParams experimental;
};

[[nodiscard]] Settings DefaultSettings();
[[nodiscard]] NamedProfile BuiltinProfile(ProfileId id);
[[nodiscard]] std::vector<NamedProfile> BuiltinProfiles();

[[nodiscard]] std::string SettingsToJson(const Settings& s);
[[nodiscard]] bool SettingsFromJson(std::string_view json, Settings& out, std::string* error);
[[nodiscard]] std::string ProfileToJson(const NamedProfile& p);
[[nodiscard]] bool ProfileFromJson(std::string_view json, NamedProfile& out, std::string* error);

bool SaveSettingsAtomic(const std::filesystem::path& path, const Settings& s, std::string* error);
bool LoadSettings(const std::filesystem::path& path, Settings& s, std::string* error);

[[nodiscard]] std::filesystem::path DefaultSettingsPath();
[[nodiscard]] std::filesystem::path DefaultProfilesDir();

AccelerationParams BlendAcceleration(const AccelerationParams& base, const MotionUiParams& ui);
SmoothingParams BlendSmoothing(const SmoothingParams& base, const MotionUiParams& ui);
NoiseParams BlendNoise(const NoiseParams& base, const MotionUiParams& ui);

}  // namespace smoothpad
