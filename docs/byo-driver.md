# Bring your own driver

SmoothPad’s user-mode app is MIT and builds without the WDK. **Mac Mode** needs a kernel HID upper filter that this repository does **not** ship as a signed binary.

You (a developer with WDK, a test VM, and eventually a Hardware Dev Center identity) supply `SmoothPadFilter.sys`. Everyday users should not do this. Nobody should disable Secure Boot, Test Signing, or Driver Signature Enforcement on a daily laptop to “make SmoothPad work.”

## What the repo gives you

| Path | Role |
| --- | --- |
| [`src/SmoothPad.Filter/driver.c`](../src/SmoothPad.Filter/driver.c) | KMDF upper filter: pass-through, heartbeat, 250 ms fail-open |
| [`src/SmoothPad.Filter/public.h`](../src/SmoothPad.Filter/public.h) | IOCTL protocol shared with user mode |
| [`src/SmoothPad.Filter/SmoothPadFilter.inf`](../src/SmoothPad.Filter/SmoothPadFilter.inf) | Binds **only** `HID_DEVICE_UP:000D_U:0005` (Precision Touchpad) |
| [`src/SmoothPad.Filter/SmoothPadFilter.vcxproj`](../src/SmoothPad.Filter/SmoothPadFilter.vcxproj) | WDK project — **not** in `SmoothPad.sln` |
| [`scripts/build-filter.ps1`](../scripts/build-filter.ps1) | Builds `.sys` if WDK is installed; does **not** install it |
| [`scripts/install-filter.ps1`](../scripts/install-filter.ps1) | Elevated `pnputil` — VM / test box only |
| [`scripts/uninstall-filter.ps1`](../scripts/uninstall-filter.ps1) | Best-effort removal |

The filter does **not** implement the motion curve. User mode does.

## What it does not do yet

Implementation order in [driver.md](driver.md):

1. Installable no-op — source exists
2. Pass-through — default path
3. PnP / power — skeleton
4. Heartbeat / fail-open — skeleton
5. Report observation — IOCTL reserved
6. **Controlled suppression of native HID reports — not armed**
7. User-mode integration — client exists; stays Degraded without a live filter

`SmoothPadEvtInternalIoctl` still forwards every HID IOCTL. Until step 6 is proven on hardware, Mac Mode **must** stay off. Enabling drop-report without a verified contact path to user mode can kill the pad or double-move the cursor.

## Build the `.sys`

1. Windows 11 VM (not your only machine).
2. Visual Studio + **Windows Driver Kit** matching the SDK.
3. From the repo root:

```powershell
.\scripts\build-filter.ps1 -Configuration Release -Platform x64
```

Output is under `out\<platform>\<config>\filter\` when the WDK project succeeds.

If the script says WDK is missing, stop. Do not copy a random `.sys` from the internet into this tree.

## Load it (test machine only)

Secure Boot **on** a production PC will refuse a self-signed or test-signed kernel driver. That is the OS working correctly.

Development load (VM):

1. `bcdedit /set testsigning on` and reboot — **inside the VM**.
2. Sign the package with a code-signing test cert (`New-SelfSignedCertificate` / `signtool`).
3. Elevated: `.\scripts\install-filter.ps1`
4. Confirm the pad still feels **stock** (pass-through). If it does not, uninstall immediately.
5. Only then consider step 6 (suppression) on that same VM.

Recovery: `.\scripts\uninstall-filter.ps1`, Safe Mode, `pnputil /delete-driver oemXX.inf /uninstall`. Know the OEM name from `pnputil /enum-drivers` before you need it.

**Do not enable Driver Verifier on a daily laptop from these scripts.**

## Production signature (other people’s PCs)

Windows 10 1607+ loads new kernel drivers when **Microsoft** signed them via the Hardware Developer Program — not when you Authenticode-sign the `.sys` yourself.

There is no free consumer path:

| Approach | Loads with Secure Boot on Win11? |
| --- | --- |
| Self-signed / private CA | No |
| OV code signing only | No (apps, not kernel) |
| Azure Trusted Signing | No (not accepted for Hardware Program) |
| Test signing / DSE off | Not a product |
| EV cert + Partner Center **attestation** (or WHCP) | Yes, on Windows 10/11 **client** |

Attestation still needs a **legal org**, an **EV code-signing certificate**, and a Hardware Dashboard account. Microsoft attestation is the signing step; the EV cert is the identity that opens the door. Details: [driver-signing.md](driver-signing.md).

This project will not proxy someone else’s EV certificate.

## Binding rules (non-negotiable)

- Upper filter of Precision Touchpad collections only (`0x0D` / `0x05`).
- **Never** `mouclass` / `mouhid` / all mice. External mice and Raw Accel must keep their stack.
- `StartType = 3` (demand), not boot-start.
- Default mode pass-through; Custom only while user-mode heartbeats (75 ms) beat the 250 ms watchdog.

## If you are “just a user”

Run the app. Use Native. Open issues for detection/motion bugs. Do not install kernel code from an unsigned GitHub folder.
