# Unpackaged / Win32 layout

`scripts/package.ps1` also copies:

- `SmoothPad.exe` (from `SmoothPad.App.exe`)
- `SmoothPad.Cli.exe`

into `out/package/<platform>/<config>/` next to `AppxManifest.xml` for MSIX.

For a classic installer (driver edition), **you** bundle those two binaries plus a Hardware Dev Center–signed driver package. This repo does not attach `SmoothPadFilter.sys`. See `docs/store/win32-installer.md` and `docs/byo-driver.md`.
