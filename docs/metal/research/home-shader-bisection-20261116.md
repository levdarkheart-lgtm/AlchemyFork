# Home shader bisection — diagnostic cuts only

Owner instruction2026-11-16: use substantial temporary shader removals to locate
the expensive/spilling section. Incorrect diagnostic images are allowed;
compilation, safe execution and the same rendering setup are required.
No such cut is a viewer implementation or accepted quality change.

PPD319/327 stay open; broad architecture acceptance remains paused. Runtime
819f98d71c/build69224 remains unchanged. The preceding
[PCF experiment](home-pcf-control-20261116.md) is unselected:121.27→116.97→116.32ms
saved-scene A/B/A, registers222versus restored224, spill304B both. Correctness
passed, but scene timing was within control drift.

Private variants/evidence: `artifacts/metal/quiet-home-shader-bisection-20261116T2245Z`.
Root owns saved-scene replay and canonical docs; agents own only private source
preparation/read-only review. No viewer launch, new viewer recording or push.

## Controls and boundaries

Reuse the saved owner69200lateHome frame on M1Max/macOS26.5.2/Xcode26.6,
with a separate working copy and Medium/Overlapping verified. Hold shared High2
quality in every comparison. Preserve draw/dispatch counts, geometry, resources,
targets, depth/stencil/blend state, textures and all bound inputs. Preserve
surface alpha/water discard and output alpha where cuts allow it. Recompile,
measure and restore a control; retain drift rather than assuming a single
control is stable. No concurrent GPU tests or heavy compilation during timing.

Start with simple-alpha probe lighting bypass versus the rest of that shader.
If its removal changes spill/cost materially, narrow inside probe work; otherwise
isolate directional shadows and remaining lighting. A shared SH-evaluator cut
has broader scope than the simple-alpha-only cut and must be labelled as such.
The existing default-only-probe control already preserved304B, so repeat it
only if it answers a new, qualified question.

For each arm retain exact edit/source hashes, compiler flags, safe-execution
result, whole-workload GPU time, selected fragment's registers/spilled bytes,
and unavailable occupancy/per-line metrics. Compiler304B is not304B external
traffic per fragment. A cut identifies a dependency, not necessarily a unique
source object: register allocation and inlining are nonlinear. Changed colors
can alter later work, so total replay deltas are not isolated pass costs or
predictions of live FPS. Final implementation must retain all probe/lighting
behavior and pass correctness plus owner runtime checks.

## Status

Original three cuts compile/link0 in private evidence; source checks preserve
unrelated surface/discard/shadow/alpha contracts. Only the conditional cuts
needed for attribution are executed. Current saved-scene observations:

| Arm | GPU workload | Simple-shadow registers | Spilled bytes |
| --- | ---: | ---: | ---: |
| A1, High2 control |122.88ms |not reread |not reread |
| B1, simple-alpha probe call bypass |98.47ms |220 |32 |
| A2, original probe call restored |118.94ms |224 |304 |
| B2, shared SH/parallax evaluator bypass |103.46ms |222 |304 |
| A3, original SH evaluator restored |123.88ms |not reread |not reread |
| B3, one guarded complete SHtap0 |92.17ms |222 |32 |
| A4, full generic probe call restored |150.17ms |224 |304 |
| B4, base helper always_inline only |115.93ms |224 |304 |
| A5, original declaration restored |124.28ms |224 |304 |

Untouched-session142.10ms is retained separately, not a recompiled control.
Medium/Overlapping is freshly verified. B1 is17.21–19.86% shorter than its
controls; its compiler spill statistic drops32versus restored304. B2 is
13.01–16.48% shorter than its controls but leaves304B. SH is expensive in this
broader shared intervention, yet its evaluation is not required for304B to
remain. Do not equate total-workload deltas with selected-shader execution
time, or identify a unique source object from these nonlinear compilations.

The two prepared SH-half sources remain uncompiled/unrun because full SH
removal did not change304B. The next conditional boundary is generic probe
traversal/family accumulation/call state versus a guarded complete SH tap.
Two independent current819private sources compile/link0: simple-alpha direct
SHtap0 with classic/count/row/dimension guards, and an always_inline attribute
only on almetal_sample_base_probes. The direct-tap timing also removes other
SHtaps and its actual runtime guard/scale must be checked before claiming the
full body executed. The inline attribute affects all shared helper consumers.
No viewer implementation is selected from a diagnostic cut.

B3 retains the full SH function behind runtime guards but removes the generic
walk/accumulator/mix call. Its32B contrasts with B2's304B despite SH's removal
there. Thus304B depends on the generic path in these compiled variants; this
does not assign272B to a particular struct, array, call frame or memory traffic.
B3 also changes the number of SH taps; actual probe0scale/guard execution was
not inspected. Its workload time is not a pure traversal measurement.

B4 retains original source math/features yet leaves224registers/304B. Its time
is lower than both adjacent controls, but A4's large150.17ms variation versus
other118.94–124.28ms controls remains explicit. No reliable speedup or spill fix
is selected. An optimized-IR comparison is checking whether the attribute
changed generated function bodies at all; AIR still is not final AGX code.

Medium/Overlapping was freshly reverified after A4; it attempts a performance
state, not fixed measured GPU MHz. Instantaneous pmset therm context has no
recorded thermal/performance warning or CPU power status, which neither
explains the variance nor excludes clock changes. Seven expensive alpha draws
still lack exact retained buffer0/2flags; main-policy classic0 in safe logs is
not a substitute for per-draw execution proof. Source producers match fd5→819.

Every cut was undone before the next; final source undo restored the captured
runtime quality expression. GPU work stopped, Don't Save selected, Replay /
Finished running observed. All ten observations, adverse drift and UI limits
are in private recording-replay.json. Runtime819/69224 and original captures
remain unchanged. No viewer launch/new viewer recording/push occurred.

Subsequent bounded offline decoding identifies a compiler-generated diffuse
texture-selector stack table. A two-resource selector retains full probes and
SH yet removes all recognized stack operations. Actual saved-scene replay
confirms214registers/0spill versus restored224/304B, but does not establish a
timing gain. See [the selector investigation](home-texture-selector-20261117.md)
for native hashes, seven actual count-two draws, controls and proposed engine
qualification. The earlier probe-path cuts establish a code-generation
dependency, not the storage object's source origin. SH-half sources remain
uncompiled/unrun; shadow-off is compiled but unrun. No runtime memory,
occupancy or FPS metric is inferred from static instructions.

The owner requested code to share with another AI. Exact production source,
1496line focused verbatim helper extract, context, notices, hashes and prior
experiment history are exported locally under
`artifacts/metal/quiet-home-shader-review-20261116/`. They contain no captures,
logs, scene assets or private account/location data; no upload was performed.
Context explicitly distinguishes current819production source from old69200
recompiled saved-frame measurements and unrecorded per-draw selection telemetry.
