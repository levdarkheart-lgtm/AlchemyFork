# Combined architecture owner evaluation —69122

Prepared2026-11-15; no viewer was launched or profiled. This is a run card for
the delivered candidate, not a new implementation phase or successful benchmark.
The [implementation status](apple-first-architecture-status.md) records enabled
foundations and measured reasons for leaving optional paths disabled.

## Exact builds

| Role | Version/source | Executable SHA256 |
|---|---|---|
| Combined candidate |26.4.0.69122 /beb6837b71f0429f116fef8e2ad7cc5d166bc010 |101a327c68852a44e2ba435cd0744a9beaf4582e511fe16b239f171069c57428 |
| Previous native trial |26.4.0.69085 /6af308d1650818d5334a0e0494a2ad825955fdf0 |0d22a4345fe703d0039d6f40da33d3888ecb5a2709d32e941f8a05ccf33d425b |

Use [the candidate launcher](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69122/Launch Apple First Combined.command>)
to apply its selected policy. The [previous native launcher](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/owner-preview-69085/Launch Architecture Preview.command>)
preserves the prior trial policy. Run separately; preserve caches and profiles.
Do not infer quality equivalence from preset labels. Record effective resolution,
AA/shadows/probes, draw distance, avatar limits, HDR/exposure and texture discard
bias. The exact settings comparison procedure is in [VALIDATION](../../VALIDATION.md).

Comparing these native builds evaluates the architecture experiment. Product
performance acceptance separately requires the pinned unmodified ARM64 Alchemy
reference defined by [PERFORMANCE](../../PERFORMANCE.md); a native-to-native win
cannot satisfy that reference requirement.

## Owner evaluation

1. Verify appearance and function before performance conclusions: attachments,
   animation/body/cloth, masks/alpha, PBR/projectors, shadows, probes/mirrors,
   terrain/water/sky, UI/HUD/editing, media/voice, texture loading and shutdown.
2. Exercise busy avatars, quiet region, PBR interior and outdoor motion; include
   turning, teleport/region transition and sustained use. These expose different
   producer, submission, lighting and streaming pressures. A shadow-only result
   cannot establish the whole-engine outcome.
3. Record online-world variation: avatar/draw population, active lights/probes,
   visible geometry and texture/loading pressure. Second Life scenes evolve;
   repeated samples and workload context are needed. Unmatched populations are
   contextual evidence and cannot isolate the architecture's causal effect.
4. If measurement is subsequently requested, collect warm repetitions separately
   from cold/loading data. Follow PERFORMANCE's minimum300 measured frames after
   warm-up and report p50/p95 frame times, stalls/tails, latency observations,
   physical working set and available CPU/GPU/streaming attribution. Preserve
   adverse samples; missing metrics remain missing. Profiling needs subsequent
   explicit instruction under the active goal.

The first useful outcome is owner feature/stability feedback on69122. A causal
performance conclusion additionally needs comparable repeated measurements;
contained cost ratios are not viewer FPS. Record regressions under their PPD IDs
before repair. PPD287 remains open for this original acceptance.

## Preflight evidence and next dependency

Ignored artifact `apple-first-persistent-native-scene-20261114/evaluation-preflight-69122.json`
pins both sources/executable hashes, candidate verification, full34-scope native
receipt and all four cost-summary hashes. Static identity checks pass. Candidate
runtime/performance results are absent. No viewer/profiler process was observed
in the continuation preflight. No app/launcher was changed or executed.

The remaining dependency is owner runtime evidence or subsequent explicit
runtime authorization. Keep the original goal and performance acceptance
partial; no completion, release or push is justified by this run card.
