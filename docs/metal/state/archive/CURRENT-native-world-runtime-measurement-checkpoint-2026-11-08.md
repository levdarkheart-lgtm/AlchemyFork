# Native world resource/runtime measurement checkpoint — 2026-11-08

This checkpoint records bounded owner-attended M1 Max runtime evidence for the
source-exact Release `68532` renderer. It covers the combined changes through
`8f4d5654e5`: vertex/index generations, bounded ordinary point lights, direct
retained shadow slices, retained G-buffer borrowing, shared sequential haze
depth, eight-slot material ladders and inline skin palettes. It is not an
isolated benchmark for any one change and is not the formal paired OpenGL gate.

## Provenance and live state

- Branch/HEAD while measuring: `codex/apple-silicon-metal` at documentation
  commit `b669269800`; renderer artifact source commit `8f4d5654e5`.
- App: Xcode Release `26.4.0.68532` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
- Executable SHA-256:
  `f242b041ebcba41cdc07a003b4fa040321c87368190b8aa192c803655953e9f9`.
- Physical MacBookPro18,2, M1 Max, 64 GiB, Metal Apple family 7; AC power;
  `pmset` and both trace tables recorded no thermal/performance warning and
  Nominal thermal state.
- Drawable: `3456x2168`; location `Spare Time (105,21,24.21459)`, global
  `(261481,241429,24.21459)`.
- Fresh LEAP sampling after the captures verified Ultra quality `6`, 256 m,
  shadows `2`, SSAO on, probes level `3`/detail `1`/count `256`, mirrors on at
  1024/every frame, anisotropy `4`, occlusion `1`, fixed exposure `1.0`, ACES
  mix `0.7`, no SSR/DoF and sharpening `0`.
- The owner changed this live process from its launch-time Mid manifest to
  Ultra and fixed the camera. Therefore the original visual-QA gate correctly
  remained blocked and no agent screenshot or formal visual conclusion was
  produced. The extended LEAP manifest is bounded runtime evidence only.

Raw evidence is local-only because it contains live-scene data:
`/private/tmp/alchemy-metal-68532-resource-churn-ultra-20261108-1`.

## Settled trace and untraced rate

The first trace began soon after the Ultra transition and recorded 189 presents
in 30.861 seconds (`6.124` traced presents/s). Untraced output then recovered,
so a repeat was captured after six further minutes without moving the camera.
The repeat is the settled measurement:

| Metric | Prior settled `68521` | Settled `68532` repeat |
| --- | ---: | ---: |
| Trace duration | 30.695 s | 30.697 s |
| Presents | 163 | 265 |
| Traced presents/s | 5.310 | 8.633 |
| Submissions/present | 38.742 | 31.702 |
| GPU-active union | 77.0% | 80.5% |
| Metal allocation | 4.778–4.910 GiB | 4.661–4.795 GiB |
| Thermal state | Nominal | Nominal |

The candidate's allocation floor/ceiling are about 120/117 MiB lower and its
submission rate is 18.2% lower. Thirty-seven untraced ten-second log samples
between the profilers and repeat trace have median `9.9`, mean `9.746`, range
`7.2–10.5` FPS (roughly 3,606 aggregate frames). These are ten-second
aggregates, not the required consecutive per-frame CSV.

Do not interpret the 62.6% traced-present or 55.9% log-median differences as a
product speedup. The prior run did not preserve its camera transform, the scene
is live, the candidate contains multiple changes, and current main opaque-post
GPU work is `1.386` versus the prior trace's `55.307` ms/present. That large
content/state difference makes end-to-end cross-build attribution inadmissible.

## Structural evidence for the fixes

The shadow workload did not become easier: direct spot-shadow renders increased
from `7.583` to `8.211` per present and hero face transactions remained six per
present. Nevertheless:

