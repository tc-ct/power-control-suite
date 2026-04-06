
@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build"
set "RELEASE_ROOT=%ROOT_DIR%\release"
set "PKG_DIR=%RELEASE_ROOT%\host_usb_demo"
set "ZIP_FILE=%RELEASE_ROOT%\host_usb_demo_release.zip"

if not exist "%BUILD_DIR%\host_usb_demo.exe" (
	echo [ERROR] Missing executable: %BUILD_DIR%\host_usb_demo.exe
	exit /b 1
)

if not exist "%ROOT_DIR%\third_party\bin\hidapi.dll" (
	echo [ERROR] Missing dll: %ROOT_DIR%\third_party\bin\hidapi.dll
	exit /b 1
)

if not exist "%ROOT_DIR%\readme.md" (
	echo [ERROR] Missing readme: %ROOT_DIR%\readme.md
	exit /b 1
)

if not exist "%RELEASE_ROOT%" mkdir "%RELEASE_ROOT%"
if exist "%PKG_DIR%" rmdir /s /q "%PKG_DIR%"
mkdir "%PKG_DIR%"
mkdir "%PKG_DIR%\config"

copy /y "%BUILD_DIR%\host_usb_demo.exe" "%PKG_DIR%\" >nul
copy /y "%ROOT_DIR%\third_party\bin\hidapi.dll" "%PKG_DIR%\" >nul
copy /y "%ROOT_DIR%\readme.md" "%PKG_DIR%\" >nul
copy /y "%ROOT_DIR%\config\power_config.json" "%PKG_DIR%\config\" >nul
copy /y "%ROOT_DIR%\scirpts\run.bat" "%PKG_DIR%\" >nul

if exist "%ZIP_FILE%" del /f /q "%ZIP_FILE%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PKG_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 (
	echo [ERROR] Failed to create zip package.
	exit /b 1
)

echo [OK] Release package ready:
echo      %ZIP_FILE%

endlocal