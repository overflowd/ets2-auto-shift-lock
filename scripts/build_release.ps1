# Build and package ETS2 Auto Shift-Lock Plugin (Windows x64)
param(
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

# Auto-discover WinLibs MinGW-w64 if installed via winget
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    $mingwBin = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "mingw64\bin" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    if ($mingwBin) {
        $env:Path = "$mingwBin;$env:Path"
    }
}

Write-Host "==> 1. Building C++ x64 plugin DLL & running C++ unit tests..."
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build build
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
ctest --test-dir build --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "C++ unit tests failed" }

Write-Host "==> 2. Ensuring default warning WAV exists..."
if (-not (Test-Path "assets/lock_warning.wav")) {
    powershell -ExecutionPolicy Bypass -File "assets/generate_lock_wav.ps1"
    if ($LASTEXITCODE -ne 0) { throw "WAV generation failed" }
}

Write-Host "==> 3. Running .NET 9 Configurator unit tests..."
dotnet restore configurator/AutoShiftLockConfig.sln
if ($LASTEXITCODE -ne 0) { throw "dotnet restore failed" }
dotnet test configurator/AutoShiftLockConfig.sln --configuration Release --no-restore
if ($LASTEXITCODE -ne 0) { throw ".NET unit tests failed" }

Write-Host "==> 4. Publishing self-contained AutoShiftLockConfig.exe (win-x64)..."
$publishDir = Join-Path $repoRoot "build/publish_configurator"
if (Test-Path $publishDir) { Remove-Item -Recurse -Force $publishDir }
dotnet publish configurator/src/AutoShiftLockConfig/AutoShiftLockConfig.csproj `
    -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true `
    -o $publishDir
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed" }

Write-Host "==> 5. Staging release ZIP in dist/..."
$stageDir = Join-Path $repoRoot "dist/stage"
$zipPath = Join-Path $repoRoot "dist/ets2-auto-shift-lock-v$Version-win-x64.zip"
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }

New-Item -ItemType Directory -Force -Path (Join-Path $stageDir "plugin") | Out-Null
Copy-Item (Join-Path $publishDir "AutoShiftLockConfig.exe") (Join-Path $stageDir "AutoShiftLockConfig.exe")
Copy-Item (Join-Path $repoRoot "build/ets2_auto_shift_lock.dll") (Join-Path $stageDir "plugin/ets2_auto_shift_lock.dll")
Copy-Item (Join-Path $repoRoot "assets/lock_warning.wav") (Join-Path $stageDir "plugin/lock_warning.wav")
Copy-Item (Join-Path $repoRoot "README.md") (Join-Path $stageDir "README.md")

# Use System.IO.Compression to avoid PowerShell Expand/Compress-Archive console Write-Progress issues
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stageDir, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
Remove-Item -Recurse -Force $stageDir

Write-Host "==> Done! Release archive: $zipPath ($((Get-Item $zipPath).Length) bytes)"
