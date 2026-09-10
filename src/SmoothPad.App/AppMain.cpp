#include "pch.h"

#include "smoothpad/EngineHost.h"
#include "smoothpad/Logging.h"
#include "smoothpad/Math.h"
#include "smoothpad/Version.h"
#include "smoothpad/platform/Win32.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace smoothpad;

namespace {

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_ENGINE_TICK = WM_APP + 2;
constexpr int HOTKEY_TOGGLE = 1;
constexpr int HOTKEY_EMERGENCY = 2;
constexpr UINT TRAY_ID = 1;

constexpr int IDC_TOGGLE = 1001;
constexpr int IDC_PROFILE = 1002;
constexpr int IDC_SPEED = 1003;
constexpr int IDC_ACCEL = 1004;
constexpr int IDC_SMOOTH = 1005;
constexpr int IDC_PREC = 1006;
constexpr int IDC_CALIBRATE = 1007;
constexpr int IDC_ADVANCED = 1008;
constexpr int IDC_STATUS = 1009;
constexpr int IDC_CURVE = 1010;
constexpr int IDC_STARTUP = 1011;
constexpr int IDC_NATIVE = 1012;

EngineHost* g_host = nullptr;
HWND g_main = nullptr;
HWND g_hidden = nullptr;
HWND g_status = nullptr;
HWND g_curve = nullptr;
NOTIFYICONDATAW g_nid{};
HINSTANCE g_inst = nullptr;
bool g_advanced = false;
HFONT g_font = nullptr;
HFONT g_fontTitle = nullptr;
HBRUSH g_bg = nullptr;
HBRUSH g_card = nullptr;

COLORREF kBg = RGB(32, 32, 32);
COLORREF kCard = RGB(43, 43, 43);
COLORREF kText = RGB(250, 250, 250);
COLORREF kMuted = RGB(180, 180, 180);
COLORREF kAccent = RGB(76, 194, 255);

void ApplyDarkTitle(HWND hwnd) {
  BOOL dark = TRUE;
  DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

HMENU IdMenu(int id) {
#pragma warning(suppress : 4312)
  return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

HFONT MakeFont(int px, bool bold = false) {
  LOGFONTW lf{};
  lf.lfHeight = -px;
  lf.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
  wcscpy_s(lf.lfFaceName, L"Segoe UI Variable");
  HFONT f = CreateFontIndirectW(&lf);
  if (!f) {
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    f = CreateFontIndirectW(&lf);
  }
  return f;
}

int SliderToUi(HWND slider) {
  return static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
}

void UiToSlider(HWND slider, double v) {
  SendMessageW(slider, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(v * 100.0)));
}

HWND AddLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height, bool title = false) {
  HWND ctl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, height, parent, nullptr, g_inst,
                             nullptr);
  SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(title ? g_fontTitle : g_font), TRUE);
  return ctl;
}

HWND AddSlider(HWND parent, int id, int x, int y, int width) {
  HWND ctl = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, x, y, width, 32,
                             parent, IdMenu(id), g_inst, nullptr);
  SendMessageW(ctl, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
  SendMessageW(ctl, TBM_SETTICFREQ, 10, 0);
  return ctl;
}

void ShowTrayMenu(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  const auto d = g_host->Diagnostics();
  AppendMenuW(menu, MF_STRING | (d.mode == EngineMode::MacMode ? MF_CHECKED : 0), 4001, L"Mac Mode");
  AppendMenuW(menu, MF_STRING | (d.mode == EngineMode::Native ? MF_CHECKED : 0), 4002, L"Windows Native");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  HMENU profiles = CreatePopupMenu();
  const auto s = g_host->CurrentSettings();
  AppendMenuW(profiles, MF_STRING | (s.profile == ProfileId::MacBalanced ? MF_CHECKED : 0), 4011, L"Mac Balanced");
  AppendMenuW(profiles, MF_STRING | (s.profile == ProfileId::MacSmooth ? MF_CHECKED : 0), 4012, L"Mac Smooth");
  AppendMenuW(profiles, MF_STRING | (s.profile == ProfileId::MacFast ? MF_CHECKED : 0), 4013, L"Mac Fast");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(profiles), L"Profile");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, 4020, L"Open Settings");
  AppendMenuW(menu, MF_STRING, 4021, L"Calibrate");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (platform::IsStartWithWindows() ? MF_CHECKED : 0), 4030, L"Start with Windows");
  AppendMenuW(menu, MF_STRING, 4099, L"Exit");
  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
  DestroyMenu(menu);
}

