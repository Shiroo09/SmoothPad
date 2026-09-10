# Microsoft Store listing (draft)

**Name:** SmoothPad  

**Short description:** macOS-inspired pointer motion for Windows Precision Touchpads.

**Description:**

SmoothPad is an open-source Windows utility that gives Precision Touchpads a smoother, velocity-aware pointer response inspired by macOS trackpads while preserving Windows gestures and external mouse behavior.

It is not an Apple driver and does not claim to be the official Apple algorithm or exact MacBook emulation.

**Feature bullets**

- Velocity-aware pointer acceleration with a precision low-speed region
- Profiles: Mac Balanced, Mac Smooth, Mac Fast, Windows Native
- Mac Mode (replace native pointer) is **not** in the Store MSIX: it needs a separately signed kernel filter this listing must not bundle
- Calibration wizard
- Diagnostics (report rate, latency, VID/PID, transport)
- External mice and Raw Accel are not modified
- No telemetry by default

**Privacy:** see `docs/privacy.md` — no accounts, no cloud, local settings, opt-in traces.

**Support URL:** repository Issues  
**Repository:** this GitHub repo  
**Minimum Windows:** Windows 11 24H2 (build 26100)  
**Architecture:** x64 (ARM64 ready)

**Installer notes**

- Driverless experiment: MSIX (`packaging/msix/AppxManifest.xml`), full-trust Win32.
- If SmoothPadFilter is required for Mac Mode: do **not** put the `.sys` in MSIX. Use a Win32 Store listing pointing at a versioned HTTPS offline `.exe`/`.msi` that installs the app + WHCP-signed driver. Silent install, no stub downloader, no bundled unrelated software.

Screenshots: not fabricated. Capture from a real laptop build before submission.
