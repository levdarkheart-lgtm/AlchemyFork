# Current engineering state — failed-travel correctness repairs

**Observed:** 2026-11-10, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `0cbb48f104ad2f7d6e358266c1abf32d4d487a63` — independent native UI publication,
terrain placeholders, impostor receipt/retry, affine skin validation and names.
Owner explicitly requested code fixes against original source and saved logs.
All earlier HUD, upload, environment, avatar and performance repairs retained.
Earlier20FPS campaign stays paused. Translator stopped; Xcode replay stopped.
No launch/login/travel, camera, account/cache/settings changes during this repair.
The old68633 process was confirmed exited before GPU checks.

## Latest owner-run — continuous corruption persists

[68638 live findings](../work/active/travel-live-68638.md): owner explicitly
requested launch/log observation, then screen inspection. Exact canonical app/
hash verified, PID27603 launched viaCUA; owner logged in. Two actual screenshots
show different giant triangle arrangements; owner says junk keeps dancing every
frame. This live geometry result is FAILED. Fresh native UI/publication receipts
continue, but no nonfinite skin operand/fallback warning appears while corruption
is visible. Earlier empty/terrain/impostor source fixes do not prove its cause.
Initial12 cloud-loading warnings later stop; no corresponding Metal error found.
Logs/receipts are private and not frame counts. Root made no settings/camera/cache
or source/build changes during trial. Viewer remains running; FPS goal paused.

## Concrete repairs and original contracts

Exact original: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
[Implementation and evidence](../work/active/travel-correctness-repair.md).

- Native interface no longer requires a complete CURRENT world to render fresh
  ordered UI/HUD. Retain committed PRE-UI color plus matching depth; never reuse
  combined old UI. Device/surface/resize/reverse-Z mismatch gets a current-size
  cleared pair. Snapshots/debug pixels still demand the exact current world.
- Typed target leases pin pools; native texture aliases survive borrowed-wrapper
  destruction. Remove redundant early main G-buffer clear (actual native pass
  clears all attachments); exclude retained depth from subsequent borrowed writes.
- Reduced/supersampled world size is resampled to full UI size as a coherent
  pair: BilinearMirror encoded color, PointClamp raw depth, no Z conversion or
  offscreen row flip. Cached pipeline/pool, completion-pinned source/output;
  one extra submission only when scaling, none for matching-size retention.
- Classic/PBR fetched terrain uses original immediate revival, Default then Null
  placeholders, correct raw/sRGB views and samplers. Strong selected owners reach
  command completion. Generated height/composition remain exact, with bounded
  role-specific missing-input diagnostics.
- Complete empty main views now continue through sky/lighting, as original;
  explicit completeness/resource/query checks remain. Complete empty impostor bakes produce transparent targets, as original clears
  even when no subject geometry is emitted. Failed native publication preserves
  dirty retry and old dimensions/cached values/timestamp; publication requires
  a validated quartet. First failing producer line survives phase resets.
- Object skin validation checks the twelve consumed XYZ coefficients, preserving
  a valid affine transform whose unused W lanes are nonfinite. Actual bad XYZ
  retains bounded inverse/world/product diagnostics; no arbitrary finite caps or
  speculative whole-palette policy. Live bad XYZ cause remains unknown.
- Display-name cache misses stay pending after distance/status lines are added;
  a stationary avatar can then gain its name without unrelated state changes.
  This retry bug is also present in pinned original; formats/RLVa policy unchanged.

## Executed validation and build status

Canonical Release **26.4.0.68638** built from checkpoint0cbb48f104, verified and
signed; subsequently launched at the owner’s explicit request. Doctor/configure/build/final configure+build/verify_app/
deep strict codesign all exit0. verify_app checked349 Mach-O files, zero invalid
non-arm64 slices. Source tree matches committed code, with docs-only handover edits.
Canonical app: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256: `6942629c44614828ed327e563536376191c992c0d2af9ead44f2c819fc8c0eb0`.
Metallib SHA256: `bca357a8868e3c4a82c9e8eb21cc3b27fe783806e83651742e2919b4671c3f0d`.
Exact source/commands/exits/hashes: private `build-final.json` in the evidence directory.
Commands/logs: ignored `artifacts/metal/private-travel-correctness-repair-20261110-01/`.
Root performed builds/GPU runs; independent source review approved all owned
components and found the earlier main-depth clear, which root corrected.
This is source/review evidence, not an independent fresh-checkout build.

