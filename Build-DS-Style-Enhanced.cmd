@echo off
setlocal EnableExtensions DisableDelayedExpansion

if /I "%~1"=="__BUILD_CHILD__" goto BUILD_CHILD

echo.
echo ============================================================
echo DS Style 7.4c Build
echo ============================================================
echo This pause confirms the CMD launched correctly.
echo Press any key to validate and build.
echo.
pause

set "SCRIPT_DIR=%~dp0"
set "PROJECT_NAME=DS-Style-7.4c-Source1"
set "BUILD_DIR=%SCRIPT_DIR%build"

rem Start every normal build with a clean build-output folder.
if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
if exist "%BUILD_DIR%" (
    echo ERROR: Could not clean the build folder:
    echo   "%BUILD_DIR%"
    pause
    exit /b 1
)
mkdir "%BUILD_DIR%"
if errorlevel 1 (
    echo ERROR: Could not create the build folder:
    echo   "%BUILD_DIR%"
    pause
    exit /b 1
)

rem Remove legacy root-level build products from older scripts.
for %%F in (
    "%SCRIPT_DIR%%PROJECT_NAME%.elf"
    "%SCRIPT_DIR%%PROJECT_NAME%.gba"
    "%SCRIPT_DIR%%PROJECT_NAME%.map"
    "%SCRIPT_DIR%build-r54-ds-style-7-4c.log"
) do if exist "%%~F" del /Q "%%~F" >nul 2>&1

set "LOG=%BUILD_DIR%\build-r54-ds-style-7-4c.log"

echo ============================================================ > "%LOG%"
echo DS Style 7.4c Build>> "%LOG%"
echo Started: %DATE% %TIME%>> "%LOG%"
echo Script: %~f0>> "%LOG%"
echo Source: %SCRIPT_DIR%>> "%LOG%"
echo ============================================================>> "%LOG%"

"%ComSpec%" /D /V:ON /C ""%~f0" __BUILD_CHILD__" >> "%LOG%" 2>&1
set "BUILD_EXIT=%ERRORLEVEL%"

echo.
echo ============================================================
echo BUILD LOG
echo ============================================================
type "%LOG%"
echo ============================================================
echo.
if "%BUILD_EXIT%"=="0" (
    echo BUILD COMPLETE.
) else (
    echo BUILD FAILED with exit code %BUILD_EXIT%.
)
echo Log:
echo   "%LOG%"
echo.
pause
exit /b %BUILD_EXIT%

:BUILD_CHILD
setlocal EnableExtensions EnableDelayedExpansion
call :MAIN
set "CHILD_EXIT=!ERRORLEVEL!"
exit /b !CHILD_EXIT!

:MAIN
cd /d "%SCRIPT_DIR%"
if errorlevel 1 (
    echo ERROR: Could not enter the source folder.
    exit /b 1
)

set "DEVKITPRO_WIN="
set "DEVKITPRO_POSIX="
if exist "C:\devkitPro-r54\devkitARM\bin\arm-none-eabi-gcc.exe" (
    set "DEVKITPRO_WIN=C:\devkitPro-r54"
    set "DEVKITPRO_POSIX=/c/devkitPro-r54"
) else if exist "C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe" (
    set "DEVKITPRO_WIN=C:\devkitPro"
    set "DEVKITPRO_POSIX=/c/devkitPro"
)
if not defined DEVKITPRO_WIN (
    echo ERROR: devkitARM was not found at C:\devkitPro-r54 or C:\devkitPro.
    exit /b 1
)

set "DEVKITARM_WIN=%DEVKITPRO_WIN%\devkitARM"
set "BUILD_CWD=%CD%"
echo DEVKITPRO Windows path: %DEVKITPRO_WIN%
echo Source root: %BUILD_CWD%

for %%F in (
    Makefile
    Build-DS-Style-Enhanced.ps1
    Build-RTS-Assembly-Preflight.ps1
    Grit\Build-All-Image-Files.ps1
    Grit\Build-Root-Image-Files.ps1
    Grit\Verify-Image-Headers.ps1
    Grit\image-build-manifest.json
    Grit\grit.exe
    Grit\FreeImage.dll
    source\saveMODE.h
    source\launcher_version.h
    source\launcher_font_extended.h
    source\Ezcard_OP.c
    source\ez_define.h
    source\draw.c
    source\ezkernelnew.c
    source\showcht.c
    source\showcht.h
    source\GBApatch.c
    source\NORflash_OP.c
    source\gba_rts_patch.s
    source\gba_rts_only.s
    source\gba_rts3_defs.inc
    source\gba_rts3_core.inc
    source\gba_rts3_menu.inc
    source\rts3_identity.c
    source\rts3_identity.h
    source\launcher_theme_assets.h
    source\launcher_topbar_patterns.h
) do (
    if not exist "%%F" (
        echo ERROR: Required file is missing: %%F
        exit /b 1
    )
)
if not exist "Grit\Build Skin Files.ps1" (
    echo ERROR: Required file is missing: Grit\Build Skin Files.ps1
    exit /b 1
)

