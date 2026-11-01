# Water/default-probe parallax checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.  
**Branch:** `codex/apple-silicon-metal`  
**Source checkpoint:** `fd0fa7cea56e3a9ba933f1405a6be68e9099374e`
(`fix(metal): keep water default probe unshifted`), pushed to the matching
origin branch.

## Source finding and correction

Source-led M-008 tracing found a bounded mismatch between
`reflectionProbeF.glsl` and the native Metal water path. GLSL deliberately
samples default reflection probe zero along the reflected water ray without
parallax. The Metal path sent that probe through the generic sphere-intersection
helper, so moving its camera-relative centre could select a different cubemap
direction even when scene direction was unchanged.

`almetal_probe_radiance()` now receives explicit water context. Only the
water/default-probe-zero combination samples `center + direction`; non-default
water probes, boxes and all non-water callers retain the previous parallax
rules. No OpenGL traversal, presentation authority or fallback gate changed.

The physical regression uses six differently colored cubemap faces, selects
only default probe zero and moves only its centre from +4000 to -4000 X. Before
the production fix the new assertion failed with aggregate RGB delta
1.596944809 (1,348/1,349 assertions). Afterward the focused case passed
1,349/1,349 under Metal API/GPU validation. This is an asymmetric negative
control: the old generic sphere correction must fail it.

## Build and validation evidence

- Complete physical RelWithDebInfo and Debug Metal validation each passed
  44,902 assertions across 50 cases after the change.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 259.22 seconds.
- The committed/pushed source produced warnings-as-errors Development build
  68026. Deep strict signing passed and `tools/metal/verify_app.sh` verified
  349/349 bundled Mach-O slices as ARM64.
- Executable SHA-256:
  `d33603803c5d45434d06e0332e14980dc095505b060624f09143a41f434e1d4e`.
  Packaged MSL SHA-256:
  `fb170421c26a1d75e556a46d14e52d660fe989a379bb62fbf80ba2eb5a317a1e`.
  Compiled metallib SHA-256:
  `40617ce97a852bb2a92454020d050d7ee146501bff24bf992b0432c006423b33`.

## Source-exact Computer Use evidence

`tools/metal/cua_preflight.py` passed for exact PID 46277, Development build
68026, the executable SHA above and isolated profile
`/private/tmp/alchemy-mirror-candidate.buhQH0`. Actual Computer Use activated
the semantic Login control and reached `STATE_STARTED` at the intended Danim
location. The 2048x1476 production Metal surface showed a coherent upright
world and UI. Targeted log inspection found no Metal validation error.

Computer Use opened the viewer's exit flow and activated semantic Quit. The
log contains `LogoutReply`, `Goodbye!` and `status: stopped`; all viewer and CEF
helper processes exited. `ALCHEMYNEXT_USER_DIR`, `MTL_DEBUG_LAYER`,
`MTL_SHADER_VALIDATION` and `MTL_DEBUG_LAYER_ERROR_MODE` were cleared afterward.
Authenticated logs and screenshots remain local and untracked.

## Product assessment and next boundary

This closes deterministic default-water no-parallax parity under a physical
control. It does not prove a live reference pair, broad water correctness,
moving mirrors, broad reflection-probe parity, M-008 completion, native-only
completion or a performance improvement.

The next admissible live step is a controlled water-visible candidate/reference
pair with identical source provenance, all 416 effective graphics settings,
camera, environment/time, warm state, window and snapshot contract. Any
mismatch invalidates the pair before scoring. If that controlled fixture is not
practical, the next priority is the next source-demonstrated probe or shadow
gap. The informational GL inventory currently scans 2,408 viewer-owned files
and reports 3,303 potential findings; macros, comments and duplicates mean that
count is not an equal-sized task total, and the strict release gate remains
open.