Actual Release tests on this M1 Max; API+shader validation enabled for GPU cases:

| Check | Assertions/cases | Result |
| --- | --- | --- |
| Affine palette/real affineTransform |201/3|pass|
| Production fetched resolver + classic/PBR terrain GPU |272/1|pass|
| Empty impostor bake and quartet replay,8 format/Z layouts |17070/1|pass|
| Retained UI, pool/borrowed lifetime, resize, scale and HUD depth |29130/3|pass|
| Existing native UI/target/pool/impostor regression cases |17964/4|pass|
| Existing four-MRT world renderer, including actual clear-only world |31498/1|pass|

Initial retained-UI fixture selected an alpha-textured shader without supplying
an image; corrected to uniform_color and rerun passed. No renderer fault was
inferred from that fixture failure. The final native scaling shader is packaged in the verified app; the initial
1101-unit viewer rebuild and final four-unit incremental build both succeeded.

These tests do not instantiate the entire viewer manager, actual network fetch,
CAMetalLayer resize/input loop, or full live teleport/name/rigging paths. No new
live visual/FPS improvement claim. Existing final grain/dither still evaluates
at world size before scaling: exact noise/effect-coordinate parity for non1
render scales remains open. No scene quality cuts or avatar caps introduced.

## Preserved evidence and open symptoms

[Failed68633 live travel](../work/active/travel-live-68633.md) and
[preceding checkpoint](archive/CURRENT-failed-travel-68633-2026-11-10.md).
Owner saw UI/movement/resize stalls, distance-only names and stretched geometry.
Saved logs/screenshots/10sCPUtrace remain private. Trace captured345 runloops,
max82.18ms gap, not a permanent deadlock; that does not prove visible publication
or keyboard delivery at every reported failure. Earlier bounded tests did not
supersede this failed live result.

[Prior avatar ownership/profiling repair](../work/active/avatar-crowd-parity.md)
remains included. Whole-manager native GPU timing/ART dispatch and crowd visuals
still need live checks; no native profile-completion receipt was observed in the
failed trial, and request activation was unknown. Original particles-disabled
loading debug sphere still lacks its native producer.

[Cold-start/environment checkpoint](archive/CURRENT-cold-start-environment-repairs-2026-11-10.md)
retains earlier EEP gamma/no-post/default-probe continuity fixes. Full sky/interior/
shadow parity, low-DD terrain/water and all random attachment corruption remain
unverified beyond the specific confirmed source repairs above.
Original8978files/1703059096bytes cache remains preserved under ignored
`artifacts/metal/private-empty-cache-start-20261110-01/original-cache`.
Do not restore over active cache; account/settings untouched. No private uploads.

## Exact next actions

68638 PID27603 is running for the owner. Preserve the current failing scene and
identify the exact native draw and vertex/index/weight/palette/transform inputs
producing moving triangles against original source. Quiet finite/error logs do
not identify it. Read-only source audit remains in progress; no cause established.
Other input/resize/HUD/environment/terrain/water/shadow acceptance remains open. Do not treat
backend tests as full visual acceptance or resume the paused FPS campaign.

## Complete-product advancement assessment

| Milestone | State | Evidence /remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native loading/UI/avatar correctness mechanisms repaired and GPU-tested; live parity remains. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new CUA phase. |
| M-014 Ultra20FPS campaign | deferred | Owner-paused; no new FPS or speedup claim. |

[Paused campaign](archive/CURRENT-20fps-owner-stop-2026-11-10.md): last controlled
current-source9.745FPS;20FPS remains unmet, no hardware ceiling proved.
Full parity, prolonged stability, multi-scene performance, strict no-GL and
independent build/review evidence remain open. Product is not complete.
