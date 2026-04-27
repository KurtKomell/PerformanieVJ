# Runs cmake configure + build inside the MSVC x64 environment (vcvars64).
# Use this from a normal PowerShell when you get:
#   No CMAKE_CXX_COMPILER could be found
#
# Default targets Visual Studio 2026 (presets windows-vs2026-*). For VS 2022 use -Vs2022.
#
# Usage (from repo root):
#   .\scripts\cmake-with-msvc.ps1
#   .\scripts\cmake-with-msvc.ps1 -Vs2022
#   .\scripts\cmake-with-msvc.ps1 -ConfigurePreset windows-debug -BuildPreset windows-debug

param(
    [string] $ConfigurePreset = "",
    [string] $BuildPreset = "",
    [switch] $Vs2022
)

$ErrorActionPreference = "Stop"

function Test-CMakeSupportsVs2026Generator {
    $line = cmake --version 2>$null | Select-Object -First 1
    if ($line -match 'cmake version (\d+)\.(\d+)') {
        $maj = [int]$Matches[1]
        $min = [int]$Matches[2]
        return ($maj -gt 4) -or ($maj -eq 4 -and $min -ge 2)
    }
    return $false
}

if ($ConfigurePreset -eq "") {
    if ($Vs2022) {
        $ConfigurePreset = "windows-vs-debug"
    } else {
        $ConfigurePreset = "windows-vs2026-debug"
    }
}
if ($BuildPreset -eq "") {
    $BuildPreset = $ConfigurePreset
}

if ($ConfigurePreset -match 'vs2026' -or $BuildPreset -match 'vs2026') {
    if (-not (Test-CMakeSupportsVs2026Generator)) {
        Write-Host @"

The presets 'windows-vs2026-*' need CMake 4.2 or newer (generator 'Visual Studio 18 2026').
Your CMake is older. Either upgrade CMake from https://cmake.org/download/
or use Visual Studio 2022 presets, e.g.:
  .\scripts\cmake-with-msvc.ps1 -Vs2022

"@ -ForegroundColor Red
        exit 1
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host @"

vswhere.exe not found. Install Visual Studio (2022 or 2026) or Build Tools with workload
'Desktop development with C++' from https://visualstudio.microsoft.com/downloads/

"@ -ForegroundColor Red
    exit 1
}

$installPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null
if (-not $installPath) {
    Write-Host @"

No MSVC C++ toolchain found. Open Visual Studio Installer, modify your install,
and enable 'Desktop development with C++' (includes MSVC, Windows SDK).

"@ -ForegroundColor Red
    exit 1
}

$vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Host "Expected file missing: $vcvars" -ForegroundColor Red
    exit 1
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$inner = "call `"$vcvars`" && cd /d `"$repoRoot`" && cmake --preset `"$ConfigurePreset`" && cmake --build --preset `"$BuildPreset`""
cmd /c $inner
exit $LASTEXITCODE
