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
| 4 | Moving CEF/VLC media | queued | Compare motion, color, alpha, scaling and interaction only after the native media ingress is source- and log-qualified. Preserve media/voice behavior. |
| 5 | Wireframe/material/editing matrix | queued | Batch remaining interactive advanced/editor views after source eligibility is complete; use identical selection/tool state and capture only questions that deterministic tests cannot answer. |

## Next owner session

Promote only cases whose source implementation and deterministic prerequisites
are ready. Review the smallest high-value batch, record decisions in the
relevant state archive, and leave all unreviewed rows open without inference.
