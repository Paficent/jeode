#!/bin/bash
set -e

find examples -name Makefile | while read -r makefile; do
    dir=$(dirname "$makefile")
    echo "Building $dir..."
    make -C "$dir"
done
