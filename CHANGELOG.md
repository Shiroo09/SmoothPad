# Changelog

## 0.1.0 — 2026-09-10

Initial public source tree (user-mode product + optional filter **source**).

- Native C++23 motion engine (velocity-aware gain, One Euro / adaptive smoothing, fractional accumulation, no pointer inertia)
- Precision Touchpad Raw Input enumerator and HID contact parser
- Trace record/replay (`SmoothPad.Cli`)
- Settings UI, tray, hotkeys, calibration wizard shell
- Synthetic PT_TOUCHPAD output path (runtime-detected `CreateSyntheticPointerDevice2`)
- Optional KMDF HID filter skeleton with fail-open heartbeat — **not shipped signed**; BYO via WDK
- Mac Mode stays Degraded unless a live filter suppresses native promotion (step 6 not armed)
- Unit, replay, and latency benches
- MSIX staging + GitHub Actions (user-mode only)
