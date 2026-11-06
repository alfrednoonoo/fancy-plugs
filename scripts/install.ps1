# Copies every .vst3 bundle under -Source into %USERPROFILE%\VST3-Dev (no admin rights needed).
# Ships inside the build artifact, and scripts\get-latest.ps1 calls it after downloading one.
param(
    [string]$Source = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

$bundles = @(Get-ChildItem -Path $Source -Recurse -Directory -Filter '*.vst3')
if ($bundles.Count -eq 0) {
    throw "No .vst3 bundles found under $Source"
}

if (Get-Process -Name 'reaper' -ErrorAction SilentlyContinue) {
    Write-Host 'REAPER is running. Close any project using these plugins first: Windows will not overwrite a loaded DLL.'
    Read-Host 'Press Enter when that is done'
}

$target = Join-Path $env:USERPROFILE 'VST3-Dev'
New-Item -ItemType Directory -Force -Path $target | Out-Null

foreach ($bundle in $bundles) {
    $destination = Join-Path $target $bundle.Name
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Copy-Item -Path (Join-Path $bundle.FullName '*') -Destination $destination -Recurse -Force
    Write-Host "Installed $($bundle.Name)"
}

Write-Host 'Done - in REAPER: Options > Preferences > Plug-ins > VST > Re-scan'
