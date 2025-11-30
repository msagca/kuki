#!/usr/bin/env bash
cd "$(dirname "$0")"
cmake -B build --preset Release
cmake --build build
