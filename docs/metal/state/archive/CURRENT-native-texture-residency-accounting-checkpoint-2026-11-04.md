# Native Metal texture residency accounting checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `3b772a630f` (`Account native Metal texture residency`)

## Replacement-first boundary

Process-wide texture residency accounting now includes native-authoritative
Metal storage. Allocation, authoritative handoff, replacement, scale-down,
discard and teardown all reconcile an image's Metal charge. The byte count is
derived from the actual Metal descriptor, including format/block geometry,
every mip, array/depth slices and sample count. An RGB8 source expanded to an
RGBA8 Metal texture is therefore charged for the real four-byte storage.

A transitional GL/Metal mirror remains one logical allocation. While a live GL
name exists, the established GL-name allocation map owns the charge and the
Metal mirror contributes zero. An authoritative GL-to-Metal handoff retires
the GL charge before deferred deletion and installs the Metal charge in the
same transaction. This preserves the existing texture-pressure signal without
inflating it during migration or creating an unbudgeted deletion window.

Physical-GPU regressions cover authoritative GL-to-Metal transfer, native-only
mip and no-mip scale-down, RGB8 expansion, discard and teardown. They prove the
process total changes by the actual descriptor delta, never doubles for a
mirror and returns to the baseline after each lifecycle.

## Exact build and validation evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The changed `llrender_gl_fresh_tests` target compiled warnings-as-errors.
  Its focused `[LLImageGL][almetal]` slice passed 1,958 assertions across 19
  cases; the complete binary passed 2,669 assertions across 99 cases.
- `python3 tools/metal/validate_harness.py` passed all 32 required files and
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 29 tests.
- `tools/metal/build.sh` exited 0 at committed source `3b772a630f`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68237` (`26.4.0.68237`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `1121b39f30c1fae9e30dd88bb5922dfc4034adffe6d9801f0c7233554c5a92b1`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Next executable boundary

Cut the fetched-texture pressure scheduler over to recognize native Metal
residency. A native-only fetched image must enter and complete its existing
Metal scale-down route without requiring or binding the legacy GL scratch
target/program. Transitional GL/dual-backed images retain the existing path
until their consumers have complete native replacements.
