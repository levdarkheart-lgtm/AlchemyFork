# Home diffuse texture selector and spill allocation

PPD-319 and PPD-327 remain open. The broad architecture rollout remains paused
behind the Home checkpoint. Last owner-tested viewer is source819f98d71c,
build69224. This work uses saved captures and contained tests; no viewer launch,
login, new viewer recording, push or capture upload occurred.

## What the native code identifies

A reviewed, pinned BSD-3-Clause G13 decoder inspected the actual M1 native
fragment object from the current819 High2 descriptor. Sixteen vector stack
stores materialize 32 literal pairs, with values `(0,1024)` through `(31,1024)`.
An indexed stack load then feeds the diffuse texture sample. The full decode
contains 17 recognized stack stores and one stack load; the extra store is
retained in the evidence rather than assigned a source object by guesswork.

This points to the 32-entry diffuse texture selector, rather than the packed SH
coefficients or probe-family arrays, as a concrete compiler-generated storage
object. The precise descriptor meaning of 1024 and the complete decomposition
of Xcode's 304-byte allocation remain unknown. Static instructions do not
measure executed spill traffic, occupancy or external memory bandwidth.

The decoder is pinned to applegpu revision
`28dd80468d4a6c94fa4dac34ed488196f5fedc5f`; only reviewed decoding modules ran
offline against private object bytes. The compiler-explorer build wrapper was
not run. Baseline native text SHA-256 is
`2716fd0e3e7dc651b5c148f14d5a86ff6a370bcd741c2556e43219aa9bbec096`.
All bytes decoded, with two explicitly retained TODO.ldcf interpretations.

## Actual saved-scene contract

Xcode vertex-output inspection of all seven previously identified expensive
2256-index draws (29008,29019,29030,29041,29052,29109,29120) reports the uniform
texture count as **two**. First-vertex texture index is zero in each, which does
not establish that every vertex uses zero. A texture-zero-only replacement
would therefore be unjustified.

The renderer copies the same validated batch texture count into vertex
constants and uses it to bind slots 0 through count minus one. The proposed
optimization can rely on that existing ownership boundary, without inferring a
count from a graphics preset, scene name or first vertex.

