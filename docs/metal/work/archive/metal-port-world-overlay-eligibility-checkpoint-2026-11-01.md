# World-overlay eligibility execution — 2026-11-01

**Work items:** M-008/M-009 advanced output and native-only cleanup.
**Integrated code:** `daa19101a9`.

## Executed work

1. Traced the final world diagnostic sequence after retained native replay and
   proved pathfinding visualization and transient object-update blips could
   contribute while native publication remained eligible.
2. Added `ALMetalWorldOverlayEligibility` so all known uncaptured world-overlay
   classes participate in one reviewable exactness decision.
3. Derived pathfinding activity from live floater visibility and enabled output
   modes so hidden/default floaters do not disable ordinary Metal frames.
4. Rejected exact publication while any debug blip remains queued.
5. Preserved all current GL traversal and output during migration. This change
   closes the silent-omission boundary but does not claim native pathfinding.
6. Kept exact RLVa behavior/image work in owner-deferred M-013. Pathfinding is
   core viewer functionality and remains release-critical.

## Executed checks

- Focused physical overlay-policy case: seven assertions under Metal API/GPU/
  load-store/shader validation.
- Complete physical suites: 45,015 assertions/52 cases in RelWithDebInfo,
  Debug and Release under the same validators.
- Neutral CTest gate: 484/484 in 256.86 seconds.
- Warnings-as-errors RelWithDebInfo and Debug Metal/pipeline objects: pass.
- Source-exact Release Development `26.4.0d68038`: build, link, stage and sign
  pass at `daa19101a9`; deep strict signing and 349/349 ARM64 verification pass.
- Executable SHA-256:
  `be70f1e858e43c1d0d059f8369dfbfff1baa766a6770f1e466dfc8e1c37c01b3`.
- Packaged MSL and metallib hashes remain unchanged from the preceding native
  Highlight Transparent checkpoint.

## Priority decision

Replace the explicit pathfinding GL boundary next. Inventory `LLPathingLib`
buffer ownership for navmesh, edges, paths/bookends, physics shapes, water plane
and selected-character capsules. Implement the smallest coherent retained Metal
batch and pipeline sequence with asymmetric depth/blend controls, keeping every
uncovered mode fail-closed. Port object-update blips after pathfinding.

Full feature/stability, strict no-GL and qualifying performance evidence remain
open. Exact RLVa parity stays in the persistent M-013 nice-to-have ledger.

## Erratum — open-source linkage audit

The pathfinding reachability and next-priority claims above are superseded.
The built ARM64 OSS target links only the nonfunctional
`llphysicsextensionsos` pathing stub: functionality is hard-false,
initialization is not implemented and no singleton/render output exists.
Implementing unavailable proprietary pathing is not part of eliminating a
reachable OpenGL owner. The guard remains latent for any future functional
extension. The independently reachable object-update-blip boundary is closed
by native replay in `b328cba4b6`.
