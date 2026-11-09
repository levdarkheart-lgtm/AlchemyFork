# Resource-aware native texture budget — 2026-11-09

Physical M1 Max / 64 GiB, macOS26.5.2, Xcode26.6. Branch
`codex/apple-silicon-metal`. This work preserves the active20FPS Ultra goal;
correct memory accounting alone is not an FPS achievement.

## Original contract and confirmed native defect

Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:

- `llwindowsdl.cpp:4474` queries total CGL video memory on Apple.
- `llviewertexture.cpp:473` uses registered textures plus deliberately doubled
  vertex allocation; automatic budget is `max(1024, detected/divisor)` MiB.
- A nonzero manual budget bypasses the divisor. Target is
  `max(768, min(budget−512, 0.8×budget))`, with independent system pressure,
  timed bias ramp, strict10% recovery headroom and background behavior.
- Registered defaults are divisor1/manual0. There is no `/16` capacity rule.

Native capacity `/16`, bounded2–8GiB, was introduced as a conversion containment
policy in `ab98aae4a9d950b1afd537100953037aa7e8fa87`, explicitly not a measured
optimum. At this host's recommendation59392MiB, capacity3712 produced target
2969.6MiB. The controller charged all-device allocation to that image target.

Apple defines recommended working set and allocated size for the overall
resource footprint, not dedicated image VRAM. This is supported by installed
`MTLDevice.h:517–522,625–628` and [Apple's property documentation](https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize).
The resource-aware formula below is this fork's policy, not an Apple preset.

## Scope repair and bounded diagnostic

Commit `3969405d3214ed55377ff96930e307ee5fc7d70e` restores independent image and
device pressure scopes. A pure evaluator combines the greater normalized
image/global overage and limiting absolute free estimate. Original GL F32
arithmetic, manual budget semantics and existing system/ramp/recovery/background
controller are preserved. A zero saved divisor clamps to1.

Focused CPU tests pass91assertions/13cases EACH Release/RelWithDebInfo. Review
confirmed the original controller from `static bool was_low` onward is unchanged.
The new numeric diagnostic samples actual operands/system_low every10seconds.
No shader code changed; app68571 retained baseline metallib8bbfe4c6… .
Doctor/configure/build/verify349ARM64/deep-strict signature all exited0.

68571 logged in full screen with Ultra/Preset123/43live controls passing. The
run ended early when the bounded accounting question was answered; there is
NO formal300frame or FPS result. Its18 numeric checkpoints show:

- Image storage1306–3053MiB against image target2969.6MiB.
- Device allocation3136.91–4875.75MiB against recommendation59392MiB.
- System-low flag0 at every checkpoint.
- First sampled image overage at11:54:56UTC:3053MiB,+2.808%, bias1.64673.
- Bias reached4 by11:56:37UTC. Image2921MiB was then only1.64% below target,
  insufficient for the original strict10% recovery condition.

Thus scope separation removes false pressure from unrelated allocations, but
the small image-capacity policy itself still drives bias in this workload.
This does not establish decode/upload churn or a particular frame-time cost.

Private whitelisted evidence:
`/private/tmp/alchemy-metal-68571-pressure-diagnostic-20261109/pressure-samples.json`,
SHA256 `5faf44449d33d812094bb7966265ca32057f20188e60c83875d9b7d9f6dc4604`;
FINDINGS SHA256 `815afacc5c19db9888e306365b222aab8828b41e06ce11ad7e5592ca5a18b407`.
PID79382 graceful Goodbye11:57:39UTC; PID absence checked,396940startup plan
restored. All temporary state from that diagnostic is cleaned up.

## Resource-aware capacity policy

Commit `46d6227526ef28850e0b306d1dcb0537c7d59f07` replaces the arbitrary share:

```text
I = raw logical LLImageGL image storage
A = all-device currentAllocatedSize
R = recommendedMaxWorkingSetSize
N = max(A − I, 0)
C = max(R − N, 0)
budget = manual != 0 ? manual : max(1024, C / max(divisor, 1))
target = max(768, min(budget − 512, 0.8 × budget))
```

All quantities are MiB. `N` is an estimate: it also reserves image padding,
unpublished/retired textures and other unaccounted resources. It is not an
exact physical non-image census. Invalid/unknown inputs produce no enlarged
automatic capacity; the original minimum target and system safeguards remain.

The controller uses unrounded image storage for `N`, then preserves rounded
F32 usage for its pressure calculation. Global pressure still compares `A`
with full `R`, independent of manual budget and divisor. Original headroom is
applied once after the residual; capacity is not derived from bias, a prior
target or free-memory output. No sticky high-water mark or second controller
was introduced. Image/device growth together leaves the non-image estimate
constant; increasing logical image accounting alone cannot manufacture recovery.

Capabilities publish `R` as a ceiling. The only runtime consumers are the
startup log and the controller; Metal uses the residual `C`, so the full
recommendation is not passed directly as the effective image allowance.
The target is an eviction threshold, not an upfront memory reservation.

Both optimized configurations pass159assertions/21pressure-capacity cases and
29assertions/3capability cases. Tests cover unknown/nonfinite/negative inputs,
non-image exhaustion, logical storage above device allocation, co-moving and
fixed-device monotonicity, strict recovery, manual/global protection and exact
legacy F32 behavior. Independent source review found no blocker. GPU tests
were not rerun for this CPU policy; no new shader correctness claim is made.

## Exact build and runtime identity

App68572 source46d622, canonical
`build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256 `84e49c91677697abf7527c93b21567950c5d525111b85ed3f3b0d189a1bbd155`;
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`,
byte-identical to the prior baseline. Doctor/configure/build/verify349ARM64/
deep-strict signature all exited0.

Logs `/private/tmp/alchemy-metal-resource-capacity-*-20261109.log` preserve:
`cmake --build build-Darwin-ninja-os-arm64 --config <config> --target newview_tests llrender_tests --parallel 4`;
`newview_tests '[texture][pressure]'`; `llrender_tests '[alrendercapabilities]'`;
`tools/metal/build.sh configure`, `tools/metal/build.sh build`,
`tools/metal/verify_app.sh <exactapp>` and
`codesign --verify --deep --strict --verbose=2 <exactapp>`.
Doctor log uses prefix `alchemy-metal-pressure-scope-doctor`.

68572 starts full screen before login; actual43controls/Ultra/Preset123 pass.
Early image storage4364MiB at bias1 grows toward7831MiB, device about9727MiB,
no sampled system-pressure flag. These are initialization checkpoints, not FPS,
full texture-residency parity or evidence of a particular cache cost.
The controlled ready300frame measurement is pending at this draft checkpoint.

## Bounded source checks during quiet measurement

A compression hypothesis was rejected by source inspection. Exact1eb already
removed generic driver compression; ordinary decoded RGB(A) uses sized
uncompressed sRGB formats. Native expands RGB into RGBA8 sRGB, while original
accounting already padded RGB8 to32bits. Authored DXT retains BC1/2/3. Neither
inspected settings table registers a runtime texture-compression control.
The unlike original tracked composite and native image/device totals do not
prove a compression discrepancy or a bandwidth cost. No change was made.

## Completed controlled68572 measurement

The pending run above completed300consecutive frames9637–9936. Every row is
source46d622/Release/Ultra6/3456×2168, exact native/submitted with zeroGL fallback.
All256assigned probes are ready/faded/current, main-visible98, stable120. All43
controls match prior native and original reference checks. Desired discard
bias is finite1 for all300rows, with no missing observations.

| Capture | Mean ms | p50 ms | p95 ms | FPS | Sampled bias |
| --- | ---: | ---: | ---: | ---: | ---: |
| Prior68566 |112.806384|110.616500|138.295291|8.864746|4 |
| Reverted trial68568 |108.776442|108.786063|113.496416|9.193167|4 |
| Resource-aware68572 |106.585321|105.948188|110.883167|9.382155|1 |
| Exact original67611 |164.054928|162.452146|185.230250|6.095519|1 |

Against68566, observed mean is5.52%lower and p9519.82%lower, but these are
single sequential windows with different texture workloads. The result does
not isolate a performance gain from either repair. Normal discard bias is
restored and substantially more image storage is retained without a lower
observed rate.20FPS remains unmet. Bias/settings agreement does not prove full
pixel/residency/geometry parity; original main-visible104 versus native98 is
still unresolved. No external scene change is assumed.

Camera counter4 was warmup history at frames3581–3584. Sample_start9637 and
sample_end9936 both report4: sampled delta0. All four sampled main/end-view
snapshots are equal; final camera exactly matches68566. Native environment,
hero/HUD restoration and43controls match. Original selected environment,
local/effective sky assets and camera-global/focus match; its numeric sun/matrix
serialization differs minutely. No scene screenshot was obtained: the fresh
QA gate passed, then CUA getScreenshot timed out15seconds after collection.
No visual pixel-parity claim is made.

At three10-second pressure checkpoints inside the formal interval, image storage
is7838MiB, target45994.1–45999.6MiB, device9730.08–9737.06MiB, recommendation
59392MiB, estimated non-image1892.53–1899.38MiB. All93pressure checkpoints through
collection show system_low0; bias remains1. These are periodic diagnostics,
not an every-frame physical-residency or system-pressure census.

Existing main-world query diagnostics show actual consumed occlusion: over299
sample_start→sample_end intervals,198468acquired/attached,168578zero-ready and
29787nonzero-ready observations,120828spatial-group skip events,0state-sort
skips, no pending/expired/untracked/duplicate polls. These are repeated source
events, not unique groups or measured saved GPU time. Earlier68566has120821
spatial skips. State-sort0 does not imply missing native culling. Original
query diagnostics are unrecorded.

Raw CSV `artifacts/metal/private-46d622-resource-ready-20261109-01.csv`, SHA256
`6934705017fe7db20f0c0e4c1ef5c867be28d033587d1edbd3a37400ec9ba75f`;
matching `.provenance.jsonl`, SHA256
`85fb936f1750f5bd6492cc5ca180e37e0bb88d2f38f61c0f40d701b36f954b9d`.
QA run `artifacts/metal/qa-runs/candidate-46d622-resource-ready-20261109-01`.
Official `perf_compare.py --require-provenance` exits0, report
`/private/tmp/alchemy-metal-resource-capacity-native-comparison-20261109.json`;
its performance target remains false and texture-quality caveat remains manual.
No profiling/GPU experiment/build overlapped the formal window.

Independent corrected report:
`/private/tmp/alchemy-metal-68572-resource-comparison-20261109/formal-resource-comparison-v2.json`,
SHA256 `3c2949480e38ccc3c6110645441a6c8d95eb76b686cbd8a53194d5f1a49c6d06`;
FINDINGS `7a50d3f5aadd09ac32d65aa865046538ca3dc4e001d7184299c50caa769205c0`.
The preserved v1 overbroad camera gate is superseded by v2's explicit sampled
counter delta and identical snapshots; it did not indicate sampled movement.

Viewer96622 graceful Goodbye12:27:46UTC; PID absence verified, temporary
46d622startup plan restored afterward. App68572 is preserved at
`/private/tmp/alchemy-metal-68572-quality-baseline.app` and remains canonical.
No viewer, test, build or profile process is pending. All user-state plans DONE.

## Further bounded contract check and remaining work

Native forward alpha has seven shader slots with at most six nearby point/spot
lights, matching exact-original selection/radius/fade/falloff/cone behavior.
No all-scene light-loop discrepancy was found. A separate non-Ultra correctness
edge was identified: disabling RenderLocalLightCount after a nearby set exists
can leave native alpha using stale lights because its producer lacks the
original enable guard. It is deferred, not an FPS attribution or current edit.

This work advances correct resource policy and removes maximum discard caused
by an arbitrary small budget. It does not meet20FPS. Full parity, multi-scene
performance, prolonged stability, strict no-GL and independent release gates
remain open. Next work needs a substantial measured renderer change; do not
restart rejected micro-arithmetic experiments or broad profiles without a
specific unanswered question. Do not discard these quality repairs to make
FPS comparisons easier.