Original reference revision
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` uses a direct sampler for its
single-channel shader generator and sampler selection for indexed channels.
Ordinary original alpha requests globally configured channels; no original
per-draw count-one PSO is claimed. The native optimization preserves the same
texture/color, alpha, water, lighting and depth behavior through a new Metal
function-constant specialization.

## Saved-scene selector intervention

The existing old69200 late Home capture was replayed in a separate copy on
M1 Max/macOS26.5.2/Xcode26.6. Shared shadow quality was held at two in all arms.
Medium/Overlapping was freshly checked; agents held compilation and GPU tests
quiet during timing. Only the simple-surface diffuse assignment changed.
Full probes, SH, PCF, local lights, fog and alpha/water discards remained.

| Arm | GPU workload | Fragment registers | Spilled bytes |
| --- | ---: | ---: | ---: |
| A1, generic selector |139.96ms |not reread |not reread |
| B, two-resource selector |116.80ms |214 |0 |
| A2, generic selector restored |114.01ms |224 |304 |

The intervention **removes the spill allocation**, and restoration brings it
back. It **does not establish a frame-time gain**: B is slower than A2 and the
controls drift substantially. Shader workload shares were 22.91% and22.30% for
B/A2; percentages are not isolated execution durations. Untouched-session
131.90ms is retained separately, not treated as a recompiled control.

The replay edit affected all simple batches. Counts of the seven hot draws
were proved; batches with more than two textures elsewhere could render
incorrectly. This was a diagnostic intervention, not a pixel-parity test. The
production selector below qualifies each batch by its actual count.

Original diffuse assignment and runtime shadow-quality expression were both
observed restored. GPU replay was stopped, Don't Save selected, and Replay /
Finished running observed. Original captures remain untouched. The A2 reload
tool call timed out during compilation; its pending activity was inspected and
the operation was not submitted twice.

## Proposed engine change — integration qualification

Optional Bool function constant4, named `almetal_simple_diffuse_pair`, selects
an opaque texture handle from slots zero and one, followed by one implicit
sample inside the existing `texture_index < texture_count` guard. The original
magenta guard output, alpha/water discards, derivatives, lighting and precision
remain. False or undefined retains the original 32-entry selector.

Forty prebuilt standard-blend simple-alpha variants compose this constant with
the existing HDR, coverage, rigid/skinned, probe single-tap and High2 states.
Selection uses actual batch count one or two after existing probe/quality
selection. Other counts, avatar, custom blend and depth-only owners retain
their previous pipelines. No draw-time PSO creation or cache mutation is added.
Missing or differently named/typed optional metadata preserves the supported
older-library path; genuine PSO creation failures still propagate.

Private prototype6c129787 compiles and exports/reopens strict-hit native
archives. Its false and undefined native text exactly match the baseline hash
above. True text SHA-256 is
`6ad8f0ab6d3a6c85f563aad4017426ecdda2a617b61c8649cb9e920a018beeb8`:
one diffuse sample, two-resource selection, no recognized stack operations.
The final production MSL, SHA-256
`125b710e6cfce1ebc240ccdf6f328d15a29e0f5b942eed0b8f130b252b619eef`,
also compiles/links and passes all four strict archive reopens. Its unset/false
and true native text match those same hashes exactly. One bounded final decode
covers all35168bytes/4767rows, with no recognized stack operations and two
retained TODO.ldcf interpretations. An optional post-parser field correction
is retained as PPD-337; raw decode, code identity and counts did not change.
This qualifies the selected main/unrigged/HDR/shadow PSO's generated code,
rather than every PSO or its runtime cost.

The private sparse gate passes **232 image comparisons / 27,433 assertions**
with API validation. All raw half output bits match, maximum error zero, and
both arms have 2560 actual non-clear pixels in every row. Count one binds only
slot zero, count two only zero/one, explicitly clearing unused slots. Distinct
texture, mip, guard, shadow-quality/probe and actual unshadowed HDR cases are
retained. Initial Catch2 compound-assertion compile failure is PPD-336; its
failed source/log remain, and only parentheses changed before the fresh build.

The bounded validation-off cost check retains 400 raw observations across five
cells, including warmups and 30 alternating measured pairs per cell. Paired
median GPU changes are -15.790%/-14.580% for count-one automatic/mixed probes,
and -15.664%/-15.159% for count two; every measured pair favors the specialized
arm. Generic count32 is neutral (-0.798%,13/30 adverse pairs). This is a
controlled overdraw fixture, not whole-scene timing or live FPS.

Independent final source review found no selection, descriptor, compatibility
or reload blocker. Code is frozen in local commit `840bae8c33`; the clean
builder fast-forwarded to that exact commit. Doctor and the prescribed Release
build pass (221.23s). Changed Release C++ commands retain `-O3 -arch arm64`;
Metal retains3.1/fast-math/macOS26/source-recording flags. These are observed
commands, not a claim that debug source records improve performance.

The actual CMake library SHA-256 is
`a40a256178b1747f9c587d631c172f1d8b6cfb4feac99f8f560e5141ebe17b22`.
Its four strict archives also pass and native text is byte-identical to the
qualified unset/false/true objects. A fresh232-image/27433-assertion sparse
gate against that exact library passes with unchanged input hashes and exact
half bits. No inference from a stale prototype executable/library is needed.

| Integrated gate | Result |
| --- | --- |
| Function constants, including named metadata |431 assertions /1case PASS |
| Explicit older library compatibility |80 /1 PASS |
| Full renderer, actual selector/High2/probe composition, reload and strict probe fusion |2380513 /457 PASS |
| Disk archive, API validation only |5905 /6 PASS |
| Selective shader instrumentation/archive behavior |98 /1 PASS |

All five commands exit0 with no failures/skips and unchanged source, executable
and built/bundled library hashes. Total466case executions/2387027assertions
includes focused overlap; it is not466distinct tests. API/GPU shader validation
is on for ordinary correctness. Disk tests correctly unset shader validation;
selective tests use their documented selection. No cache policy or correctness
threshold was weakened. Default developer benchmark protocol unit cases in the
full selector are not used as new performance evidence.

The separate **26.4.0.69228** app has8272 exact non-root nodes, comparing file
contents, modes and link targets against the built source app. ARM64 inspection
passes346Mach-O files; strict/deep signature verification passes. Strict source
audit scans3608first-party files with0blockers/0scan errors. Main executable
links Metal and has no inspected forbidden graphics-framework or owned GL
imports. New viewer runtime/context and foreign subprocess behavior are not
established by those static checks.

Staged executable SHA-256 is
`d6045737fd0231df26be6e8d2099e1c40cbdcf642be5acbb9dff410f1f22ba9a`.
Exact app, commands, hashes and logs are in private
`artifacts/metal/quiet-home-selector-integration-20261117T0120Z/ready-manifest.json`.
The unchanged previously qualified180s Home launcher passes dry-run, preserves
the same19architecture arguments, and uses Overview/native timing with API/
shader validation and capture layers off. No new run has started. The owner
availability question is pending; the concrete app is ready for that checkpoint.

Default-off diagnostic census records constants0/3/4 from successfully encoded
descriptors. Owner Home initial/settled FPS, quality and reset/decline behavior
remain the runtime acceptance boundary. This component is ready for owner
evaluation; PPD-319 and overall performance/architecture acceptance stay open.

Private evidence roots are
`artifacts/metal/quiet-home-shader-bisection-20261116T2245Z/texture-selector-cut`,
`texture-pair-specialization`, and `texture-pair-specialization-checks`.
Earlier failed/unselected accumulator, ambient, PCF and inline experiments are
preserved in [the bisection history](home-shader-bisection-20261116.md) and its
linked reports. This discovery corrects source attribution; it does not erase
the measured probe-dependent cost or explain the complete live decline alone.
