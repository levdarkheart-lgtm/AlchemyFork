# Home checkpoint — build 69233, 2026-11-17

The Home FPS decline persists with the first GPU candidate-preparation group active. Owner reports about31FPS initially and16FPS settled, using the region environment rather than Midday. Native timing measures29.690→15.886FPS. This run verifies activation and reproduces PPD319; it does not establish a whole-viewer gain or a new regression. The viewer is closed. Implementation remains paused at the agreed group checkpoint; no next group, additional profile or push occurred.

## Identity and retained coverage

- App: `artifacts/metal/probe-candidate-preparation-20261117/candidate-26.4.0.69233/Alchemy Metal Alpha G2G5.app` in the original repository. Embedded source `647dc369d15729ecf98414439be1a0477dc2e36c`; executable SHA256 `6a3f872c8d3526e4413e4fbbac286d30e2dc9b18118d21875dda33bdb0967508`; metallib `bdc013915e078aff90605b2ebd426163cee5daa271d4e4a97c3575269b616782`.
- Integration source `e17e705926`/`9a7f0d10fa`; root checkpoint `083e7c97a60aac3c6a263daa66429c3d75da6a84`. Builder and root indra tree match `7e9a8d8301a3923b3bd5a4baae2fbd9c435b942d`. Prior candidate69231 is preserved.
- Private run: `/Users/dekstroza/Alchemy-home-5p2dqheq`, exact PID13721. Owner manually clicked Login and later logged out. No settings, camera or location changes were made by the agent.
- Overview recorder/collector finish0/complete. Trace16:11:04.809–16:14:05.700UTC lasts180.890540s; actual native world start16:11:57.636462042UTC. Common retained CPU/GPU coverage is world−52.568344084→+127.722175249s, covering all requested world0–120 phases. CPU176,361target rows; GPU5,792rows include four metric variants, counted once in analysis. All37zero-frame GPU buckets precede world startup.
- Native117,609,586B continues to world248.449s. Footer101,759created/101,756committed and completed/3abandoned, no pending/full/budget/I/O losses,231contention drops: event evidence is partial, not exhaustive. The additional native-only tail is kept separate.
- Normal requestQuit/logout/Goodbye appears at16:16:05/06/08UTC and PID is absent. Viewer process exit code was not collected by the manual launcher. No observed observer EOF/exit2 is asserted.

## What explains the recorded cadence change

Native periods and clipped wait unions use the same consecutive, focused, stable-quality/dimension eligible frames. Remainder is elapsed period minus recorded main waits, not CPU running time.

| World seconds | FPS | Period ms | Main GPU completion wait ms | Other elapsed ms | Main configured probes |
|---|---:|---:|---:|---:|---:|
|0–10|29.690|33.682|11.616|22.066|0→1|
|10–30|23.626|42.327|19.219|23.107|1→16|
|30–60|18.102|55.243|33.491|21.752|16→28|
|60–100|16.148|61.925|41.444|20.481|28→39|
|80–120|15.886|62.949|43.026|19.923|34→45|

The increase in frame period is accounted for by increased completion waiting, while other elapsed time decreases. All1,268late wait starts target already-running GPU work exactly two display frames older. Of43.026ms/frame waiting,42.581ms overlaps the target GPU command envelope and0.445ms follows its end. The main wait owners are the light-map/deferred-light chain20.405ms/frame and opaque/optical post22.621ms/frame. These are waiting/command-envelope facts, not exclusive pass execution costs, occupancy or proof of one shader bottleneck.

Independent Overview cadence is29.290→15.880FPS. Main running CPU falls50.17→28.00% of one core. Decoder work falls from21.179CPU-s in0–10s and44.222CPU-s in10–30s to0.001CPU-s in80–120s. Late main running work still includes ordinary probes3.684CPU-s, sun shadows2.520, geometry1.872 and deferred lighting1.027 over40s, but sustained late decline is not main-core saturation or continuing decode work. GPU-end→display remains about3.5–3.7ms. Thermal state is Nominal throughout; clocks, bandwidth, power and fragment occupancy were not measured. Aggregate GPU active/inflight envelopes must not be presented as exclusive GPU time per frame.

