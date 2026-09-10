# Troubleshooting

## Precision Touchpad not detected

Run `SmoothPad.Cli.exe inspect`. If empty: Settings → Bluetooth & devices → Touchpad should say the PC has a precision touchpad. Legacy ELAN/Synaptics mouse-HID pads are unsupported (by design).

## Mac Mode unavailable / Degraded

This is the default for anyone who only cloned the repo.

Mac Mode requires:

1. A **you-built-and-signed** SmoothPadFilter loaded and heartbeating, **and**
2. `CreateSyntheticPointerDevice2` on this OS.

GitHub does not attach `SmoothPadFilter.sys`. User-mode cannot mute native PTP pointer promotion. The UI stays Degraded so native+injected motion cannot combine. That is intentional, not a missing checkbox.

See [byo-driver.md](byo-driver.md). Do not disable Secure Boot to “fix” Degraded.

## Double movement

Turn Mac Mode off immediately (`Ctrl+Alt+Shift+M` emergency, or tray → Windows Native). File a bug. Double movement means suppression failed; the product must not stay in Mac Mode.

## Gestures broken

Filter should be pass-through. Uninstall the filter (`scripts/uninstall-filter.ps1`) and confirm two-finger scroll works stock. Reinstall only after pass-through is verified.

## Driver not loaded

`\\\\.\\SmoothPadFilter` missing ⇒ user mode continues without Mac Mode. Expected unless **you** built the WDK project and installed a signed/test-signed package on a test box.

## Store install

MSIX needs a trusted sign. Sideload: `Add-AppxPackage`. Driver edition is a Win32 Store listing, not a kernel-inside-MSIX fantasy.

## Startup

HKCU Run `SmoothPad` → `"SmoothPad.exe" --tray`. No admin at logon.

## Sleep / resume

Engine re-opens Raw Input on the same HWND. If the device vanished (Fn-disable), diagnostics show disconnected; native path is the OS.

## External mouse / Raw Accel

Mice never enter the PTP usage registration. Acceptance: SmoothPad ON vs OFF must not change external mouse statistics. Raw Accel continues to see the mouse class stack SmoothPad does not filter.
