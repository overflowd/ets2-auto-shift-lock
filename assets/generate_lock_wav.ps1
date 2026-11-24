# Generate a crisp, subtle mechanical shift-lock warning WAV sound (16-bit 44.1 kHz mono PCM)
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "lock_warning.wav")
)

$sampleRate = 44100
$durationSec = 0.20
$totalSamples = [int]($sampleRate * $durationSec)
$dataBytes = $totalSamples * 2

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)

# RIFF header
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
$bw.Write([int](36 + $dataBytes))
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))

# fmt chunk
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
$bw.Write([int]16)          # PCM chunk size
$bw.Write([int16]1)         # PCM format
$bw.Write([int16]1)         # Mono
$bw.Write([int]$sampleRate) # Sample rate
$bw.Write([int]($sampleRate * 2)) # Byte rate
$bw.Write([int16]2)         # Block align
$bw.Write([int16]16)        # Bits per sample

# data chunk
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
$bw.Write([int]$dataBytes)

$twoPi = 2.0 * [Math]::PI
for ($i = 0; $i -lt $totalSamples; $i++) {
    $t = $i / [double]$sampleRate
    $sample = 0.0

    if ($t -ge 0.0 -and $t -lt 0.065) {
        $lt = $t
        $attack = [Math]::Min(1.0, $lt / 0.004)
        $decay = [Math]::Exp(-$lt * 42.0)
        $env = $attack * $decay
        $sample = $env * (0.65 * [Math]::Sin($twoPi * 320.0 * $lt) +
                          0.35 * [Math]::Sin($twoPi * 640.0 * $lt))
    } elseif ($t -ge 0.105 -and $t -lt 0.185) {
        $lt = $t - 0.105
        $attack = [Math]::Min(1.0, $lt / 0.004)
        $decay = [Math]::Exp(-$lt * 34.0)
        $env = $attack * $decay
        $sample = $env * (0.70 * [Math]::Sin($twoPi * 260.0 * $lt) +
                          0.30 * [Math]::Sin($twoPi * 520.0 * $lt))
    }

    $pcm = [int][Math]::Round($sample * 24000.0)
    if ($pcm -gt 32767) { $pcm = 32767 }
    if ($pcm -lt -32767) { $pcm = -32767 }
    $bw.Write([int16]$pcm)
}

$bw.Flush()
[System.IO.File]::WriteAllBytes($OutputPath, $ms.ToArray())
$bw.Dispose()
$ms.Dispose()
Write-Output "Generated $OutputPath ($((Get-Item $OutputPath).Length) bytes)"
