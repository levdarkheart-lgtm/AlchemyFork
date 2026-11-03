# Native non-HDR legacy glow and presentation checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `d17e94c4c694` (`Add native non-HDR glow presentation`)

## Replacement-first boundary

This commit implements the legacy non-HDR glow and full RGBA8 presentation
transaction in Metal. It does not delete the retained OpenGL feature path.
Eligible frames use the native transaction; any missing resource, unsupported
state or failed stage keeps GL authoritative for that frame.

The native feature path includes:

- the legacy 512-wide reduced glow target with `2^resolution_pow` height;
- RGBA8 or RGBA16F intermediate precision;
- luminance, warmth and authored-alpha glow extraction with the configured
  strength, maximum alpha, minimum luminance and true-noise behavior;
- the static 128x128 RGBA16F true-noise publication derived from the same CPU
  values used by the retained path;
- alternating eight-tap blur and additive combination;
- RGBA8 FXAA, SMAA neighborhood and CAS pipelines; and
- the production order of display mapping and first-quantization dither, glow
  extraction, AA, CAS, additive glow, RLVa sphere, scopes/reference and final
  presentation.

Non-HDR presentation no longer depends on HDR-only dynamic exposure, bloom,
cross-filter, lens-flare or lens-dirt readiness. Dither is applied at its first
RGBA8 quantization and not again at final presentation; HDR retains its final
boundary dither. Display, glow, AA/CAS and combine can stay in one command
buffer without a CPU round trip. Cached noise publication is invalidated on
resource release, recreation and Metal-device change.

This is feature implementation before source retirement, not feature deletion
and not an audit-count exercise. The 6-file change adds 1,099 lines and deletes
101. The retained non-HDR GL owner remains open for a later evidence-backed
authority cutover.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The exact `tools/metal/build.sh build` invocation exited 0 at committed
  source `d17e94c4c694`. The warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68229`
  (`26.4.0.68229`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `757305e9e3be84b91522291ab001e763d255b91d5213ce4fe16068eab7316cb0`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  packaged metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68229 and
  revision `d17e94c4c694`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,526 assertions across 56 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,301/136.
- The focused native legacy-glow case passed 42 assertions. It exercises exact
  reduced-target dimensions, RGBA8 and RGBA16F precision, true-noise effect,
  additive combination, invalid input rejection and a producer-to-consumer
  chain inside one uncommitted command batch.
- The focused Metal-only vertex-upload case in `llrender_gl_shared_tests`
  passed 17/17 assertions.
- `newview_tests` self-reported build 68229 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature; the
  focused native upload case passes in that same environment.
- Harness structure validation passes for 32 required files; all 27
  harness/tool tests pass.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-native-legacy-glow-strict.json` exits 1 while other
  application-owned GL remains. The exact report is 2,406 files / 3,217
  findings / 2,906 unique path/kinds/snippet records.
- The count is intentionally unchanged from the preceding checkpoint: this
  commit adds the Metal replacement before deleting the retained GL owner. No
  audit rule or suppression changed. Report SHA-256:
  `487aeae27ab89b19b7d9d3fc10d148b582b291ba9865e039cee13fa89d43cf8b`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
non-HDR glow/presentation case was added to the owner-attended daily review
queue. Deterministic GPU evidence is not being misreported as visual parity.

## Next executable action

Census the remaining non-HDR presentation eligibility and fallback
responsibilities. Add deterministic evidence and make the Metal transaction
authoritative before retiring the GL owner; do not trade functionality for a
lower inventory count.
