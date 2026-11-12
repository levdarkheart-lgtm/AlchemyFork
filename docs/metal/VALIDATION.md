# Validation, parity and release gates

## Reference definition
The comparison is **Alchemy's same-source ARM64 open-source-only build**, not an unbuildable proprietary configuration or a different graphics-quality preset. Record source revision, toolchain, dependency set and scene/quality settings. Run the reference separately; the Metal fork itself never needs a GL fallback.

## Quality and functionality
The versioned `feature-matrix.csv` is the functional contract. For each critical item, capture reference behavior, Metal result and an evidence pointer. A successful login is necessary but insufficient. Include cold/warm texture loading, UI/fonts, avatar/attachments and alpha geometry, PBR/glTF, transparent water, terrain, reflection probes/mirrors, HUD, editing, selection, mesh asset upload using the open-source physics path, voice and media display, scripts/interaction, camera, teleport, region transition and shutdown/restart.

`unverified` means untested, not passing. New functionality discovered from source must be added to the matrix. An agent must not mark a missing feature `not_applicable` without an actual reference behavior check; only owner can explicitly waive a regression.

Record every discovered defect and reported regression in the mandatory
[post-port defect register](POST_PORT_DEFECTS.md), including unknown causes.
Retain stable IDs and failed/reverted history. The feature matrix and visual
queue reference defect IDs; they do not replace the register. A verified repair
requires corrective evidence and owner confirmation, and still needs explicit
push approval. `validate_harness.py --release` also rejects unresolved defect
records; ordinary harness validation checks retention and evidence structure.

## Pixel / GPU validation
- Use Xcode Metal frame capture, command-buffer diagnostics and Metal API validation on real Apple Silicon hardware. Where practical, automate deterministic camera paths and capture screenshots with a consistent framebuffer color space.
- Compare equivalent scene, exposure, shader level, avatar limit, resolution, antialiasing, SSR and reflectivity. Before scoring any paired images, capture each isolated profile's effective `Render*` plus Alchemy exposure/render settings with `tools/metal/graphics_settings.py` and require its comparison command to exit zero. A mismatch, missing required setting or unilateral visual setting invalidates the pair; image metrics from such a pair are not parity evidence. For every shadow comparison, shadows must be enabled and saved through Graphics Preferences in both viewers before capture; shadows do not require a restart, while RLVa state changes do. An off/on or both-off pair is not shadow-parity evidence. Allow defined stochastic/temporal differences; capture deviations in colors, depth, PBR normals, alpha sorting, selection and shadow edges.
- Track CPU/GPU stalls, resource hazards, retained/overreleased Metal objects, command completion, resize and drawable starvation. Validate correctness with Metal debug enabled and performance with validation disabled.
- OpenGL-static audit: `python3 tools/metal/audit_gl.py --mode inventory` reports current owned-application API use; **`--mode strict` must pass at release**. Static grep is necessary but not sufficient. Check runtime context creation and linked binaries (and document legitimate foreign media subprocess API use).
  The expanded gate scans first-party `indra` runtime source and reports
  vendor/submodule/test exclusions. Strict fails API/context/header/prototype/
  loader blockers and scan errors; native names remain informational rows.
  Exact CPU-helper exemptions never exempt their bodies. Categories can overlap.
  Lexical analysis cannot prove arbitrary computed symbols, full dataflow or
  runtime safety. [Source-gate evidence](work/active/native-opengl-tail-removal.md)
  includes positive detections and adversarial regression checks.
- `tools/metal/verify_app.sh <path-to-app>` checks native slice at release; code-sign and runtime load checks require actual macOS host.

## Build/test evidence
- `tools/metal/doctor.sh` (arm64 host, Xcode, CMake, Metal toolchain).
- `tools/metal/build.sh configure`, `tools/metal/build.sh build` and Alchemy's documented relevant test suite (`doc/TESTING.md`). Add Metal-specific GPU tests and headless/CPU policy tests to existing test infrastructure.
- `python3 tools/metal/validate_harness.py`, Python tool unit tests, shell syntax and `git diff --check`.
- Smoke test for normal signed-in viewer operations **without committing logs that contain account, location or session data**.

## Release gates — fail closed
1. Buildable from fresh checkout without private registries or licensed proprietary SDKs, in reproducible ARM64-only release configuration.
2. No viewer-owned OpenGL context/calls, Vulkan, Rosetta or translated rendering. App-owned executable and in-process non-system binary components correctly target arm64.
3. Critical `feature-matrix.csv` items all `verified` with evidence; no blocker that invalidates usability. No unapproved feature reductions.
4. GPU validation captures and stability sessions pass. Rendering artifacts fixed or accepted **by owner**, not ignored.
5. Performance criteria in `PERFORMANCE.md` met on primary M1 Max, with complete raw samples and environment descriptions, and no hidden reduction in image fidelity.
6. Licence inventory, third-party notices and viewer identity policy reviewed; no secrets. A distribution decision requires owner approval.

An unverified gate remains open; do not report release readiness on the basis of a green CI compile alone.
