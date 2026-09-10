# Privacy policy

SmoothPad does not require an account and has no cloud backend.

- Settings are stored locally (`%AppData%\SmoothPad\settings.json`).
- There is no telemetry by default and no analytics SDK.
- Raw touch coordinates never leave the machine.
- Diagnostic traces are written only after an explicit user action (`SmoothPad.Cli record` or an in-app trace).
- Startup registration uses the documented HKCU Run key when the user enables it.

A future optional “check GitHub releases” feature, if added, will be off by default.
