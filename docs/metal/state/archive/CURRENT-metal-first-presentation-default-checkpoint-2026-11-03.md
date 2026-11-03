# Metal-first default presentation checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `04edf7b27d` (`Make native world presentation Metal-first`)

## Authority boundary

This commit makes the existing exact native Metal
world/presentation/ordered-UI transaction the default path. Source and
packaged `settings_alchemy.xml` declare `ALMetalWorldPresentation=1`; the C++
cached-control fallback is also `true`. The setting remains hidden,
nonpersistent and restart-scoped so an explicit false value can be used only
for controlled fallback diagnosis while application-owned GL remains.

This is not a backend-name switch and it does not remove a feature. The default
transaction includes the established native world materials, alpha, water,
sky, probes, shadows, post processing, presentation effects and ordered UI/HUD
capture. The immediately preceding checkpoint added legacy non-HDR glow and
the complete RGBA8 presentation sequence before this authority change.

The transition remains fail closed:

- one complete native world/presentation/ordered-UI transaction must reach the
  drawable before continuous publication unlocks;
- every later frame starts with a transparent Metal surface and independently
  earns opacity;
- only a successfully committed current-frame drawable suppresses its matching
  GL swap; and
- an incomplete resource, unsupported batch, failed command or drawable
  submission retains the complete GL frame for that iteration.

The 2-file change adds 9 lines and deletes 8. It changes normal authority
preference without reducing the strict GL inventory. The transitional fallback
and live GL context remain open work; the completed product cannot retain them.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The exact `tools/metal/build.sh build` invocation exited 0 at committed
  source `04edf7b27d`. The warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68231`
  (`26.4.0.68231`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `58026eb643ad39f15b0c42e3f0ea50f752762157a99a12ef26797b1ceff800d0`.
- Source and packaged MSL remain byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  packaged metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`.
  An XML query of the packaged file returns `1` for
  `ALMetalWorldPresentation`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68231 and
  revision `04edf7b27d`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,526 assertions across 56 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,301/136.
- The focused Metal-only vertex-upload case in `llrender_gl_shared_tests`
  passed 17/17 assertions.
- `newview_tests` self-reported build 68231 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature; the
  focused native upload case passes in that same environment.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-metal-first-default-strict.json` exits 1 while
  application-owned GL remains. The exact report is 2,406 files / 3,217
  findings / 2,906 unique path/kinds/snippet records.
- The count is intentionally unchanged because this checkpoint promotes
  native authority without pretending the transitional fallback was removed.
  No audit rule or suppression changed. Report SHA-256:
  `eb8b00822f94cd6b3e7a2149891e6bb359a5ee04b670b5778de35c65ad7ff449`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
Metal-first non-HDR presentation case remains in the owner-attended daily
review queue, updated to exact build 68231. Default authority is not being
misreported as visual parity.

## Next executable action

Instrument and census default Metal publication failures and remaining GL side
effects without independently driving a viewer. Continue replacing those
responsibilities, then retire fallback only when complete native behavior is
durable; do not trade functionality for a lower GL count.
