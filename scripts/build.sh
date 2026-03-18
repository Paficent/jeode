#!/bin/bash

# Usage: ./scripts/build.sh [version]

VERSION=$1

make clean
bash scripts/dependencies.sh

if [ -n "$VERSION" ]; then
    make JEODE_VERSION="$VERSION"
    makensis -DJEODE_VERSION="$VERSION" -DBUILD_DIR=build -NOCD scripts/jeode.nsi
else
    make
fi

./scripts/build_examples.sh
