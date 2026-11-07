# Empty UI clip / Conversations checkpoint — 2026-11-07

**Implementation commit:** `abde32f46e`
**Reference revision:** `upstream/develop` at `65c39b7bad928e165f1acdc0cbf39478e017ef1d`
**Candidate package:** `Alchemy Metal Development 26.4.0.68497`

## Original-reference mapping

The Conversations toolbar command and `Floater.ToggleOrBringToFront` callback
match `upstream/develop`; the floater registry did create, show and front the
`im_container`. The original renderer's `LLRender::setScissor()` accepts
non-negative width and height and forwards zero-sized clips to `glScissor`,
whose result is an empty fragment region rather than a draw or frame failure.

The Metal UI path instead forwarded the empty clip to `MTLScissorRect`. Metal
rejects a zero-area scissor, so one transient layout clip failed the ordered
world/UI transaction closed and retained the prior drawable. That made the
correctly opened Conversations floater appear to do nothing.

## Native correction

`ALMetalUIRenderer::draw()` and `drawHUDIndexed()` now preserve the original
empty-clip invariant as a successful no-op after validating the batch, before
opening a frame or encoding a Metal scissor. Actual encoder calls remain
strict; invalid non-empty rectangles still fail closed. A focused GPU test
proves that a valid empty clip succeeds, reports no error and records no frame,
batch or vertex work.

## Evidence

- Focused UI renderer: 600 assertions / 1 case pass.
- Full Metal suite: 52,772 assertions / 57 cases pass.
- Diagnostics-free Xcode Release build succeeds with warnings as errors.
- `verify_app.sh` passes 349/349 ARM64 slices.
- Final executable SHA-256: `c98702736c0b61589a1c2c04cc2c56e9bff4b4aeefb815d65bb376afd7a85921`.
- Final metallib SHA-256: `5a868c5a499eac5ee1addd0530cda175344e794d0514c4e657c9acd694a40e99`.
- In an owner-attended instrumented `68497` precursor containing the same
  correction, Conversations opened, closed and reopened visibly. The log
  showed the floater becoming shown/visible/frontmost and no subsequent empty
  scissor or ordered-transaction failure. Temporary diagnostics were removed.
- The same diagnostics showed `RenderMirrors` changing off and on with an
  initialized pipeline; the earlier inability to uncheck it did not reproduce
  after the resource-lifecycle checkpoint. Persisted shadows later caused a
  separate overload/freeze and remain an isolated unresolved issue.

This is bounded UI-command evidence, not general UI parity, shadow stability,
mirror visual parity or release readiness. The final diagnostics-free package
is build/architecture verified; its attended source-exact rerun remains open.
