#!/usr/bin/bash
cd "$(dirname "$0")"
cmake -B build
cmake --build build
cmake --install build
echo "Press any key to continue..."
read
