#!/usr/bin/env bash
cd "$(dirname "$0")"
cmake -S . -B build --preset Release
cmake --build build
