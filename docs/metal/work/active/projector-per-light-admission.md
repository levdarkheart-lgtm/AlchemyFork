# PPD-001 — projector admission belongs to each light

2026-11-12. Owner selected PPD-001 in the defect register. Base source
`7a49b990ffd3b62b4ba8c4641d09388c1a42bb57`, local branch
`codex/apple-silicon-metal`. No push approval for this repair.

## Original mapping and chosen contract

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `pipeline.cpp`
ordinary-light loop11928–12023, projector loops12052–12081/12154–12180 and
void `setupSpotLight`12479: projectors draw independently after ordinary lights.
A problematic projector cannot veto remaining lights or world publication.
Original `deferredUtil.glsl`343–352/393 and native
`almetal_builtin.metal`12363–12365/12416–12418 use signed
`(distance-focus)/range` for cookie LOD. Preserve finite nonzero signed range
and the original forward projection with its [-1,1] cookie Z remap.

Concrete valid case: FOV pi/2, scale(1,1,2), physical radius1.1/effective1.65,
identity rotation: near.5, far.15, range-.35. A receiver at(0,0,-.75) lies
inside the cookie and light volume and can receive legacy/PBR light. Positive
near/range is not the original contract. FOV0 and range0 are singular;
nonfinite packed parameters likewise cannot be submitted to Metal.
The tools UI admits FOV0; network light parameters are not UI-clamped.

Chosen repair: exclude only the offending singular/nonfinite per-light math,
continue collecting valid neighbors, and share packed-parameter validation
with the backend to avoid a later all-light rejection. Keep cookie-resource,
shadow readiness, G-buffer/target compatibility and actual encoder failures
distinct and fail-closed. Publish an empty projector stage when no eligible
lights remain, as for an ordinary empty list. Do not bypass composition flags.
No shader math, draw budget, texture fallback or shadow ownership change.

## Evidence and limits

Local evidence: `artifacts/metal/ppd001-reproduction-20261112/`.
`python3 artifacts/metal/ppd001-reproduction-20261112/run.py` compiles the actual
collector/composition bodies with API spies: baseline compiles0, run1,15/34
failed checks. Negative-range B must retain A/B/C; zero-range or FOV0 B must
retain A/C; each successful stage must allow current-frame composition.
Valid-only, empty, excluded attachment and real renderer/shadow failure controls
pass at baseline. This proves CPU routing, not shader pixels or the cause of
the owner's busy-region freeze PPD-002. Physical GPU regression and candidate build now pass as recorded below.
Owner verification remains pending.

## Local corrective verification

Implementation commit `6404f9f6ad431dda9e679f9d220b5a79ba86a20e`:
collector zero-only input and derived finite/nonzero-range guards continue per
light. Failed affine inverse is checked before its unwritten destination can
be used. Complete packed-parameter admission precedes native cookie readiness;
only admitted lights can require shared shadows. The same validator remains
strict backend preflight before command acquisition. No shader change.
Independent original/source review found no remaining actionable issue.

- Actual production collector/composition and parameter validator: final16-case
  fixture compiles0 (`-Wall -Wextra -Werror`); base7a49 fails37/72 checks,
  candidate6404 passes72/72. Includes valid signed FOV/scale/range, zero FOV/range,
  nonfinite ambiance, injected inverse API failure, malformed shadow owner,
  cookie fallback, unavailable native cookie, empty list and real failure
  controls. `run.py --baseline` exits1 as intended; `run.py` exits0.
  See `artifacts/metal/ppd001-reproduction-20261112/{red,green}-receipt.json`.
  Matrix/inverse/resource APIs are spies: routing evidence only.
- Physical Apple M1 Max Metal test passes27016 assertions/1case, exit0, with
  API and GPU validation confirmed in log. Original-derived forward matrices
  with range-.35 and finite negative near produce cookie-lit pixels in legacy
  and PBR. A direct backend batch with a valid light followed by range0 is
  rejected at index1 and leaves the whole zero target untouched.
- Doctor/configure-tests/build-tests/configure-viewer/build-viewer all exit0.
  ARM64 Release, LTO off; existing accepted experimental OpenHTJ2K remains on.
  Owner-accepted68860 app is preserved; new candidate is separate channel
  `Alchemy Metal Projector Fix`,26.4.0.68869. No viewer launch/login performed.
- `verify_app.sh` exit0:346Mach-O files,0non-ARM64 slices. Deep strict signature
  verification exit0. Candidate executable SHA256
  `d71560aa643ddd90b7aa07c79120c5d5311516a9bd58ff2761e0e71b598e94e5`.
  Preserved68860 executable still matches its accepted SHA256.
