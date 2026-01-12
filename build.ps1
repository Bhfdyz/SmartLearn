$ErrorActionPreference = "Continue"
cd "D:\code\c++\Qt\SmartLearn-main"

# Try to find qmake
$qmakePaths = @(
    "D:\Qt\6.8.0\msvc2022_64\bin\qmake.exe",
    "D:\Qt\6.6.0\msvc2022_64\bin\qmake.exe",
    "D:\Qt\5.15.2\msvc2019_64\bin\qmake.exe",
    "C:\Qt\6.8.0\msvc2022_64\bin\qmake.exe"
)

$qmake = $null
foreach ($path in $qmakePaths) {
    if (Test-Path $path) {
        $qmake = $path
        break
    }
}

if ($null -eq $qmake) {
    Write-Host "ERROR: qmake not found!"
    exit 1
}

Write-Host "Using qmake: $qmake"
& $qmake

if ($LASTEXITCODE -ne 0) {
    Write-Host "qmake failed"
    exit 1
}

# Try to find jom or nmake
$jom = "D:\Qt\Tools\QtCreator\bin\jom.exe"
$nmake = "nmake.exe"

if (Test-Path $jom) {
    Write-Host "Building with jom..."
    & $jom
} elseif (Get-Command nmake -ErrorAction SilentlyContinue) {
    Write-Host "Building with nmake..."
    & nmake
} else {
    Write-Host "ERROR: No build tool found (jom or nmake)"
    exit 1
}

Write-Host "Build complete!"
