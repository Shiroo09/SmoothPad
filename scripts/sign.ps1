# Signing is optional. Without secrets this script no-ops.
param(
    [string]$Path,
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
if (-not $env:SMOOTHPAD_SIGNING_PFX) {
    Write-Host "SMOOTHPAD_SIGNING_PFX not set; leaving $Path unsigned (development artifact)."
    exit 0
}
if (-not (Test-Path $Path)) { throw "missing $Path" }
$signtool = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter signtool.exe |
    Where-Object { $_.DirectoryName -match "x64" } |
    Select-Object -First 1
if (-not $signtool) { throw "signtool.exe not found" }
& $signtool.FullName sign /fd SHA256 /td SHA256 /tr $TimestampUrl /f $env:SMOOTHPAD_SIGNING_PFX /p $env:SMOOTHPAD_SIGNING_PASSWORD $Path