echo.
echo Verifying and regenerating graphics with bundled Grit...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%BUILD_CWD%\Grit\Build-All-Image-Files.ps1" -Root "%BUILD_CWD%"
if errorlevel 1 (
    echo ERROR: Image verification/regeneration failed.
    exit /b 1
)
echo PASS: All image headers reproduced from PNG/BMP sources.

echo.
echo Validating merged and optimized source...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%BUILD_CWD%\Build-DS-Style-Enhanced.ps1" -Root "%BUILD_CWD%"
if errorlevel 1 (
    echo ERROR: Merged/optimized source validation failed.
    exit /b 1
)

echo.
echo DS Style 7.4c source validation passed.
echo Grit image verification/regeneration completed before source validation.

for %%F in (SD_LIST SET START HELP) do (
    if not exist "images\blank\%%F.h" (
        echo ERROR: Required generated skin header is missing: images\blank\%%F.h
        echo Run Grit\Build Skin Files.bat first.
        exit /b 1
    )
)

set "MSYS_BIN="
for %%D in (
    "%DEVKITPRO_WIN%\msys2\usr\bin"
    "%DEVKITPRO_WIN%\msys\bin"
    "C:\devkitPro\msys2\usr\bin"
    "C:\devkitPro\msys\bin"
    "C:\msys64\usr\bin"
) do (
    if not defined MSYS_BIN if exist "%%~D\make.exe" if exist "%%~D\sh.exe" set "MSYS_BIN=%%~D"
)
if not defined MSYS_BIN (
    echo ERROR: make.exe and sh.exe were not found.
    exit /b 1
)

set "GBAFIX_DIR="
for /r "%DEVKITPRO_WIN%" %%F in (gbafix.exe) do if not defined GBAFIX_DIR set "GBAFIX_DIR=%%~dpF"
set "PATH=%MSYS_BIN%;%DEVKITARM_WIN%\bin;%DEVKITPRO_WIN%\tools\bin;%GBAFIX_DIR%;%PATH%"
set "DEVKITPRO=%DEVKITPRO_POSIX%"
set "DEVKITARM=%DEVKITPRO_POSIX%/devkitARM"
set "LIBGBA=%DEVKITPRO_POSIX%/libgba"

echo MSYS_BIN=%MSYS_BIN%
echo DEVKITPRO=%DEVKITPRO%
echo DEVKITARM=%DEVKITARM%
echo LIBGBA=%LIBGBA%

set "PROJECT_NAME=DS-Style-7.4c-Source1"
set "BUILD_DRIVE="
set "BUILD_OUTPUT_GBA=build\%PROJECT_NAME%.gba"
set "BUILD_OUTPUT_ELF=build\%PROJECT_NAME%.elf"
set "BUILD_OUTPUT_MAP=build\%PROJECT_NAME%.map"

set "BUILD_CWD_NO_SPACES=!BUILD_CWD: =!"
if not "!BUILD_CWD_NO_SPACES!"=="!BUILD_CWD!" (
    for %%D in (X W V U T S R Q P O N M L K J I H G) do (
        if not exist %%D:\NUL if not defined BUILD_DRIVE set "BUILD_DRIVE=%%D:"
    )
    if not defined BUILD_DRIVE (
        echo ERROR: Could not find a free temporary drive letter.
        exit /b 1
    )
    subst !BUILD_DRIVE! "%BUILD_CWD%" >nul
    if errorlevel 1 (
        echo ERROR: Could not map !BUILD_DRIVE! to the source folder.
        exit /b 1
    )
    pushd !BUILD_DRIVE!\
) else (
    pushd "%BUILD_CWD%"
)
if errorlevel 1 (
    if defined BUILD_DRIVE subst !BUILD_DRIVE! /D >nul
    echo ERROR: Could not enter the build directory.
    exit /b 1
)

if exist "!BUILD_OUTPUT_GBA!" del /q "!BUILD_OUTPUT_GBA!"
if exist "!BUILD_OUTPUT_ELF!" del /q "!BUILD_OUTPUT_ELF!"
if exist "ezkernelnew.bin" del /q "ezkernelnew.bin"

