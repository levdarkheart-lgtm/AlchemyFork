# Remaining upstream correctness batch — 2026-11-13

Local work after owner instruction “now the remaining if need it”. Upstream
snapshot remains408fd6c014; baseb04d9bf6ef already contains the First batch.
The owner has not accepted either candidate or authorized publishing this scope.
No viewer/grid launch, scene parity, audible test or performance gain is inferred.

## Implemented source groups

| Local commit | Applicable correction |
| --- | --- |
| 863c6bf1df | PBR bake representation/body hiding, all native material consumers, gained mesh-face TE/materials and unknown-region overrides. |
| 9d8a87d637 | Complete URI/LLSD/XML/date/string corrections, finite quaternions, default sky angle ordering and object-cache disk/extras pairing. |
| 3cac0c4963 | Octree capacity/settings admission and placed-probe frustum/reentry/default-preview policy, retaining native completion queries. |
| 2e77df8376/b17a8c5aad/4e230f49ad | Native4096 readback/nonfatal oversize refusal, async alpha-answer owner notifications, expiring texture boosts and visibility/media/fetch policy. |
| f297b3b665/2a5af4fdac | Retained optimizer incidence-cost budget and unconditional valid-triangle admission, without the replacement geometry stack. |
| 95e0f568dd | Navigation, XML-RPC/SLURL, social/inventory/notecard, teleport/maturity, preferences/input and FAudio fixes. |
| d7b1bbbf1c | Script Studio/converter/hover/explorer/notices, coroutine-safe external editor and immutable built-in name snapshots. |
| 7005584d48/7de38c3a00 | Object admission/local-ID/cache-UUID/kill/orphan/crossing/handshake corrections and actual target test registration. |
| 03a3c14095/a32bc96ad3 | Native HDR finite boundaries and a shared live-viewer additive-pass bound, replacing upstream's insufficient native63-draw assumption. |

Final additions: fdc0c4aad2 font-aware noticebar regression; d95d1e911c independent
whole-asset HTTP policy; b6c20181a2 retained allocation safety;209a4a3bbd sculpt
aspect/copy/refresh/low-area correction;cea5e68c23 remaining legacy zero-normal
path;348c503a7b native tangent failure guards;af5c263988 dedicated sculpt target.
These are canonical integration SHAs; isolated reports retain their original
worktree SHAs. Current final source af5c263988 is building; no code changes are
planned while this immutable-source build and its checks execute.

The integration preserves accepted decoder4/6, CoreAudio music, WebRTC/FAudio,
First security/lifetime corrections, and current native rendering/publication.
PBR media precedence and resolved texture lifetime were independently reviewed.
Default sun/moon direction and audio attenuation are intentional behavioral
corrections; owner visual/audible confirmation remains necessary.

## Focused evidence

- [Common/data/cache](upstream-remaining-common-validation-2026-11-13.md):
  520892assertions/358cases across four real CPU suites; sixteen concurrent URI
  first calls; old cache5failures vs corrected17cases; Release+LTO quaternion
  old10failures vs corrected36checks. Unsupported finite-only compiler mode is
  explicitly excluded from shipping evidence.
- [Texture](upstream-remaining-texture-validation-2026-11-13.md): fifteen actual
  Release TUs and1170assertions/10cases, physical native readback/alpha-answer
  publication plus boost levels. Frozen owner-notification control fails10checks;
  original fatal2048guard rejects4096. No async readback performance claim.
- [Native HDR](upstream-remaining-hdr-validation-2026-11-13.md):1230new assertions/
  2cases and243759existing assertions/19cases on M1Max with Metal API validation;
  full production MSL controls, real half-float additive and RG11B10 stores.
  Extreme-float reciprocal flush required an actual native normalization fix.
  High light-count tests prove finite storage rather than improved image quality.
- [Material/texture peer review](upstream-remaining-material-review-2026-11-13.md):
  all native consumers mapped; PPD-045 registration corrected; PPD-047 legacy
  state retained without inventing a native residency leak.
