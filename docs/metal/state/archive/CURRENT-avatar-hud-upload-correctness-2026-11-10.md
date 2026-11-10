# Current engineering state — HUD and native upload correctness

**Observed:** 2026-11-10, M1 Max / 64 GiB / macOS 26.5.2 / Xcode 26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `d18641c3557996807de03d99a105419161e95dfb`.
**Current viewer:** 68626, PID 43249, native Release build, API validation enabled.
The owner authorized this avatar/HUD investigation separately. The earlier 20 FPS
campaign remains paused; no performance experiments or quality changes were made.

## Implemented from the exact original

Reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`. Existing prior repairs
remain included; both previous source611e1f3a0f and renderer88528d27a5 are ancestors.
The owner explicitly requested retaining all previous fixes and the latest discovery.

- `547daf9c31`: world/UI capture now uses native authority. The old mirror-mode
  caller left authority false, so fullbright/mask/alpha/bump HUD pools skipped
  fills while edit outlines still rendered. World color/depth ownership and the
  original HUD-before-interface order are preserved.
- `082075249b`: restores the original `LLDrawPoolAlpha::forwardRender` HUD blend,
  depth and RGBA bracket plus per-draw custom RGB factors. Direct native batches
  had inherited disabled blending from opaque pools. Also restores explicit
  AnisoWrap / particle AnisoClamp and texture activity/bind statistics. Opaque
  and MASK PBR HUDs now have direct native dispatch and the original distinct
  fragment contract: biased cutoff, emissive ordering, sRGB shading, output alpha0.
- `08bc61af05`: failed native vertex/index uploads remain on the mapped retry
  queue. Previously dirty flags survived but the queue entry was discarded.
  Destruction and shutdown disarm pending raw pointers. This is reproduced and
  repaired in a real-device test; no production upload failure ties it to the shoe.
- `d18641c355`: pending HUD textures use the original default/Null placeholder
  behavior instead of failing the whole world/UI publication. The live68625 log
  showed `HUD attachment encoder: Metal HUD batch is missing a diffuse texture`.
  Native texture pointers and activity are resolved without a legacy slot bind.

No original fallback renderer, camera/shadow changes, outfit changes or asset
network rewrite was introduced. No private captures were uploaded.

## Build and executed checks

`tools/metal/doctor.sh`, `tools/metal/build.sh configure`,
`tools/metal/build.sh build`, `tools/metal/verify_app.sh <canonical app>`, and
`codesign --verify --deep --strict --verbose=2 <canonical app>` all passed (0).
The final placeholder build reran configure/build/verify/signature; doctor is
recorded in the preceding upload-retry build. Native ARM64 and bundled/produced
metallib identity verified. Code68626 is source `d18641c3557996807de03d99a105419161e95dfb`; later docs commits do not
change that binary identity.

Focused Release tests, with `MTL_DEBUG_LAYER=1` and `MTL_SHADER_VALIDATION=1`:

- UI/HUD SoA test: 763 assertions / 1 case, exit0, including opaque/MASK PBR
  raw RGBA/depth, nonwhite colors, biased coverage, U16/U32 and forward/reverse Z.
- Selection contour test: 4732 assertions / 1 case, exit0.
- Native vertex-buffer tests: 93 assertions / 2 cases, exit0. Before the retry
  repair, the new fault-injection test failed 4 byte-equality assertions (43/47
  passed). After repair, restoring the device and flushing alone recovers exact
  VB/IB bytes, preserves in-flight generations and safely destroys a pending buffer.
- First PBR test attempt failed only negative-test validation precedence; fixture
  corrected in `916ad7e79d`, then all checks above passed.

These are bounded checks, not full viewer parity or a speedup claim. Native UI
fixtures bypass the pipeline caller; the actual account inspection and owner
feedback supply the routing/visual evidence. Owner said previous fixes were OK.
The final placeholder delta is built and launched. At19:24UTC its startup check
recorded26 success/publication log entries, zero missing-HUD-diffuse messages,
zero vertex/index upload failures and zero Metal API assertion errors. Two
informational startup log entries (through ONCE10) reported a generic missing
buffer/texture mirror before native publication resumed; these are not two-frame
counts or evidence that all startup waits are removed. Check startup-check.json.

Private ignored evidence directories:
`artifacts/metal/private-avatar-account-repro-20261110-01/`,
`private-hud-authority-fix-20261110-01/`, `private-hud-blend-fix-20261110-02/`,
`private-native-upload-retry-20261110-03/`, `private-hud-placeholder-fix-20261110-04/`.
The first directory preserves the four owner screenshots and their hashes.

## Runtime and remaining issue

Canonical app: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256: `1050ef5ab4907c1fb15874a4bb216c81b057bc82e03030dfe446dc865570675a`.
Metallib SHA256: `a155144459e5899d838c35834d31ecd0b4c5ae67e511e26e81523e464cd994f8`.
Old PIDs72137,90316,26572 exited before their replacements; currentPID43249
was observed as68626 in the UI. No forced account/destination/profile/environment/
camera/outfit was passed. User drove login and scene/outfit changes; Codex did not
move the avatar. All CUA phases completed. Translator stays fully stopped;
Xcode replay stays stopped. Root owns source/docs/runtime; agents idle/private only.

Random missing shoe and outfit-change screen spikes remain unexplained. Repeated
Wear restored the shoe before diagnosis. Current logs before this final launch had
no vertex/index upload, palette, Metal validation or asset-fetch error establishing
that cause. The buffer retry repair must not be called the proven shoe fix.
Original/native source comparison found matching attachment membership, legacy/PBR
rigged routes, selector encoding, palette bytes, geometry rebuild/flush and query
invalidation. No CPU double-skin or stale-neighbor ticket defect was demonstrated.
A new recurrence needs tracing the actual attachment through CPU geometry, render
map, palette and native batch; do not delete original admission rules or speculate.

## Exact next actions

Final68626 startup inspection completed; code and evidence are recorded. Leave
the viewer running for the owner and preserve current logs.
If the attachment failure recurs, trace that actual geometry/map/palette/batch.
The performance campaign remains paused.

## Paused performance campaign

Prior campaign closure/evidence: [owner stop](archive/CURRENT-20fps-owner-stop-2026-11-10.md).
Last controlled current-source result was9.745FPS;20FPS remains unmet. No hardware
ceiling was proved. Keep prior binding caches, bounded lights, rawR32 shadows,
resource borrowing, memory accounting, arenas, probe and anisotropy repairs.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native implementation retained; broad original parity remains open. |
| M-012 Native UI automation | complete | Prior exact-build login flow; current HUD inspection executed. |
| M-014 Ultra20FPS campaign | deferred | Owner paused it; no new benchmark or performance claim. |

Full parity, prolonged stability, multi-scene performance, strict no-GL and
independent build/review evidence remain open. Product is not complete.
