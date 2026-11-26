# PBR MASK palm: native MetalFX isolation

The tested viewer RGB resolve does **not** introduce the remaining palm contrast/stability failure. Immediate MetalFX output, the viewer-resolved output, direct classic-API output and a separately linked native replay produce identical RGB for the complete retained histories. All retain the same four failed palm bounds. No improved candidate or replacement viewer/DMG is justified by this result. This is **not** a finding of an Apple defect or an intrinsic vendor limit; PPD-610 remains open.

## Workload and provenance

- One offscreen production PBR MASK palm, Temporal 2/3 at 256×128 input→384×192 output,18 jitter phases, bias−1.5849624872207642 and normal device depth. Main history: 36 warm-up worlds,54 stationary worlds,18 moving worlds and 36 stopped worlds. Each path has a separate54-world final-camera reference history; each stream resets once at its first world.
- All 144 main and 54 reference worlds retain full color,depth,motion,rejection,output and per-world metadata. Same full frame dimensions/order,jitter,reset,depth and exposure are replayed. Source float32 color represents the actual16F input exactly; lossless half-packing is checked. RGB comparisons use the same pixels, with no flips,registration or color conversion.
- Evidence root: private `artifacts/metal/temporal-palm-native-isolation-20261126/`. Final receipt `direct-final-02.json` pins all test/header/CLI/CMake source hashes at run start. Test executable SHA-256:`059551fab4780cacac2350da5b9d86c546a54390ababb045a20b40c152ec601f`; unchanged69507 shader:`5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a`.
- Receipts identify starting HEAD `945246965c7325a39a427bc3d6efc0f676f65045` plus recorded fixture edits, not a clean-HEAD executable. This work changes tests/export only. Production quality settings/shaders are unchanged. No viewer login, live image-quality or FPS measurement was performed.

## Complete comparison

`almetal_world_temporal_color_fragment()` samples reconstructed RGB on the unjittered output grid and independently restores original glow-alpha at its input-jitter offset. Both outputs have equal dimensions/format. `direct-final-02` proves zero differing RGB components for raw→resolved and raw→direct across 31,850,496 components in 144 main worlds and 11,943,936 components in 54 reference worlds.

The independent `almetal_temporal_direct_replay` executable consumes the exported streams using Metal/MetalFX directly. It links Apple frameworks/system runtimes and no Alchemy rendering library; it loads no Alchemy shader library. `standalone-main-01` and `standalone-reference-01` complete144/54 frames, each with one reset and zero RGB differences against the integrated immediate output. Its executable SHA-256 is`0606dc0e00d23d0b3e1f71c4f8dcfdaa515f65ffc1b5a894f93580cb819b3289`. Dependency and independent pixel checks are retained in`offline-final-audit/`.

| Existing metric | Immediate / resolved / direct API | Standalone replay | Bound |
|---|---:|---:|---:|
| Mean adjacent RGB change |0.0036729858417721907|identical output|≤1/255, passed|
| Mean edge-band change |0.004314364957619261|identical output|≤1/255, **failed**|
| Edge-band frame p95 |0.0045685057890792174|identical output|≤2/255, passed|
| Minimum recovered recall |0.85446920949441629|identical output|≥.90, **failed**|
| Minimum recovered area |0.72172860622982393|identical output|≥.85, **failed**|
| Mean output edge energy |0.20455696902988851|identical output|existing detail floor passed|
| Stopped18 phase-matched error |0.0059885653451804913|identical output|≤1/255, **failed**|
| Stopped36 phase-matched error |0.002298313084378386|identical output|≤1/255, passed|
| Stopped18 old-history fraction |0.00043351592909049132|identical output|≤.02, passed|
| Stopped36 old-history fraction |0.00017885918770396472|identical output|≤.02, passed|

The logged17-digit C++ scores are identical for the first three paths. Independent offline scoring of the standalone pixels also matches them within floating accumulation rounding; byte-identical output and reference pixels imply identical deterministic scores. Four failed assertions repeated at three scored paths are12 failures, not12 causes. `direct-final-02.log` records 10515 assertions,10503 passed and 12 failed; the original quality bounds/oracle are unchanged.

Scores use the same84×84 palm ROI and fixed native-reference edge band over 54 adjacent stationary pairs. Stopped samples are frame indices125/143 (world IDs126/144) against each path's own final-camera phase17 reference; neither raw nor direct is scored against an unrelated resolved reference. Signed linear-green contrast retains 73.7611% of actual input across the54 stationary worlds in all four paths, without leaf classification or alpha recovery. Recovered area is an appearance proxy, not a missing-geometric-leaf count; Native0 is not an exact anti-aliased image oracle. The frozen [original twenty failures](temporal-foliage-quality-69506-baseline.md) and [69507 disposition](temporal-foliage-69507-review.md) remain separate and unchanged.

## Alpha and the first oracle failure

