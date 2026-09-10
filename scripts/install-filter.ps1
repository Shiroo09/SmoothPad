# Development-only: stages the KMDF filter with the Microsoft-supported test-signing workflow.
# Requires: WDK-built SmoothPadFilter.sys next to the INF, test-signing ON, elevated prompt.
# VM / test machine only. Never disable Secure Boot on a daily laptop for this.
# Never run Driver Verifier automatically on a production laptop.

param(
    [string]$Inf = (Join-Path $PSScriptRoot "..\src\SmoothPad.Filter\SmoothPadFilter.inf")
)

$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "install-filter.ps1 must run elevated"
}

Write-Host "This installs SmoothPadFilter as an upper filter of HID digitizer/touchpad collections."
Write-Host "Default driver mode is PASS-THROUGH. Custom suppression requires a live heartbeat from SmoothPad.exe."
if (-not (Test-Path $Inf)) { throw "INF not found: $Inf" }

$pnputil = "$env:SystemRoot\System32\pnputil.exe"
& $pnputil /add-driver $Inf /install
if ($LASTEXITCODE -ne 0) { throw "pnputil failed" }
Write-Host "Installed. If touchpad input is lost, run uninstall-filter.ps1 or boot Safe Mode."
