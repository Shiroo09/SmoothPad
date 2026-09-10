# Security Policy

## Supported versions

| Version | Supported |
| ------- | --------- |
| 0.1.x   | yes       |

## Reporting a vulnerability

Report privately via [GitHub Security Advisories](https://github.com/Shiroo09/SmoothPad/security/advisories/new) on this repository.

Do **not** open a public issue for:

- kernel filter bugs
- privilege-escalation in the driver IOCTL path
- input injection that can be triggered by an unprivileged process in another session

If a driver is installed, treat `SmoothPadFilter.sys` as a trusted kernel component. Reports should include:

- SmoothPad version
- Windows version and Secure Boot state
- whether the filter was loaded
- a minimal repro that does not require exploiting third-party software

## Signing stance

This project does not load unsigned production drivers and does not document bypasses of Secure Boot, Driver Signature Enforcement, or Test Signing as a supported install path.

Filter source is provided so contributors with WDK and (for distribution) Hardware Dev Center signing can build their own binary. A self-built `.sys` is the builder’s responsibility.
