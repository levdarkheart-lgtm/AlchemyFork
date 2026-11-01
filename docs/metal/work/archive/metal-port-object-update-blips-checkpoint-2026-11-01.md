# Native object-update blips execution — 2026-11-01

**Work items:** M-008/M-009 advanced output and native-only cleanup.
**Integrated code:** `b328cba4b6`.

## Executed work

1. Traced the reachable **Show Updates to Objects** owner through
   `LLPipeline::renderDebug()` and retained its post-update point position and
   color in the native world transaction.
2. Added an eight-pixel Metal point pipeline with GL-to-Metal depth conversion,
   RGB-only writes, always-pass depth comparison and depth writes for both HDR
   and non-HDR world targets.
3. Appended replay after native Highlight Transparent and before display post,
   matching the production GL order.
4. Added fail-closed validation for missing storage, invalid counts, size/cast
   overflow, nonfinite point data and nonfinite transforms.
5. Removed the blip eligibility rejection only after successful native capture;
   all remaining uncovered overlay classes stay fail-closed.
6. Fixed the source owner's expired-entry use-after-erase by continuing after
   erase.
7. Audited the built OSS pathfinding linkage and proved it is a nonfunctional
   stub, correcting the prior release-priority claim rather than implementing
   unavailable proprietary functionality.

## Executed checks

- Focused physical world case: 1,385 assertions under Metal API/GPU/load-store/
  shader validation.
- Complete physical suites: 45,035 assertions/52 cases in RelWithDebInfo,
  Debug and Release under the same validators.
- Neutral CTest gate: 484/484 in 244.24 seconds.
- Warnings-as-errors renderer/MSL/GPU-test/production-pipeline compilation and
  linking: pass.
- Source-exact Release Development `26.4.0d68040`: build, link, stage and sign
  pass at `b328cba4b6`; deep strict signing and 349/349 ARM64 verification pass.
- Executable SHA-256:
  `fcba555e47abed64eedafa6f60f5b577104ffeb2a8c0f1692f7e813082e82d32`.
- Packaged MSL SHA-256:
  `ab73e9f26014d8ec13d7e02bf78cf5e23a0f24c75337779d8db3ddd6b9d57146`.
- Compiled metallib SHA-256:
  `7ca21604f3fe91e3755df3e034266c1cd2b4ce766b2bd31c86d2d90ba3905b30`.

## Priority decision

Do not port the dormant pathfinding bodies: the OSS target's pathing library is
hard-disabled and cannot emit output. Continue a source-led production audit of
reachable general render-debug, wireframe, reflection-probe-volume and far-plane
owners. Implement the smallest confirmed omission with asymmetric physical
proof and leave all other uncovered owners fail-closed.

Shadow comparisons require shadows enabled in both viewers. Exact RLVa parity
stays in the persistent M-013 nice-to-have ledger and retains its enable/save/
restart prerequisite. Full feature/stability, strict no-GL and qualifying M1
Max performance evidence remain open.
