param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration -Platform $Platform
$exe = Join-Path $root "out\$Platform\$Configuration\SmoothPad.Tests.exe"
if (-not (Test-Path $exe)) { throw "tests executable missing: $exe" }
& $exe
if ($LASTEXITCODE -ne 0) { throw "tests failed" }
