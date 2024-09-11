#!/bin/bash
git clone --depth 1 https://github.com/microsoft/vcpkg.git
cd vcpkg
bash bootstrap-vcpkg.sh
