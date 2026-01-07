@echo off
REM =============================================================================
REM CICS Emulation Enterprise - Build Script for Windows (MinGW)
REM =============================================================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo +==============================================================+
echo |       CICS Emulation Enterprise v3.1.6 - Build Script        |
echo +==============================================================+
echo.
echo Project root: %PROJECT_ROOT%
echo Build type: %BUILD_TYPE%
echo.

REM Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: CMake not found. Please install CMake 3.20+
    exit /b 1
)

REM Check for MinGW
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: g++ not found. Please install MinGW-w64
    exit /b 1
)

echo CMake found
for /f "tokens=3" %%i in ('cmake --version ^| findstr /i "cmake"') do echo CMake version: %%i

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure
echo.
echo Configuring...
cmake "%PROJECT_ROOT%" ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCICS_BUILD_TESTS=ON ^
    -DCICS_BUILD_EXAMPLES=ON ^
    -DCICS_BUILD_BENCHMARKS=ON

if %errorlevel% neq 0 (
    echo Configuration failed!
    exit /b 1
)

REM Build
echo.
echo Building...
cmake --build . --parallel

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo +==============================================================+
echo |                     Build Complete!                          |
echo +==============================================================+
echo.
echo Binaries are in: %BUILD_DIR%\bin
echo.
echo To run tests:    cd %BUILD_DIR% ^& ctest --output-on-failure
echo To run demo:     %BUILD_DIR%\bin\cics-demo.exe

endlocal
