# Local decoder sizing and codec comparison — 2026-11-12

Owner request: fix decoder worker sizing, then benchmark Grok and OpenHTJ2K on
actual SL textures on M1 Max. All changes, inputs and results stay local until
owner testing and explicit push approval. The application retains OpenJPEG.
Defect register ID: PPD-005, source_confirmed, ready_for_owner; owner verification
pending and push approval not_granted. The register was introduced by concurrent
work during this task and is maintained locally without staging that work.

## Original behavior and minimal fix

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` and pre-fix
`d537f10611386616203e3057004568467aa44dd2` both size ImageDecode workers in
`LLAppViewer::initThreads`: hardware concurrency, optional legacy graphics-vendor
halving, `EmulateCoreCount` cap, then `clamp(cores - 6, 2, 16)`.
Thread initialization precedes window/vendor detection. The native renderer's
Apple flag is separate from the default-false legacy `gGLManager.mIsApple`.
On this 10-core/no-SMT M1 Max, halving produces five effective cores and only
two workers. The original invariant is bounded background decode concurrency,
at least two independent lanes, a six-core reserve and an explicit emulated
core cap. GPU vendor state is not needed for this CPU policy.

Local commit `160a5f45bb` removes only that obsolete halving and logs detected,
effective and selected counts. Default selection is now four workers; the cap,
reserve and 2–16 bounds remain. Extracted actual source bodies compile and run
against 16 boundary/cap cases: original nine failures, fixed zero. Hardware
probe reports ten cores, old two workers and fixed four. This is policy
correctness evidence, not proof of a viewer loading-time or frame-time win.

## Codec experiment

Host: Apple M1 Max, 10 physical/logical CPUs, 64 GiB, macOS26.5.2,
Xcode26.6/AppleClang21, native ARM64. Doctor exit0.
Raw local evidence: `artifacts/metal/texture-decode-20261112/`.
No viewer session, network asset requests or texture uploads are part of this
experiment. Source cache is read only; copied inputs remain ignored/local.

- Baseline: viewer-installed patched OpenJPEG2.5.4#30 static Release library.
- Grok20.4.25: `1d5e48ebd6f5ea4a0c0dec41e3982c2cb661ac00`, AGPLv3,
  minimal Release ARM64 CPU build; proprietary GPU plugin excluded.
- OpenHTJ2K0.27.0: `72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2`,
  BSD-3-Clause, Release ARM64/NEON, apps/RTP/QUIC disabled.

The extractor reconstructs the 600-byte header plus body of each cached J2K
codestream. There were 3,523 supported complete and 12,533 partial inputs;
565 unsupported headers and 13,653 inactive entries were excluded. Selection
balances size/component strata rather than reflecting natural scene frequency.
Complete corpus:192 assets,47.12MiB compressed,95 RGB/97 RGBA,4–2048 pixels
per largest dimension. Partial corpus:48 assets,0.74MiB,25 RGB/23 RGBA.

The standalone adapters preload bytes, create a fresh codec per request,
decode all components and interleave top-down unsigned8 pixels. This includes
setup, decode, output conversion and destruction, with one inline execution
lane per outer worker. OpenJPEG API threads=0; Grok/OpenHTJ2K API threads=1.
OpenHTJ2K's selected safe input API makes one compressed-data copy; the others
use memory input directly. All adapter compiles use AppleClang/C++23/O3/ARM64.

Validation compares complete pixel buffers, including alpha, across 768
complete-input/reduction cases and192 partial cases. Each resolution manifest
contains independent requests at reductions3/2/1/0, constrained by available
main COD/COC levels. An independent scan found no tighter tile override in
these240 assets. This models mixed-resolution CPU work, not network arrival or
incremental decoder-state reuse. All three codecs successfully decode all960
cases with matching dimensions/component counts.

For complete inputs, all244 reversible cases are byte-identical. Grok has365
exact/403 differing cases; OpenHTJ2K406 exact/362 differing. Every difference is
at most one8-bit value, including alpha. The irreversible reconstruction
rounding differs; no exact visual/cutout parity is inferred. Partial inputs:
Grok44 exact/148 differing, with maximum141 and alpha23; OpenHTJ2K71 exact/121
differing, all at most one. Grok partial-stream reconstruction needs resolution
before it can replace the current viewer codec.

The benchmark excludes failed decodes and measures complete inputs only, with two/four outer
workers, one warmup pass, two measured rounds and three interleaved blocks.
Hashes/file reads/dumps are outside timing; batch wall time includes worker
creation/join. No rendering or compilation runs concurrently with timing.

## Measured CPU results

Timing source `64f10455ce`,36 sequential invocations and six measured batch
samples per variant (three blocks). Every run exits0, all measured cases
succeed, and thermal state is0/Nominal before/after every invocation. AC power;
power-mode setting2 is preserved in `timing-environment.json` without changing
the host's settings. `timing-summary.json` contains ranges and all two/four
worker combinations; `timing-runs.json` records argv, exits and thermal samples.

Median seconds per complete corpus batch:

| Decoder / outer workers | Full resolution,192 requests | Mixed resolution,768 requests |
| --- | ---: | ---: |
| Current OpenJPEG /2 | 4.378 | 6.989 |
| Fixed OpenJPEG /4 | 2.351 | 3.613 |
| Grok /2 | 3.592 | 5.999 |
| Grok /4 | 1.864 | 3.112 |
| OpenHTJ2K /2 | 3.613 | 6.025 |
| OpenHTJ2K /4 | 1.889 | 3.126 |

Changing only OpenJPEG outer concurrency from two to four reduces batch wall
time46.30% full/48.31% mixed (1.86x/1.93x throughput). At the same four-worker
count, Grok reduces time20.68% full/13.86% mixed, and OpenHTJ2K19.63%/13.49%.
The chosen corpus therefore provides no evidence for a several-fold codec
replacement gain. Retain OpenJPEG with corrected concurrency first.

These isolated CPU figures do not establish a46–48% improvement in viewer
relaunch time or FPS. Viewer main-thread processing, competing work, disk I/O,
networking, Metal texture creation/mips and real visibility/priority scheduling
are excluded. The balanced corpus deliberately differs from scene frequency.
Codec changes also require progressive-stream/alpha correctness and license
review; Grok's partial results currently make it the weaker compatibility
candidate, despite its slightly faster full-input timing.

Reproduction and evidence: `tools/metal/texture_decode/README.md`,
`corpus.json`, `adapter-builds.json`, `openjpeg-provenance.json`,
`grok/build-receipt.json`, `openhtj2k/provenance.json`,
`validation-runs.json`, `pixel-comparison.json`, `reversible-checks.json`,
`timing-{environment,runs,summary}.json` and the exact local orchestration
scripts in the artifact directory. None of the corpus or raw pixels is committed.

## Build and handoff

Source64f10455ce, Release/LTO OFF, dependency installation disabled; reused the
existing local shadow-candidate build directory with a new channel/app name.
`viewer-build.json` records complete argv, environment and exits:

| Check | Result |
| --- | --- |
| `tools/metal/doctor.sh` | exit0; M1 Max/64GiB/macOS26.5.2/Xcode26.6 |
| Actual worker-sizing bodies,16 cap/boundary cases | old exit1/9 failures; fixed exit0/0 failures |
| `python3 -m unittest discover -s tools/metal/tests -v` | exit0;56 tests pass |
| CMake `ninja-os-arm64` configure | exit0 |
| `AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=6 tools/metal/build.sh build` | exit0 |
| `tools/metal/verify_app.sh <candidate>` | exit0;346 ARM64 Mach-O files,0 invalid |
| `codesign --verify --deep --strict --verbose=2 <candidate>` | exit0 |
| Cache integrity check | original entry/header hashes and all copied input hashes unchanged |

The first documentation check reported the160/180-line recovery-document limits;
the existing text was condensed and the final harness/diff checks pass. Exact
final commands, captured exits and logs are in `verification.json`.
An independent chat briefly added a required defect-register check before its
document existed, causing one final tools rerun to fail; the failure log is
preserved. With that document present, all56 tests and the33-file harness pass.
Those independent uncommitted changes were left intact and excluded here.

Local candidate26.4.0.68855 (`26.4.0d68855`):
`build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Decoder Workers.app`.
Executable SHA256:
`13c2862c6e9eaa87a596e9f2dde2ee4c61516e246b697934a6bc96be6f1af146`.
It retains the current local renderer changes and OpenJPEG; the only application
source change in this task is worker sizing/logging. A new app/channel directory
preserves the prior candidate and owner-passed viewer. No candidate launch,
login, settings modification, push or upload occurred during this task.

Owner next action: launch this exact candidate and confirm startup logs report
four ImageDecode workers, then compare warm-disk relaunch texture settling and
interactive frame pacing against the prior viewer under the same scene/settings.
Actual relaunch time and sustained busy-scene contention remain unmeasured;
alternative codecs have not been integrated. All changes stay local pending
owner testing and explicit push approval. This advances the measured CPU asset
decode boundary, not full-product parity/performance/runtime release gates.
