#!/bin/bash

VERSION="${1:-0.0.0.0}"
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DJEODE_VERSION="$VERSION"
cmake --build build -j
cmake --build build --target installer
