@echo off
REM DigiByte Windows Build Script
REM Run this in a Visual Studio Developer Command Prompt

echo ========================================
echo DigiByte Windows Native Build
echo ========================================
echo.

REM Check if we're in a Visual Studio command prompt
if "%VCINSTALLDIR%"=="" (
    echo ERROR: This script must be run from a Visual Studio Developer Command Prompt!
    echo.
    echo Please open "Developer Command Prompt for VS 2022" from the Start Menu
    exit /b 1
)

echo Step 1: Current directory
cd
echo.

echo Step 2: Generating Visual Studio project files...
python build_msvc\msvc-autogen.py
if %errorlevel% neq 0 (
    echo ERROR: Failed to generate project files!
    echo Make sure Python is installed and in PATH
    exit /b 1
)

echo.
echo Step 3: Building DigiByte with MSBuild...
msbuild build_msvc\digibyte.sln /p:Configuration=Release /m
if %errorlevel% neq 0 (
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo.
echo Executables are located in:
echo   build_msvc\x64\Release\
echo.
dir build_msvc\x64\Release\*.exe /b
echo ========================================