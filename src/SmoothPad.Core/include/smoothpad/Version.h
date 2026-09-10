#pragma once

#define SMOOTHPAD_VERSION_MAJOR 0
#define SMOOTHPAD_VERSION_MINOR 1
#define SMOOTHPAD_VERSION_PATCH 0
#define SMOOTHPAD_VERSION_STRING "0.1.0"

namespace smoothpad {

inline constexpr int kVersionMajor = SMOOTHPAD_VERSION_MAJOR;
inline constexpr int kVersionMinor = SMOOTHPAD_VERSION_MINOR;
inline constexpr int kVersionPatch = SMOOTHPAD_VERSION_PATCH;
inline constexpr const char* kVersionString = SMOOTHPAD_VERSION_STRING;
inline constexpr const wchar_t* kVersionStringW = L"0.1.0";

}  // namespace smoothpad
