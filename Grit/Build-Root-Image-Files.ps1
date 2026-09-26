param(
    [string]$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
)
$ErrorActionPreference = 'Stop'

$gritDir = $PSScriptRoot
$gritExe = Join-Path $gritDir 'grit.exe'
$manifestPath = Join-Path $gritDir 'image-build-manifest.json'

if (!(Test-Path -LiteralPath $gritExe)) { throw "Missing Grit executable: $gritExe" }
if (!(Test-Path -LiteralPath $manifestPath)) { throw "Missing image manifest: $manifestPath" }

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('dsstyle-grit-root-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    $built = 0
    foreach ($asset in $manifest.assets) {
        $sourcePath = Join-Path $Root ($asset.source -replace '/', '\')
        $headerPath = Join-Path $Root ($asset.header -replace '/', '\')
        if (!(Test-Path -LiteralPath $sourcePath)) { throw "Missing image source: $($asset.source)" }

        $stem = [IO.Path]::GetFileNameWithoutExtension($sourcePath)
        $unitFlag = if ([int]$asset.unit_bits -eq 16) { '-gu16' } else { '-gu8' }
        $symbolPrefix = [string]$asset.symbol + '___'

        $tempInput = Join-Path $tempRoot ([IO.Path]::GetFileName($sourcePath))
        Copy-Item -LiteralPath $sourcePath -Destination $tempInput -Force
        Push-Location -LiteralPath $tempRoot
        try {
            & $gritExe $tempInput $unitFlag -gb -gB16 -ftc -s $symbolPrefix
            if ($LASTEXITCODE -ne 0) { throw "grit failed for $($asset.source)" }

            $cFile = Join-Path $tempRoot ($stem + '.c')
            if (!(Test-Path -LiteralPath $cFile)) { throw "grit did not produce $cFile" }
            $text = Get-Content -LiteralPath $cFile -Raw
            $text = $text.Replace('___Bitmap', '').Replace('___', '')
            Set-Content -LiteralPath $headerPath -Value $text -NoNewline -Encoding ASCII

            Remove-Item -LiteralPath $cFile -Force
            $generatedH = Join-Path $tempRoot ($stem + '.h')
            if (Test-Path -LiteralPath $generatedH) { Remove-Item -LiteralPath $generatedH -Force }
        }
        finally {
            Pop-Location
        }
        $built++
    }
    Write-Host "PASS: regenerated $built root image header(s) with bundled Grit."
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
