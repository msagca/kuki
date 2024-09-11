@echo off
git clone --depth 1 https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
