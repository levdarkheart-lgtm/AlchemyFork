# Scheduling candidate 69450 — PPD-556 and native preferences

Status: **owner accepted; commit/push approved on2026-11-22**. Owner reports
"looks good..commit and push" after testing the exact staged69450 candidate.
Full build, component/static validation and immutable staging pass. Previously
observed exact PID31214 launch used temporary AutoLogin=false; agent performed no
login or scene collection. Accepted Maintenance69445 remains unchanged.

Source 960bd3644a09e6da8da57715947ef479fa16de18, indra tree
7d8c095d360bd3f77ac64d1b69e23c21840e0011, codex/apple-silicon-metal.
Local implementation commits a27fb1a57e and 960bd3644a.
Host: Apple M1 Max/64GiB/macOS26.5.2/Xcode26.6; native ARM64 Release.
[Execution ledger](../work/active/ppd556-bounded-scheduling-20261121.md) retains
commands, source identities, all failed attempts and private evidence locations.

## Resulting ownership

PPD-556 is a conditional correctness defect: a request retry could block its sole
consumer by posting into the full bounded queue that the same consumer must drain.
The actual old scheduler reproduces this with a tiny positive capacity. The repair
separates two kinds of ownership already present in the viewer:

- Every accepted request retains one reusable scheduling node until it is reaped.
  Retry, wake and promotion rekey that node without callback admission or further
  allocation. Registered requests, including retained terminal results, remain
  bounded by the existing default1,048,576 request limit.
- Named callback queues remain independently bounded, with truthful nonblocking
  admission and a coalesced native owner wake. No unbounded overflow, dropped
  accepted retry, consumer self-post or cross-owner request lock is introduced.
- Worker/cache/LFS/audio producers distinguish busy from closed/nonapplicable
  work, roll back nonaccepted ownership and retry where appropriate. Once-only
  WAV finalization and exact generation/pointer priority pins preserve lifecycle.
  Shutdown closes admission, settles accepted work and waits for the actual owner.

The existing scene/render/resource/upload owners remain intact. This is a native
CPU scheduling repair inside the existing Metal viewer, with no new graphics
backend, general engine framework, tuning switch or decoder fallback.

## Native settings maintenance

PPD-560/563 retire seven settings with no current production consumer:
RenderDeferred, RenderTerrainDetail, GPUBenchmarkResult, SkipBenchmark,
RenderClass1MemoryBandwidth, RenderCPUBasis and ALMesaRDNA35SettingsRestored.
Their definitions, feature masks, unused translated comments and exact old-profile
preservation retire together. The prior 24 retired names remain, for31 total.

The unreachable automatic OpenGL bandwidth classifier and its child startup mode
are removed. Native class 3/actual renderer/unknown bandwidth remain. All 66 live
ordinary graphics/scene bindings, quality/preset implementations, RLVa atmospheric
compatibility, media/mesh controls and the manual Metal GPU benchmark remain.
PPD-561 corrects two pacing help descriptions without changing defaults/policy.
PPD-565 makes human-readable About diagnostics report the Metal backend, preserving
native GPU identity and the structured OPENGL_VERSION=not present compatibility
field. Untranslated labels use the English base; no translations are invented.

PPD-562 repairs truthful texture-priority admission and exact worker lifecycle;
PPD-564 retains/corrects the first invalid-zero-capacity test setup. These records
remain distinct from original556and from previous owner-accepted maintenance.

## Verification boundary

The actual old scheduler fails two expected saturation cases; actual modified
scheduler/worker passes793 assertions / 18 cases under ASan/UBSan, including concurrent
native producers, exact-once callbacks, full queues, retry/wake/fairness, handle
reuse, pause and shutdown. Final read-only reviews of scheduler, callers and
preferences find no concrete blocker. Contained checks establish these contracts,
not owner workload saturation, scene/visual parity or viewer FPS improvement.

