#!/usr/bin/env bash
# Read-only environment inventory for real Apple Silicon host; does not install or mutate anything.
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: This verification requires macOS" >&2
  exit 2
fi
if [[ "$(uname -m)" != "arm64" ]]; then
  echo "ERROR: Native arm64 execution required (no Rosetta)" >&2
  exit 2
fi

printf 'OS: '; sw_vers -productVersion
printf 'Kernel architecture: '; uname -m
printf 'Hardware identifier: '; sysctl -n hw.model
printf 'RAM bytes: '; sysctl -n hw.memsize
printf 'CPU: '; sysctl -n machdep.cpu.brand_string 2>/dev/null || true
printf '\nXcode: \n'; xcodebuild -version
printf '\nSelected developer directory: '; xcode-select -p
printf '\nCMake: '; cmake --version | head -1
printf 'Git: '; git --version
printf 'Metal compiler: '; xcrun --find metal || { echo "Missing Metal compiler" >&2; exit 3; }
printf '\nTool optional: python3='; command -v python3 || true
printf 'Tool optional: xctrace='; xcrun --find xctrace || true
printf '\nMetal GPU capability (Swift)\n'
if [[ -f tools/metal/device.swift ]]; then
  xcrun swift tools/metal/device.swift
else
  echo 'device.swift missing' >&2
  exit 3
fi
printf '\nDoctor complete. This does not verify a viewer build or performance.\n'
