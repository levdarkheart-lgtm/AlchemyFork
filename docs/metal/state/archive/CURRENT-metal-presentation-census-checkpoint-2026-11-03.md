# Metal presentation fallback-census checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `c2bcd90b04` (`Instrument Metal presentation fallback causes`)

## Cutover instrumentation

The default native presentation transaction now exposes a current-frame state
at the exact ownership boundary: disabled, not attempted, capture started,
capture failed, Metal surface unavailable, fused surface submission failed or
submitted. The low-level ordered-capture owner returns a structured end state;
the pipeline records it for the current frame; and the existing explicit
benchmark collector writes it beside requested, attempted, exact-native and GL
fallback flags.

The collector still uses the actual committed current-frame drawable marker for
`native_exact`. It does not infer success from the default setting. An eligible
requested frame without exact Metal publication is marked `gl_fallback=1`.
Contradictory state/flag combinations invalidate the evidence sequence. The
performance analyzer validates the complete optional census group and reports
per-scenario state counts plus exact-publication and fallback rates. Those
diagnostics do not decide the performance gate.

This checkpoint removes no feature and removes no fallback. It makes the
remaining cutover boundary measurable so native failures can be fixed before
the complete GL owner is retired. No viewer comparison or screenshot was
performed.

## Exact build and test evidence

- `tools/metal/build.sh build` exited 0 at committed source `c2bcd90b04`.
  The warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and
  staged Alchemy Metal Development `26.4.0d68233` (`26.4.0.68233`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64. Deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `32ac96653c5ccea6301862cf4112266d74eefc756654be3b7d39a638026ece29`.
- The configured `LL_VIEWER_SOURCE_REVISION` and CMake revision are
  `c2bcd90b041beed5d06f3c37c1f7ab73cc362bc4`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  packaged metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains `1`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68233 and the
  same full source revision. The focused benchmark suite passed 46 assertions
  across 7 cases. Complete `newview_tests` self-reported build 68233 and passed
  98,978 assertions across 744 cases: 743 passed and the established CRT case
  skipped.
- With Metal API/GPU, shader and load-action validation enabled,
  `llappearance_gl_tests` passed 13,208 assertions across 3 cases, including
  the structured complete-capture result.
- The complete shared-state diagnostic suite reproduced its established
  non-green signature: exit 42, 8,352/8,357 assertions, 182 passed, 4 failed
  and 39 skipped across 225 cases. Four frame-fence timing assertions and one
  depth-format expectation fail; none intersects this change.
- Harness structure validation passed for 32 required files; all 29 harness
  and tool tests passed; `git diff --check` passed.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-presentation-census-strict.json` exited 1 as
  required while application-owned GL remains. The report is unchanged at
  2,406 files / 3,217 findings / 2,906 unique path/kinds/snippet records.
  Report SHA-256:
  `a6787d443073bca3f462b3308f5d5573b280b29530bf07dba60b98609fe908f8`.
- The Release executable still links both OpenGL and Metal. Static, linked and
  runtime no-OpenGL release gates remain open.

## Visual and runtime evidence boundary

No viewer executable was launched, driven, photographed or compared. The
owner-attended queue now records build 68233 and the optional census fields for
the next shared session. No state count or fallback rate is claimed until a
real CSV is collected.

## Next executable action

Continue implementing missing native feature owners, beginning with legacy
BLEND/stained-glass material behavior, while the next owner-attended session
collects a default-path census. Use the measured state counts to repair native
publication blockers; retire GL recovery only after complete Metal behavior is
durable.
