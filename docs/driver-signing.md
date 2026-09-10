# Driver signing (2026)

This repository does **not** ship a production `SmoothPadFilter.sys`. If you build one, **you** sign it. Production filter packages **must not** ship unsigned.

Windows 10 version 1607 and later load **new** kernel drivers when Microsoft has signed them through the Hardware Developer Program. Your Authenticode/EV signature on a CAB authenticates the *submission*. The signature the kernel checks is Microsoft’s.

There is no consumer workaround that keeps Secure Boot on:

| Path | Result on Windows 11 + Secure Boot |
| --- | --- |
| Self-signed / private CA | Rejected |
| OV Authenticode | Fine for `SmoothPad.exe`; not a kernel signature |
| Azure Trusted Signing | Not accepted for Hardware Program / kernel |
| Test signing / DSE off | Dev VM only — not a supported user install |
| EV + Partner Center attestation or WHCP | Loads on Windows 10/11 **client** |

Attestation does **not** need HLK. It still needs a legal organization, an EV code-signing certificate, and a Hardware Dashboard account. Microsoft has described attestation as testing-oriented in some 2026 docs; re-read the dashboard at submission time. Windows Server does not accept attestation.

This project will not tell users to turn off Secure Boot.

## Development (VM)

1. Test machine / VM, not the only laptop.
2. Enable test signing **in the VM**: `bcdedit /set testsigning on` then reboot.
3. Sign with a test certificate (`New-SelfSignedCertificate` Code Signing).
4. `pnputil /add-driver SmoothPadFilter.inf /install`

See [byo-driver.md](byo-driver.md).

## Production (Hardware Developer Program)

Verify at submission time on [partner.microsoft.com/dashboard](https://partner.microsoft.com/dashboard) / Hardware:

1. Organization in Partner Center, Hardware Developer Program enrolled.
2. EV code-signing certificate registered (Azure Trusted Signing is **not** a substitute).
3. Attestation or WHCP submission of the driver package (CAB with INF + catalog).
4. Microsoft-signed artifacts downloaded and bundled in **your** installer — not this GitHub repo unless you are the signer.
5. PE files of the installer itself Authenticode-signed.

Secure Boot systems will not load a test-signed SmoothPadFilter.

## CI

Signing steps are optional. `scripts/sign.ps1` no-ops unless `SMOOTHPAD_SIGNING_PFX` is set. Never commit PFX/keys. GitHub Actions here build **user-mode** binaries only.

## Cost / identity (not a SmoothPad fee)

EV certificates are sold by CAs Microsoft lists (Certum, DigiCert, GlobalSign, Sectigo, …). Hardware Dashboard registration itself is not a Microsoft “driver tax,” but EV validation requires an organization. Individuals generally cannot open that door. Do not use a freelancer’s EV to sign this driver; the publisher of record is whoever owns the cert.

