#!/usr/bin/env bash
cd "$(dirname "$0")"
cmake -B build --preset Release
cmake --build build
cmake --install build
echo "Press any key to continue..."
read
