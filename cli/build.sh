#!/bin/bash
# Build civhash.exe from civhash.cpp (MinGW g++ on Windows, or the host g++).
# -fwrapv keeps signed overflow wrapping, matching the original MSVC behaviour.
set -euo pipefail
cd "$(dirname "$0")"
CXX="${CXX:-g++}"
"$CXX" -std=c++17 -O2 -fwrapv -static -s -o civhash.exe civhash.cpp
echo "BUILD OK: $(pwd)/civhash.exe"
