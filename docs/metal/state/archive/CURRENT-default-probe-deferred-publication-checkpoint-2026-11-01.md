# Default-probe deferred-publication checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.  
**Branch:** `codex/apple-silicon-metal`  
**Source checkpoint:** `5f64b5ac2b47b4bf4ffbe0cc7c70d8118db1c6ef`
(`fix(metal): defer default probe publication`), pushed to the matching origin
branch.

## Source finding and correction

Source-led M-008 tracing found a bounded publication race in
`LLReflectionMapManager`. The ordinary native probe array queues SH work, but
the separate default-probe/material-preview mirror previously copied its cube
only if the 64-byte CPU-visible SH row happened to be readable during the same
radiance pass. Sustained GPU latency could therefore skip native default
radiance publication and leave that preview on a stale pair or GL/PBO fallback.

The default mirror now retains one private pending radiance cube plus the probe
generation and capture sequence. `collectNativeMetalProbe()` promotes the cube
only when the matching SH command completes and the row is readable. An error,
stale/recycled generation, sequence mismatch, device change or cube-layout
reconciliation discards the pending texture. A successfully retained coherent
native pair suppresses the redundant GL/PBO default mirror for that pass; a
later failure leaves the last coherent published environment intact. No global
OpenGL traversal, presentation authority or fallback gate was disabled.

A one-time diagnostic marker identifies actual delayed promotion:

```text
Published native default reflection-probe radiance after deferred matching SH completion
```

That marker did not occur naturally in either live run below. This checkpoint
therefore records a source-corrected timing defect and runtime regression
coverage, not physical execution of the rare branch.

## Build and validation evidence

- Warnings-as-errors object compilation passed in both Debug and
  RelWithDebInfo. A full wrapper build from the final source content produced
  build 68023, passed deep strict signing and verified 349/349 bundled Mach-O
  slices as ARM64. Its executable SHA-256 was
  `fce347735af5e80e57c954dc1833d078724384a2c733fdd5a201e0a9c0dabeec`;
  because the source was not yet committed during that build, it is not called
  a source-exact commit build.
- After committing and pushing `5f64b5ac2b`, the incremental wrapper output was
  observed as build 68024. `codesign --verify --deep --strict --verbose=2` and
  `tools/metal/verify_app.sh` both exited 0; 349/349 slices were ARM64. The exact
  executable SHA-256 is
  `fa718e362311afa7c74bff8cd641620e7a6b2b644f0465d3fee1fea7fa0e5fd3`.
- Complete RelWithDebInfo physical Metal tests passed 44,882 assertions across
  50 cases with Metal API/GPU validation enabled.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 259.50 seconds.

## Source-exact Computer Use evidence

The first direct-background launch exited before binding; the exact-path
preflight failed closed with `Expected one exact viewer process, found 0`.
LaunchServices was then used with the same isolated authenticated profile,
Metal validation environment and parser-safe repeated `--set NAME VALUE`
arguments, with `--url secondlife://Danim/101/108/64` last.

`tools/metal/cua_preflight.py` passed for exact PID 32032, Development build
68024, executable SHA above and profile
`/private/tmp/alchemy-mirror-candidate.buhQH0`. Actual Computer Use activated
the semantic Login control, reached `STATE_STARTED`, and observed a coherent,
upright world and UI at the intended Danim location. The log recorded the
production Metal surface and native world/probe warm-up; no Metal validation
error was observed. The deferred-promotion marker was absent.

Computer Use opened Me, activated Exit Alchemy and confirmed Quit. The run
logged `LogoutReply`, `Goodbye!` and `status: stopped`. CEF helpers were still
draining at the immediate census and reached zero 14.4 seconds later. Metal
validation launch environment was removed afterward. Authenticated logs and
screenshots remain local and untracked.

## Product assessment and next boundary

This advances the complete product by preventing a real native default
environment from being skipped solely because CPU SH visibility lags cube
encoding. It does not prove broad reflection-probe parity, eliminate OpenGL,
close M-008, or establish a performance improvement.

The next evidence-based priority is the next source-demonstrated non-default
probe-publication, post or advanced-view gap. Do not keep forcing this rare
scheduler boundary without new evidence. Any candidate/reference image pair
must fail closed unless exact source provenance, all 416 effective graphics
settings, camera, environment/time, warm state, window and snapshot contracts
match.
