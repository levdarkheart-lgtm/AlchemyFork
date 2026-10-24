#!/usr/bin/env bash
# Strict static Mach-O slice inspection, on Apple Silicon. Doesn't prove shaders or rendering backend.
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then echo 'macOS required' >&2; exit 2; fi
if [[ $# -ne 1 || ! -d "$1/Contents" ]]; then
  echo 'Usage: tools/metal/verify_app.sh /path/to/AlchemyMetal.app' >&2
  exit 2
fi
APP="$1"
count=0
bad=0
while IFS= read -r -d '' path; do
  [[ -L "$path" ]] && continue
  kind="$(file -b "$path")"
  [[ "$kind" == *'Mach-O'* ]] || continue
  count=$((count+1))
  archs="$(lipo -archs "$path" 2>/dev/null || true)"
  if [[ "$archs" != 'arm64' ]]; then
    echo "BAD: $path -> [$archs]" >&2
    bad=$((bad+1))
  fi
done < <(find "$APP/Contents" -type f -print0)

echo "Mach-O slices checked: $count; invalid non-arm64 slices: $bad"
if (( count == 0 || bad > 0 )); then exit 1; fi

echo 'ARM64 slice inspection passed. Run dyld/otool checks for runtime-loaded paths separately.'
