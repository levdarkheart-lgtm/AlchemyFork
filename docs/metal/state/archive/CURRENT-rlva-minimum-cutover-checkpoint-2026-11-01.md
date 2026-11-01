# RLVa minimum-cutover checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.  
**Branch:** `codex/apple-silicon-metal`  
**Code checkpoints:** `9c141e0887` (`@setsphere`) and `79356bfd4f`
(`@setoverlay`).

## Scope decision

The owner designated exact parity for all RLVa functionality as a non-release
nice-to-have. RLVa still needs enough native support that enabling it cannot
block Metal presentation or viewer-owned OpenGL removal. This exception does
not relax any core viewer, native-only, stability or performance gate.

Any future candidate/reference RLVa run is admissible only after RLV is enabled
in Preferences in both viewers, saved, and both viewers are restarted. The
persistent itemized remainder is
`docs/metal/work/deferred/rlva-parity.md` (M-013).

## Minimum renderer owners

Commit `9c141e0887` replaced the unconditional active-`@setsphere` presentation
rejection with a fail-closed Metal post owner. It consumes the same per-frame
RLVa policy/tween sample as GLSL, retained depth and inverse projection, and
chains blend, fixed blur, bounded variable blur, chromatic aberration and
pixelation in visual-effect priority order. Missing, malformed or failed state
retains GL authority for that frame.

Commit `79356bfd4f` removed the unconditional `@setoverlay` rejection from the
ordered native world/UI capture. The established overlay draw already enters
that stream as a textured `gUIProgram` batch; the native mirror carries its
texture, AnisoWrap sampler, streamed tint/alpha and blend. A missing Metal
texture mirror or any unsupported UI batch still invalidates the complete
transaction and leaves GL authoritative.

The bounded source audit found no other explicit RLVa native-eligibility
rejection. Forced camera textures mutate the shared face/draw-info texture
before native capture; avatar-distance silhouettes select the shared
impostor/color route; camera restrictions update shared CPU camera state; and
the remaining restriction families stay in CPU/UI policy paths. This is a
minimum cutover finding, not exact behavioural parity.

The preceding production-call reflection-probe audit also closed without a
code change: no reachable viewer-produced divergence was found and Metal's
extra bounds checks do not alter valid viewer-produced data.

## Validation evidence

- Focused `@setsphere` physical validation passed 44 assertions; that case plus
  the offline builtin contract passed 255 assertions across two cases.
- The retained-world ordered UI case covering the overlay sampler,
  tint/alpha and source-over composition passed 455 assertions in both
  RelWithDebInfo and Debug under Metal API/GPU validation.
- Complete RelWithDebInfo and Debug physical Metal suites each passed 44,947
  assertions across 51 cases.
- The architecture-neutral gate passed 484/484 in 247.94 seconds.
- Warnings-as-errors source-exact Development build 68031 at `79356bfd4f`
  staged and signed. An immediate repeat reported no work; deep strict signing
  and `tools/metal/verify_app.sh` passed 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `3f481298d69ecd8892d14271af072568996065ab10bd44c7571bf5eacae37efd`.
  Packaged/source MSL SHA-256:
  `06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`.
  Compiled metallib SHA-256:
  `53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.
- Harness validation and all 24 tooling tests passed after the documentation
  checkpoint.

## Boundary and next work

No restart-qualified live RLVa comparison was performed or required for this
minimum cutover checkpoint. Exact sphere/overlay image and tween behaviour,
touch masks, command/reply edges, malformed cases and RLVa cutout semantics
remain recoverable M-013 nice-to-haves.

Release work returns to the non-RLVa M-008/M-009 graph, beginning with the
isolated projector/cascade-shadow fixture and then transparency/material replay
and native-authority removal. Full feature/stability, strict no-GL and paired
M1 Max performance gates remain open.
