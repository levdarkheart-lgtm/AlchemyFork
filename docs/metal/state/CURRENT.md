# Current engineering state — PPD-567 measured follow-through

Build observed2026-11-21; owner acceptance2026-11-22.
Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
Branch: codex/apple-silicon-metal, canonical checkout.
**Renderer code checkpoint:** `960bd3644a` (accepted baseline).
Ancestor indra tree: `7d8c095d360bd3f77ac64d1b69e23c21840e0011`;
new local PPD567/common-observer source is explicitly hashed in both arm manifests.
Scheduling **26.4.0.69450 is owner accepted; commit/push approved**.
PPD-556/560–565 are verified, owner verification passed and push approval granted.
Accepted Maintenance 69445 remains preserved and previously pushed at629984ee37;
the new explicit2026-11-22 approval now covers Scheduling69450 as well.

## Local implementation and completed comparisons —2026-11-22

Owner now authorizes PPD567 implementation, repeated controlled Club comparisons,
Home regression checks, bounded Home census, exclusive Club CPU budget and worker/
queue table. [Current execution ledger](../work/active/ppd567-performance-20261122.md)
owns the complete five-part goal. Owner now says "Commit and push all the changes."
Current-scope commit/push is approved; owner visual/performance verification is pending.

Local invocation-owned lazy forward-pack slice and independent pending-view tests
are integrated. Actual baseline fails eight duplicate-pack byte checks; fixed test
passes. Full transparency1825343 assertions/12cases and related GPU checks581147/
40cases pass with API/Shader Validation. A read-only getCamera query is common to
both new A/B builds; no frame hook, shader or quality change. Accepted69450 untouched.
Both full builds/app checks pass. Tool suite passes185tests with1optional skip;
initial dependency failures and the isolated PyYAML repair remain retained.

Home census actually executes1024 records/966 covered receivers on M1 Max: zero
ambient bit mismatches, mean7.74SH taps including default, substantial containment
rejection. Explicit decision is STOP GPU kernel variants; no new redundant kernel
operation selected, PPD353 regressions retained. Exclusive Club budget and worker
progress table complete; loading decode backlog clears before late observations.
PPD572–574 retain private reducer/generator/format failures and corrected evidence.

Both A/B full builds, signatures, app verification and matching dSYMs are complete;
exact identities below are unchanged. Final10selected runs explicitly qualify true,
reasons[]: Club A01b,B01,B02,A02,A04,B04 and4Home runs. A04/B04 have815/803periods,
zero FRAME gaps. The initial pilot and whole A03/B03 pair remain retained/excluded;
B03's6258gap and premature all10 claim/review error stay in PPD581.
Normal Publication69452 is logged in at verified Club in native full screen.
Leave it there for owner busy-scene testing; Home regression checks are complete.
**Owner visual/performance verification is pending; no isolated FPS win is claimed.**

- Control: `~/Alchemy-candidates/Alchemy Metal Probe Control 26.4.0d69452.app`;
  executable SHA256 `381c23ff409992515792c447960fd2dedb635fa22a85839786d663882b680fe2`,
  UUID `67A89320-97EB-3D74-B6E3-EE2312C53555`.
- Candidate: `~/Alchemy-candidates/Alchemy Metal Probe Publication 26.4.0d69452.app`;
  executable SHA256 `1ded8f254087832c2d28d71084854eb1ff07474e545deaf211c8d890fffe099e`,
  UUID `6A967A9B-4925-321E-865A-FF3A1446BED8`.

Exact source/working-patch hashes and symbols are in private
`artifacts/metal/ppd567-20261122/identities/{control,candidate}.json` and `symbols/`.
These new binaries share the observer; they are not the accepted69450 executable.
The [final report](../research/ppd567-performance-20261122.md) retains full results.
Final gate0: private analysis-comparison/final-comparison-02/comparison.json;
actual B03 negative rejection proof remains. No qualification rule was weakened.
Club broad opaque-post hash CPU2.193–2.940→0.0186–0.0237ms/period (~99%lower),
including other constants/palettes; this is not exact probe-pack self-time.
Mean-period pair changes are−6.35%,+1.30%,+1.62%; third p95+11.76%is retained.
That pair has~30%more logical vertex-byte counter span and point lights88.38→90.88;
unequal activity is not causal normalization or proof against regression. Home mean
changes stay within0.5%. No isolated sustained FPS benefit is established.

Final preservation audit02 actual root0 covers20selected pre/post snapshots:
66controls and per-scene camera matrices/pose match exactly; saved18present/
48absent controls have0diffs, all3app hashes unchanged. FullScreen is absent in
saved XML and0live; actual native full screen was independently verified.
Whole XML byte equality is not claimed. PPD575–581 retain failures and corrected
checks, B01 offline recovery and lossy command-footer limits; no false owner closure.

## Completed overnight investigation —2026-11-22

