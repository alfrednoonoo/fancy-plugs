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

$toolsTarget = Join-Path $env:USERPROFILE 'VST3-Dev-Tools'

# JUCE's AudioPluginHost, for checking sidechain and MIDI routing without a DAW.
$pluginHost = Join-Path $Source 'tools\AudioPluginHost.exe'
if (Test-Path $pluginHost) {
    New-Item -ItemType Directory -Force -Path $toolsTarget | Out-Null
    Copy-Item -Path $pluginHost -Destination $toolsTarget -Force
    Write-Host "Installed AudioPluginHost.exe in $toolsTarget"
}

# FancyBench, for measuring our plugins against others (docs/bench.md). The example test files are
# only copied if they aren't there yet, so edited ones are kept.
$bench = Join-Path $Source 'tools\FancyBench.exe'
if (Test-Path $bench) {
    New-Item -ItemType Directory -Force -Path $toolsTarget | Out-Null
    Copy-Item -Path $bench -Destination $toolsTarget -Force
    $examplesTarget = Join-Path $toolsTarget 'examples'
    New-Item -ItemType Directory -Force -Path $examplesTarget | Out-Null
    foreach ($example in @(Get-ChildItem -Path (Join-Path $Source 'tools\examples') -Filter '*.json' -ErrorAction SilentlyContinue)) {
        $destination = Join-Path $examplesTarget $example.Name
        if (-not (Test-Path $destination)) { Copy-Item -Path $example.FullName -Destination $destination }
    }
    Write-Host "Installed FancyBench.exe and its examples in $toolsTarget"
}

Write-Host 'Done - in REAPER: Options > Preferences > Plug-ins > VST > Re-scan'
