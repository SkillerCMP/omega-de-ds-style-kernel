param(
    [string]$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path,
    [switch]$SkipVerify
)
$ErrorActionPreference = 'Stop'

if (!$SkipVerify) {
    & (Join-Path $PSScriptRoot 'Verify-Image-Headers.ps1') -Root $Root
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& (Join-Path $PSScriptRoot 'Build-Root-Image-Files.ps1') -Root $Root
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'Build Skin Files.ps1')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'PASS: all DS Style image headers regenerated from source pictures.'
