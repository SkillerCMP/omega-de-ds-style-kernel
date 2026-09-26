param(
    [string]$Root = $PSScriptRoot,
    [string]$ArmGcc = 'arm-none-eabi-gcc'
)
$ErrorActionPreference = 'Stop'
# No Python, libgba link, runtime execution, or source-tree outputs required.
# Compile exactly the assembly files consumed by this project's Makefile.
$Workspace = $null
try {
    $SourceRoot = (Resolve-Path -LiteralPath $Root).Path
    $SourceDir = Join-Path $SourceRoot 'source'
    $Compiler = (Get-Command -Name $ArmGcc -CommandType Application -ErrorAction Stop).Source
    $Sources = @(Get-ChildItem -LiteralPath $SourceDir -Filter '*.s' -File | Sort-Object Name)
    if ($Sources.Count -eq 0) { throw 'No assembly source files found.' }
    $Workspace = Join-Path ([System.IO.Path]::GetTempPath()) ('dsstyle-asm-' + [guid]::NewGuid().ToString('N'))
    [System.IO.Directory]::CreateDirectory($Workspace) | Out-Null
    foreach ($Source in $Sources) {
        $Object = Join-Path $Workspace ($Source.BaseName + '.o')
        $Dependency = Join-Path $Workspace ($Source.BaseName + '.d')
        # Match the assembler-with-cpp, -mthumb, interworking, and include flags
        # from the logged devkitARM r54 Makefile command. Each source selects
        # its own .arm/.thumb sections just as it does during the normal build.
        $CompilerArgs = @(
            '-MMD', '-MP', '-MF', $Dependency,
            '-x', 'assembler-with-cpp', '-g', '-mthumb', '-mthumb-interwork',
            '-I', (Join-Path $SourceRoot 'build/source'), '-I', $SourceDir,
            '-c', $Source.FullName, '-o', $Object
        )
        Write-Host ('Assembly preflight: ' + $Source.Name)
        & $Compiler @CompilerArgs
        if ($LASTEXITCODE -ne 0) { throw ($Source.Name + ': assembler exit code ' + $LASTEXITCODE) }
        if (-not (Test-Path -LiteralPath $Object -PathType Leaf)) { throw ($Source.Name + ': no object produced') }
        if ((Get-Item -LiteralPath $Object).Length -eq 0) { throw ($Source.Name + ': empty object produced') }
    }
    Write-Host ('PASS: GNU assembly preflight (' + $Sources.Count + ' files). Full link and hardware tests remain separate.')
} catch {
    Write-Host ('ERROR: Assembly preflight failed: ' + $_.Exception.Message)
    exit 1
} finally {
    # Only delete the unique temporary folder created by this invocation.
    if ($Workspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force -ErrorAction SilentlyContinue
    }
}
