#include "smoothpad/Logging.h"
#include "smoothpad/Math.h"
#include "smoothpad/Settings.h"
#include "smoothpad/Trace.h"
#include "smoothpad/Version.h"
#include "smoothpad/input/RawInputSource.h"
#include "smoothpad/motion/MotionEngine.h"
#include "smoothpad/output/PointerOutput.h"
#include "smoothpad/platform/Win32.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace smoothpad;

static void PrintCaps(const DeviceCaps& c) {
  std::wcout << L"  name:      " << c.identity.deviceName << L"\n";
  std::wcout << L"  path:      " << c.identity.instancePath << L"\n";
  std::wprintf(L"  VID:PID    %04X:%04X\n", c.identity.vendorId, c.identity.productId);
  std::wprintf(L"  usage      0x%04X / 0x%04X\n", c.identity.usagePage, c.identity.usage);
  const wchar_t* t = L"unknown";
  if (c.identity.transport == Transport::I2c) t = L"I2C";
  if (c.identity.transport == Transport::Usb) t = L"USB";
  if (c.identity.transport == Transport::Bluetooth) t = L"Bluetooth";
  std::wcout << L"  transport  " << t << L"\n";
  std::wprintf(L"  logical X  %d .. %d\n", c.x.logicalMin, c.x.logicalMax);
  std::wprintf(L"  logical Y  %d .. %d\n", c.y.logicalMin, c.y.logicalMax);
  std::wprintf(L"  physical   %.2f x %.2f mm (%s)\n", c.widthMm, c.heightMm,
               c.physicalSizeKnown ? L"from HID" : L"fallback");
}

static int Inspect() {
  input::RawInputSource src;
  const auto devices = src.Enumerate();
  std::wcout << L"SmoothPad Touchpad Inspector " << kVersionStringW << L"\n";
  if (devices.empty()) {
    std::wcout << L"Windows Precision Touchpad not detected.\n";
    return 2;
  }
  std::wcout << L"Found " << devices.size() << L" Precision Touchpad device(s):\n";
  for (const auto& d : devices) {
    std::wcout << L"-\n";
    PrintCaps(d);
  }
  bool present = false, enabled = false, mouse = false;
  std::string err;
  if (platform::QueryTouchpadParameters(&present, &enabled, &mouse, &err)) {
    std::wcout << L"\nSPI_GETTOUCHPADPARAMETERS:\n";
    std::wcout << L"  present=" << present << L" enabled=" << enabled << L" externalMouse=" << mouse << L"\n";
  } else {
    std::wcout << L"\nSPI_GETTOUCHPADPARAMETERS unavailable: " << platform::Utf8ToWide(err) << L"\n";
  }
  std::string oerr;
  output::SyntheticTouchpadOutput syn;
  const bool ok = syn.Initialize(devices.front().physicalSizeKnown ? devices.front() : motion::SyntheticCaps(), &oerr);
  std::wcout << L"\nSynthetic PT_TOUCHPAD: " << (ok ? L"available" : L"unavailable") << L"\n";
  if (!ok) std::wcout << L"  " << platform::Utf8ToWide(oerr) << L"\n";
  syn.Shutdown();
  return 0;
}

static LRESULT CALLBACK RecWnd(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_INPUT) {
    auto* src = reinterpret_cast<input::RawInputSource*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (src) src->OnRawInput(reinterpret_cast<void*>(lParam));
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int Record(const std::filesystem::path& path, int seconds) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = RecWnd;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"SmoothPadRecorder";
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
  input::RawInputSource src;
  std::string err;
  if (!src.Initialize(hwnd, &err)) {
    std::cerr << err << "\n";
    return 2;
  }
  DeviceCaps caps = src.Selected() ? *src.Selected() : motion::SyntheticCaps();
  TraceWriter writer;
  writer.Open(path, caps);
  src.SetCallback([&](const RawContactFrame& f, const DeviceCaps&) { writer.Write(f); });
  SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&src));
  std::wcout << L"Recording " << seconds << L"s to " << path.wstring() << L" ...\n";
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  MSG msg;
  while (std::chrono::steady_clock::now() < end) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(1);
  }
  writer.Close();
  src.Shutdown();
  std::wcout << L"frames: " << writer.FrameCount() << L"\n";
  return 0;
}

