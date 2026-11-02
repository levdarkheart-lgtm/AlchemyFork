# Native Impostors checkpoint

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `e4180f8e7d` (`feat(metal): replay impostor debug outline`)

## Observed owner contract

Develop **Impostors** emits four independent white line segments immediately
before each avatar impostor billboard. The source enables one/one additive RGB
and alpha blending, keeps forward `LEQUAL` depth read/write, and derives a line
width that decays from five pixels to a one-pixel floor after the impostor bake.

The native path captures while that exact state is live and retains the already
published center/left/up billboard basis and normal rebase. Metal has no portable
wide-line state, so the native vertex function expands the four edges into four
filled screen-space quads (24 vertices) with the exact full pixel width. The
outline:

- uses the impostor's forward/reverse depth convention and mirror clip plane;
- is encoded immediately before the same billboard;
- writes additive white albedo with zero source alpha, retaining destination
  alpha and every auxiliary G-buffer attachment;
- ignores culling like line primitives and restores the requested wireframe or
  fill mode before the billboard;
- accepts only finite widths in the source-produced one-to-five range and exact
  live blend, equation, mask, cull and depth state; and
- fails the frame closed for owner/state/width disagreement, missing mirrors or
  allocation failure. Ordinary non-debug output is unchanged.

Only `RENDER_DEBUG_IMPOSTORS` was removed from native-presentation
ineligibility. The remaining 10 visual debug modes and latent pathfinding guard
remain fail-closed.

## Physical validation

- Focused RelWithDebInfo avatar-impostor case: 126 assertions, pass under Metal
  API/load-action validation.
- Complete RelWithDebInfo, Debug and Release suites: 46,148 assertions in 52
  cases each, pass; warm elapsed times 2.47, 2.65 and 2.42 seconds.
- Offline MSL compiled in all three configurations, including both new impostor
  debug entry points.
- Controls prove five-pixel edge reach, the one-pixel floor, additive white with
  destination alpha and auxiliary attachments preserved, forward/reverse depth
  writes, mirror clipping, occlusion by an earlier nearer impostor, invalid
  owner/width rejection and no-debug albedo/depth identity.
- Exact viewer translation units `llvoavatar.cpp.o` and `pipeline.cpp.o` compile
  under warnings-as-errors.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after 127 cacheable tasks.
- The non-manual/non-GL gate passed 484/484 in 37.46 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `e4180f8e7d` after 127 cacheable tasks and
  staged Development build `26.4.0d68069`.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. The main executable reports arm64.

SHA-256 provenance:

- executable: `a9aa52c7225bed1b51a3abd32907095d14898ecc878023ca826f5a1419b1c6f5`;
- source and packaged MSL: `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`;
- compiled metallib: `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open.

## Persisted owner decisions

- Exact RLVa parity remains deferred non-release nice-to-have M-013. Exact
  comparisons require enabling RLV in both viewers, saving and restarting both;
  mandatory work is only the minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Implement `RENDER_DEBUG_SHADOW_FRUSTA`. Its centralized finite source block owns
eight camera frusta, four optional translucent volume strips, four cascade point
sets, four extent boxes and four 12-edge outlines. Preserve conditional presence,
color, alpha, topology, state and order; remove only that bit after physical
proof. Any live pair must have shadows enabled in both viewers.
