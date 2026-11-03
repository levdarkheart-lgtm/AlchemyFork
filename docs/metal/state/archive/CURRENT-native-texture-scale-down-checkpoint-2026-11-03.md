# Native-authoritative texture scale-down checkpoint

**Observed:** 2026-11-03 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `63f8f0d528` (`Keep native texture downscale Metal-only`)

## Replacement-first boundary

`LLImageGL::scaleDown()` no longer creates, allocates or copies through an
OpenGL texture when its input is already native-authoritative Metal storage.
The Darwin branch is selected only when the image has a live Metal texture and
no GL name. Mip-backed images copy the requested existing Metal mip and
regenerate the smaller mip tail; single-level images use the existing cached
Metal bilinear resampler. Both routes stay on the GPU.

Publication is transactional. Allocation, encoding and command-buffer
submission must all succeed before the smaller native texture replaces the
old one. Failure therefore retains the prior texture and discard metadata.
Success updates the discard level, allocated mip count, per-image memory and
last-use metadata while preserving the no-GL-name invariant. The legacy
dual-backed route remains for textures whose complete per-frame GL recovery is
still required; this change does not delete a rendering feature or weaken
fail-closed presentation.

Two physical-GPU regressions cover the native-authoritative paths. The mip
case proves the selected mip becomes level zero, the regenerated tail remains
byte-correct, memory/discard metadata shrinks and no tracked GL allocation or
name appears. The no-mip case proves the bilinear Metal render produces the
smaller byte-correct texture with render-target usage and likewise never
creates GL backing.

Source inspection also corrects a stale recovery action: the native main-world
and impostor legacy BLEND material implementations and broad deterministic
physical-GPU coverage already exist. Legacy BLEND/stained-glass visual parity
remains open in the owner-attended daily queue; implementation is not missing.

## Exact build and validation evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `63f8f0d528`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68235` (`26.4.0.68235`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed.
- Viewer executable SHA-256:
  `0bf2f79ec710324438e20a2db6370e174022127302699bbc9863e8c0a8411bec`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`.
- The exact Ninja warnings-as-errors tree configured at build 68235 and linked
  `almetal_tests`, `llrender_gl_fresh_tests`,
  `llrender_gl_shared_tests`, `llappearance_gl_tests`, `alrender_tests`,
  `newview_tests` and `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `llrender_gl_fresh_tests` passed 2,650 assertions across 98 cases, including
  both new physical native-authoritative scale-down cases;
  `almetal_tests` passed 52,526/56; and `llappearance_gl_tests` passed
  13,208/3.
- `alrender_tests` passed 557,301 assertions across 136 cases.
  `newview_tests` self-reported build 68235 and passed 98,978 assertions across
  744 cases: 743 passed and the established platform CRT case skipped.
- The complete shared-state diagnostic suite reproduced the established
  non-green host signature: 8,352/8,357 assertions, 182 passed, 4 failed and
  39 skipped across 225 cases. Four legacy/headless frame-fence assertions and
  one depth-format expectation failed; neither new scale-down case lives in
  this binary.
- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-native-texture-scale-down-strict.json` scanned
  2,406 files and reported 3,217 findings / 2,906 unique records, exiting 1 as
  expected while application-owned GL remains. The count is unchanged from
  the prior checkpoint; no audit rule or suppression changed. Report SHA-256:
  `4b3803cc47b02651b3a6a44f246253bcba20df37f45b53aa7f4847e9fad2b6a7`.
- The packaged executable still links both OpenGL and Metal. Static, linked
  and runtime no-GL release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Next executable boundary

Make texture residency accounting backend-neutral. Native-only Metal images
currently update their per-image byte count, but the process-wide texture
budget is keyed only by GL names. Add explicit Metal allocation/replacement/
destruction accounting without double-counting transitional dual-backed
resources, then physically regress create, downscale, replacement and teardown
before changing any eviction policy.
