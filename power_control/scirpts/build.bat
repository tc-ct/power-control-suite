
rmdir /s /q build/Desktop_Qt_6_7_3_MinGW_64_bit-release

cmake -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=make.exe -S . -B build/Desktop_Qt_6_7_3_MinGW_64_bit-release
cmake --build build/Desktop_Qt_6_7_3_MinGW_64_bit-release