void AddTray(HWND hwnd) {
  g_nid = {};
  g_nid.cbSize = sizeof(g_nid);
  g_nid.hWnd = hwnd;
  g_nid.uID = TRAY_ID;
  g_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
  g_nid.uCallbackMessage = WM_TRAY;
  g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wcscpy_s(g_nid.szTip, L"SmoothPad");
  Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTray() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

void RefreshStatus(HWND hwnd) {
  const auto d = g_host->Diagnostics();
  const auto s = g_host->CurrentSettings();
  wchar_t buf[1024];
  swprintf_s(buf,
             L"Status: %s\n"
             L"Precision Touchpad: %s\n"
             L"Mode: %s\n"
             L"Input: Raw Input\n"
             L"Output: %s\n"
             L"Driver: %s\n"
             L"Report rate: %.0f Hz  (p95 interval %.1f ms, jitter %.1f ms)\n"
             L"Engine latency  median %.3f ms   p95 %.3f ms   p99 %.3f ms\n"
             L"VID/PID: %04X:%04X   size: %.1f x %.1f mm\n"
             L"%s",
             d.ptpDetected ? L"Precision Touchpad detected" : L"Windows Precision Touchpad not detected.",
             d.ptpDetected ? L"Connected" : L"Not found", EngineModeName(d.mode),
             d.syntheticAvailable ? L"Synthetic PT_TOUCHPAD" : L"Unavailable on this SDK/OS",
             d.driverConnected ? L"Connected" : L"Not required / not installed", d.reportHz, d.p95IntervalMs,
             d.jitterMs, d.latencyMedianMs, d.latencyP95Ms, d.latencyP99Ms, d.caps.identity.vendorId,
             d.caps.identity.productId, d.caps.widthMm, d.caps.heightMm, d.statusText.c_str());
  SetWindowTextW(g_status, buf);
  CheckDlgButton(hwnd, IDC_TOGGLE, s.enabled && s.profile != ProfileId::WindowsNative ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, IDC_STARTUP, platform::IsStartWithWindows() ? BST_CHECKED : BST_UNCHECKED);
  SendMessageW(GetDlgItem(hwnd, IDC_PROFILE), CB_SETCURSEL, static_cast<WPARAM>(s.profile == ProfileId::MacSmooth   ? 1
                                                                               : s.profile == ProfileId::MacFast     ? 2
                                                                               : s.profile == ProfileId::WindowsNative ? 3
                                                                                                                       : 0),
               0);
  UiToSlider(GetDlgItem(hwnd, IDC_SPEED), s.motion.pointerSpeed);
  SendMessageW(GetDlgItem(hwnd, IDC_ACCEL), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
  SendMessageW(GetDlgItem(hwnd, IDC_SMOOTH), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
  SendMessageW(GetDlgItem(hwnd, IDC_PREC), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
  SendMessageW(GetDlgItem(hwnd, IDC_ACCEL), TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(s.motion.acceleration * 100)));
  SendMessageW(GetDlgItem(hwnd, IDC_SMOOTH), TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(s.motion.smoothness * 100)));
  SendMessageW(GetDlgItem(hwnd, IDC_PREC), TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(s.motion.precision * 100)));
  InvalidateRect(g_curve, nullptr, TRUE);
}

void DrawCurve(HWND hwnd, HDC hdc) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  FillRect(hdc, &rc, g_card);
  const auto s = g_host->CurrentSettings();
  motion::AccelerationCurve curve;
  curve.Configure(BlendAcceleration(s.acceleration, s.motion));
  const auto d = g_host->Diagnostics();
  const int w = rc.right - rc.left - 16;
  const int h = rc.bottom - rc.top - 16;
  HPEN axis = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
  HPEN line = CreatePen(PS_SOLID, 2, kAccent);
  HGDIOBJ old = SelectObject(hdc, axis);
  MoveToEx(hdc, 8, rc.bottom - 8, nullptr);
  LineTo(hdc, rc.right - 8, rc.bottom - 8);
  MoveToEx(hdc, 8, rc.bottom - 8, nullptr);
  LineTo(hdc, 8, 8);
  SelectObject(hdc, line);
  const double maxSpeed = s.acceleration.curveEndMmS * 1.15;
  const double maxGain = std::max(4.0, s.acceleration.maxGain * s.motion.pointerSpeed * 1.2);
  bool first = true;
  for (int i = 0; i <= w; ++i) {
    const double speed = maxSpeed * (static_cast<double>(i) / w);
    const double gain = curve.Gain(speed) * s.motion.pointerSpeed;
    const int x = 8 + i;
    const int y = rc.bottom - 8 - static_cast<int>((gain / maxGain) * h);
    if (first) {
      MoveToEx(hdc, x, y, nullptr);
      first = false;
    } else {
      LineTo(hdc, x, y);
    }
  }
  const double markerX = Clamp(d.lastSpeedMmS / maxSpeed, 0.0, 1.0);
  const int mx = 8 + static_cast<int>(markerX * w);
  const int my = rc.bottom - 8 - static_cast<int>((d.lastGain / maxGain) * h);
  HBRUSH dot = CreateSolidBrush(RGB(255, 180, 80));
  SelectObject(hdc, GetStockObject(NULL_PEN));
  SelectObject(hdc, dot);
  Ellipse(hdc, mx - 5, my - 5, mx + 5, my + 5);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, kMuted);
  SelectObject(hdc, g_font);
  TextOutW(hdc, 12, 8, L"gain vs finger speed", 20);
  SelectObject(hdc, old);
  DeleteObject(axis);
  DeleteObject(line);
  DeleteObject(dot);
}

