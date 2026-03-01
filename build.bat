@echo off
setlocal enabledelayedexpansion

REM =======================================================
REM Input Arguments
REM =======================================================
set "options=-c:Release -o:dist -a:32"
:: Set the default option values
for %%O in (%options%) do for /f "tokens=1,* delims=:" %%A in ("%%O") do set "%%A=%%~B"
:loop
:: Validate and store the options, one at a time, using a loop.
if not "%~1"=="" (
  set "test=!options:*%~1:=! "
  if "!test!"=="!options! " (
    rem No substitution was made so this is an invalid option.
    rem Error handling goes here.
    rem I will simply echo an error message.
    echo Error: Invalid option %~1
  ) else if "!test:~0,1!"==" " (
    rem Set the flag option using the option name.
    rem The value doesn't matter, it just needs to be defined.
    set "%~1=1"
  ) else (
    rem Set the option value using the option as the name.
    rem and the next arg as the value
    set "%~1=%~2"
    shift
  )
  shift
  goto :loop
)

set ARCH=
if "%-a%"=="32"        ( 
    set ARCH=x86
    set WINARCH=Win32
)
if "%-a%"=="64"   (
    set ARCH=x86_64
    set WINARCH=x64
)
if "%ARCH%"=="" (
    echo ERROR: Must specify architecture flag: 32 or 64
    exit /b 1
)

set BUILD_TYPE=%-c%
if NOT "%BUILD_TYPE%"=="Debug" if NOT "%BUILD_TYPE%"=="Release" (
    echo Invalid Build Type. Choose Debug or Release
    exit /b 1
)

REM =======================================================
REM Paths
REM =======================================================
set BUILD_DIR=build
set OUT_DIR=%-o%
if NOT EXIST %OUT_DIR% (
    mkdir "%OUT_DIR%"
    if ERRORLEVEL 1 (
        exit /b 1
    )
)

REM -------------------------------------------------------
REM Step 1: Check Conan profile
REM -------------------------------------------------------
conan profile list | findstr /C:"default" >nul
IF ERRORLEVEL 1 (
    echo Conan default profile not found. Detecting...
    conan profile detect -f
) ELSE (
    echo Conan default profile already exists
)


REM -------------------------------------------------------
REM Step 2: Clean previous builds
REM -------------------------------------------------------
rmdir /S /Q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

REM =======================================================
REM Step 3: Build Application
REM =======================================================
echo.
echo Building iMS FW Upgrade Application...
conan install . --profile default -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 -s:b compiler.cppstd=17 -s arch=%ARCH% --build=missing -of %BUILD_DIR%
cd %BUILD_DIR%
cmake -S .. -B . -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A %WINARCH%
cmake --build . --config %BUILD_TYPE%

IF NOT EXIST %BUILD_TYPE%\ims_fw_upgrade.exe (
    echo ERROR: Built Application not found!
    exit /b 1
)
cd ..


REM -------------------------------------------------------
REM Step 4: Copy Artifacts to output folder
REM -------------------------------------------------------
copy "%BUILD_DIR%\%BUILD_TYPE%\*.exe" "%OUT_DIR%"


echo ==============================================================
echo Build complete!
echo  - ims_fw_upgrade.exe is in %OUT_DIR%
echo ==============================================================
echo on