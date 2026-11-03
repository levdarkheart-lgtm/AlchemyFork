# Owner-attended daily visual review queue

**Cadence established:** 2026-11-02 by owner direction.
**Scope:** visual comparisons that require operating reference/candidate viewers.

## Operating rule

Codex does not independently launch, drive or take snapshots from either viewer
between review sessions. Engineering continues through source inspection,
builds, deterministic tests, physical Metal validation, static/link/runtime
audits and profiling. Visual questions accumulate here and are reviewed with
the owner in one batch at most once per day.

No automation or daily launch is implied. The owner starts the review session.
Rows remain open until an owner-attended session records the exact builds,
profiles, settings, location/fixture, artifacts and decision. Existing evidence
may be reused only when its provenance satisfies `docs/metal/VALIDATION.md`.

## Status vocabulary

- `queued`: useful visual question; prerequisites may still need source work.
- `ready`: exact builds, fixture and settings are known for the next session.
- `reviewed`: owner-attended outcome and evidence are linked.
- `blocked`: a concrete external prerequisite prevents the session.

## Queue

| Priority | Case | Status | Question and prerequisites |
| --- | --- | --- | --- |
| 1 | Legacy BLEND / stained glass | queued | After the next alpha-owner source slice, compare patterned transparency, authored diffuse alpha, specular/environment glare and depth ordering in a controlled fixture. Match all visual settings and camera state. |
| 2 | Projector-visible shadows | queued | Use a fixture with an unmistakable projected spotlight contribution. Save shadows on in both profiles before capture; distinguish projector, alpha/avatar caster, cascade and PCSS/quality results. |
| 3 | Broader authored mirrors | queued | Extend beyond the one accepted dry sample. Require identical seated/fixed camera, mirror settings, SSR state, time/weather and converged assets; review content differences rather than raw metrics alone. |
| 4 | Terrain height/composition-map cutover | queued | On exact build 68133 or later, compare terrain geometry/composition at a stable region and camera after asset convergence. Match graphics preset, draw distance, environment and region state; inspect dirty-region boundaries as well as broad terrain appearance. |
| 5 | Water cell/displacement-map cutover | queued | On exact build 68135 or later, use a stable water fixture, camera and environment. Inspect flat-field/cell boundaries, near/far wave mip transitions, displacement amplitude and water haze/exclusion behavior after asset convergence. Match graphics preset, draw distance, environment and region state. |
| 6 | Parcel-owner terrain overlay cutover | queued | On exact build 68137 or later, enable Show Parcel Owners in stable classic and PBR terrain fixtures. Inspect ownership colors, point-clamped cell boundaries, region edges and refresh after a parcel-overlay update; match camera, environment, terrain mode and region state. |
| 7 | CPU-generated sky/shiny-face cutover | queued | On exact build 68139 or later, use a fixed environment and camera in the legacy CPU-sky path. Inspect all six sky seams, current/next transition, color/alpha linearity and shiny/water cubemap response after convergence; match sky settings and render mode. |
| 8 | Minimap object/parcel-layer cutover | queued | On exact build 68141 or later, use a stable populated region and fixed minimap size/zoom. Inspect object colors/positions, parcel boundary color and alignment, panning/recentering, periodic refresh, parcel updates and resize regeneration; match minimap settings and region state. |
| 9 | Font-atlas cutover | queued | On exact build 68143 or later, inspect grayscale text at normal and subpixel positions, color emoji, shadows, font reload, UI-scale/DPI changes and atlas eviction/reappearance. Confirm no missing/stale glyph rectangles or color-space/swizzle change; match skin, font settings and window scale. |
| 10 | Moving CEF/VLC media | queued | Compare motion, color, alpha, scaling and interaction only after the native media ingress is source- and log-qualified. Preserve media/voice behavior. |
| 11 | Wireframe/material/editing matrix | queued | Batch remaining interactive advanced/editor views after source eligibility is complete; use identical selection/tool state and capture only questions that deterministic tests cannot answer. |

## Next owner session

Promote only cases whose source implementation and deterministic prerequisites
are ready. Review the smallest high-value batch, record decisions in the
relevant state archive, and leave all unreviewed rows open without inference.
