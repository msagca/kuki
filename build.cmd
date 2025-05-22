@echo off
cd /d "%~dp0"
cmake -B build
cmake --build build
