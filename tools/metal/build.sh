#!/usr/bin/env bash
# Wrapper for upstream's documented OSS arm64 preset. This script does not alter upstream CMake definitions.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
  echo 'ERROR: This fork must be built natively on Apple Silicon macOS' >&2
  exit 2
fi

ACTION="${1:-build}"
PRESET="${AL_METAL_PRESET:-xcode-os-arm64}"
BUILD_DIR="${AL_METAL_BUILD_DIR:-build-Darwin-${PRESET}}"
CONFIG="${AL_METAL_CONFIG:-Release}"

case "$ACTION" in
  configure)
    cmake -S indra --preset "$PRESET" -DAL_CHANNEL:STRING="Alchemy Metal Development"
    ;;
  build)
    if [[ ! -d "$BUILD_DIR" ]]; then
      echo "Build directory not configured: $BUILD_DIR. Run: tools/metal/build.sh configure" >&2
      exit 2
    fi
    cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "${AL_METAL_JOBS:-4}"
    ;;
  test)
    if [[ ! -d "$BUILD_DIR" ]]; then echo "Missing $BUILD_DIR" >&2; exit 2; fi
    ctest --test-dir "$BUILD_DIR" -C "$CONFIG" --output-on-failure
    ;;
  help|--help|-h)
    printf 'Usage: tools/metal/build.sh {configure|build|test}\n'
    printf 'Env: AL_METAL_PRESET (default xcode-os-arm64), AL_METAL_BUILD_DIR (default build-Darwin-$AL_METAL_PRESET), AL_METAL_CONFIG (Release), AL_METAL_JOBS (4)\n'
    ;;
  *)
    echo "Unknown action: $ACTION" >&2
    exit 2
    ;;
esac
