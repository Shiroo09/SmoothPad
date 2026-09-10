# SmoothPadFilter

KMDF HID **upper filter** for Precision Touchpad collections only (`HID_DEVICE_UP:000D_U:0005`).

- Compiling `SmoothPadFilter.sys` requires the Windows Driver Kit.
- `SmoothPad.sln` does **not** build this project. User-mode still compiles without WDK.
- `driver.c` is wrapped in `#ifdef _KERNEL_MODE` and also provides a user-mode stub.
- Default runtime mode is **pass-through**. Native-report suppression (implementation step 6) is **not armed**.

This directory ships **source and INF**, not a signed `.sys`.

| Next step | Doc |
| --- | --- |
| Why it exists | [docs/architecture.md](../../docs/architecture.md) |
| Build / sign / install yourself | [docs/byo-driver.md](../../docs/byo-driver.md) |
| IOCTL / watchdog / order | [docs/driver.md](../../docs/driver.md) |
| Microsoft signing | [docs/driver-signing.md](../../docs/driver-signing.md) |

```powershell
# from repo root, WDK required
.\scripts\build-filter.ps1
```

Do not run `install-filter.ps1` on a daily laptop. Do not disable Secure Boot to load an unsigned build.