- Retained mesh budget: actual cacheOptimize(false), shuffled10000-triangle fan
  with incidence cost100040000 >2^26; old source fails preserved-order assertion,
  fixed source passes3checks. Existing public compressed decoder201checks/4cases
  and empty/bad-index/nontriangle admission3checks pass with ASan/UBSan. Logs/
  exact argv at artifacts/metal/upstream-remaining-20261113/mesh-budget/.
  The bounded test does not establish an owner freeze cause or scene FPS gain.

Raw isolated logs/receipts and final MSL/control sources were copied from scratch
into ignored artifacts/metal/upstream-remaining-20261113/isolated-evidence/.
Further final source additions and combined candidate results are recorded below
when observed; the listed focused runs are not an all-product release gate.

## Unexpected test fixture failures

PPD-046 records both owner-visible almetal_tests crashes: nil NSError diagnostic
string and an unspecialized Metal test fragment. Both fixtures were corrected;
subsequent HDR and regression runs passed under LLDB containment with actual
child exit0. Neither crash involved a launched viewer. The fixture failures were
unexpected, not successful negative controls. Raw private reports/screenshots
remain outside Git; exact stack summaries and failed/interrupted run history
are retained in the HDR evidence.

PPD-049 records mismatched scratch noticebar/datapacker objects; PPD-050 records
a hardcoded font-dependent noticebar test height. Preserve these setup failures
separately from production findings. Canonical source target includes matching
implementation units. Further test executions use debugger containment where
appropriate, without changing global CrashReporter preferences.

## Disposition of remaining upstream families

- New ALSky/starfield and its later depth/water/sky-metering corrections depend
  on replacing today's sky composition. Those followups are not independent
  repairs for the retained native raster sky; do not import GL passes wholesale.
- The full geometry/tangent/uploader/SIMD/custom-meshoptimizer stack changes
  algorithms, dependency ownership and uploader output. Its introduced-algorithm
  followups must travel with that project. Current safety/cost/sculpt/face fixes
  are selected separately; inherited allocation hazards remain PPD-048 until
  their bounded adaptation is validated. No full dc0f4b478c equivalence claimed.
- Region retirement/dead queues/emergency purge, probe reach/eclipse and region
  bounds rejection, palette reuse and memory accounting are broader native
  lifetime/performance projects. Existing native exact descriptor accounting and
  completion ownership must be reconciled before adoption; no measured benefit
  authorizes replacement automatically.
- The new platform precise waiter explicitly lacked upstream macOS build proof.
  Worker/pacing/writer-preference changes need a native measured workload. Real
  immutable snapshot and coroutine lock-lifetime defects are fixed directly;
  broad lock-type churn and Windows process-job fixes are unnecessary here.
- Native probe mip allocation already has the upstream final2x2level. GL query,
  PBO/fence, Nvidia, fallback backend and UBO mechanics are inapplicable. Preserve
  native capability/lifetime implementations instead of reintroducing them.
- Units/oracle/test-packaging, extended-key/emoticon features and bulk dependency
  baseline changes are separate optional work. No CVE or performance claim is
  inferred from an upstream baseline update.

This disposition does not assert that every later upstream feature has been
ported or that full feature parity/performance release gates have passed.

## Final independent additions and review

- [General viewer/asset policy](upstream-remaining-viewer-validation-2026-11-13.md):
  7950assertions/321cases plus separate new-solver2205/165; Debug/O0 boost1088/6
  and actual independent HTTP policy37/2 pass. Policy lookup uses the created
  policy identifier, avoiding an enum-index assumption; no throughput claim.
- [Retained mesh allocation](upstream-remaining-mesh-allocation-validation-2026-11-13.md):
  293/8 normal andASan/UBSan, private real aligned/null/new/fallback217/4 pass;
  exact old source fails both new allocation cases. The ordinary weighted and
  unweighted grid/tangent/order fingerprint stays004fd052b5f3a793.