- Production strict source audit:3515files,912informational,0blockers/0errors,
  exit0. This is static source evidence, not runtime/foreign-media no-GL proof.
- Documentation harness passes33required files; no repository tool changes.

Exact commands (logs and hashes retained locally under
`artifacts/metal/ppd001-projector-fix-20261112/`, `validation-receipt.json`):

```sh
tools/metal/doctor.sh
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Projector Fix' -DAL_BUILD_TESTS=ON
cmake --build build-Darwin-ninja-os-arm64-shadow-occlusion --config Release --target almetal_tests --parallel 6
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1 build-Darwin-ninja-os-arm64-shadow-occlusion/almetal/Release/almetal_tests.app/Contents/MacOS/almetal_tests 'almetal: native world G-buffer feeds exact deferred point lights' --reporter compact
python3 artifacts/metal/ppd001-reproduction-20261112/run.py
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Projector Fix' -DAL_BUILD_TESTS=OFF
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=6 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Projector Fix.app'
codesign --verify --deep --strict 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Projector Fix.app'
python3 tools/metal/audit_gl.py --mode strict --output artifacts/metal/ppd001-projector-fix-20261112/source-audit.json
python3 tools/metal/validate_harness.py
```

Next executable action: owner runs68869 in their busy scene and checks retained
world/lighting behavior. PPD-001 is `ready_for_owner`; PPD-002 attribution remains
unproved, PPD-003 diagnostics remain a separate defect. No performance gain or
full visual parity is claimed. Keep this scope local until owner testing and
explicit push approval.

## Owner-requested launch

2026-11-12 14:05:32 UTC: owner requested starting the viewer. Verified exact
68869 executable hash against the candidate receipt, confirmed no Alchemy viewer
already running, then `open -n` the absolute `Alchemy Metal Projector Fix.app`
bundle. Command exit0; exact executable observed as PID85968. Receipt:
`artifacts/metal/ppd001-projector-fix-20261112/launch-receipt.json`.
Owner controls login/location/settings; no scene, screenshot or visual-parity
verification is implied. Owner acceptance/push approval remain pending.

## Owner-requested teleport profile

Owner restarted the same hash-verified68869 candidate at14:15UTC (PID86326),
then requested profiling before teleport and an explicit ready signal.
`xcrun xctrace record --template 'Game Performance Overview' --attach 86326
--time-limit 3m --no-prompt --output <private>/projector-before-teleport-3min.trace`
ran to its time limit and exited0. Recording-active confirmation preceded the
owner's teleport. Exported TOC exits0:14:23:13.598–14:26:14.638UTC,
181.040704seconds. Sanitized viewer-log anchors place teleport start14:23:39
and finish14:23:42UTC, both inside the trace (about25.4/28.4seconds after start).
Owner reported arrival during recording. This captures the transition, unlike
the earlier post-arrival decoder trace.

Local ignored evidence: `artifacts/metal/ppd001-profile-20261112/`, including
capture-plan, recording-active, owner-arrival-report, runtime-events,
capture-complete, record.log, profile-toc.xml and raw trace. Instruments emitted
DWARF cursor warnings during finalization but saved successfully; detailed CPU
stack-symbol completeness and metrics have not yet been analyzed. No matched
performance comparison, fresh-world/visual pass or owner fix acceptance inferred.
PPD-001 remains ready_for_owner; PPD-002 cause remains unproved. No push.

## Profile analysis completed

See [decode, render and cadence analysis](../../research/projector-teleport-profile-2026-11-12.md).
CPU410258samples/410.258CPU-s; main119.640s, four decoders204.803s.
Shadows48.475s/40.52%main, texture updates13.139s/10.98%, geometry12.585s/10.52%.
First30safterarrival decode115.564CPU-s; late150–180sbin2.364s. Decoder MQ/bitplane
exclusive leaves84.23%; projector-validation leaves0.010s. All CPU/metrics exports
and analyses exit0; local plot verified. Overview zero-present anomaly49.368–64.358s
has continuing CPU/frame-loop work, no single long sampled CPU block; window
visibility and actual-present/fresh-world correlation remain unknown. No PPD-002
cause, owner acceptance, measured gain or new implementation claimed.

## Second session with retained disk cache

Owner requested another run using the existing cache, then said Start. Exact
68869/source6404/hash d71560aa643d was relaunched asPID89531 without cache purge
or graphics/location overrides. Recording-active prompt was observed before
signaling the owner; Game Performance Overview attached for3m.

