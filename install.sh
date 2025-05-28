#!/usr/bin/bash
cd "$(dirname "$0")"
cmake -B build --preset Release
cmake --build build --preset Release
cmake --install build
echo "Press any key to continue..."
read
