# Installer

Driverless: prefer MSIX (`scripts/package.ps1`). GitHub Releases, if any, are **user-mode** and unsigned unless a maintainer added signing secrets.

Driver edition is **not** provided by this repository. If **you** have a Hardware Dev Center–signed `SmoothPadFilter`, a Win32 offline installer (WiX or signed EXE bootstrapper) that:

1. Sets SmoothPad custom mode off / filter pass-through
2. Stages the signed SmoothPadFilter package
3. Installs SmoothPad.exe
4. Registers optional startup only if the user opted in

Uninstall reverses that order. See `docs/store/win32-installer.md`.
