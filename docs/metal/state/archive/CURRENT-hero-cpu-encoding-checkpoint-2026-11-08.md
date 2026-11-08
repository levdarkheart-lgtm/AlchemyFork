# Hero-view CPU encoding repair — 2026-11-08

Renderer implementation is committed as `18aedd2ecb9ca388cd0e04dffe0c4f32ece9e323`
from clean `988ec690af` on `codex/apple-silicon-metal`, after the owner
authorized the six code-grounded repairs. No new FPS or visual claim follows
from this implementation.

## Observed starting point

Source-exact Release `68543` is the measured `68532` renderer plus only its
encoder-local texture/sampler cache. Owner-attended fixed-camera Ultra evidence
is local at `/private/tmp/alchemy-metal-68543-root-cause-20261108-1.U9wKEH`.
The 30.634-second Metal trace contains 178 presents (5.810 presents/s), 31.60
submissions/present, 57.3% GPU-active union, 4.452–4.592 GiB allocation and
Nominal thermals. Later settled log samples have median 7.1 FPS.

Independent CPU reference resolution attributes 14,590 of 28,785 main-thread
running milliseconds to `LLHeroProbeManager::renderProbes`, versus 5,200 of
19,947 in `68532`. Remaining main work is approximately 12.2 versus 12.7
seconds; ordinary probe work is approximately 1.95 versus 2.06 seconds.
Captured batch value copies account for 1,384 ms of current self `memmove`
(1,061 shadow, 323 G-buffer); inline command-data copies account for 821 ms.
These categories overlap their callers and are not whole-frame GPU timings.

More live wrappers do not establish more admitted geometry. The prior
residency explanation remains a hypothesis. The generic `renderSpotShadow`
name/trace label includes both directional cascades and projector maps;
its entire cost is not removable projector work.

## Original-to-native contract

Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` remains authoritative.

- `llheroprobemanager.cpp` updates all six faces at update rate 1. Preserve
  that cadence, dynamic content, face-specific matrices and directional maps.
- Original `pipeline.cpp` computes projector receiver matrices per face but
  guards projector caster camera/cull/cache/draw with `!gCubeSnapshot`.
  Native probe captures must borrow the main projector texture and validity;
  cube receivers retain the original single-tap filtering shortcut.
- Original draw/texture owners skip unchanged shader, buffer, texture,
  sampler and compatible skin-palette state. Native caches are per encoder
  and compare actual native resource identities and offsets.
- Native capture records have no original storage equivalent: original draws
  directly. Constructing an admitted native record in its final storage must
  preserve draw order, expected counts and whole-transaction failure behavior.
- Shadow coverage equations, raw R32 depth, reverse-Z, depth clamping,
  alpha cutoff/stipple and caster admission remain unchanged. Narrow resource
  arrays require matching MSL entry points and PSOs, not merely fewer binds.
- Shared frame data is immutable after publication. Recycle a page only after
  every native command that referenced it completes or is discarded before
  submission. Exact byte comparison prevents pointer reuse or view-dependent
  palette aliasing from selecting stale data.

## Implemented ownership and data changes

Covered shadows select matching one/eight/32-texture MSL signatures and PSOs,
sharing the unchanged alpha/stipple/raw-depth calculation. Static and ordinary
rigged opaque casters publish 80-byte MVP/config data; avatar cloth and coverage
retain their required larger layouts. Six shadow producer families and eight
hot G-buffer producers populate final vector storage in place. The impostor's
separate allocation-failure publication path keeps its local record.

Native encoder state caches compare PSO, raster state, native buffer and offset;
same-buffer offset changes use Metal offset setters. Inline bytes invalidate
the matching explicit buffer cache. Texture/sampler cache hits compare native
handles before acquiring ARC locals, preserving distinct linear/sRGB views.

Main/probe/hero owners share `ALMetalWorldFrameData` per device and frame. The
arena appends unique view/object constants without hashing, shares immutable
palette/material bytes using a hash plus exact comparison, and uses 256-byte
aligned ranges in 1-MiB shared pages. PBR's view matrix now resides only in the
world block, making its 1,296-byte material block reusable across stages/views.
All page users retain completion tokens; a completed first user cannot retire
storage still referenced by another uncommitted user. Abandoned uncommitted
commands release ownership without waits; submitted native commands keep
buffers alive after C++ wrapper/arena destruction. Spare pages are bounded.
Persistent scene-membership and caster-visibility caches were not introduced.

Projector captures borrow current main-owner textures/validity, preserve face
receiver matrices and single-tap filtering, and skip cube caster traversal.
Both-unbuilt maps explicitly produce lit/no-shadow publication. Main/hero
directional cascades, resolution, update cadence and coverage are unchanged.

## Verification

Local evidence: `/private/tmp/alchemy-metal-cpu-encoding-20261108.Xi7qTD`.

- `tools/metal/doctor.sh`: exit 0, M1 Max/64 GiB, macOS 26.5.2,
  Xcode 26.6, Metal Apple family 7.
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target almetal_tests --parallel 4`:
  exit 0. Initial integration caught a water-loop variable shadowing the native
  command pointer; corrected before the successful build.
- With `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, the complete
  `almetal/RelWithDebInfo/almetal_tests` and `almetal/Release/almetal_tests`
  each exit 0 with **53,125 assertions/63 cases**. The corresponding Release
  build command also exits 0. All GPU-test world renderer instances use the
  arena, including existing PBR/material/alpha/water/terrain pixel baselines.
- Focused cache/completion cases pass 201 assertions/three cases. Early runs
  exposed missing labels and a nil depth state in the new cache fixture;
  labelled descriptors and an actual always/no-write depth state corrected
  the fixture. The original wrapper validation contract was not weakened.
- `tools/metal/build.sh build`: initial and committed-source rebuild exit 0
  with warnings-as-errors. Source-exact candidate is `26.4.0.68547` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
  Generated metadata and executable embed full renderer SHA `18aedd2ecb…`;
  the executable now contains the opt-in benchmark arming code.
- `codesign --verify --deep --strict <app>`: exit 0.
- `tools/metal/verify_app.sh <app>`: exit 0, 349 inspected Mach-O slices,
  zero non-ARM64 slices. `python3 tools/metal/validate_harness.py`: exit 0,
  all 32 required files validated. `git diff --check`: exit 0.
- Executable SHA-256:
  `6f942194c757fb8a3335970ed1f527b5cf815d4d177d71e1d5ed5ee270c62609`.
- Packaged metallib SHA-256:
  `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.

Fresh attended performance and visual comparisons remain open. Neither reduced
API calls nor a passing build establishes improved FPS or release readiness.