void RunCalibrationWizard(HWND hwnd) {
  MessageBoxW(hwnd,
              L"Calibration wizard (20–30 s)\n\n"
              L"1. Keep one finger still on the touchpad.\n"
              L"2. Move very slowly.\n"
              L"3. Move at a normal speed.\n"
              L"4. Perform a fast swipe.\n"
              L"5. Swipe across the full surface.\n\n"
              L"Without a live hardware trace this build applies conservative defaults derived from "
              L"the synthetic device model. Re-run on a laptop to measure the real noise floor and report rate.",
              L"SmoothPad calibration", MB_OK | MB_ICONINFORMATION);
  CalibrationResult c;
  c.noiseFloorMm = 0.014;
  c.medianHz = 125.0;
  c.slowMmS = 22.0;
  c.mediumMmS = 135.0;
  c.fastMmS = 480.0;
  c.widthMm = g_host->Diagnostics().caps.widthMm;
  c.heightMm = g_host->Diagnostics().caps.heightMm;
  g_host->ApplyCalibration(c);
  RefreshStatus(hwnd);
}

void ReadSliders(HWND hwnd) {
  MotionUiParams ui = g_host->CurrentSettings().motion;
  HWND speed = GetDlgItem(hwnd, IDC_SPEED);
  SendMessageW(speed, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
  ui.pointerSpeed = SliderToUi(speed) / 100.0;
  ui.acceleration = SendMessageW(GetDlgItem(hwnd, IDC_ACCEL), TBM_GETPOS, 0, 0) / 100.0;
  ui.smoothness = SendMessageW(GetDlgItem(hwnd, IDC_SMOOTH), TBM_GETPOS, 0, 0) / 100.0;
  ui.precision = SendMessageW(GetDlgItem(hwnd, IDC_PREC), TBM_GETPOS, 0, 0) / 100.0;
  g_host->SetMotionUi(ui);
  InvalidateRect(g_curve, nullptr, TRUE);
}

LRESULT CALLBACK HiddenProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_INPUT && g_host) {
    g_host->OnRawInput(reinterpret_cast<void*>(lParam));
    return 0;
  }
  if (msg == WM_HOTKEY && g_host) {
    g_host->OnHotkey(static_cast<int>(wParam));
    if (g_main) RefreshStatus(g_main);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_font = MakeFont(16);
      g_fontTitle = MakeFont(28, true);
      g_bg = CreateSolidBrush(kBg);
      g_card = CreateSolidBrush(kCard);
      AddLabel(hwnd, L"SmoothPad", 32, 20, 400, 40, true);
      AddLabel(hwnd, L"macOS-inspired pointer motion for Windows Precision Touchpads", 32, 62, 640, 24);
      AddLabel(hwnd, L"Mac-like Touchpad", 32, 110, 220, 22);
      CreateWindowExW(0, L"BUTTON", L"On", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 260, 108, 80, 24, hwnd,
                      IdMenu(IDC_TOGGLE), g_inst, nullptr);
      AddLabel(hwnd, L"Profile", 32, 150, 120, 22);
      HWND combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 160, 146, 280, 200,
                                   hwnd, IdMenu(IDC_PROFILE), g_inst, nullptr);
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mac Balanced"));
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mac Smooth"));
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mac Fast"));
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Windows Native"));
      AddLabel(hwnd, L"Pointer Speed", 32, 200, 200, 22);
      AddSlider(hwnd, IDC_SPEED, 32, 224, 360);
      AddLabel(hwnd, L"Acceleration", 32, 268, 200, 22);
      HWND a = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 32, 292, 360, 32, hwnd,
                               IdMenu(IDC_ACCEL), g_inst, nullptr);
      SendMessageW(a, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
      AddLabel(hwnd, L"Smoothness", 32, 336, 200, 22);
      HWND sm = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 32, 360, 360, 32, hwnd,
                                IdMenu(IDC_SMOOTH), g_inst, nullptr);
      SendMessageW(sm, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
      AddLabel(hwnd, L"Precision", 32, 404, 200, 22);
      HWND pr = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 32, 428, 360, 32, hwnd,
                                IdMenu(IDC_PREC), g_inst, nullptr);
      SendMessageW(pr, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
      CreateWindowExW(0, L"BUTTON", L"Calibrate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 32, 480, 120, 32, hwnd,
                      IdMenu(IDC_CALIBRATE), g_inst, nullptr);
      CreateWindowExW(0, L"BUTTON", L"Advanced Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 164, 480, 180, 32, hwnd,
                      IdMenu(IDC_ADVANCED), g_inst, nullptr);
      CreateWindowExW(0, L"BUTTON", L"Start with Windows", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 360, 486, 200, 24,
                      hwnd, IdMenu(IDC_STARTUP), g_inst, nullptr);
      g_curve = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 430, 110, 430, 350, hwnd,
                                IdMenu(IDC_CURVE), g_inst, nullptr);
      g_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 32, 530, 830, 180, hwnd,
                                 IdMenu(IDC_STATUS), g_inst, nullptr);
      SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
      EnumChildWindows(
          hwnd,
          [](HWND c, LPARAM) -> BOOL {
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            return TRUE;
          },
          0);
      SetTimer(hwnd, 1, 250, nullptr);
      RefreshStatus(hwnd);
      return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      SetTextColor(hdc, kText);
      SetBkColor(hdc, kBg);
      return reinterpret_cast<LRESULT>(g_bg);
    }
    case WM_ERASEBKGND: {
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(reinterpret_cast<HDC>(wParam), &rc, g_bg);
      return 1;
    }
    case WM_DRAWITEM: {
      const auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
      if (di->CtlID == IDC_CURVE) DrawCurve(di->hwndItem, di->hDC);
      return TRUE;
    }
    case WM_HSCROLL:
      ReadSliders(hwnd);
      return 0;
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (id == IDC_TOGGLE) {
        const bool on = IsDlgButtonChecked(hwnd, IDC_TOGGLE) == BST_CHECKED;
        if (on) {
          if (g_host->CurrentSettings().profile == ProfileId::WindowsNative) {
            g_host->SetProfile(ProfileId::MacBalanced);
          }
          g_host->SetEnabled(true);
        } else {
          g_host->SetEnabled(false);
        }
        RefreshStatus(hwnd);
      } else if (id == IDC_PROFILE && HIWORD(wParam) == CBN_SELCHANGE) {
        const int sel = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_PROFILE), CB_GETCURSEL, 0, 0));
        ProfileId p = ProfileId::MacBalanced;
        if (sel == 1) p = ProfileId::MacSmooth;
        if (sel == 2) p = ProfileId::MacFast;
        if (sel == 3) p = ProfileId::WindowsNative;
        g_host->SetProfile(p);
        RefreshStatus(hwnd);
      } else if (id == IDC_CALIBRATE || id == 4021) {
        RunCalibrationWizard(hwnd);
      } else if (id == IDC_ADVANCED) {
        g_advanced = !g_advanced;
        MessageBoxW(hwnd,
                    L"Advanced parameters (low/mid/max gain, One Euro minCutoff/beta/dCutoff, prediction, "
                    L"resampling, raw traces) are stored in %AppData%\\SmoothPad\\settings.json.\n\n"
                    L"Built-in profiles are read-only. Duplicate a profile JSON in the profiles folder to customize.",
                    L"Advanced Settings", MB_OK | MB_ICONINFORMATION);
      } else if (id == IDC_STARTUP || id == 4030) {
        const bool on = !platform::IsStartWithWindows();
        std::string err;
        platform::SetStartWithWindows(on, &err);
        RefreshStatus(hwnd);
      } else if (id == 4001) {
        g_host->SetProfile(ProfileId::MacBalanced);
        g_host->SetEnabled(true);
        RefreshStatus(hwnd);
      } else if (id == 4002) {
        g_host->SetProfile(ProfileId::WindowsNative);
        g_host->SetEnabled(false);
        RefreshStatus(hwnd);
      } else if (id == 4011) {
        g_host->SetProfile(ProfileId::MacBalanced);
        RefreshStatus(hwnd);
      } else if (id == 4012) {
        g_host->SetProfile(ProfileId::MacSmooth);
        RefreshStatus(hwnd);
      } else if (id == 4013) {
        g_host->SetProfile(ProfileId::MacFast);
        RefreshStatus(hwnd);
      } else if (id == 4020) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
      } else if (id == 4099) {
        DestroyWindow(hwnd);
      }
      return 0;
    }
    case WM_TIMER:
      RefreshStatus(hwnd);
      return 0;
    case WM_TRAY:
      if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayMenu(hwnd);
      if (lParam == WM_LBUTTONDBLCLK) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
      }
      return 0;
    case WM_CLOSE:
      ShowWindow(hwnd, SW_HIDE);
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd, 1);
      RemoveTray();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
  g_inst = inst;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  EngineHost host;
  g_host = &host;

  WNDCLASSEXW hidden{};
  hidden.cbSize = sizeof(hidden);
  hidden.lpfnWndProc = HiddenProc;
  hidden.hInstance = inst;
  hidden.lpszClassName = L"SmoothPadHidden";
  RegisterClassExW(&hidden);
  g_hidden = CreateWindowExW(0, L"SmoothPadHidden", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, inst, nullptr);

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MainProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = L"SmoothPadMain";
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  RegisterClassExW(&wc);

  g_main = CreateWindowExW(0, L"SmoothPadMain", L"SmoothPad", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 900, 760, nullptr, nullptr, inst, nullptr);
  ApplyDarkTitle(g_main);

  std::string err;
  host.Start(g_hidden, &err);

  const auto s = host.CurrentSettings();
  if (s.hotkey.enabled) {
    if (!platform::RegisterToggleHotkey(g_hidden, s.hotkey.modifiers, s.hotkey.vk, HOTKEY_TOGGLE)) {
      SP_LOG_WARN("toggle hotkey registration failed (conflict)");
    }
    platform::RegisterToggleHotkey(g_hidden, s.hotkey.emergencyModifiers, s.hotkey.emergencyVk, HOTKEY_EMERGENCY);
  }

  AddTray(g_main);
  const bool trayOnly = cmd && wcsstr(cmd, L"--tray");
  ShowWindow(g_main, trayOnly ? SW_HIDE : show);
  if (!trayOnly) RefreshStatus(g_main);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  platform::UnregisterHotkey(g_hidden, HOTKEY_TOGGLE);
  platform::UnregisterHotkey(g_hidden, HOTKEY_EMERGENCY);
  host.Stop();
  if (g_font) DeleteObject(g_font);
  if (g_fontTitle) DeleteObject(g_fontTitle);
  if (g_bg) DeleteObject(g_bg);
  if (g_card) DeleteObject(g_card);
  return static_cast<int>(msg.wParam);
}
