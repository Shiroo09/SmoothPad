param()
$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "uninstall-filter.ps1 must run elevated"
}

# Fail-open first: if the service exists, stop custom mode by unloading.
Stop-Service SmoothPadFilter -ErrorAction SilentlyContinue
$pnputil = "$env:SystemRoot\System32\pnputil.exe"
& $pnputil /enum-drivers | Out-Null
Write-Host "Removing published SmoothPadFilter OEM INF entries if present..."
& $pnputil /enum-drivers |
    Select-String -Pattern "Published Name|Original Name" |
    ForEach-Object { $_.Line }

# Best-effort: delete matching OEM packages named SmoothPadFilter.inf
$oem = & $pnputil /enum-drivers
$blocks = $oem -join "`n"
# Users should confirm the OEM name from /enum-drivers. We do not blindly wipe HID UpperFilters.
Write-Host "Manually run: pnputil /delete-driver oemXX.inf /uninstall /force  for the SmoothPadFilter published name."
Write-Host "Then verify no orphaned UpperFilters remain on HID\VID_* touchpad instances."
