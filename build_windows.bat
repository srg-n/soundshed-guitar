@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: ---------------------------------------------------------------------------
:: Full release build: JUCE Standalone + VST3 + CLAP, then installer
:: Modified: No Generator forcing, Auto-detect Visual Studio, No AVX2.
:: ---------------------------------------------------------------------------

set "WORKSPACE_ROOT=%~dp0"
set "JUCE_BUILDS=%WORKSPACE_ROOT%juce\Builds"
set "INSTALLER_SCRIPT=%WORKSPACE_ROOT%juce\packaging\build-installer.bat"
set "UI_DIR=%WORKSPACE_ROOT%core\ui"

:: AVX2'yi kalıcı olarak kapatıyoruz
set "CORE_ENABLE_AVX2=OFF"

:: Mimariyi argüman olarak yakala, yoksa x64 kullan
set "ARCH_INPUT=%~1"
if not defined ARCH_INPUT (
    set "ARCH_INPUT=x64"
)

:: Mimarileri ayarla
set "ARCH="
if /I "%ARCH_INPUT%"=="x86" set "ARCH=Win32"
if /I "%ARCH_INPUT%"=="x64" set "ARCH=x64"
if /I "%ARCH_INPUT%"=="arm64" set "ARCH=ARM64"
if not defined ARCH (
    set "ARCH=x64"
)

for /f %%I in ('powershell -NoProfile -Command "[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()"') do set "BUILD_START_MS=%%I"

echo [0/5] Configuring CMake...
echo       Architecture: %ARCH%
echo       AVX2 support: %CORE_ENABLE_AVX2%

:: DİKKAT: -G parametresini tamamen kaldırdık, CMake kendi bulacak!
cmake -A "%ARCH%" -S juce -B "%JUCE_BUILDS%" -DGUITARFX_CORE_ENABLE_AVX2=%CORE_ENABLE_AVX2%
if !ERRORLEVEL! neq 0 (
    echo ERROR: CMake configure failed.
    goto :fail
)
echo CMake configure succeeded.
echo.

:: --- Build UI ---------------------------------------------------------------
echo [1/5] Building UI (npm install ^& build)...
pushd "%UI_DIR%"

echo Running npm install...
call npm install
if !ERRORLEVEL! neq 0 (
    echo ERROR: npm install failed.
    popd
    goto :fail
)

echo Running npm run build...
call npm run build
if !ERRORLEVEL! neq 0 (
    echo ERROR: UI build failed.
    popd
    goto :fail
)

popd
echo UI build succeeded.
echo.

:: --- Build Standalone -------------------------------------------------------
echo [2/5] Building Standalone (Release)...
cmake --build "%JUCE_BUILDS%" --config Release --target SoundshedGuitar_Standalone --parallel
if !ERRORLEVEL! neq 0 (
    echo ERROR: Standalone build failed.
    goto :fail
)
echo Standalone build succeeded.
echo.

:: --- Build VST3 -------------------------------------------------------------
echo [3/5] Building VST3 (Release)...
cmake --build "%JUCE_BUILDS%" --config Release --target SoundshedGuitar_VST3 --parallel
if !ERRORLEVEL! neq 0 (
    echo ERROR: VST3 build failed.
    goto :fail
)
echo VST3 build succeeded.
echo.

:: --- Build CLAP -------------------------------------------------------------
echo [4/5] Building CLAP (Release)...
cmake --build "%JUCE_BUILDS%" --config Release --target SoundshedGuitar_CLAP --parallel
if !ERRORLEVEL! neq 0 (
    echo ERROR: CLAP build failed.
    goto :fail
)
echo CLAP build succeeded.
echo.

:: --- Build Installer --------------------------------------------------------
echo [5/5] Building installer...
call "%INSTALLER_SCRIPT%"
if !ERRORLEVEL! neq 0 (
    echo ERROR: Installer build failed.
    goto :fail
)

echo Build and packaging succeeded.
set "BUILD_EXIT_CODE=0"
goto :report_elapsed

:fail
set "BUILD_EXIT_CODE=!ERRORLEVEL!"

:report_elapsed
for /f %%I in ('powershell -NoProfile -Command "$elapsed=[TimeSpan]::FromMilliseconds([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() - %BUILD_START_MS%); '{0:00}:{1:00}:{2:00}.{3:000}' -f [int]$elapsed.TotalHours, $elapsed.Minutes, $elapsed.Seconds, $elapsed.Milliseconds"') do set "BUILD_ELAPSED=%%I"
echo.
if "%BUILD_EXIT_CODE%"=="0" (
    echo Total elapsed time: %BUILD_ELAPSED%
) else (
    echo Total elapsed time before failure: %BUILD_ELAPSED%
)

endlocal & exit /b %BUILD_EXIT_CODE%
