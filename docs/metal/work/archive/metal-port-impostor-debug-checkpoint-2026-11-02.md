# Metal-port execution archive — Impostors

**Date:** 2026-11-02
**Code commit:** `e4180f8e7d` (`feat(metal): replay impostor debug outline`)

## Implemented boundary

1. Audited `LLVOAvatar::renderImpostor` and retained the four independent white
   edges immediately before the existing native impostor billboard.
2. Captured the existing center/left/up basis, normal rebase and time-decaying
   five-to-one-pixel width while the exact source state is live.
3. Required one/one additive RGB and alpha, `FUNC_ADD`, RGBA writes, enabled
   culling, and forward `LEQUAL` depth read/write; mismatches fail closed.
4. Added a Metal vertex owner that expands the four wide lines to 24 filled
   screen-space triangle vertices at exact full pixel width.
5. Retained forward/reverse depth conversion and endpoint-based mirror clipping.
6. Added additive white G-buffer output that preserves destination alpha and all
   auxiliary attachments, then restored billboard wireframe/fill state.
7. Rejected debug fields on non-impostor terrain/simple batches and rejected
   non-finite or out-of-range widths.
8. Added allocation-failure closure and proved no-debug albedo/depth identity.
9. Removed only `RENDER_DEBUG_IMPOSTORS` from the visual eligibility veto; 10
   visual modes and the latent pathfinding guard remain fail-closed.

## Evidence

- Focused physical RelWithDebInfo impostor case: 126 assertions, pass under
  Metal API/load-action validation.
- Complete RelWithDebInfo, Debug and Release suites: 46,148 assertions in 52
  cases each, pass; 2.47/2.65/2.42 warm seconds.
- Offline MSL and exact warnings-as-errors viewer translation units: pass in all
  applicable configurations.
- Full RelWithDebInfo production integration: pass after 127 cacheable tasks.
- Neutral non-manual/non-GL gate: 484/484 in 37.46 seconds.
- Source-exact Release Development `26.4.0d68069`: pass at `e4180f8e7d` after
  127 cacheable tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `a9aa52c7225bed1b51a3abd32907095d14898ecc878023ca826f5a1419b1c6f5`.
- Source/packaged MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`.
- Metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.

## Owner constraints retained

- Exact RLVa parity is M-013 nice-to-have. Exact comparisons require enabling
  RLV in both viewers, saving and restarting both; mandatory work is only the
  minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture; off/on is invalid and both-off cannot close parity.
- The executable still links OpenGL, so strict no-GL and product completion are
  explicitly open.

## Next boundary

Port `RENDER_DEBUG_SHADOW_FRUSTA`: the centralized eight-frustum owner with four
optional translucent volume strips, four cascade point sets, four extent boxes
and four 12-edge outlines. Preserve conditional data, color, alpha, topology,
state and order; require physical proof before removing only that eligibility
bit. Any live pair requires shadows enabled in both viewers.
