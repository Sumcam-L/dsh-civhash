@echo off
rem Build civhash.exe from civhash.cpp (MinGW g++, static link).
rem -fwrapv keeps signed overflow wrapping, matching the original MSVC behaviour.
setlocal
set "CXX=%CXX%"
if "%CXX%"=="" set "CXX=g++"
%CXX% -std=c++17 -O2 -fwrapv -static -s -o "%~dp0civhash.exe" "%~dp0civhash.cpp"
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK: %~dp0civhash.exe
endlocal
