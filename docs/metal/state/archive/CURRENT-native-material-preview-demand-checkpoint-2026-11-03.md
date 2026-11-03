# Native material-preview retained-demand checkpoint — 2026-11-03

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer commit:** `bceb839ea5a9b55bb9d8d1917b2136d1218626b0`

## Product movement

Eligible Darwin PBR material previews no longer cause the retained probe pack,
post targets or bloom chain to be planned merely because a preview exists. A
successful native Metal geometry/post/publication transaction completes with
no retained material-preview resource demand.

Fallback remains explicit and fail closed. A failed native attempt raises a
per-preview request counted across all live previews. If retained resources are
already ready, the original target-backed callback runs in the same frame. If
they are not ready, the preview remains pending while the next resource
reconcile builds the pack. Waiting frames do not repeat the identical native
attempt, and successful retained publication clears that preview's request so a
later reconcile can remove preview-only resources. Destroying a pending preview
also releases its request. Non-Darwin keeps planning from the live-preview
count.

This is a resource-demand cut, not preview GL elimination. Classic, non-HDR,
post-suppressed and incomplete native cases still use retained fallback. The
dynamic-texture base still constructs a GL texture, the global GL-disabled
scheduler guard still prevents targetless work, the shared Preview target is
still used by fallbacks and other dynamic textures, and application-owned
OpenGL remains linked and reachable.

## Source changes

- `LLGLTFPreviewTexture` now reference-counts explicit retained-fallback demand
  separately from live preview instances. Request transitions dirty the
  pipeline resource inputs so the normal reconciler creates or removes the
  pack.
- Darwin preview creation continues to derive dimensions from probe settings
  but no longer dirties the retained plan simply because `ProbePostPing` is
  absent.
- After a native failure, `renderWithoutTarget()` requests fallback. It either
  selects ready same-frame fallback or returns `PENDING` until resource
  reconciliation makes the retained transaction safe.
- Pending fallback keeps the preview scheduled despite texture-residency
  movement and skips redundant Metal retries. A successful `postRender()` or
  destructor releases the counted request.
- `LLPipeline::resourceInputsFromSettings()` uses retained-fallback count on
  Darwin and live-preview count elsewhere. Resource-plan comments and tests now
  state the input's demand semantics; the established matrix proves that false
  demand omits and true demand adds the retained probe post/bloom resources.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- `tools/metal/build.sh build` exited 0 at exact source
  `bceb839ea5a9b55bb9d8d1917b2136d1218626b0`. The warnings-as-errors Apple
  Silicon Xcode Release build produced Development build `26.4.0d68179`
  (`26.4.0.68179`), linked, signed, validated and staged successfully.
- Exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility configuration reported build
  68179 and its target had no work pending.
- Focused lit/PBR material-preview GPU coverage passed 426 assertions across
  two cases with Metal API and shader validation enabled.
- With those validation layers, full `almetal_tests` passed 52,381 assertions
  across 55 cases. `alrender_tests` passed 557,312 assertions across 137 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed all 1,513 assertions across 203 cases: 202 passed and
  the established single-palette font case skipped.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both
  reported 68179.
- `llrender_gl_shared_tests` reproduced the known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `eeec153f9092884ad9a9ee35ba88d03f18dcf3de583b3bd8e5787572965cc2a0`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-material-preview-demand.json` exited 1 as
  expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `e0c8e680275632fd2f47f3b6df95e010e95ff252d7d5c291aa41a6aa8fa16603`.
- `file`, `otool -L` and `nm -u` confirm the primary binaries are arm64, the
  viewer has no undefined `CGLTexImageIOSurface2D`, and both Apple OpenGL and
  Metal remain linked. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared. Visual
  material-preview questions remain in owner-attended queue row 17, advanced
  only to exact build 68179.

## Next executable boundary

Split the global GL-disabled dynamic-texture guard so targetless Darwin work can
continue after context removal while retained-target work remains pending and
the display caller does not clear GL depth without GL authority. Then remove
the native preview's dual GL texture ownership without weakening explicit
fallback, and extend native ownership to classic/non-HDR/post-suppressed modes.

The product remains incomplete. Unsupported preview modes, other dynamic
textures, application-owned GL context/traversal/resources, complete parity
and stability, paired profiling, independent review and strict release gates
remain open.
