# Convert a square PNG into a multi-size Windows .ico (16, 32, 48, 64, 128, 256 px PNG-in-ICO)
param(
    [string]$InputPng = (Join-Path $PSScriptRoot "icon.png"),
    [string]$OutputIco = (Join-Path $PSScriptRoot "app_icon.ico")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $InputPng)) {
    throw "Input PNG not found: $InputPng"
}

$sizes = @(16, 32, 48, 64, 128, 256)
$pngBlobs = @()

$src = [System.Drawing.Image]::FromFile($InputPng)
try {
    foreach ($sz in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap($sz, $sz, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $g.DrawImage($src, 0, 0, $sz, $sz)
            } finally {
                $g.Dispose()
            }
            $ms = New-Object System.IO.MemoryStream
            try {
                $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
                $pngBlobs += ,($ms.ToArray())
            } finally {
                $ms.Dispose()
            }
        } finally {
            $bmp.Dispose()
        }
    }
} finally {
    $src.Dispose()
}

# Write standard ICO header + directory + PNG payloads
$outMs = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($outMs)
try {
    # ICONDIR (6 bytes)
    $bw.Write([uint16]0)             # reserved
    $bw.Write([uint16]1)             # type = 1 (icon)
    $bw.Write([uint16]$sizes.Count)  # image count

    $offset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $sz = $sizes[$i]
        $blob = $pngBlobs[$i]
        $dimByte = if ($sz -ge 256) { [byte]0 } else { [byte]$sz }
        $bw.Write($dimByte)          # width
        $bw.Write($dimByte)          # height
        $bw.Write([byte]0)           # color count
        $bw.Write([byte]0)           # reserved
        $bw.Write([uint16]1)         # planes
        $bw.Write([uint16]32)        # bit count
        $bw.Write([uint32]$blob.Length)
        $bw.Write([uint32]$offset)
        $offset += $blob.Length
    }

    foreach ($blob in $pngBlobs) {
        $bw.Write($blob)
    }

    $bw.Flush()
    [System.IO.File]::WriteAllBytes($OutputIco, $outMs.ToArray())
} finally {
    $bw.Dispose()
    $outMs.Dispose()
}

Write-Output "Generated $OutputIco ($((Get-Item $OutputIco).Length) bytes, sizes: $($sizes -join ','))"
