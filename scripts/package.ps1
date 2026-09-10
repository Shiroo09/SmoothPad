param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$assets = Join-Path $root "packaging\msix\Assets"
if (-not (Test-Path (Join-Path $assets "StoreLogo.png"))) {
    & (Join-Path $PSScriptRoot "gen-msix-assets.ps1")
}
& (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration -Platform $Platform

$out = Join-Path $root "out\$Platform\$Configuration"
$stage = Join-Path $root "out\package\$Platform\$Configuration"
$pack = Join-Path $root "packaging\msix"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item (Join-Path $out "SmoothPad.App.exe") (Join-Path $stage "SmoothPad.exe")
Copy-Item (Join-Path $out "SmoothPad.Cli.exe") (Join-Path $stage "SmoothPad.Cli.exe") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $pack "AppxManifest.xml") (Join-Path $stage "AppxManifest.xml")
Copy-Item (Join-Path $pack "Assets") (Join-Path $stage "Assets") -Recurse -ErrorAction SilentlyContinue

$manifest = Get-Content (Join-Path $stage "AppxManifest.xml") -Raw
$manifest = $manifest -replace "0\.1\.0\.0", "$Version.0"
Set-Content -Path (Join-Path $stage "AppxManifest.xml") -Value $manifest -NoNewline

$msixDir = Join-Path $root "out\msix"
New-Item -ItemType Directory -Force -Path $msixDir | Out-Null
$msix = Join-Path $msixDir "SmoothPad-$Version-$Platform.msix"

$sdkBin = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "x64\makeappx.exe" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1

if ($sdkBin) {
    & $sdkBin pack /d $stage /p $msix /o
} else {
    Write-Warning "makeappx.exe not found; staged unpackaged layout at $stage"
}

$lines = @()
Get-ChildItem $out -Filter "*.exe" | ForEach-Object {
    $hash = Get-FileHash $_.FullName -Algorithm SHA256
    $lines += "{0}  {1}" -f $hash.Hash.ToLowerInvariant(), $_.Name
}
if (Test-Path $msix) {
    $hash = Get-FileHash $msix -Algorithm SHA256
    $lines += "{0}  {1}" -f $hash.Hash.ToLowerInvariant(), (Split-Path $msix -Leaf)
}
$lines | Set-Content (Join-Path $msixDir "SHA256SUMS")
Write-Host "Package output: $msixDir"
