# Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [string]$QtPrefixPath = "",
    [switch]$Clean,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildPath = Join-Path $repoRoot $BuildDir

function Resolve-CMakePath {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $vsGlob = "C:\Program Files\Microsoft Visual Studio\2022\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    $matches = Get-ChildItem -Path $vsGlob -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
    if ($matches -and $matches.Count -gt 0) { return $matches[0].FullName }
    throw "CMake not found. Install CMake or Visual Studio 2022."
}

$cmake = Resolve-CMakePath
if ($Clean -and (Test-Path $buildPath)) { Remove-Item -Recurse -Force $buildPath }
if (-not (Test-Path $buildPath)) { New-Item -ItemType Directory -Path $buildPath | Out-Null }

if (-not $QtPrefixPath) {
    $breadcrumb = Join-Path $repoRoot ".qt-path"
    if (Test-Path $breadcrumb) {
        $line = Get-Content $breadcrumb | Where-Object { $_ -match "^QT_PREFIX_PATH=(.+)$" } | Select-Object -First 1
        if ($line -match "^QT_PREFIX_PATH=(.+)$") { $QtPrefixPath = $Matches[1].Trim() }
    }
}

$configureArgs = @("-S", $repoRoot, "-B", $buildPath, "-G", "Visual Studio 17 2022", "-A", "x64")
# VTK's Qt widget was built with the vcpkg Qt 6.10, so the app links that Qt rather than the 6.8 kit.
$vcpkgPrefix = "C:\Users\Adam\vcpkg\installed\x64-windows"
if (Test-Path (Join-Path $vcpkgPrefix "share\vtk\vtk-config.cmake")) {
    $configureArgs += @(
        "-DCMAKE_PREFIX_PATH=$vcpkgPrefix",
        "-DQt6_DIR=$vcpkgPrefix\share\Qt6",
        "-DVTK_DIR=$vcpkgPrefix\share\vtk"
    )
} elseif ($QtPrefixPath) {
    $configureArgs += @("-DCMAKE_PREFIX_PATH=$QtPrefixPath")
}
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($ConfigureOnly) { exit 0 }

& $cmake --build $buildPath --config $Config --target WeldingTableGenerator
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build complete."
Write-Host "  $buildPath\$Config\Welding Table Generator.exe"
