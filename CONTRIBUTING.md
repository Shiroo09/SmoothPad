# Contributing

## Environment

- Windows 11 24H2+
- Visual Studio 2026 (or 2022+) with MSVC / C++ desktop workload
- Windows SDK 10.0.26100.0
- **WDK only if you are changing or building SmoothPadFilter**

You can contribute motion, HID parsing, UI, tests, and docs with no kernel tools.

## Build

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
.\scripts\test.ps1
```

CLI:

```text
out\x64\Release\SmoothPad.Cli.exe inspect
out\x64\Release\SmoothPad.Cli.exe bench
out\x64\Release\SmoothPad.Cli.exe curve
```

Filter (optional, WDK):

```powershell
.\scripts\build-filter.ps1
```

That produces an **unsigned** `.sys`. It does not install anything. See [docs/byo-driver.md](docs/byo-driver.md).

## Layout

| Path | Role |
| --- | --- |
| `src/SmoothPad.Motion` | Pure motion library. No Win32. |
| `src/SmoothPad.Input` | Raw Input + HID parse |
| `src/SmoothPad.Output` | Synthetic PT_TOUCHPAD + diagnostic SendInput |
| `src/SmoothPad.App` | Settings UI / tray (Win32) |
| `src/SmoothPad.Filter` | Optional KMDF HID upper filter — **not** in `SmoothPad.sln` |

Motion changes must include a replay/unit test. Do not tune Apple-exact numbers; document perceptual intent in `docs/tuning.md`.

## Driver changes

Follow `docs/driver.md` and `docs/byo-driver.md`. Order: no-op filter → pass-through → PnP/power → heartbeat → observe → suppress. Default is fail-open.

Do **not**:

- Attach to `mouclass` / generic mice
- Check in `.sys`, `.cat`, `.pfx`, or EV material
- Document Secure Boot / DSE bypass as a user install path
- Enable step 6 (drop native reports) without a VM repro and a recovery note in the PR

PRs that only add “disable driver signature enforcement” instructions will be closed.

## Style

C++23, `/W4`, `/permissive-`, RAII, no hot-path allocations, no per-report logging in Release.

## Commits

```
feat(motion): ...
fix(input): ...
test(motion): ...
docs: ...
```

## Issues

Use the templates. Hardware reports need VID/PID from `SmoothPad.Cli.exe inspect`. Kernel bugs go through [SECURITY.md](SECURITY.md), not a public issue.
