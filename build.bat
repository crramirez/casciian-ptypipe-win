@echo off
REM Build script for ptypipe.exe
REM Requires Visual Studio 2022 or later with C++ tools installed

echo Building ptypipe.exe...
echo.

REM Try to find MSBuild
set MSBUILD_PATH=

REM Check for Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe
)

if "%MSBUILD_PATH%"=="" (
    echo ERROR: MSBuild.exe not found!
    echo Please ensure Visual Studio 2022 is installed with C++ development tools.
    echo Or use Visual Studio Developer Command Prompt to build.
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