echo.
echo Compiler:
arm-none-eabi-gcc --version
if errorlevel 1 (
    popd
    if defined BUILD_DRIVE subst !BUILD_DRIVE! /D >nul
    echo ERROR: arm-none-eabi-gcc could not be executed.
    exit /b 1
)

echo.
echo Checking all ARM assembly with the selected devkitARM compiler...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%BUILD_CWD%\Build-RTS-Assembly-Preflight.ps1" -Root "." -ArmGcc "%DEVKITARM_WIN%\bin\arm-none-eabi-gcc.exe"
if errorlevel 1 (
    set "PREFLIGHT_EXIT=!ERRORLEVEL!"
    popd
    if defined BUILD_DRIVE subst !BUILD_DRIVE! /D >nul
    echo ERROR: ARM assembly preflight failed. The full kernel build was not started.
    exit /b !PREFLIGHT_EXIT!
)

echo.
echo Cleaning...
make clean
if errorlevel 1 (
    set "MAKE_EXIT=!ERRORLEVEL!"
    popd
    if defined BUILD_DRIVE subst !BUILD_DRIVE! /D >nul
    echo ERROR: make clean failed with exit code !MAKE_EXIT!.
    exit /b !MAKE_EXIT!
)

echo.
echo Building...
make
set "MAKE_EXIT=!ERRORLEVEL!"
popd
if defined BUILD_DRIVE subst !BUILD_DRIVE! /D >nul
if not "!MAKE_EXIT!"=="0" (
    echo ERROR: Build failed with exit code !MAKE_EXIT!.
    exit /b !MAKE_EXIT!
)

set "BUILT_GBA=%BUILD_CWD%\!BUILD_OUTPUT_GBA!"
set "BUILT_ELF=%BUILD_CWD%\!BUILD_OUTPUT_ELF!"
set "BUILT_MAP=%BUILD_CWD%\!BUILD_OUTPUT_MAP!"
if not exist "!BUILT_GBA!" (
    echo ERROR: make completed but !BUILD_OUTPUT_GBA! was not created.
    exit /b 1
)

if exist "!BUILT_ELF!" (
    echo.
    echo Complete ELF size report:
    arm-none-eabi-size "!BUILT_ELF!"
    arm-none-eabi-size -A -d "!BUILT_ELF!"
    echo.
    echo IWRAM section audit:
    set "IWRAM_LIMIT=50364416"
    for %%S in (.iwram .data .init_array .fini_array) do (
        for /f "tokens=2,3" %%A in ('arm-none-eabi-size -A -d "!BUILT_ELF!" ^| findstr /B /C:"%%S"') do (
            set /a "SECTION_END=%%A+%%B"
            echo %%S end: !SECTION_END!  ^(physical IWRAM end: !IWRAM_LIMIT!^)
            if !SECTION_END! GTR !IWRAM_LIMIT! (
                echo ERROR: %%S exceeds physical GBA IWRAM.
                exit /b 1
            )
        )
    )
    echo.
    echo EWRAM section audit:
    set "SBSS_END="
    for /f "tokens=2,3" %%A in ('arm-none-eabi-size -A -d "!BUILT_ELF!" ^| findstr /B /C:".sbss"') do set /a "SBSS_END=%%A+%%B"
    if defined SBSS_END (
        echo .sbss end: !SBSS_END!  ^(physical EWRAM end: 33816576^)
        if !SBSS_END! GTR 33816576 (
            echo ERROR: .sbss exceeds physical GBA EWRAM.
            exit /b 1
        )
    ) else (
        echo WARNING: The .sbss section could not be read from the ELF.
    )
) else (
    echo WARNING: The ELF was not found, so memory-section audits were skipped.
)

copy /Y "!BUILT_GBA!" "%BUILD_CWD%\ezkernelnew.bin" >nul
if errorlevel 1 (
    echo ERROR: Could not create ezkernelnew.bin.
    exit /b 1
)
echo.
echo BUILD COMPLETE:
echo   GBA: !BUILT_GBA!
echo   ELF: !BUILT_ELF!
echo   BIN: %BUILD_CWD%\ezkernelnew.bin
if exist "!BUILT_MAP!" echo   MAP: !BUILT_MAP!
echo   LOG: %BUILD_CWD%\build\build-r54-ds-style-7-4c.log

echo.
echo SHA-256:
certutil -hashfile "%BUILD_CWD%\ezkernelnew.bin" SHA256
exit /b 0
