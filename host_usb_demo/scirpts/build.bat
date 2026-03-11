@REM set VCPKG_ROOT=D:\workspace\vcpkg
@REM set PATH=%VCPKG_ROOT%;%PATH%

@REM vcpkg list
rmdir /s /q build
xcopy /y /d third_party\bin\hidapi.dll build\

cmake -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=make.exe -B build -S . -Dhidapi_ROOT="third_party"

cmake --build build --config Release
