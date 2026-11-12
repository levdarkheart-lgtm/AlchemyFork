# Local OpenHTJ2K busy-scene profile — 2026-11-12

The local experiment successfully uses OpenHTJ2K on all four ImageDecode
workers. The three-minute profile is saved and usable. This is a **post-arrival
profile**, not a recording of the teleport transition and not a matched codec
performance comparison.

## Exact candidate and capture

- Source `db398cc3a0edd1f596bd5bd6c1da3abdf4dae644`, plus the preceding scoped
  decoder commit23ca383870. Alchemy Metal OpenHTJ2K Experiment26.4.0.68860,
  Release/O3/LTO OFF, ARM64, Apple M1 Max/64GiB/macOS26.5.2/Xcode26.6.
- Executable SHA256
  `6df603e39c6505634594f123f150cf5f1dde717da6f811fc2bf0788358b34807`,
  UUID `B931A822-BEFC-33EB-9DF4-64D7B547F786`, recorded PID67814.
- Pinned OpenHTJ2K0.27.0, source72ea0450d2b5, CPU/NEON static library.
  Four outer decode workers, one inline execution lane per image. OpenJPEG
  remains the encoder/header parser; no runtime OpenJPEG decode fallback.
- Build,346-file ARM64 bundle inspection and deep strict signature checks
  exit0. Actual image tests pass736 assertions/21 cases across five CTest
  groups, including progressive-prefix quality and baked auxiliary channels.
  Tool tests76 pass; production source audit has0 blockers/errors. These are
  bounded checks, not full visual parity or release readiness.
- The exact app was launched for owner-controlled login/location/graphics.
  Startup and a real decode receipt confirm OpenHTJ2K and four workers.
  Cache1.71/address64 retains `OpenJPEG runtime: 2.5.4`; no cache purge observed.
  The worker-only68855 reference remains byte-identical and separate.

Recording was started only after the owner said “profile now”:

```sh
xcrun xctrace record --template 'Game Performance Overview' --attach 67814 --time-limit 3m --no-prompt --output <private>/openht-busy-teleport-3min.trace
```

TOC start11:59:04.046UTC, end12:02:05.801UTC, **181.755495 seconds**, Deferred,
end reason Time limit reached. The output-saved message and process-exit event
were observed; the detached launch did not collect a `waitpid` exit code.
TOC/CPU/metric exports and analyses exit0. Xcode emitted DWARF cursor warnings
while finalizing; function symbols remain usable. The matching dSYM exits0 and
has the same UUID. Its preparation was suspended before recording and resumed
after completion, so no symbol-build CPU work competed during the measured run.

The viewer log places the completed non-login teleport at11:58:30UTC, **34.046s
before trace start**. No teleport/arrival event occurs inside the trace. Live
state matches that arrival and sampled graphics stay unchanged. The prior
profile used shadow occlusion=true, while this run observes0. These differences
alone prevent attributing an FPS change to the decoder.

## Observed metrics

| Measure | Result |
| --- | ---: |
| Target CPU samples | 297,917 |
| Main-thread sampled CPU | 152.702 s |
| Four ImageDecode workers, sampled CPU total | 70.591 s |
| Mean decode core equivalent | 0.388 |
| Combined shadow ancestry on main thread | 70.649 s /46.27% |
| Main geometry owner ancestry | 25.082 s |
| Presented FPS over covered count intervals | about41.76 |
| Reported peak physical footprint | 17.338→17.758 GiB |
| Thermal state | Nominal throughout |

Decode workers contribute17.149,17.842,17.415 and18.185 CPU seconds.
OpenHTJ2K MQ arithmetic coding, significance propagation, cleanup and magnitude
refinement dominate their sampled stacks. This independently establishes the
actual runtime decode path, rather than only a startup label.

Decode work is not steady: successive30-second windows contain24.709,3.653,
3.804,19.477,18.807 and0.118 sampled CPU seconds (plus0.023 in the final1.755s).
It is therefore inappropriate to label the whole recording texture-settled.
The lower total than the older OpenJPEG profile is not a controlled speedup
measurement; asset histories and timing windows differ.

FPS is duration-weighted over179.052522s of available count intervals after
clipping the final bucket to the trace. It is presentation cadence, not proof
of fresh world rendering. Around20.91s there is a4.05FPS interval, followed by
a2.695s gap in count data and about0.309s of reported zero FPS. The gap is not
zero-FPS evidence, and the cause is unknown. Per-frame interval export is
empty, so individual-frame p95/tail latency cannot be established. Peak physical
footprint is a peak process metric, not current texture/GPU residency.

No OpenHTJ2K failure message was found; failures are logged at a suppressible
debug level, so this is not an observed zero-failure count or pixel-correctness
claim. Owner visual/alpha/texture acceptance remains pending.

## Evidence and next action

Private root: `artifacts/metal/openhtj2k-experiment-20261112/`.
`run.json`, `source-receipt.json`, `capture-plan.json`, `profile-timer-complete.json`,
`profile-toc.xml`, `cpu-{export,analysis,analysis-execution}.json`,
`decode-windows.json`, `runtime-analysis.json`, `symbol-preparation.json` and
`metrics/overview-analysis.json` retain exact commands, states, counts and
limitations. Raw traces, runtime locations and unredacted logs stay local and
ignored; none is published.

See [implementation/handoff](../work/active/openhtj2k-local-experiment.md) and
[the controlled CPU corpus comparison](../work/active/texture-decoder-workers-and-codecs.md).
Keep this experimental app local. Owner reports visual behavior/acceptance;
a future decoder A/B needs matched controls, scene, cache and arrival timing.
Recording the transition itself requires starting before a subsequent teleport.
