# SmoothPadFilter

Minimal KMDF HID **upper filter** bound only to Precision Touchpad collections (`HID_DEVICE_UP:000D_U:0005`). It is not a mouclass/mouhid filter and must never attach to USB/Bluetooth mice.

## Device stack (target)

```
HID class PDO  (touchpad collection 0x0D/0x05)
    SmoothPadFilter   ← this driver (upper filter)
    hidclass / vendor HID minidriver (I2C/USB/BT)
```

## INF

`src/SmoothPad.Filter/SmoothPadFilter.inf`

- `StartType = 3` (demand) — not boot-start.
- `UpperFilters` on the matched hardware key only.
- Copy to DriverStore (`DestinationDirs = 13`).

## IOCTL protocol

Shared in `src/SmoothPad.Filter/public.h`:

| Code | Direction | Meaning |
| --- | --- | --- |
| HEARTBEAT | UM → KM | Reset watchdog |
| SET_MODE | UM → KM | 0 pass-through, 1 observe, 2 custom |
| GET_STATUS | KM → UM | mode, matched, dropped, last heartbeat age |
| READ_REPORTS | reserved | future ring; currently unused |

## Heartbeat / watchdog

User mode: 75 ms. Kernel: 250 ms. Expired watchdog forces pass-through. No motion math in the kernel.

## Implementation order (mandatory)

1. Installable no-op filter  
2. Pass-through verified (touchpad identical to stock)  
3. PnP / power  
4. Heartbeat / fail-open  
5. Report observation  
6. Controlled suppression of native reports  
7. User-mode integration  

This tree implements 1–4 in source. Step 6 is **not** enabled: `SmoothPadEvtInternalIoctl` still forwards every HID IOCTL. Enabling suppression requires a WDK build, test-signed load in a VM, and a hardware confirmation that contacts still reach user mode via Raw Input or a dedicated read path.

Contributors who want to complete the filter: [byo-driver.md](byo-driver.md). This GitHub project does not distribute a signed `.sys`.

## Security

- KMDF only. No undocumented hooks, no mapping, no DSE bypass.
- IOCTLs are `METHOD_BUFFERED`.
- No user pointers at elevated IRQL.
- Production: WHCP / attestation signing. Dev: Microsoft test-signing in a VM.

## Recovery

`scripts/uninstall-filter.ps1` (elevated). Safe Mode must boot without the engine. If the filter is stuck, delete the OEM INF with `pnputil` and confirm no orphaned `UpperFilters` on HID instances.

## Driver Verifier

Use in a **VM**. Do not enable on the user’s daily laptop from this repo’s scripts.
