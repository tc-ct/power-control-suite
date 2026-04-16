@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"
set "ASTYLE_DIR=%REPO_ROOT%\scripts\build_utils\astyle"
set "ASTYLE_EXE=%ASTYLE_DIR%\astyle.exe"
set "OPTION_FILE=%ASTYLE_DIR%\.astylerc"
set "RET=0"

if not exist "%ASTYLE_EXE%" (
	echo [ERROR] Cannot find formatter: "%ASTYLE_EXE%"
	exit /b 1
)

if not exist "%OPTION_FILE%" (
	echo [ERROR] Cannot find formatter options: "%OPTION_FILE%"
	exit /b 1
)

if "%~1"=="" (
	call :format_target "%REPO_ROOT%"
	exit /b %RET%
)

set "INPUT_PATH=%~1"

if exist "%INPUT_PATH%" (
	call :format_target "%INPUT_PATH%"
) else if exist "%REPO_ROOT%\%INPUT_PATH%" (
	call :format_target "%REPO_ROOT%\%INPUT_PATH%"
) else (
	echo [WARN] Target not found: "%INPUT_PATH%"
	set "RET=1"
)

exit /b %RET%

:format_target
set "TARGET=%~1"

if exist "%TARGET%\*" (
	for %%E in (h c) do (
		for /R "%TARGET%" %%F in (*.%%E) do (
			"%ASTYLE_EXE%" "--options=%OPTION_FILE%" "%%F"
			if errorlevel 1 set "RET=1"
		)
	)
	goto :eof
)

if exist "%TARGET%" (
	"%ASTYLE_EXE%" "--options=%OPTION_FILE%" "%TARGET%"
	if errorlevel 1 set "RET=1"
) else (
	echo [WARN] Target not found: "%TARGET%"
	set "RET=1"
)

goto :eof