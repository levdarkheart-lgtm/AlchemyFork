#!/usr/bin/env bash
# Explain available native capture tools; do not fabricate captures or assume an xctrace template name.
set -euo pipefail
if [[ "$(uname -s)" != "Darwin" ]]; then echo 'Requires macOS' >&2; exit 2; fi
printf 'Available Instruments templates:\n'
xcrun xctrace list templates || true
cat <<'GUIDE'

For real GPU frame captures: run the native Metal viewer with Xcode attached,
enable Metal GPU Frame Capture, and capture a controlled in-world frame.
For programmatic capture on macOS 14+, launch with MTL_CAPTURE_ENABLED=1
and use MTLCaptureManager in viewer code with explicit capture boundaries.
Save local .gputrace / .trace under artifacts/metal/ (ignored from git).
Document source SHA, device, scene, settings, exact commands and salient
observations in docs/metal/state/CURRENT.md or a versioned redacted report.
No trace was recorded by this script.
GUIDE
