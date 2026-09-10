#include "smoothpad/platform/Win32.h"

#include "smoothpad/Logging.h"

#ifdef _WIN32
#include <shellapi.h>
#endif

namespace smoothpad::platform {
namespace {

#ifdef _WIN32
constexpr UINT kSpiGetTouchpadParameters = 0x00AE;
constexpr UINT kSpiSetTouchpadParameters = 0x00AF;

struct TouchpadParametersV1 {
  UINT versionNumber;
  UINT maxSupportedContacts;
  UINT legacyTouchpadFeatures;
  UINT statusBits;
  UINT settingBits;
  UINT sensitivityLevel;
  UINT cursorSpeed;
  UINT feedbackIntensity;
  UINT clickForceSensitivity;
  UINT rightClickZoneWidth;
  UINT rightClickZoneHeight;
};

static_assert(sizeof(TouchpadParametersV1) >= 40, "layout sanity");
#endif

}  // namespace

std::uint64_t QpcNow() {
#ifdef _WIN32
  LARGE_INTEGER v{};
  QueryPerformanceCounter(&v);
  return static_cast<std::uint64_t>(v.QuadPart);
#else
  return 0;
#endif
}

double QpcToSeconds(std::uint64_t qpc) {
#ifdef _WIN32
  LARGE_INTEGER f{};
  QueryPerformanceFrequency(&f);
  if (f.QuadPart <= 0) return 0.0;
  return static_cast<double>(qpc) / static_cast<double>(f.QuadPart);
#else
  return static_cast<double>(qpc) * 1e-9;
#endif
}

double QpcToMilliseconds(std::uint64_t delta) {
  return QpcToSeconds(delta) * 1000.0;
}

bool QueryTouchpadParameters(bool* present, bool* enabled, bool* externalMouse, std::string* error) {
#ifdef _WIN32
  // Use documented SPI_GETTOUCHPADPARAMETERS with versioned struct from the SDK
  // when available. Fallback: zeroed probe.
  TOUCHPAD_PARAMETERS_V1 params{};
  params.versionNumber = TOUCHPAD_PARAMETERS_VERSION_1;
  if (!SystemParametersInfoW(kSpiGetTouchpadParameters, sizeof(params), &params, 0)) {
    if (error) *error = "SPI_GETTOUCHPADPARAMETERS failed";
    return false;
  }
  if (present) *present = params.touchpadPresent ? true : false;
  if (enabled) *enabled = params.touchpadEnabled ? true : false;
  if (externalMouse) *externalMouse = params.externalMousePresent ? true : false;
  return true;
#else
  (void)present;
  (void)enabled;
  (void)externalMouse;
  if (error) *error = "Windows only";
  return false;
#endif
}

bool SetTouchpadEnabled(bool enabled, std::string* error) {
#ifdef _WIN32
  TOUCHPAD_PARAMETERS_V1 params{};
  params.versionNumber = TOUCHPAD_PARAMETERS_VERSION_1;
  if (!SystemParametersInfoW(kSpiGetTouchpadParameters, sizeof(params), &params, 0)) {
    if (error) *error = "SPI_GETTOUCHPADPARAMETERS failed";
    return false;
  }
  params.touchpadEnabled = enabled ? TRUE : FALSE;
  if (!SystemParametersInfoW(kSpiSetTouchpadParameters, sizeof(params), &params, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    if (error) *error = "SPI_SETTOUCHPADPARAMETERS failed";
    return false;
  }
  return true;
#else
  (void)enabled;
  if (error) *error = "Windows only";
  return false;
#endif
}

bool SetStartWithWindows(bool enable, std::string* error) {
#ifdef _WIN32
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr,
                      0, KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    if (error) *error = "failed to open Run key";
    return false;
  }
  if (enable) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring cmd = L"\"";
    cmd += path;
    cmd += L"\" --tray";
    const auto st = RegSetValueExW(key, L"SmoothPad", 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                                   static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (st != ERROR_SUCCESS) {
      if (error) *error = "failed to write Run value";
      return false;
    }
  } else {
    RegDeleteValueW(key, L"SmoothPad");
    RegCloseKey(key);
  }
  return true;
#else
  (void)enable;
  if (error) *error = "Windows only";
  return false;
#endif
}

bool IsStartWithWindows() {
#ifdef _WIN32
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE,
                    &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD type = 0, size = 0;
  const auto st = RegQueryValueExW(key, L"SmoothPad", nullptr, &type, nullptr, &size);
  RegCloseKey(key);
  return st == ERROR_SUCCESS;
#else
  return false;
#endif
}

bool RegisterToggleHotkey(void* hwnd, unsigned modifiers, unsigned vk, int id) {
#ifdef _WIN32
  return RegisterHotKey(static_cast<HWND>(hwnd), id, modifiers, vk) != FALSE;
#else
  (void)hwnd;
  (void)modifiers;
  (void)vk;
  (void)id;
  return false;
#endif
}

void UnregisterHotkey(void* hwnd, int id) {
#ifdef _WIN32
  UnregisterHotKey(static_cast<HWND>(hwnd), id);
#else
  (void)hwnd;
  (void)id;
#endif
}

std::wstring Utf8ToWide(std::string_view s) {
#ifdef _WIN32
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
#else
  return std::wstring(s.begin(), s.end());
#endif
}

std::string WideToUtf8(std::wstring_view s) {
#ifdef _WIN32
  if (s.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
  return out;
#else
  std::string out;
  out.assign(s.begin(), s.end());
  return out;
#endif
}

}  // namespace smoothpad::platform
