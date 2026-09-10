param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with C++ toolset not found" }
$msbuild = Join-Path $vs "MSBuild\Current\Bin\MSBuild.exe"
& $msbuild (Join-Path $root "SmoothPad.sln") /m /p:Configuration=$Configuration /p:Platform=$Platform /p:RestorePackages=false
if ($LASTEXITCODE -ne 0) { throw "build failed" }
$out = Join-Path $root "out\$Platform\$Configuration"
Write-Host "Output: $out"
