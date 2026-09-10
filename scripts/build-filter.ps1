# Builds SmoothPadFilter.sys when the Windows Driver Kit is installed.
# Does NOT sign or install the driver. See docs/byo-driver.md.

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$proj = Join-Path $root "src\SmoothPad.Filter\SmoothPadFilter.vcxproj"
if (-not (Test-Path $proj)) { throw "Filter project missing: $proj" }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Install Visual Studio." }

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with C++ toolset not found" }

$msbuild = Join-Path $vs "MSBuild\Current\Bin\MSBuild.exe"
$wdkProps = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\build\WindowsDriver.Common.props"
$wdkAlt = Get-ChildItem (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\build") -Filter "WindowsDriver.Common.props" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not (Test-Path $wdkProps) -and -not $wdkAlt) {
    throw "Windows Driver Kit not found. User-mode SmoothPad does not need it; the filter does. See docs/byo-driver.md"
}

Write-Host "Building SmoothPadFilter ($Configuration|$Platform). Output is unsigned."
Write-Host "This script will not install the driver or disable Secure Boot."

& $msbuild $proj /m /p:Configuration=$Configuration /p:Platform=$Platform /p:SignMode=Off
if ($LASTEXITCODE -ne 0) { throw "filter build failed" }

Write-Host "If the WDK project succeeded, look under out\$Platform\$Configuration\filter\"
Write-Host "Next: sign (test cert in a VM, or Hardware Dev Center for other PCs). Do not pnputil this onto a daily laptop."