Vendor alpha has no assigned viewer glow contract. Restoring current original alpha intentionally differs from immediate MetalFX alpha in 6,966,905 main-output samples, maximum 1; this does not establish a viewer alpha defect. Direct/standalone alpha also happens to match the immediate output here, but is recorded observationally rather than treated as a general API guarantee.

The first `resolve-01`run failed a **new** ideal-float bilinear-alpha expectation at maximum error0.0031068471726030111. That run/log and its9 failed assertions are preserved: eight repeated palm-quality failures plus this new oracle failure. Independent offline analysis of 10,616,832 alpha samples shows nearest 1/256 fractional filter weights plus 16F store predicts measured alpha within 0.00048828125 maximum error and 2.286e−6 mean; no samples exceed.002 under that observed model. Ideal float interpolation was too exact an oracle for this sampler. This empirical M1 fit is not imposed as a universal GPU rule.

Only the new unsupported assertion was removed. The ideal-error observation remains reported; all existing glow-alpha tests and quality thresholds remain unchanged. RGB equality does not independently qualify later alpha-driven bloom in a different live scene. No such live defect is demonstrated here.

## Closed contracts and remaining limit

The source/installed-SDK audit covers classic API formats/usages, supported extents, current-to-previous input-pixel motion without double-counted jitter, reversed-depth flags, unit exposure/pre-exposure, reactive-mask grid, first-use resets and resource lifetime. Existing analytical fixtures and retained inputs identify no demonstrated contract violation in this feed. The separate replay now removes Alchemy command/resource/history/resolve wrappers from execution while preserving the supplied input history.

The installed MacOSX26.5 `MTLFXTemporalScaler.h` and [Apple integration guidance](https://developer.apple.com/videos/play/wwdc2022/10103/) independently define the applicable conventions. The complete retained inputs and existing analytical fixtures close the tested extent/format, row orientation, jitter correspondence, current→previous pixel motion, normal device-depth, unit exposure and first-reset/order checks. The replay also validates exact16F input packing. The SDK declaration/usage checks and analytical rendering checks are distinct evidence; neither promises a particular thin-cutout quality level. The newer macOS27 jittered-motion option is not used by this installed classic API. Exact header identities and per-contract evidence scopes are retained in `contracts/CONTRACT.md` in the portable reproducer.

This establishes that the remaining RGB response is already present at the native reconstruction boundary for this exact input/configuration. It does not expose private filtering/history weights or distinguish every possible input/material-policy improvement from the scaler's current quality response. Correct API use and a repeatable native result do not assign responsibility to Apple or prove an immutable quality limit.

Keep Temporal experimental and PPD-610 open; Native stays the default and Spatial remains an available alternative. No production correction, new application or DMG is claimed. A subsequent change requires a specific supported mechanism and must improve the retained contrast/edge/settling bounds while preserving detail,motion/occlusion and visibility regressions. Do not conceal the result through relaxed scoring, blanket rejection, added blur or a jitter freeze.

Final affected regressions pass15/15 suites with Metal API/GPU validation (`correctness-final.{log,json}`); the test bundle passes its one-executable ARM64 verification. These are correctness checks, not acceptance of the unchanged palm-quality result. No new application/DMG was produced.

## Reproduction evidence

- `direct-final-02.{log,json}` and full`direct-final-02/native-isolation/`: full-history inputs, per-world identities and all source outputs/references.
- `standalone-main-01.{log,json}` / `standalone-reference-01.{log,json}` and their `summary.json`/output files:144/54complete independent frames, one reset each, zero RGB differences.
- `offline-final-audit/{audit.py,result.json,standalone-identity.json,standalone-dependencies.log}`: independently checked counts, same-grid scoring and dependency evidence.
- `offline-alpha-audit/{audit.py,result.json}` and preserved`resolve-01.{log,json}`: empirical sampler analysis and first new-oracle failure.
- `contract-audit/{CONTRACT.md,source-and-sdk-identities.json,retained-sequence-check.json}`: installed classicSDK/official-reference checks and their scope. No large recording or live performance campaign was added.


## Portable reproducer

`Temporal-PBR-Palm-Native-Reproducer-20261126.zip` is a native command-line reproducer, not a viewer
package. It contains1023manifestedfiles: the complete198input/golden frames, both metadata streams,
SDK-only ARM64 executable/source, exact build/dependency receipts and findings. It requires no Alchemy
library, shader library, login or preferences. The included README documents standalone compilation and
replay. SHA256:`8a300a02f75e2b82ee130ea968a1f159c925d9c333dc0762535ebf64b3450622` (61209375bytes).
The ZIP and checksum file are on the private iCloud Desktop; Foundation confirmed both uploaded,
not uploading and no error at2026-11-26 10:53:03UTC. Compiled source files match sourcecommit
`6b2dcd8e44464a06d77ba5611e34cc7c0dffab18` exactly; final handoff receipt is private `final-handoff.json`. Different
GPU/OS kernels may produce different output; strict golden equality is scoped to the recorded M1 Max.
