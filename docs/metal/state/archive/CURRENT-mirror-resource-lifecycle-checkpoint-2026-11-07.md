# Mirror and Metal resource-lifecycle checkpoint — 2026-11-07

**Source checkpoint:** `ab98aae4a9` on `codex/apple-silicon-metal`.
**Host:** physical MacBookPro18,2, Apple M1 Max, 64 GiB unified memory,
macOS 26.5.2, Xcode 26.6, Apple GPU family 7.

## Original-viewer contract

The mandatory reference gate used `upstream/develop` at `65c39b7bad`:

- `indra/llrender/llrendertarget.cpp` retains allocated attachments until an
  explicit resize or release rather than allocating an equivalent framebuffer
  graph every frame.
- `indra/newview/llheroprobemanager.cpp` obtains dynamic avatar inclusion from
  `RenderReflectionProbeDetail` and passes that policy to every cube face.
- ordinary UI composition consumes the retained world color/depth state. A
  Metal implementation must preserve the ownership and ordering, not the GL
  framebuffer mechanics.

## Native implementation

Commit `ab98aae4a9` adds exact-descriptor, byte-bounded Metal render-target
reuse. Borrowed attachments never enter the pool, stale window sizes are
evicted under the byte bound, and world G-buffer ownership publishes pool
statistics. The UI renderer retains and re-adopts the reusable world/UI color
output and copied depth rather than creating equivalent attachments every
frame.

The same checkpoint repairs first-face reflection-probe bootstrap without
weakening visible-world completeness, uses the correct dynamic-detail setting
for hero mirrors, includes avatars when either the mirror object or its face is
dynamic, and preserves clear-only cube faces. It also restores original PBR
mask/default-map fallback semantics and native minimap texture availability.

`MTLDevice.recommendedMaxWorkingSetSize` remains the full renderer working-set
ceiling, not a dedicated-texture VRAM value. The viewer now publishes a
separate bounded streamed-texture capacity into the original
`RenderMaxVRAMBudget`/`RenderTextureVRAMDivisor` controller. That automatic
capacity is a conservative containment default; it is not a scene-derived
performance optimum and must not be tuned from one location.

## Evidence and limits

- `almetal_tests`: 52,765 assertions pass in 57 cases.
- `llrender_tests`: 1,867 assertions pass in 223 cases; one expected skip.
- `tools/metal/doctor.sh` and `git diff --check` pass.
- One owner-attended diagnostic run observed 20,475 target reuses from 20,484
  acquisitions, four cached targets using 107.354 MiB and zero evictions, with
  continuing current-frame world/UI publication.

The reuse ratio is architectural evidence, not a paired FPS or scene-independent
memory claim. Higher fixed-cap experiments were discarded and are absent from
`ab98aae4a9`. A source-exact packaged Release and the required quiet-region,
dense-avatar, PBR-interior and outdoor-motion matrix remain open.

Two owner-observed regressions are next: Conversations does not open, and
`RenderMirrors` cannot be toggled off. Each requires an original-viewer state and
dispatch trace before implementation.
