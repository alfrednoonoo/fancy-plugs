# Downloads the newest successful build of the current branch and installs it.
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$branch = (git -C $repoRoot rev-parse --abbrev-ref HEAD).Trim()

$runId = gh run list --workflow build.yml --branch $branch --status success --limit 1 --json databaseId --jq '.[0].databaseId'
if (-not $runId) {
    throw "No successful build found for branch '$branch'"
}

$downloadDir = Join-Path ([IO.Path]::GetTempPath()) "fancy-plugs-$runId"
if (Test-Path $downloadDir) {
    Remove-Item -Recurse -Force -Path $downloadDir
}

Write-Host "Downloading build $runId of '$branch'..."
gh run download $runId --pattern 'FancyPlugs-VST3-*' --dir $downloadDir
if ($LASTEXITCODE -ne 0) {
    throw 'Download failed'
}

& (Join-Path $PSScriptRoot 'install.ps1') -Source $downloadDir

Remove-Item -Recurse -Force -Path $downloadDir
