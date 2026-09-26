param(
    [string]$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
)
$ErrorActionPreference = 'Stop'

$gritDir = $PSScriptRoot
$gritExe = Join-Path $gritDir 'grit.exe'
$manifestPath = Join-Path $gritDir 'image-build-manifest.json'
$images = Join-Path $Root 'images'
$topBarHeight = 19

if (!(Test-Path -LiteralPath $gritExe)) { throw "Missing Grit executable: $gritExe" }
if (!(Test-Path -LiteralPath $manifestPath)) { throw "Missing image manifest: $manifestPath" }
Add-Type -AssemblyName System.Drawing

function Get-ImageHeaderInfo([string]$Text) {
    $unitBits = if ($Text -match 'const\s+unsigned\s+short') { 16 } elseif ($Text -match 'const\s+unsigned\s+char') { 8 } else { throw 'Could not determine generated array unit type.' }
    $symbolMatch = [regex]::Match($Text, 'gImage_[A-Za-z0-9_]+')
    if (!$symbolMatch.Success) { throw 'Could not determine generated image symbol.' }
    $bodyMatch = [regex]::Match($Text, '(?s)=\s*\{(?<body>.*?)\};')
    if (!$bodyMatch.Success) { throw 'Could not locate generated image array.' }
    $body = [regex]::Replace($bodyMatch.Groups['body'].Value, '(?s)/\*.*?\*/', '')
    $hex = [regex]::Matches($body, '0[xX]([0-9A-Fa-f]+)')
    $values = New-Object 'System.UInt32[]' $hex.Count
    for ($i = 0; $i -lt $hex.Count; $i++) {
        $values[$i] = [Convert]::ToUInt32($hex[$i].Groups[1].Value, 16)
    }
    return [pscustomobject]@{ UnitBits = $unitBits; Symbol = $symbolMatch.Value; Values = $values }
}

function Compare-GeneratedHeader([string]$ReferencePath, [string]$GeneratedText, [string]$Label) {
    if (!(Test-Path -LiteralPath $ReferencePath)) { throw "Missing reference header: $ReferencePath" }
    $reference = Get-ImageHeaderInfo ([IO.File]::ReadAllText($ReferencePath))
    $generated = Get-ImageHeaderInfo $GeneratedText
    if ($reference.UnitBits -ne $generated.UnitBits) { throw ('{0}: unit mismatch (reference u{1}, generated u{2}).' -f $Label, $reference.UnitBits, $generated.UnitBits) }
    if ($reference.Symbol -ne $generated.Symbol) { throw ('{0}: symbol mismatch ({1} vs {2}).' -f $Label, $reference.Symbol, $generated.Symbol) }
    if ($reference.Values.Count -ne $generated.Values.Count) { throw ('{0}: data length mismatch ({1} vs {2}).' -f $Label, $reference.Values.Count, $generated.Values.Count) }
    for ($i = 0; $i -lt $reference.Values.Count; $i++) {
        if ($reference.Values[$i] -ne $generated.Values[$i]) {
            throw ('{0}: first data mismatch at element {1}: reference=0x{2:X}, generated=0x{3:X}' -f $Label, $i, $reference.Values[$i], $generated.Values[$i])
        }
    }
    Write-Host ("MATCH: {0}" -f $Label)
}

function Save-CompatibleBmp([string]$SrcPath, [string]$DstPath, [bool]$TopStripOnly = $false) {
    $srcImage = [System.Drawing.Image]::FromFile($SrcPath)
    try {
        $dstWidth = if ($TopStripOnly) { 240 } else { $srcImage.Width }
        $dstHeight = if ($TopStripOnly) { $topBarHeight } else { $srcImage.Height }
        $normalised = New-Object System.Drawing.Bitmap $dstWidth, $dstHeight, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($normalised)
            try {
                if ($TopStripOnly) {
                    $graphics.DrawImage($srcImage,
                        (New-Object System.Drawing.Rectangle 0, 0, 240, $topBarHeight),
                        (New-Object System.Drawing.Rectangle 0, 0, 240, $topBarHeight),
                        [System.Drawing.GraphicsUnit]::Pixel)
                } else {
                    $graphics.DrawImage($srcImage, 0, 0, $srcImage.Width, $srcImage.Height)
                }
            } finally { $graphics.Dispose() }
            $normalised.Save($DstPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
        } finally { $normalised.Dispose() }
    } finally { $srcImage.Dispose() }
}

