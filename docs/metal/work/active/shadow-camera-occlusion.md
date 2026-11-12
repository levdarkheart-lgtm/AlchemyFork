# Native directional-shadow occlusion — 2026-11-12

Status: implemented locally; first enabled profile analyzed; owner acceptance pending.
Owner explicitly requested implementation of the missing native
shadow-camera query path, using the original viewer as the behavior reference.
Keep changes local until owner testing and explicit approval to push.

## Original mapping, recorded before validation

Reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:
`drawShadow` renders ordinary opaque static/rigged families, calls
`doOcclusion(shadow_cam)`, then renders pool geometry and masked/alpha families.
Queries have no color/depth writes, no culling, and LEQUAL depth testing
(GEQUAL under native reverse Z); caster depth stays strict LESS/GREATER.

Preserve separate `CAMERA_SUN_SHADOW0+j` state, fully-inside-map bounds checks,
the intentional zero-origin full-box query, occluded-group requery, existing
light-direction invalidation and frame-retirement semantics. Projector, cube
snapshot and union-cull exclusions remain as in the original. The setting's
existing false default is unchanged.

Native mapping: capture boxes with explicit shadow view/projection and native
batch offsets after the opaque prefix. Each shadow transaction uses a separate
non-main ALQueryPool capture and ticket list. The shadow renderer depth-tests
those boxes against its D32 attachment, masks R32 writes, and returns a retained
GPU visibility batch. End-of-transaction publication attaches matching results;
every failed/incomplete transaction discards its collected tickets. No synthetic
zero results and no per-shadow CPU/GPU wait.

## Implementation ownership

- Integrator: `pipeline.cpp/h`, frontend capture/settlement and canonical docs.
- Native backend agent: `almetalworld.cpp/h`, focused shadow GPU tests.
- Lifecycle test agent: `alquerypool_metal_lifecycle_test.cpp` only.
- Independent original/frontend review: read-only source comparison.

## Current progress and validation

Frontend and backend are implemented; focused tests and the full candidate build pass.
Queries split bulk and covered-coalescing runs at exact boundaries rather than
disabling existing batching. No-query paths retain their existing emission.
Focused native GPU tests pass 6,229 assertions in three cases with Metal API and
GPU validation enabled: depth equality/reverse Z, query placement, no color/depth
mutation, bulk/indirect/coverage boundaries, invalid admission and async lifetime.
Query/lifecycle suites pass 1,424 assertions in 11 cases across four files with
Metal API/GPU validation, including sequential shadow/main captures, aborts and
rejected late attachment. A private probe extracting production frontend bodies
passes 52 routing/settlement checks (not a matrix-arithmetic or visual test). Unproduced shadow queries now also
clear stale hidden state, rather than clearing pending state and leaving a
previous occlusion result with no future result to expire it.

Doctor exit 0 on M1 Max/64 GiB/macOS26.5.2/Xcode26.6. Isolated candidate configure
exit 0 in `build-Darwin-ninja-os-arm64-shadow-occlusion`; dependency installation
disabled, existing ARM64 vcpkg libraries reused. Full build uses
`AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion tools/metal/build.sh build`.
Original passed68841 app preserved as `Alchemy Metal Alpha 4.pre-busy-turn-cb840a5b9d.app`,
SHA256 `736d56f5fd940803a2e1c24cb236cfa7a48c9fe33b88f06aa8002eed02064875`.
Candidate26.4.0.68849 built successfully (exit0), ARM64 verification346 Mach-O
files/0 invalid and deep strict code-signature check pass. First build was
intentionally interrupted130 to exclude unrelated app-test targets; resumed
Release build used jobs8 and AL_BUILD_TESTS=OFF after focused suites passed.
Launched exact candidate PID31451, executable SHA256
`4d04fdd723f9cebb3433e8ac0ed59cf69c5b929f75f1803f79e59b16c1959b88`.
Session-only overrides: AlchemyRenderShadowOcclusion=true, RenderShadowCullMode=0,
UseOcclusion=true. Owner controls login/scene/settings with warm cache.
LEAP10:37:39UTC observes those values, RenderShadowDetail=2, far clip64,
resolution scale1 and projector cap2048. Native log confirms nonzero query
production for all four directional cascades0–3; no capture started before readiness.
Computer Use window access was not approved; no screenshot or visual-parity
claim. Actual executable/PID, settings and query production verified by logs/API.

Evidence: `artifacts/metal/shadow-occlusion-implementation-20261112/`.
Owner readied and completed the three-minute session. Overview timer stopped;
Instruments finalizer stalled. Raw data preserved and imported successfully as
181.281 seconds Deferred; original record exited1 after explicit finalizer
termination, recovery import/CPU/metric exports exited0. Exact recorded KCDATA
PID/UUID/load map plus matching dSYM recovers main-executable symbols in a
separate derived XML, with unchanged sample/weight/timestamp totals. Library
leaves remain unresolved. See [enabled profile analysis](../../research/shadow-occlusion-enabled-profile-2026-11-12.md).

Observed28.284 presentedFPS; combined shadow owner41.03% of mainCPU, compared
with prior35.57FPS/41.26%. Unmatched workload; no causal improvement/regression
claim. Decode activity perwallsecond2.69x prior and image working set grew. Follow-up:
first30s decode1.983cores, last30s0.272; capture began before texture work settled.
No visual parity or fresh-world cadence pass; owner controls acceptance.
Source strict audit0 blocking/0 errors; final harness and diff checks pass.

Next action: owner reviews local candidate; if further performance testing is
requested, use matched same-session gate changes with actual culling/world-frame
counts. Do not push without explicit owner approval. Binary is base65bc4e110f
plus exact recorded implementation diff, not a claimed clean-HEAD build.

Cross-capture follow-up: [recurring code hotspots](../../research/three-profile-hotspots-2026-11-12.md).
Source d537f10611; same-taxonomy offline parse validates all three inputs,
first limited to84main samples. Both long captures retain~41%shadow owner,
~12%maingeometry; sort union15.93–16.66%. Concrete palette scan/hash, glyph
Position regeneration/native allocation, GBuffer query-joining and D32 extent
issues mapped to source/original. Analysis only; no further renderer change.
Evidence artifacts/metal/three-profile-hotspots-20261112/; next implementation
selection remains owner-directed, all proposed fixes local.

2026-11-12 owner explicitly approves committing/pushing this directional-shadow
occlusion implementation and its GPU/query tests, together with the named
PPD-001 repair and restored4normal/6maximum decoder policy. Publish on
codex/apple-silicon-metal. This supersedes the earlier no-push boundary; visual
parity, controlled speedup and the unrelated full-product gates remain open.
