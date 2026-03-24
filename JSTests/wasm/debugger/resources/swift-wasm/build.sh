#!/bin/bash

# This script is used to compile Swift WASM.
# Note that the corresponding tests needs to be updated due to updated WASM binary.
#
# Usage:
#   ./build.sh <test-folder>   # build if folder exists, scaffold a new Swift package otherwise

set -e

# Resolve swift binary: prefer swiftly default location, fall back to PATH.
# Install via Swiftly: https://www.swift.org/documentation/articles/wasm-getting-started.html
if [ -x "$HOME/.swiftly/bin/swift" ]; then
    SWIFT="$HOME/.swiftly/bin/swift"
elif command -v swift &>/dev/null; then
    SWIFT=$(command -v swift)
else
    echo "Error: swift not found. Install via Swiftly: https://www.swift.org/documentation/articles/wasm-getting-started.html"
    exit 1
fi

SDK=swift-6.2.3-RELEASE_wasm

if [ -z "$1" ]; then
    echo "Usage: $0 <test-folder>"
    exit 1
fi

FOLDER="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -d "$SCRIPT_DIR/$FOLDER" ]; then
    # Build Swift WASM with Swiftly https://www.swift.org/documentation/articles/wasm-getting-started.html
    echo "Building Swift WebAssembly $FOLDER..."
    cd "$SCRIPT_DIR/$FOLDER"
    "$SWIFT" build --swift-sdk "$SDK"

    # Move compiled WASM beside main.js
    echo "Moving $FOLDER.wasm..."
    mv ".build/wasm32-unknown-wasip1/debug/$FOLDER.wasm" "./$FOLDER.wasm"

    # Clean up build artifacts
    echo "Cleaning up..."
    rm -rf .build

    echo "Done! $FOLDER.wasm is ready for testing."
else
    echo "Creating new Swift WASM test folder '$FOLDER'..."
    cd "$SCRIPT_DIR"
    mkdir "$FOLDER"
    cd "$FOLDER"
    "$SWIFT" package init --type executable
    echo "Done! Edit Sources/$FOLDER/main.swift then run: $0 $FOLDER"
fi