- [Legacy zero normals](upstream-remaining-legacy-normals-validation-2026-11-13.md):
  actual direct and65535-vertex fallback old controls fail15/17checks in2cases;
  repaired311/10 normal andASan/UBSan pass, ordinary-output fingerprint unchanged.
- [Sculpt](upstream-remaining-sculpt-validation-2026-11-13.md): actual copy/volume
  73checks/5cases pass; both live viewer units compile. Existing volume42cases
  return0 with23passing and19expected-failure cases retained (not42ordinary
  passes). Same-level refresh covers both save/admission and actual consumer.
- Independent read-only peers found no actionable allocation ownership/alignment/
  weight/remap issue or sculpt native size/discard/selection/publication issue.
  Root integrates all four LLFace and one material-preview tangent failure
  checks before any later raw tangent reads. No physical OOM incident is claimed.

Frozen2a5af4fdac canonical doctor/configure/full2335-step Release build passed
(1078.159seconds). Later348c503a7b intermediate rebuild was deliberately stopped
130 to complete the omitted sculpt test registration; no compiler failure.
Receipts retained in checkpoint-2a5af4fdac/ and interrupted-348c503a7b/. Final
app proof must come from the complete af5c263988 build and matching checks.

## Final combined candidate — observed

Source **`af5c263988ad554acd6c1b142e502b28c21ad452`**, canonical codex/apple-silicon-metal,
Apple M1Max/64GiB/macOS26.5.2/Xcode26.6. Release/LTOOFF,8build jobs.
Doctor/configure/full build all exit0; final rebuild972steps617.268seconds.
The preceding2a5 full2335-step build also passed. All subsequent source work
was frozen before this final build and all final checks; docs change separately.

- **197registered CPU/core/network/math/script test entries pass**,53.02seconds.
- **22fresh LLDB-contained UI/cache/material/boost processes pass**,21862assertions/205cases. Every actual child exit0/state10; debugger exit alone is not proof.
- Native texture82/4, new HDR1230/2 and retained HDR/scene regression243759/19
  pass on M1Max with Metal API validation:245071assertions/25cases total.
- `tools/metal/verify_app.sh` and deep/strict codesign verification exit0:
  **346ARM64 Mach-O files,0invalid slices**. Exact embedded source matches.
- Owned main/CEF/VLC host library/import checks pass; compiled native CoreAudio
  music23markers,WebRTC477,OpenHTJ2K61,OpenJPEGencoder1 remain present. This is
  compiled feature/import evidence, not a live voice/music test or whole-bundle
  foreign/runtime no-context claim.
- Strict production source3536files/0blockers/0scan errors.116tools tests pass;
  tool source is unchanged since the passing2a5 tool run. Harness/diff checks
  rerun after final documentation.

Exact candidate:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Upstream Fixes.app`

Version **26.4.0.68936**; executable SHA256
`0c45c86cb9c05285f8756fc819fe4921602692d10e9530348f3c6b6255bfc0b2`.

13individual skipped cases are existing conditions in unchanged files:5HTTP
(pthread cancellation/header assertions),5Release debug-log tests,1flaky host
lookup,1main-thread-task hang test and1architecture-dependent matrix comparison.
No skip was added or test expectation weakened. Existing llvolume_test retains
19expected-failure contracts:34cases/15pass/19expected-fail and3963assertions/
3940pass/23expected-fail. PPD-053 explicitly tracks this larger geometry work;
green CTest must not be described as complete geometry correctness.

Final exact argv/status/child results: ignored
artifacts/metal/upstream-remaining-20261113/{build-state,validation-state,
app-identity,final-summary}.json and per-step logs/receipts. Decoder4/6 and
accepted native music/voice remain source-preserved; no viewer/account/grid
launch, visual/audible acceptance, performance comparison, push or release.

Selected correctness backports are ready for owner testing. The coherent full
geometry/sky/lifetime-performance projects remain separate, explicitly mapped
above. Open product parity, runtime and measured performance gates remain open.
