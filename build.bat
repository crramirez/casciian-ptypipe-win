@echo off
REM Build script for ptypipe.exe
REM Requires Visual Studio 2026 or later with C++ tools installed

echo Building ptypipe.exe...
echo.

REM Try to find MSBuild
set MSBUILD_PATH=

REM First, try vswhere (installed with recent Visual Studio / Build Tools)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`
        "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
    `) do (
        if not defined MSBUILD_PATH set "MSBUILD_PATH=%%i"
    )
) else if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`
        "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
    `) do (
        if not defined MSBUILD_PATH set "MSBUILD_PATH=%%i"
    )
)

REM If vswhere did not find MSBuild, try msbuild from PATH (e.g. Developer Command Prompt)
if "%MSBUILD_PATH%"=="" (
    where msbuild >nul 2>&1
    if %ERRORLEVEL%==0 (
        set "MSBUILD_PATH=msbuild"
    )
)

if "%MSBUILD_PATH%"=="" (
    echo ERROR: MSBuild.exe not found!
    echo Please ensure Visual Studio or Build Tools are installed with C++ development tools.
    echo Or run this script from a Visual Studio Developer Command Prompt.
    exit /b 1
)

echo Found MSBuild at: %MSBUILD_PATH%
echo.

REM Default configuration and platform
set CONFIG=Release
set PLATFORM=x64

REM Parse command line arguments
if not "%1"=="" set CONFIG=%1
if not "%2"=="" set PLATFORM=%2

echo Building configuration: %CONFIG%
echo Building platform: %PLATFORM%
echo.

REM Build the solution
"%MSBUILD_PATH%" ptypipe.sln /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /v:minimal

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Build successful!
echo Output: %PLATFORM%\%CONFIG%\ptypipe.exe
