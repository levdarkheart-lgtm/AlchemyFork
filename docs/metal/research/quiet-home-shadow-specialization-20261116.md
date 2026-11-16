# Home shadow specialization: recorded-scene evidence

The next owner checkpoint is Home only. PPD-319 remains open; PPD-330 is a
proposed repair with GPU evidence, not an accepted viewer FPS result. The broad
Apple-first architecture rollout remains paused.

## Original boundary and native repair

Pinned reference `1eb44bbd1b` compiles shadow quality 2 with
`SHADOW_PCF_KERNEL=6`, excluding PCSS. Native MSL selects quality at runtime,
retaining PCSS and variable-width PCF, including private `wx[6]`/`wy[6]` arrays.
Both saved Home logs report `AlchemyRenderShadowFilterQuality=2`. The verified
producer maps that setting into the shadow pack with a separate cube bit.
Graphics Q6 does not establish shadow quality 3.

The proposed MSL adds optional boolean function constant 3. True specializes
quality 2; false or undefined preserves runtime selection. The renderer must
select it from the exact pack subsequently bound. Sampling, weights, precision,
probe count, lights and shadow policy remain the same. The diagnostic shader
that hardcodes quality 2 must not ship.

## Separate fixture and actual recording

Private evidence is under `quiet-home-fix-20261116T183158Z`.
`probe-accumulator-checks/overdraw` retains source, executable and library hashes,
all raw pairs and adverse observations. The fixture uses the production shadow
PSO, quality 2, 20 overlapping probes, automatic/mixed families, 1024×576,
seven blended draws per command, ten alternating warmup commands and 30 pairs
per family. High2 matched all 36 shadow images exactly. All 60 pairs favored
it; paired median GPU reductions were 12.59% and 10.95%. Classic specialization
gave roughly 2% amid noise and is not selected. Earlier short quality-3 tests
remain stress checks, not matched Home costs.

The actual owner recording is `quiet-qy__4ufq`, source `fd5e8e088e12`, build
69200. Only a separate working copy of its late GPU capture was replayed.
Xcode's popover confirmed **Medium / Overlapping** on M1 Max, macOS 26.5.2,
Xcode 26.6. Captured resolution 3456×2168, 20 command buffers, 41 render
encoders, 5,104 draws and 10 dispatches were retained.

| Replay | GPU workload duration |
| --- | ---: |
| Original installed shader, before edits | 140.89 ms |
| A1: original expression recompiled, harmless comment | 142.99 ms |
| B: shared filter source specialized to quality 2 | 119.21 ms |
| A2: original expression restored and recompiled | 140.90 ms |

B is 15.39–16.63% shorter than adjacent recompiled controls. This is one fixed
capture source A/B/A intervention, not live FPS, a same-binary uniform test or
isolated per-pass execution costs. B affects every captured consumer of the
shared helper. A renderer change covering a subset cannot claim all that gain.
All recorded scene, probe and light work remains present.

| Shader | Restored A2: registers / spill bytes | B: registers / spill bytes |
| --- | ---: | ---: |
| Simple shadow alpha | 245 / 304 | 224 / 304 |
| Projector | 200 / 0 | 188 / 0 |
| Shadow/SSAO light map | 234 / 0 | 188 / 0 |
| PBR alpha | 254 / 80 | 254 / 32 |
| Material alpha | 254 / 32 | 242 / 0 |
| Base geometry | 214 / unknown | 214 / unknown |

Simple alpha still spills 304 bytes. This does not establish a complete spilling
repair or attribute the entire live 24→14 FPS decline to shadow filtering.
Growing probe-dependent GPU work remains the earlier measured limiter.

The original shader logic was restored, the GPU workload stopped, and
**Don't Save** selected. Xcode returned to Replay / Finished running. No new
viewer launch, login, live profiling or push occurred. Independent input
verification already matched all 17,254 regular-file hashes; all 4,851 capture aliases also match their recorded content. The strict nofollow representation
failure is retained.

## Compiler and integration

The current Release graph has 1,641 C/C++ commands, all `-O3` and ARM64. This
does not claim every object was rebuilt today. Production Metal uses 3.1,
fast math and performance optimization. All 367 production function definitions
are identical between default and explicit `-O2`; only recorded command metadata
differs. Scalarization does remove intermediate runtime accumulator indexing,
but the measured variants did not justify selection.

Root owns MSL, CMake, documentation and integration. Integrated `almetalworld.cpp`
work prepares 39 additional High2 PSOs, including matching fused tile producers for light map, projector, simple alpha,
material alpha and PBR alpha. Avatar, custom blend, water, probe-tile light map
and impostor paths initially retain generic pipelines and exact quality behavior.
Build, native correctness checks, app verification and separate staging precede
the Home checkpoint. Live FPS, full spilling closure and product gates remain
partial.


## Combined verification and live outcome still pending

Runtime819f98d71c /69224 passes the corrected native matrix: 470case-executions,
2391076assertion-executions, plus18budget and179tool tests. ARM64/signature/app
and exact staged-node checks pass. Initial69221 probe parity failure was repaired
by giving both separate and fused light-map producers matching High2 variants;
no pixel equality relaxation or fusion disable. Old explicit native libraries
retain generic compatibility rather than failing for absent optional constant3.
All initial failures and the rejected stage remain retained.

After owner launch authorization, two Home-only Overview/native recordings
completed successfully, then both viewers closed. Region-environment run:
Alchemy-home-upwlt_pz. Midday: Alchemy-home-dwgbpexo, with owner reports27→17→15FPS.
The reported decline remains. Live gain/fix acceptance is unproved; analyze the
runs separately and respect their lighting state, focus, workload and retained
coverage before comparing. No further implementation precedes that discussion.