static int Replay(const std::filesystem::path& path) {
  TraceReader r;
  std::string err;
  if (!r.Open(path, &err)) {
    std::cerr << err << "\n";
    return 2;
  }
  motion::MotionEngine e;
  DeviceCaps caps = motion::SyntheticCaps();
  caps.widthMm = r.Header().widthMm > 0 ? r.Header().widthMm : caps.widthMm;
  caps.heightMm = r.Header().heightMm > 0 ? r.Header().heightMm : caps.heightMm;
  e.Configure({caps, DefaultSettings()});
  double dist = 0;
  int emits = 0;
  std::vector<double> proc;
  for (const auto& raw : r.ToRawFrames()) {
    const auto t0 = std::chrono::steady_clock::now();
    auto o = e.Process(raw);
    const auto t1 = std::chrono::steady_clock::now();
    proc.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    dist += o.deltaMm.length();
    if (o.emit) ++emits;
  }
  std::sort(proc.begin(), proc.end());
  std::printf("frames=%zu emit=%d path_mm=%.3f  process_ms median=%.4f p95=%.4f p99=%.4f\n", r.Frames().size(), emits,
              dist, Percentile(proc, 0.5), Percentile(proc, 0.95), Percentile(proc, 0.99));
  return 0;
}

static int Bench() {
  const char* kinds[] = {"stationary", "micro", "slow", "medium", "fast", "reversal", "circle", "diagonal", "1to2",
                         "2to1",       "lift"};
  for (const auto* k : kinds) {
    auto samples = motion::SynthesizeTrace(k, motion::SyntheticCaps(), 125.0);
    motion::MotionEngine e;
    e.Configure({motion::SyntheticCaps(), DefaultSettings()});
    std::vector<double> ms;
    ms.reserve(samples.size() * 50);
    for (int r = 0; r < 50; ++r) {
      e.Reset();
      for (const auto& s : samples) {
        RawContactFrame f{};
        f.qpc = static_cast<std::uint64_t>(s.tSeconds * 10'000'000.0);
        f.contactCount = s.contactCount;
        if (s.tip) {
          f.contacts[0] = {s.primaryId, true, true, true, static_cast<int>(s.logical.x),
                           static_cast<int>(s.logical.y), {}, {}, {}};
        }
        const auto t0 = std::chrono::steady_clock::now();
        (void)e.Process(f);
        const auto t1 = std::chrono::steady_clock::now();
        ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
      }
    }
    std::sort(ms.begin(), ms.end());
    std::printf("%-12s n=%zu  median=%.4f  p95=%.4f  p99=%.4f  max=%.4f ms\n", k, ms.size(), Percentile(ms, 0.5),
                Percentile(ms, 0.95), Percentile(ms, 0.99), ms.back());
  }
  return 0;
}

static int Curve() {
  auto s = DefaultSettings();
  motion::AccelerationCurve c;
  c.Configure(BlendAcceleration(s.acceleration, s.motion));
  std::printf("speed_mm_s,gain\n");
  for (int v = 0; v <= 800; v += 10) {
    std::printf("%d,%.5f\n", v, c.Gain(v) * s.motion.pointerSpeed);
  }
  return 0;
}

static void Usage() {
  std::puts(
      "SmoothPad CLI\n"
      "  inspect                         Enumerate Precision Touchpads\n"
      "  record <file.sptrace> [sec]     Record a hardware trace\n"
      "  replay <file.sptrace>           Replay a trace through the motion engine\n"
      "  export-json <file.sptrace>      Convert trace to JSON on stdout\n"
      "  export-csv <file.sptrace>       Convert trace to CSV on stdout\n"
      "  bench                           Motion-engine latency bench\n"
      "  curve                           Print default gain curve as CSV\n");
}

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) {
    Usage();
    return 1;
  }
  const std::wstring cmd = argv[1];
  if (cmd == L"inspect") return Inspect();
  if (cmd == L"record") {
    if (argc < 3) {
      Usage();
      return 1;
    }
    const int sec = argc >= 4 ? _wtoi(argv[3]) : 10;
    return Record(argv[2], sec > 0 ? sec : 10);
  }
  if (cmd == L"replay") {
    if (argc < 3) return 1;
    return Replay(argv[2]);
  }
  if (cmd == L"export-json" || cmd == L"export-csv") {
    if (argc < 3) return 1;
    TraceReader r;
    std::string err;
    if (!r.Open(argv[2], &err)) {
      std::cerr << err << "\n";
      return 2;
    }
    const auto text = cmd == L"export-json" ? TraceToJson(r) : TraceToCsv(r);
    std::fwrite(text.data(), 1, text.size(), stdout);
    return 0;
  }
  if (cmd == L"bench") return Bench();
  if (cmd == L"curve") return Curve();
  Usage();
  return 1;
}
