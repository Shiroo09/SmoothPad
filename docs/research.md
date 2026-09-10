# Research

Evidence tags: **DOCUMENTED** · **SOURCE VERIFIED** · **EXPERIMENT VERIFIED** · **UNVERIFIED**

Date: 2026-09-10. Windows SDK on the build machine: `10.0.26100.0`. Visual Studio Professional 2026 18.8.2, MSVC 14.51.36231 (v145).

## Open-source projects

| Project | Technique | Relevant files | License | Reuse | Risks |
| --- | --- | --- | --- | --- | --- |
| [emoacht/RawInput.Touchpad](https://github.com/emoacht/RawInput.Touchpad) | Enumerate `RIM_TYPEHID`, match Usage Page `0x0D` / Usage `0x05`, `RegisterRawInputDevices`, `WM_INPUT`, `GetRawInputData` + `RIDI_PREPARSEDDATA`, `HidP_GetCaps` / `HidP_GetValueCaps` / `HidP_GetUsageValue`. Contact ID `0x51`, X `0x01/0x30`, Y `0x01/0x31`, Scan Time `0x56`, Contact Count `0x54`. | `Source/RawInput.Touchpad/TouchpadHelper.cs` | MIT (`LICENSE.txt` **SOURCE VERIFIED**) | Conceptual only. Native C++ reimplementation in `SmoothPad.Input`. | C# sample; foreground-only registration in the sample (`dwFlags = 0`). Does not suppress native pointer promotion. **SOURCE VERIFIED** |
| [ClementGre/ThreeFingerDragOnWindows](https://github.com/ClementGre/ThreeFingerDragOnWindows) | Production PTP utility: WinUI 3, tray, RawInput.Touchpad-derived helper, Store listing. Does **not** replace single-finger pointer motion; synthesizes mouse drag for 3-finger. | `ThreeFingerDragOnWindows/touchpad/TouchpadHelper.cs`, `ContactsManager.cs`, `Package.appxmanifest` | MIT **SOURCE VERIFIED** | Conceptual (tray / packaging / device handling). | Still user-mode overlay on top of native PTP motion. **SOURCE VERIFIED** |
| [tasos14/ScrollStrip](https://github.com/tasos14/ScrollStrip) | Hidden message-only window, Raw Input, HidP parse, `SendInput` wheel. README states it **never intercepts** normal input. | `Native/NativeMethods.cs`, `README.md` | No LICENSE file as of 2026-09 (**SOURCE VERIFIED** absence) | Conceptual only. Inertia **not** used for pointer. | Edge-scroll + inertia is a different problem. Double-motion if copied to pointer. **SOURCE VERIFIED** |
| [ZiMiaoWorkshop/Touchpad-Shield](https://github.com/ZiMiaoWorkshop/Touchpad-Shield) | WinUI 3 C++/WinRT, NSIS, PTP sensitivity / palm-rejection-ish zones, external mouse monitoring, enable/disable built-in pad. | `src/`, `TouchpadShield.sln` | Apache-2.0 **SOURCE VERIFIED** | Conceptual (SPI settings, external mouse policy, tray). No source copied. | Changes OS touchpad settings; SmoothPad must restore originals if it ever writes them. **SOURCE VERIFIED** |
| [jxleyo/PTP_HidFilter](https://github.com/jxleyo/PTP_HidFilter) | WDM/KMDF-ish HID filter over PTP collections, several revisions (`PTP_HidFilter0.1`–`0.5`, Bluetooth variants). | `PTP_HidFilter0.5/SRC/PTP_HidFilter.c`, `.inf` | No LICENSE file | Not reused. Informal sample quality. | Unsigned, incomplete INF matching, not WHCP. **SOURCE VERIFIED** (structure only) |
| [microsoft/Windows-driver-samples](https://github.com/microsoft/Windows-driver-samples) `hid/firefly`, `input/moufiltr` | KMDF filter, `EvtDeviceAdd`, INF UpperFilters, WMI, PnP. | `hid/firefly/driver`, `input/moufiltr` | Microsoft sample license | Conceptual KMDF/INF. **Not** treated as production-ready. | moufiltr is a **mouse class** filter — SmoothPad must **not** bind globally to mouclass/mouhid. **DOCUMENTED** + **SOURCE VERIFIED** |

Other 2025–2026 references considered: Precision Touchpad Programming Guide (Learn, updated 2026), `TOUCHPAD_PARAMETERS_V1` (24H2), David Gardiner’s 2025 SPI write-up. RawInput.Sharp / AbsoluteTouchEx / TouchpadGestures_Advanced: not used; they either target absolute mapping or gestures, not pointer-gain replacement.

## Microsoft APIs

| API | Minimum OS | Purpose | Verified | Used | Notes |
| --- | --- | --- | --- | --- | --- |
| `RegisterRawInputDevices` / `WM_INPUT` / `GetRawInputData` / `GetRawInputDeviceInfo` / `RAWINPUT` / `RAWHID` | Windows XP+ | Background HID reports from PTP | **DOCUMENTED** + headers **SOURCE VERIFIED** (`WinUser.h` 10.0.26100.0) | Yes | `RIDEV_INPUTSINK` for tray process. |
| `HidP_GetCaps` / `HidP_GetValueCaps` / `HidP_GetUsageValue` / `HidP_GetUsages` / `HidD_*` | Windows | Parse PTP input report | **DOCUMENTED** (`hidpi.h` present) | Yes | Usage Page 0x0D / Usage 0x05. |
| `HID_USAGE_PAGE_DIGITIZER` `0x0D`, `HID_USAGE_DIGITIZER_TOUCH_PAD` `0x05` | — | Device identity | **SOURCE VERIFIED** `hidusage.h` | Yes | Distinct from mouse 0x01/0x02. |
| `PT_TOUCHPAD` | Windows 8+ pointer types; 11 for injection | Pointer type enum | **SOURCE VERIFIED** `WinUser.h` line ~6306 | Yes | |
| `CreateSyntheticPointerDevice` | Windows 10 1809 | Inject `PT_TOUCH` / `PT_PEN` only | **DOCUMENTED** + **SOURCE VERIFIED** in 26100 `WinUser.h` | Fallback probe | Cannot create touchpads (**DOCUMENTED**). |
| `CreateSyntheticPointerDevice2` + `SYNTHETIC_DEVICE_CREATION_PARAMS` + `SDCO_PHYSICAL_SIZE` + `SDCO_TOUCHPAD_GESTURE_ONLY` | Windows 11 current (Learn last updated 2026-07-24 / 2026-04-16). Header on this machine: **not present** in 10.0.26100.0 `WinUser.h` (comment: “TODO … Feature_TouchpadPublicApis3”). | Create `PT_TOUCHPAD` injector with physical himetric size | **DOCUMENTED** (Learn) · headers absent **SOURCE VERIFIED** · runtime `GetProcAddress` + successful create **EXPERIMENT VERIFIED** 2026-09-10 on this Windows 11 24H2 machine (`SmoothPad.Cli inspect`: “CreateSyntheticPointerDevice2(PT_TOUCHPAD) succeeded”) | Preferred output | Requirements from Learn: `feedbackMode = POINTER_FEEDBACK_NONE`, `hMonitor = NULL`, `maxCount <= 5`, non-zero width/height, `SDCO_PHYSICAL_SIZE` mandatory. Without `SDCO_TOUCHPAD_GESTURE_ONLY`, injected contacts are treated like a physical pad (gestures + pointer). Loaded by name, then User32 ordinal 2690. |
| `InjectSyntheticPointerInput` | 1809 | Inject frames | **DOCUMENTED** + **SOURCE VERIFIED** | Yes if device exists | For PTP, coordinates in `ptHimetricLocation` when `SDCO_PHYSICAL_SIZE`. |
| `DestroySyntheticPointerDevice` | 1809 | Cleanup | **DOCUMENTED** | Yes | |
| `InjectTouchpadAction` | Windows 11 current | Gesture-only injection helper | **DOCUMENTED** · not in 26100 headers | Not used for pointer | Gesture-only path; SmoothPad needs pointer promotion. |
| `RegisterTouchpadCapableWindow` / `RegisterTouchpadCapableThread` | Windows 11 current | Receive `WM_POINTER` for two-finger pad gestures | **DOCUMENTED** · not in 26100 headers | Not used for suppression | App-window gesture consumption. Does **not** stop system cursor promotion. |
| `GetPointerTouchpadInfo` / `GetPointerFrameTouchpadInfoHistory` | Windows 11 current | Read PTP pointer frames | **DOCUMENTED** | Diagnostics only later | |
| `SPI_GETTOUCHPADPARAMETERS` `0x00AE` / `SPI_SETTOUCHPADPARAMETERS` `0x00AF` / `TOUCHPAD_PARAMETERS_V1` | Windows 11 24H2 (build 26027+) | Read/write user PTP settings, presence, external mouse | **DOCUMENTED** + **SOURCE VERIFIED** in 26100 `WinUser.h` | Detection + optional “disable pad when mouse present” | Does **not** suppress per-device native motion. `touchpadEnabled=FALSE` disables the pad entirely — used only if the user picks that external-mouse policy. |
| `QueryPerformanceCounter` | — | dt | **DOCUMENTED** | Yes | |

## Feasibility: native pointer promotion

**Question:** Can user-mode, documented APIs suppress the physical PTP’s native single-finger pointer promotion while still delivering raw contacts?

| Candidate | Result |
| --- | --- |
| Raw Input | Reads a **copy**. Native stack continues. Confirmed by ScrollStrip README and RawInput.Touchpad design. **SOURCE VERIFIED** |
| `RegisterTouchpadCapableWindow` | Delivers `WM_POINTER` to that window for gestures; system cursor still moves. **DOCUMENTED** |
| `CreateSyntheticPointerDevice2(PT_TOUCHPAD)` | Injects an **additional** device. Native physical device still promotes. **DOCUMENTED** (injection, not replacement) |
| `SPI_SETTOUCHPADPARAMETERS` `touchpadEnabled=0` | Disables the pad completely — loses raw contacts too. Unusable as a suppress-and-replace. **DOCUMENTED** |
| `SDCO_TOUCHPAD_GESTURE_ONLY` | Makes **synthetic** input gesture-only; does not mute the physical pad. **DOCUMENTED** |
| `WH_MOUSE_LL` / cursor counter-move / `SendInput` negative deltas | Explicitly forbidden by product constraints. Unreliable, affects external mice. |
| KMDF HID upper filter on **only** `HID_DEVICE_UP:000D_U:0005` | Only documented, device-scoped way to drop or rewrite reports before mouhid/PTP class. **DOCUMENTED** pattern (HID filter) · hardware suppression **UNVERIFIED** until a signed driver is loaded on a laptop |

**Decision (driver gate):** User-mode cannot meet acceptance criterion F (no native+injected double movement) with documented APIs. SmoothPad therefore:

1. Ships a **complete user-mode engine** (inspect, parse, motion, UI, traces, tests).
2. Advertises Mac Mode **only** when SmoothPadFilter is connected, heartbeating, and in Custom mode **and** synthetic PTP injection is available.
3. Otherwise remains **Native** or **Degraded** — never a fake “Mac Mode ON”.
4. Filter default is pass-through; watchdog 250 ms fail-open. Motion math stays in user mode.

This is the proof recorded under “WHY A DRIVER IS REQUIRED” in `docs/architecture.md`. Hardware confirmation of Custom-mode suppression remains **UNVERIFIED** (no WDK on this agent machine; no driver load).
