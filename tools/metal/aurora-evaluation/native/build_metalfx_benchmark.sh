#!/bin/sh
# Evaluation executable only. No viewer target or production dependency.
set -eu
TASK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTPUT=${1:-"$TASK_DIR/../../../artifacts/metal/aurora-evaluation-20261126/metalfx-cost-benchmark"}
mkdir -p "$(dirname -- "$OUTPUT")"
xcrun --sdk macosx clang -std=c11 -O3 -DNDEBUG -ffp-contract=off -arch arm64 -mmacosx-version-min=26.0 \
    -c "$TASK_DIR/workload/input_workload.c" -o "$OUTPUT.workload.o"
xcrun --sdk macosx clang++ -std=c++20 -O3 -DNDEBUG -fobjc-arc -arch arm64 -mmacosx-version-min=26.0 \
    "$TASK_DIR/native/metalfx_benchmark.mm" "$OUTPUT.workload.o" -framework Foundation -framework Metal -framework MetalFX \
    -o "$OUTPUT"
printf '%s\n' "$OUTPUT"