| Final check | Observed result |
| --- | --- |
| Full Release build / doctor |0/0 on the real M1Max host. |
| CPU/asset/settings/OpenHT |58/58scopes pass,0skip,19.92sec. Actual OpenHT engine/primary+auxiliary test:56assertions/3cases. |
| Native Metal API+shader validation |56/56scopes pass,0skip,270.34sec. |
| Pipeline-cache modes |Ordinary, global instrumentation and selective instrumentation pass separately. |
| App signatures / ISA |Source and staged deep/strict codesign0;346Mach-O files ARM64-only. |
| Strict source audit / XML / harness |0blocking/0scanerrors;15modified XML files parse;33required harness files pass. |
| Exact source/stage inventory |7,839regular files,2internal symlinks, no external links;797,452,771bytes. |
| Required decoder/package identity |Pinned OpenHTJ2K0.27.0 pixel decoder and licence/attribution pass; no exact _opj_decode symbol, header/encoding APIs retained. |

Staged app: `/Users/dekstroza/Alchemy-candidates/Alchemy Metal Scheduling 69450.app`.
Executable: `Contents/MacOS/Alchemy Metal Scheduling`.
Version26.4.0.69450; bundle version26.4.0d69450.
Executable SHA256: `df28cec46e833d8eba36c38e45385a4ed469964cc2ae84a753a78a86ccd47335`.
Metallib SHA256: `6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`;
identical to the actual native test bundle. OpenHT licence SHA256:
`42d0a6780d64697987c5dfc4b1b0b42633c1bc1dbc2256be297107b33d000a57`.
Exact candidate/launch inventories and command/environment receipts remain under
`artifacts/metal/ppd556-20261121/`. No signed bundle was modified after staging.

## Retained outstanding work

[Canonical priorities](../work/active/metal-port.md#outstanding-work-in-priority-order)
retain frame-triggered HTTP/asset service, permit-wait cancellation, General job
separation, substantial immutable scene preparation, safe reply service around
earlier GPU waits, upload CPU preparation/policies and pipeline compilation/archive
ownership. These require their own ownership design, scoped tests and appropriate
observations; this candidate does not silently declare them fixed.

PPD-319/002/311/015 investigations, broad feature parity, prolonged stability,
owned-runtime no-GL/foreign subprocess checks, independent reproducibility and
release gates remain open. Automated correctness checks do not close owner records
or authorize publication. The owner-requested launch is process-verified only; no login, saved-setting change,
visual-regression capture or live profiling was performed.

The owner asks for explicit disposition of initial audit items: LFS sleep is fixed
(PPD-538); asset paths have bounded replacements with fallback/HTTP/measurement
residuals (532/533); geometry required/deferrable decomposition is unfinished despite
the accepted API correction (530); earlier-wait/slack accounting is unfinished
despite the accepted live regular budget (537). These remain distinct in the active
ledger and initial assessment; no broad completion is inferred from narrow repairs.

## Owner acceptance and publication approval

On2026-11-22, after the requested exact69450 launch, the owner reports
"looks good..commit and push". This accepts the candidate's implemented bounded
scheduling/caller repairs, seven-setting/classifier/About cleanup and associated
records (PPD-556/560–565), and explicitly authorizes a normal push to
origin/codex/apple-silicon-metal. The two implementation commits are a27fb1a57e and
960bd3644a; qualification records were committed as9f4d1b75bb. The subsequent
acceptance-only commit preserves built indra7d8c095d360bd3f77ac64d1b69e23c21840e0011.
Exact local/remote identity and source/app verification will be retained after
the verified normal push in private owner-approval-push-20261122.json under the
task evidence directory.

The owner report closes only the implemented candidate scope. It does not establish
measured FPS improvement, saturation in owner workloads, complete feature parity
or completion of the retained geometry scheduling, earlier GPU-wait accounting,
asset-service and other production gates. No release/tag/default-branch merge,
private evidence upload or additional viewer recording is authorized by this push.