Objects settle near8,479 by60s while configured probes continue growing. Main dimensions3456×2168, nine point lights,51projectors and their summed rectangles are essentially stable after10s. This strengthens the probe-dependent work hypothesis but does not isolate accepted taps, overdraw, spilling, mask benefit or resource-pressure cost. Source-cache near-budget logical bytes and cumulative evictions remain observations, not physical-memory/leak or FPS attribution.

## Did the new path run?

Yes. At the last unambiguous in-Overview log sample16:13:58UTC (world120.364–121.364s), shared main/probe counters report1,379preparations,7reuses,2,665declines and0failures. Published-mask bytes964,569,088 are cumulative encoded output, not resident memory or measured bandwidth. The next second-quantized sample straddles the trace cutoff and is excluded. No new preparation failure or shader compilation failure is logged.

Low reuse is actionable evidence for the next source audit, not proof that preparation caused the decline or lost performance. Inspect actual input/library epoch turnover and generic decline reasons before choosing a cache change; preserve exact-input ownership and completion safety. This capture does not isolate preparation cost or count fragment exclusions. Sampled late main shadow batches800/800 use High2 constant3 and pair constant4; these are CPU encoding observations, not exhaustive fragment coverage. No new compiler GPR/spill allocation was obtained, so PPD343 remains open.

Both49-control snapshots and all19launch architecture arguments match the previous healthy69231B2 in type and value. Drawable dimensions nevertheless differ:3456×2168 versus3456×1916, or13.1524% more pixels. Light, probe, object and resource workloads also differ. The descriptive15.886 versus15.040FPS late native comparison therefore cannot establish a causal gain or regression. The brief world100s read-only settings query does not explain sustained decline: excluding affected periods gives15.883FPS and43.038ms wait.

## Later owner environment change

Outside Overview, native cadence averages15.575FPS until world218.308604458 (~16:15:35.945UTC), when main configured probes reset55→1. The remaining native-only interval averages24.093FPS, with probes regrowing to19 before logout. Owner subsequently confirms changing the environment. This identifies the interaction context; the precise selection frame/preset and each GPU cost contribution remain unmeasured. It is not an unchanged-scene probe-only counterfactual and is not pooled with the timed region-environment run.

## Warnings and limits

Complete log audit retains153WARNING rows at40source sites,0ERROR/FATAL and three libpng stderr warnings. Relevant observations include one startup texture-cache body/index mismatch purged under existing PPD340,19attachment expiry plus19late/unrequested arrivals,15real HTTP Easy_28 timeouts (seven within Overview/eight in the tail), and19native mirror guards withholding incomplete publication for retry. These are functional/loading observations; none establishes its FPS share or a new architecture regression. No fatal connection/logout alert or new candidate-preparation failure is observed. Actual per-PSO validation enum is not recorded; no enum value is inferred from absence of notices.

Private evidence: `analysis/native/findings.md` with immutable `owner-environment-addendum-20261117.md`; `analysis/overview/report-v2.md` and settings-match erratum (the first report's unmatched-controls wording is superseded); `analysis/log-audit/report.md`; input/output manifests; raw native/log/Overview trace and exports. Root `analysis/checkpoint-receipt.json` binds final reports and owner context. Private account, asset and location data remain local.

Product advancement: one previously missing main/probe GPU preparation boundary now executes in the real viewer with no logged preparation failures. Remaining critical requirements include useful complete-path cost/reuse, alpha mapping, tile-local lights, useful worker/visibility/command routes, physical working-set qualification and whole-viewer behavior/performance acceptance. PPD319 remains investigating/reproduced; PPD356/357 source remedies remain ready_for_owner, not closed by this run. Discuss this checkpoint before another implementation group. Club loading/settlement was not recorded in this Home-only run. No push is authorized.
