$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$out = Join-Path (Split-Path -Parent $PSScriptRoot) "packaging\msix\Assets"
New-Item -ItemType Directory -Force -Path $out | Out-Null

function New-Logo([string]$name, [int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::FromArgb(32, 32, 32))
    $accent = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(76, 194, 255))
    $pad = [Math]::Max(4, [int]($h * 0.18))
    $g.FillEllipse($accent, $pad, $pad, $w - 2 * $pad, $h - 2 * $pad)
    $path = Join-Path $out $name
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
}

New-Logo "StoreLogo.png" 50 50
New-Logo "Square44x44Logo.png" 44 44
New-Logo "Square150x150Logo.png" 150 150
New-Logo "Wide310x150Logo.png" 310 150
Write-Host "Wrote MSIX assets to $out"
