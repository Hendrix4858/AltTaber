# AltTaber Installer Build Script
# Requires: CMake, MSVC 2022, Qt 6, Inno Setup 6

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "build"
$ReleaseDir  = Join-Path $BuildDir "Release"
$QtDir       = $env:QT_DIR
if ($env:ISCC) {
    $InnoCompiler = $env:ISCC
} else {
    $InnoCompiler = @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup\ISCC.exe",
        "C:\Program Files\Inno Setup\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $InnoCompiler) { throw "ISCC.exe not found. Set `$env:ISCC or install Inno Setup 6." }

# Step 1: Build Release
Write-Host "=== Building Release ==="
Push-Location $ProjectRoot
& cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# Step 2: Deploy Qt DLLs
Write-Host "=== Deploying Qt DLLs ==="
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
& $windeployqt --release --no-translations --no-opengl-sw (Join-Path $ReleaseDir "AltTaber.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Step 3: Compile Inno Setup installer
Write-Host "=== Compiling Installer ==="
if (-not (Test-Path $InnoCompiler)) {
    throw "Inno Setup compiler not found at $InnoCompiler"
}
& $InnoCompiler (Join-Path $ProjectRoot "installer\setup.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }

Pop-Location
Write-Host "=== Done ==="
Get-ChildItem (Join-Path $ProjectRoot "build\output")
