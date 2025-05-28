@echo off
cd /d "%~dp0"
cmake -B build --preset Release
cmake --build build --preset Release
cmake --install build
pause
