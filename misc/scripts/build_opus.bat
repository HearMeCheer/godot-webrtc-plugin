mkdir build
cd build
rem cmake .. -DOPUS_BUILD_PROGRAMS=ON -DOPUS_BUILD_TESTING=ON
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

@REM cmake .. -G "Visual Studio 17 2022"
@REM cmake --build . --config Debug
@REM cmake --build . --config Release