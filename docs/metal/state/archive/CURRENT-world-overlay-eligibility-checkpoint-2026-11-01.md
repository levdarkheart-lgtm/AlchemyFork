# World-overlay eligibility checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.
**Branch:** `codex/apple-silicon-metal`.
**Code checkpoint:** `daa19101a9` (`fix(metal): reject uncaptured world overlays`).

## Reachable omissions

Source tracing of the final world-overlay sequence found two outputs that occur
after retained native world replay and outside ordered UI capture, but did not
make native publication ineligible:

- `renderDebug()` can render or clear pathfinding visualization without the
  general render-debug mask. Its owners include selected-character physics
  capsules plus console navmesh, edge, path/bookend, shape, x-ray and water-
  plane modes.
- **Show Updates to Objects** can emit depth-tested transient `mDebugBlips`
  independently of the other debug flags.

An otherwise eligible Metal frame could therefore publish while silently
omitting active viewer output.

## Integrated eligibility boundary

Commit `daa19101a9` adds `ALMetalWorldOverlayEligibility`, an explicit policy
covering the existing render-debug, wireframe, reflection-probe-volume and far-
plane boundaries plus pathfinding and debug blips. Exact native publication is
permitted only when every uncaptured class is absent.

Pathfinding activation is derived narrowly from live contribution state. A
character floater rejects publication only when visible outside mouselook and
its physics capsule is enabled. A console floater rejects only when visible
outside mouselook and it clears the world or enables navmesh, shapes, water or
path output. Hidden and default floaters do not penalize ordinary Metal frames.
Blips reject only while the retained list is nonempty.

This checkpoint preserves the current GL traversal and output. It prevents
silent loss; it does not claim native pathfinding or blip rendering.

## Validation evidence

- Focused physical policy case: seven assertions prove the empty policy is
  exact and each uncaptured overlay class independently rejects publication.
- Complete physical Metal suites: 45,015 assertions across 52 cases in each of
  RelWithDebInfo, Debug and Release under Metal API/GPU/load-store/shader
  validation.
- Neutral CTest gate: 484/484 in 256.86 seconds.
- Updated Metal and pipeline objects compile warnings-as-errors in
  RelWithDebInfo and Debug.
- Source-exact Release Development `26.4.0d68038` compiled, linked, staged and
  signed at `daa19101a9`. Deep strict signing and all 349 ARM64 Mach-O checks
  passed; the main executable reports arm64.
- Executable SHA-256:
  `be70f1e858e43c1d0d059f8369dfbfff1baa766a6770f1e466dfc8e1c37c01b3`.
  Packaged MSL SHA-256:
  `21093c0c3ef9481e55327ecfa17b34dcf25ab771a0021c07d7d7cdcea7bce5d0`.
  Compiled metallib SHA-256:
  `656aa2ec5a7c259f6a86198a12f75fd7ce50914c5ff2865ed7a31208cdb6e1bc`.

## Boundary and next work

Native pathfinding remains a critical M-008/M-009 and strict no-GL owner. It is
not part of the M-013 exact-RLVa nice-to-have deferral. Trace `LLPathingLib`'s
retained buffers and preserve its clear, depth-only, solid, alpha-blend, line/
x-ray and polygon-offset order while porting the first coherent native mode.
All uncovered modes must remain fail-closed. Port the smaller object-update blip
owner after pathfinding.

The complete feature, stability, strict no-GL and performance release gates
remain open. The executable still links both Metal and OpenGL during migration.