function Invoke-GritText([string]$InputPath, [string]$Stem, [string]$Symbol, [int]$UnitBits, [string]$WorkDir) {
    $unitFlag = if ($UnitBits -eq 16) { '-gu16' } else { '-gu8' }
    Push-Location -LiteralPath $WorkDir
    try {
        & $gritExe $InputPath $unitFlag -gb -gB16 -ftc -s ($Symbol + '___')
        if ($LASTEXITCODE -ne 0) { throw "grit failed for $InputPath" }
        $cFile = Join-Path $WorkDir ($Stem + '.c')
        if (!(Test-Path -LiteralPath $cFile)) { throw "grit did not produce $cFile" }
        $text = Get-Content -LiteralPath $cFile -Raw
        $text = $text.Replace('___Bitmap', '').Replace('___', '')
        Remove-Item -LiteralPath $cFile -Force
        $generatedH = Join-Path $WorkDir ($Stem + '.h')
        if (Test-Path -LiteralPath $generatedH) { Remove-Item -LiteralPath $generatedH -Force }
        return $text
    } finally { Pop-Location }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('dsstyle-grit-verify-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$verified = 0
try {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($asset in $manifest.assets) {
        $sourcePath = Join-Path $Root ($asset.source -replace '/', '\')
        $headerPath = Join-Path $Root ($asset.header -replace '/', '\')
        if (!(Test-Path -LiteralPath $sourcePath)) { throw "Missing image source: $($asset.source)" }
        $stem = [IO.Path]::GetFileNameWithoutExtension($sourcePath)
        $tempInput = Join-Path $tempRoot ([IO.Path]::GetFileName($sourcePath))
        Copy-Item -LiteralPath $sourcePath -Destination $tempInput -Force
        $generated = Invoke-GritText $tempInput $stem ([string]$asset.symbol) ([int]$asset.unit_bits) $tempRoot
        Compare-GeneratedHeader $headerPath $generated $asset.header
        $verified++
    }

    $themeSpecs = @(
        @{ Folder='blank'; Suffix='BLANK'; Dark=$false; Base=$true },
        @{ Folder='pale_blue'; Suffix='PALE_BLUE'; Dark=$false; Base=$false },
        @{ Folder='light_blue'; Suffix='LIGHT_BLUE'; Dark=$false; Base=$false },
        @{ Folder='blue'; Suffix='BLUE'; Dark=$false; Base=$false },
        @{ Folder='dark_blue'; Suffix='DARK_BLUE'; Dark=$false; Base=$false },
        @{ Folder='green'; Suffix='GREEN'; Dark=$false; Base=$false },
        @{ Folder='pale_green'; Suffix='PALE_GREEN'; Dark=$false; Base=$false },
        @{ Folder='bright_green'; Suffix='BRIGHT_GREEN'; Dark=$false; Base=$false },
        @{ Folder='lime'; Suffix='LIME'; Dark=$false; Base=$false },
        @{ Folder='yellow'; Suffix='YELLOW'; Dark=$false; Base=$false },
        @{ Folder='red'; Suffix='RED'; Dark=$false; Base=$false },
        @{ Folder='orange'; Suffix='ORANGE'; Dark=$false; Base=$false },
        @{ Folder='brown'; Suffix='BROWN'; Dark=$false; Base=$false },
        @{ Folder='pink'; Suffix='PINK'; Dark=$false; Base=$false },
        @{ Folder='pale_pink'; Suffix='PALE_PINK'; Dark=$false; Base=$false },
        @{ Folder='magenta'; Suffix='MAGENTA'; Dark=$false; Base=$false },
        @{ Folder='purple'; Suffix='PURPLE'; Dark=$false; Base=$false },
        @{ Folder='dark'; Suffix='DARK'; Dark=$true; Base=$false }
    )
    $fullScreenNames = @('SD_LIST','SD_HORIZONTAL','SD_VERTICAL','SET','START','HELP')
    foreach ($spec in $themeSpecs) {
        $dir = Join-Path $images $spec.Folder
        if (!(Test-Path -LiteralPath $dir)) { continue }
        foreach ($bmp in (Get-ChildItem -LiteralPath $dir -Filter '*.bmp' -File)) {
            $input = [IO.Path]::GetFileNameWithoutExtension($bmp.Name)
            if ($input -eq 'NOR' -or $input -eq 'RECENTLY') { continue }
            if (!$spec.Dark -and !$spec.Base -and $fullScreenNames.Contains($input)) { continue }
            $topStripOnly = (!$spec.Dark -and !$spec.Base -and $input -eq $spec.Folder)
            $headerPath = Join-Path $dir ($input + '.h')
            if (!(Test-Path -LiteralPath $headerPath)) { throw "Missing reference theme header: $headerPath" }
            $referenceInfo = Get-ImageHeaderInfo ([IO.File]::ReadAllText($headerPath))
            $tempBmp = Join-Path $tempRoot ($spec.Folder + '_' + $bmp.Name)
            Save-CompatibleBmp $bmp.FullName $tempBmp $topStripOnly
            $generated = Invoke-GritText $tempBmp ([IO.Path]::GetFileNameWithoutExtension($tempBmp)) $referenceInfo.Symbol 8 $tempRoot
            Compare-GeneratedHeader $headerPath $generated ("images/{0}/{1}.h" -f $spec.Folder, $input)
            Remove-Item -LiteralPath $tempBmp -Force
            $verified++
        }
    }

    Write-Host "PASS: $verified image header(s) reproduced exactly with bundled Grit."
} finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
