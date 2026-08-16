@echo off
setlocal EnableExtensions DisableDelayedExpansion

if /I "%~1"=="__BUILD_CHILD__" goto BUILD_CHILD

echo.
echo ============================================================
echo DS Style v7.3 Enhanced 13.5 clean build helper v4
echo ============================================================
echo This pause confirms the CMD launched correctly.
echo Press any key to validate and build.
echo.
pause

set "SCRIPT_DIR=%~dp0"
set "LOG=%SCRIPT_DIR%build-r54-3-ds-style-v7-3-enhanced-13-5.log"

echo ============================================================ > "%LOG%"
echo DS Style v7.3 Enhanced 13.5 clean build helper v4>> "%LOG%"
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
    source\reset_table.h
    source\saveMODE.h
    source\launcher_version.h
    source\ezkernelnew.c
    source\showcht.c
    source\showcht.h
    source\GBApatch.c
    source\NORflash_OP.c
    source\gba_rts_patch.s
    source\gba_rts_only.s
    source\launcher_theme_assets.h
    source\launcher_topbar_patterns.h
) do (
    if not exist "%%F" (
        echo ERROR: Required file is missing: %%F
        exit /b 1
    )
)

echo.
echo Validating merged and optimized source...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%BUILD_CWD%\Build-DS-Style-Enhanced.ps1" -Root "%BUILD_CWD%"
if errorlevel 1 (
    echo ERROR: Merged/optimized source validation failed.
    exit /b 1
)

echo.
echo Standalone PowerShell validation passed.
echo Python validation is not required by this clean build helper.

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

for %%I in ("%BUILD_CWD%") do set "PROJECT_NAME=%%~nxI"
set "BUILD_DRIVE="
set "BUILD_OUTPUT_GBA=%PROJECT_NAME%.gba"
set "BUILD_OUTPUT_ELF=%PROJECT_NAME%.elf"
set "BUILD_OUTPUT_MAP=%PROJECT_NAME%.map"

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
    set "BUILD_OUTPUT_GBA=!BUILD_DRIVE:~0,1!.gba"
    set "BUILD_OUTPUT_ELF=!BUILD_DRIVE:~0,1!.elf"
    set "BUILD_OUTPUT_MAP=!BUILD_DRIVE:~0,1!.map"
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
set "BUILT_MAP=%BUILD_CWD%\build\!BUILD_OUTPUT_MAP!"
if not exist "!BUILT_GBA!" (
    echo ERROR: make completed but !BUILD_OUTPUT_GBA! was not created.
    exit /b 1
)

if /I not "!BUILD_OUTPUT_GBA!"=="%PROJECT_NAME%.gba" (
    copy /Y "!BUILT_GBA!" "%BUILD_CWD%\%PROJECT_NAME%.gba" >nul
    if errorlevel 1 (
        echo ERROR: Could not create the project-named GBA output.
        exit /b 1
    )
    set "BUILT_GBA=%BUILD_CWD%\%PROJECT_NAME%.gba"
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
if exist "!BUILT_MAP!" copy /Y "!BUILT_MAP!" "%BUILD_CWD%\%PROJECT_NAME%.map" >nul

echo.
echo BUILD COMPLETE:
echo   GBA: !BUILT_GBA!
echo   BIN: %BUILD_CWD%\ezkernelnew.bin
if exist "%BUILD_CWD%\%PROJECT_NAME%.map" echo   MAP: %BUILD_CWD%\%PROJECT_NAME%.map

echo.
echo SHA-256:
certutil -hashfile "%BUILD_CWD%\ezkernelnew.bin" SHA256
exit /b 0