| Structural channel | `68521` | `68532` repeat |
| --- | ---: | ---: |
| Shadow-retention copy submissions/present | 7.589 | 0 |
| Shadow-retention labelled GPU ms/present | 8.002 | 0 |
| Direct spot-shadow submission encoder ms/present | 31.715 | 23.583 |
| Hero transaction encoder ms/present | 12.004 | 10.750 |
| Main G-buffer/sky encoder ms/present | 14.219 | 10.780 |

The complete disappearance of the retention-copy stream is direct runtime
evidence for `f93c9f1927`. Its former `7.589` submissions/present account for
nearly all of the total submission reduction. The roughly 117–120 MiB lower
Metal allocation is directionally consistent with direct retained slices,
compatible G-buffer borrowing and shared sequential haze depth, but live-scene
resource variation prevents assigning exact bytes to one owner.

The 30.7-second Time Profiler sampled 19,947 ms of running main-thread work.
Sun/shadow generation was 10,168 ms (51.0%), direct spot-shadow encoding 6,215
ms (31.2%), hero/reflection work approximately 7,401 ms (37.1%, inclusive),
and the native G-buffer renderer 3,478 ms (17.4%). Standard
`setFragmentTexture` wrapper stacks fell from 4,885/24,353 ms (20.1%) in the
prior capture to 3,305/19,947 ms (16.6%); sampled native device buffer creation
fell from 9 to 2 ms. Inclusive categories overlap, scene work differs and this
does not isolate the eight-slot ladder or inline palettes, but the direction is
consistent with the source-level reduction.

Settled main GPU-labelled channels were haze `16.901`, base light `17.847`,
light map `5.425`, projector `4.255`, local point lights `3.279`, and SSAO
horizontal/vertical `3.117/3.131` ms per present. Labelled intervals overlap
and are not whole-frame GPU time. The opaque-post mismatch above means these
must not be ranked against `68521` as though camera/content were identical.

No Metal validation, stale-batch, incompatible-target, assertion, fatal or
out-of-memory error appeared after the Ultra transition; exact native world/UI
publication continued. Repeated HTTP asset timeouts are present and remain a
live-service/content limitation rather than renderer proof.

## Evidence hashes

| Export | SHA-256 |
| --- | --- |
| `presents-repeat.xml` | `d087d8155fc7a9ba1af205e4a552605ef0de357498be3c8258127360c4b6d53a` |
| `gpu-intervals-repeat.xml` | `eab6e18393f0f2b5f7cb537dcb2cfdb2a05bd2c9f5b5705544db8f11ec5f97c3` |
| `allocated-repeat.xml` | `c4884f8065c67fffdd27bea4f571d082c4bf6f07eaa1344d4296a2fcabbb1e09` |
| `gpu-state-repeat.xml` | `822053e75d6fea84129bdd3950378d7f6759dfa2a87571c325057c010d35afdd` |
| `thermal-repeat.xml` | `5b06030041850c6866595062a9b1d988f4f1beb17cafb5ee962cfc2ae7f0e3e1` |
| `submissions-repeat.json` | `0a9e60a52f82654c1fe01f9f46db09cd3eeecc24a6253a091115981e9d6b9f5f` |
| `time-profile.xml` | `4e2baa124003c49581c95dbe44f988f8cd21ed83711a5966b6a3a94b513aed3f` |
| `leap-final.json` | `7007e611f2dcb7b78186775281acc7ab96bd4dcc170645d820aea2bd366c46ec` |
| `viewer-snapshot.log` | `d51c3ec9873a5c8280e508dd310d2a81a2585aad4ae3467578dec3be160b5ba8` |
| `measurement-summary.json` | `31b38b5d48fbb93f4b83623d5f32a5a979e3cddd369afdce4d7c676e245ce9fb` |

## Next executable action

Save and launch a dedicated Ultra profile from the outset, capture the camera
transform, and repeat the same source-exact protocol against exact-base OpenGL
`67607`. Add a separate owner-attended visual smoke for ordinary-light camera
boundaries and moving directional/projector shadows. Only then may the current
directional gains enter the formal 300-frame quality-equivalent comparison.