The [readable report](../research/performance-investigation-20261122.md) and
[ledger](../work/active/performance-investigation-20261122.md) retain the completed
evidence-only investigation under private artifacts/metal/performance-investigation-20261122.
Accepted69450 stayed unchanged. Its late native periods were Home60.587msmean/
73.151p95 and Club51.869/58.044; these are not scanout. Home was GPU-dependent;
Club main CPU was98.7%of one core. These older baselines are separate from new A/B.
Source-enabled Home probe work was66.40%of BASE and57.51%of simple-shadow roots;
pack55 differed from normal late39, not a timing A/B. Dominant shaders already use
masks. Old coefficient/scalar/tile-SH regressions remain; the census STOP supersedes
the earlier proposal to investigate another GPU representation.

PPD568's157msHome/212msClub captured blocks are Apple driver allocation locks,
not ResourcePlan mutexes; no-capture impact unknown. One58msHomeblock remains
unclassified. PPD569 slot3 warning has a32-slot guarded active-prefix contract;
exact warning draw/count remains unqualified. Club detailed-MST789msobserverhole
is excluded; native frames/commits continue. GPU frequency/bandwidth/spilling and
scanout latency are not established. PPD319 and product release gates stay open.

Private helper defects566/570/571 have actual corrected live/native staging
evidence, originals retained and owner acceptance pending. Prior doctor/Release/app
checks pass. The66checked controls and8273-entry accepted-app inventory were unchanged;
canonical metadata remainsOFF. Investigation collection/exports are complete; raw
evidence and authorized configuration backup stay private, without report credentials.

## Current implementation and evidence

[Candidate report](../research/scheduling-candidate-69450-20261121.md) records exact
source/app hashes, executed validation and retained limitations.
[Current execution ledger](../work/active/ppd556-bounded-scheduling-20261121.md)
retains commands, original failures, independent reviews and residual actions.
[Previous accepted maintenance](../research/maintenance-candidate-69445-20261121.md)
retains PPD527–555/557–559 owner acceptance. All records survive in the
[defect register](../POST_PORT_DEFECTS.md).

- Reusable scheduling, bounded callback admission, native wake/fairness and
  busy/closed caller handling are integrated. ASan/UBSan793assertions/18cases,
  integrated CPU/asset/settings/OpenHT58/58 and native56/56 pass; failures remain.
- Invalid pool widths, texture handoffs/cancellation, main-setting liveness and
  accepted ownership/resource fixes are present. Do not diagnose them as absent.
- Accepted checkpoint passes Release build, deep/strict signing,346ARM64 Mach-O,
  strict GL source audit and required OpenHT decoder/licence checks. OpenHTJ2K is
  the sole pixel decoder; separate OpenJPEG metadata/encoding support remains.
- Seven orphan settings and the automatic GL GPU classifier were retired; all66
  ordinary graphics controls, quality presets, manual benchmark and viewer features
  remain. Detailed source/stage inventories, warning and failed-fixture history
  are preserved in the linked accepted report/ledger.

## Ownership and recovery

Root owns active PPD567 integration/runtime. Accepted scheduling evidence remains
under artifacts/metal/ppd556-20261121; unrelated worktrees stay untouched. Source
and A/B identities above supersede historical process/build status below.

Owner-tested candidate: /Users/dekstroza/Alchemy-candidates/Alchemy Metal Scheduling 69450.app
(executable SHA256 df28cec46e833d8eba36c38e45385a4ed469964cc2ae84a753a78a86ccd47335).
Prior launch history and full69445/69417 identities remain in the accepted reports;
those apps are untouched. Active comparison authorization is in its ledger.

Normal candidate is ready at Club for owner rendering/stability checks. Preserve
graphics preferences and keep exact locations/authentication data private.

## Complete-product advancement assessment

| ID | State | Evidence / remaining boundary |
| --- | --- | --- |
| M-008 | doing | Bounded scheduler/caller repair owner accepted; broad feature acceptance remains. |
| M-011 | doing | Final static app/source gates pass; runtime/foreign subprocess qualification remains. |
| M-012 | complete | Existing harness/build tools retained, maintained and exercised. |
| M-013 | doing | Alchemy/RLVa/media features retained; broader runtime qualification remains. |

No product/release completion or FPS improvement is claimed. PPD319 completion/
performance, broad parity, loading, prolonged stability and foreign subprocess
qualification remain open. Reproducible distinct open-source release, strict no-GL
and independent build/review evidence remain required. Component checks and process
launch do not establish whole-viewer runtime/performance or visual acceptance.

## Exact next actions

Owner reviews Publication69452 for visual/loading/behavior and retained mixed cadence/
adverse p95. Five goal parts/audit02/Home checks are complete; Club handoff is ready.
Owner explicitly authorizes committing/pushing all current-scope source/docs changes;
publication is not visual acceptance, an FPS claim or permission to upload private evidence.
Prior publication reached d219d538ab. Existing asset/geometry/slack/General/streaming/
compiler/product work remains in the canonical active priorities.