Recorder reached its limit but exitedSIGTRAP(-5) during Apple ktracePlugin /
XRTapAgent teardown (libdispatch: release of a suspended object). Viewer stayed
running. Original partial trace and423432368-byte temporary raw ktrace were
preserved with APFS clones (both exit0), then imported into a new recovered
trace with the same template (exit0). TOC export0 confirms181.345001seconds,
14:51:43.667–14:54:45.012UTC. CPU analysis must explicitly filterPID89531 because
recovery imports AllProcesses. Original failure and sanitized crash analysis
are retained; recovery is not a successful original recorder exit.

Evidence: `artifacts/metal/ppd001-warm-profile-20261112/`, launch/capture plans,
record-exit, recovery-preservation, recovery-import, profile-toc, runtime-events,
crash-analysis and capture-complete receipts. No teleport event occurs within
this recording; the latest logged teleport-finished event is20.667s before it.
This is a post-arrival warm-session capture, not another teleport transition.
Existing disk cache was retained; actual cache hits, decoded residency and
matched scene/settings remain unmeasured. Recovered CPU export/analyzer exit0 verifies335315target Running samples,
132.096main CPU-s and full0.005–181.345s main-sample span; all37five-second bins
contain main samples. Raw imported function names are address-only; UUID/load
verified offline main-binary symbol restoration is in progress before hotspot
comparison. All six metric exports/analyzer exit0:180.874seconds coverage, one
target viewer layer,22.423FPS covered cadence, nozero-FPS buckets. These are
Overview metrics, not a fresh-world pass or causal performance comparison.
No owner acceptance/push inferred.

Warm symbol recovery now passes: recordedKCDATA PID89531/mainTID98151771 and
UUID506571D0-455F-3A76-9DB9-C714EFADF593 prove load0x102e44000; native SDK array
layout and exact binary SHA/UUID/text range validated. Exact-binary atos resolves
20351/20575main-text PCs (98.91%unique addresses, not weighted CPU coverage).
224main PCs and8086external frame definitions remain explicitly unresolved.
Derived `time-profile-symbolicated.xml` changes only frame-name strings; all
335315rows, weights, times, refs, raw frame addresses and non-name XML bytes
are identical. Raw XML/ktrace/binary hashes remain unchanged. See
`symbol-recovery/receipt.json`. Extended dSYM generation was intentionally
interrupted; its preserved partial bundle is invalid and was not used.

Next executable action: run the preserved clean CPU analyzer on the derived XML
with `--pid 89531 --toc <warm>/profile-toc.xml --bins 5 30 --top 80`, writing
`<warm>/cpu-analysis-symbolicated.json`; exact argv is in capture-complete.json.
Then compare appropriate post-arrival/late windows, preserving unmatched timing,
visibility, scene/settings and unknown actual cache-hit coverage. Recording has
stopped; this handoff does not claim a warm-cache speedup or owner verification.

## Completed retained-cache analysis

[Full warm CPU/decode/GPU report](../../research/warm-cache-profile-analysis-2026-11-12.md)
completes the analysis after the earlier capture-only handoff. Canonical/detailed
analyzers both exit0 with335315rows/main132.096CPU-s/all335.315CPU-s exact.
Four decoders142.911CPU-s; first30s104.310, exactlast30s0.645. Shadows56.053CPU-s /
42.43%main, remaining44.91%main inlast30s. Recordedcadence22.423FPS, last30s22.377.
Appleinstalledenumproof establishesdurationvariant0=Average: GPUassociated
inflight69.823ms, GPUbegin-end70.232ms, CPUbegin-present38.254ms, frameinterval44.620ms.
Theseareoverlappingframespans, nothardwareoccupancy orisolatedGPUexecutioncost.

BoundedMetalSystemTrace reimport ofexistingraw exits0; detailedGPUencoder/label/
command/execution tables have0rows, so per-passGPUhotspots cannotbe reconstructed.
CApresent request/callback events exist buttheirlatencysemanticsremainunverified.
StrongestnextCPUexperiment: generation-qualifiedvalidatedpublished attachment
palette reuse (2.726CPU-s immediatehashcaller), followedbybroadermeasuredshadow
prep/emission work. RGB8-specializeddirectstagingtargetsloading, notlatecadence.
KeepacceptedOpenHT; noapplicationcodechanges orpush. PPD-001owneracceptancepending;
PPD-002causeunproved, PPD-003diagnosticlimitsretained. Harnessrerunatthishandoff.

2026-11-12 owner explicitly approves committing/pushing this named projector
repair and its tests, together with directional-shadow occlusion and the
restored4normal/6maximum adaptive decoder policy. Publish on
codex/apple-silicon-metal. No explicit scene-correctness confirmation is inferred
from permission to push: PPD-001 owner verification remains pending, PPD-002
symptom/cause remains unresolved and raw captures stay local.
