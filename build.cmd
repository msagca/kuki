@echo off
cd /d "%~dp0"
cmake -S . -B build --preset Release
cmake --build build --preset Release
