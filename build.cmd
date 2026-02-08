@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: build.cmd - MeetingApp Build Script
:: Usage:
::   build.cmd              - Default Release build
::   build.cmd release      - Release build
::   build.cmd debug        - Debug build
::   build.cmd test         - Build and run all tests
::   build.cmd clean        - Clean build directories
::   build.cmd rebuild      - Clean then Release build
:: =============================================================================

:: -------------------- Path Configuration --------------------

:: Visual Studio 2022 Enterprise vcvarsall.bat path
set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"

:: Qt installation path
set "QT_DIR=D:\qt6.4\6.8.3\msvc2022_64"

:: Qt Tools path (Ninja, CMake)
set "NINJA_PATH=D:\qt6.4\Tools\Ninja"
set "CMAKE_PATH=D:\qt\Tools\CMake_64\bin"

:: Project root directory (script location)
set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

:: Build output directory
set "BUILD_DIR=%PROJECT_DIR%\build\release"
set "BUILD_TYPE=Release"

:: -------------------- Parse Arguments --------------------
set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=release"

if /I "%ACTION%"=="release" (
    set "BUILD_TYPE=Release"
    set "BUILD_DIR=%PROJECT_DIR%\build\release"
    goto :build
)
if /I "%ACTION%"=="debug" (
    set "BUILD_TYPE=Debug"
    set "BUILD_DIR=%PROJECT_DIR%\build\debug"
    goto :build
)
if /I "%ACTION%"=="clean" (
    goto :clean
)
if /I "%ACTION%"=="rebuild" (
    set "BUILD_TYPE=Release"
    set "BUILD_DIR=%PROJECT_DIR%\build\release"
    goto :rebuild
)
if /I "%ACTION%"=="test" (
    set "BUILD_TYPE=Release"
    set "BUILD_DIR=%PROJECT_DIR%\build\test"
    goto :test
)

echo [ERROR] Unknown argument: %ACTION%
echo.
echo Usage:
echo   build.cmd              Default Release build
echo   build.cmd release      Release build
echo   build.cmd debug        Debug build
echo   build.cmd test         Build and run all tests
echo   build.cmd clean        Clean build directories
echo   build.cmd rebuild      Clean then Release build
exit /b 1

:: -------------------- Clean --------------------
:clean
echo.
echo ========================================
echo   Cleaning build directories...
echo ========================================
echo.

if exist "%PROJECT_DIR%\build\release" (
    echo Removing build\release ...
    rmdir /s /q "%PROJECT_DIR%\build\release"
)
if exist "%PROJECT_DIR%\build\debug" (
    echo Removing build\debug ...
    rmdir /s /q "%PROJECT_DIR%\build\debug"
)
if exist "%PROJECT_DIR%\build\test" (
    echo Removing build\test ...
    rmdir /s /q "%PROJECT_DIR%\build\test"
)

echo.
echo [DONE] Build directories cleaned.
exit /b 0

:: -------------------- Clean + Build --------------------
:rebuild
echo.
echo ========================================
echo   Clean rebuild (%BUILD_TYPE%) ...
echo ========================================

if exist "%BUILD_DIR%" (
    echo Removing %BUILD_DIR% ...
    rmdir /s /q "%BUILD_DIR%"
)
goto :build

:: -------------------- Build --------------------
:build
echo.
echo ========================================
echo   MeetingApp Build Script
echo ========================================
echo   Build Type:   %BUILD_TYPE%
echo   Output Dir:   %BUILD_DIR%
echo   Qt Path:      %QT_DIR%
echo ========================================
echo.

:: 1. Check vcvarsall.bat
if not exist "%VCVARSALL%" (
    echo [ERROR] Cannot find vcvarsall.bat:
    echo   %VCVARSALL%
    echo Please update the VCVARSALL path in this script.
    exit /b 1
)

:: 2. Check Qt path
if not exist "%QT_DIR%\lib\cmake\Qt6" (
    echo [ERROR] Cannot find Qt6 CMake config:
    echo   %QT_DIR%\lib\cmake\Qt6
    echo Please update the QT_DIR path in this script.
    exit /b 1
)

:: 3. Initialize MSVC x64 environment
echo [1/3] Initializing MSVC x64 environment...
call "%VCVARSALL%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] MSVC environment initialization failed!
    exit /b 1
)
echo       OK

:: 4. Add Ninja and CMake to PATH
set "PATH=%CMAKE_PATH%;%NINJA_PATH%;%PATH%"

:: 5. CMake configure
echo [2/3] CMake configure (%BUILD_TYPE%)...
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%\ninja.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe"

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configure failed!
    exit /b 1
)

:: 6. Build
echo [3/3] Building project...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ========================================
echo   [SUCCESS] Build complete!
echo   Output Dir:   %BUILD_DIR%
echo   Executable:   %BUILD_DIR%\MeetingApp.exe
echo ========================================
exit /b 0

:: -------------------- Test --------------------
:test
echo.
echo ========================================
echo   Build and Run Tests (GoogleTest)
echo ========================================
echo   Output Dir:   %BUILD_DIR%
echo   Qt Path:      %QT_DIR%
echo ========================================
echo.

:: 1. Check tools
if not exist "%VCVARSALL%" (
    echo [ERROR] Cannot find vcvarsall.bat
    exit /b 1
)
if not exist "%QT_DIR%\lib\cmake\Qt6" (
    echo [ERROR] Cannot find Qt6
    exit /b 1
)

:: 2. Initialize MSVC
echo [1/4] Initializing MSVC x64 environment...
call "%VCVARSALL%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] MSVC environment initialization failed!
    exit /b 1
)
echo       OK

:: 3. Set PATH
set "PATH=%CMAKE_PATH%;%NINJA_PATH%;%PATH%"

:: 4. CMake configure (enable tests)
echo [2/4] CMake configure (BUILD_TESTS=ON)...
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%\ninja.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe" ^
    -DBUILD_TESTS=ON

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configure failed!
    exit /b 1
)

:: 5. Build tests
echo [3/4] Building project and tests...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    exit /b 1
)

:: 6. Run tests (add Qt bin dir to PATH so DLLs can be found)
echo [4/4] Running tests...
echo.
set "PATH=%QT_DIR%\bin;%PATH%"
cd /d "%BUILD_DIR%"
ctest --output-on-failure --timeout 30 -C %BUILD_TYPE%

set TEST_RESULT=%ERRORLEVEL%
cd /d "%PROJECT_DIR%"

if %TEST_RESULT% neq 0 (
    echo.
    echo ========================================
    echo   [FAILED] Some tests did not pass!
    echo ========================================
    exit /b 1
)

echo.
echo ========================================
echo   [SUCCESS] All tests passed!
echo ========================================
exit /b 0
