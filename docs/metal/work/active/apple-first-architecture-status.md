# Apple-first architecture implementation status

Updated 2026-11-15. The original whole-engine goal is active and incomplete.
This is the short current checklist; detailed source and retained evidence are in
[the execution ledger](apple-first-direct-cohorts-20261114.md).

The owner-tested preview is **69085 / 6af308d165**. Its five-minute recording
covers301.686seconds and reports13.998FPS; it is not a controlled before/after
speedup result. That staged app remains unchanged. The next-source baseline is
**aa5645944d / 69089**, built with native correctness coverage. Its palette cost
run found an equal-byte upload regression; that repair and the tile-pass changes
are integrated for the next build.

| Required boundary | Implemented and observed | Remaining work |
|---|---|---|
| Immutable scene/resource publication across main, shadows and probes | Shared producer generations, material/resource tuples, dirty notifications, native leases and retained static Legacy/PBR groups. Group route is active in the69085 launcher. | Complete sharing through specialized animated/terrain/tree capture boundaries; remove remaining repeated preparation without treating view-dependent culling, ordering or transforms as static. |
| Animated data ownership | Existing body pose and attachment records; shadows already consume immutable attachment identity. The next source preserves that identity through main/probe adoption, validation and GBuffer/forward alpha/glow/impostor binding. | Execute new parity, queued-reader lifetime and complete-cost checks. Preserve view-dependent system-body palette composition. Conservative animated GPU bounds remain unqualified and fail visible. |
| Bounded CPU preparation | Owned immutable view packets, shared frame credits, joined shadow preparation and a world preparation implementation. | World worker path remains disabled because prior complete-path costs regressed. Finish a useful preparation boundary for actual workloads; do not count unused worker infrastructure as performance success. |
| Native visibility and command preparation | Retained bounds, batched GPU frustum preparation, ordered compositions and two native command-bank variants are implemented and tested experimentally. | They remain disabled in the primary trial because total-time/GPU results are mixed or worse. Establish a useful measured selection/path for the small groups present in SL scenes. |
| Frame-wide work and Apple unified-memory lifetime | Frame budget, bounded decode/upload admission, native generation COW, CPU packet retirement and command-completion native-resource leases. | Resolve publication/residency churn and admission interactions across views. New typed diagnostics identify the failed stage and configured cap; raising caps without that evidence is not a repair. |
| Pass ownership and probe work | Main/probe/shadow ownership is explicit; existing lighting, projector, query and reflection behavior is retained. | Native experiment underway: keep an eligible unblurred probe light-map intermediate in tile memory and consume it in the same render pass. Must preserve hardwareRG8 conversion, shadow filtering, HDR, reverse-Z and sky. |
| Combined product validation |69085 builds, native checks and static gates passed; owner runtime and full-duration profile exist. New code is integrated locally. | Finish combined builds/parity/lifetime/cost checks, resolve demonstrated regressions, stage one verified combined candidate and assess it as a whole. Core feature/stability and performance acceptance remain partial. |

## Current implementation queue

1. **Done at source46:** build and native correctness for attachment sharing and
   typed diagnostics. Fixture repairs PPD247/248 retain the failed evidence.
2. **Doing:** correct measured equal-byte upload amplification and rerun the
   complete costs. Source46 CPU improved85/96 cases, but that alone does not
   accept the publication regression or establish an FPS gain.
3. **Doing:** native probe tile-lighting and validated empty-light no-op owners
   are implemented/integrated. Execute pixel/depth parity, physical pass counts,
   old-library behavior and complete costs before accepting activation.
4. **Next:** use typed publication/capture reasons and the saved whole-engine
   profile to resolve probe/source churn, then finish the useful CPU/GPU
   preparation boundaries above. Continue through remaining source gaps.
5. **Acceptance:** deliver the combined candidate with exact settings/source and
   evidence for owner whole-engine assessment. No percentage-complete or FPS
   improvement claim is supported yet.

## Five-minute profile implications

Ordinary probes account for28.810of85.117sampled main-thread CPU seconds.
Capture, native shadow rendering, resource lists, state binding and draw encoding
all contribute; their inclusive categories overlap. The export has no GPU pass
or shader timing table, so it cannot rank GPU shaders or prove ALU saturation.
The light-map pass experiment follows source-level duplicate work and is subject
to measurement, rather than a claim that this pass alone explains14FPS.

Only217new stability deferrals occurred between interior measured census
endpoints. Large budget-denial counters are not CPU timings. The new attachment
work removes confirmed repetition; its FPS contribution is still unproved.

## Execution constraints

Keep staged69085 and69065 intact. No new viewer launch, login, live profile or
push is authorized by this checklist. The separate owner-requested five-minute
capture has completed and the build/test hold is released. Raw logs/traces remain
local and private. All open defects retain their IDs and owner verification state.
