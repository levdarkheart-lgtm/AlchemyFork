# Native occlusion/anisotropy attended performance evidence — 2026-11-08

This checkpoint records bounded M1 Max evidence after native visibility-result
queries and live anisotropy preference propagation. It corrects the next-pass
priority inferred from the earlier base-light capture. It is not the formal
paired 300-frame Metal/OpenGL performance gate.

## Candidate provenance and live state

- Branch/HEAD: `codex/apple-silicon-metal` at `876970a97820`.
- Renderer changes under test: `e52de13acc` (native Metal occlusion queries)
  and `f799262971` (live native anisotropy preference).
- App: Xcode Release `26.4.0.68521` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
- Executable SHA-256:
  `a527489ee69388abd6c34003e5d22632190807d7b8d95b589087f72b63a04353`.
- Drawable: `3456x2168`; physical M1 Max, 64 GiB; AC power; thermal state
  remained Nominal.
- Live location: `Spare Time (105,21,24.21459)`, global
  `(261481,241429,24.21459)`.
- Ultra: quality `6`, 256 m draw distance, shadows `2`, SSAO enabled,
  reflection probes level `3`/detail `1`/count `256`, mirrors enabled at 1024
  and update rate `1`, fixed exposure `1.0`, ACES mix `0.7`, no SSR, no DoF,
  sharpening `0`.
- Extended post-capture LEAP verification confirmed
  `RenderAnisotropicLevel=4` and `UseOcclusion=1`. The local profile and run
  manifest were amended to pin these controls after launch, so this is live
  verification rather than an original launch-gate assertion.

The raw trace directory is local-only because it contains private live-scene
data:
`/private/tmp/alchemy-metal-68521-post-fixes-ultra-20261108-1`.

## Settled result and bounded predecessor comparison

The first 30.835-second Metal System Trace began roughly three minutes after
launch and recorded 220 presents (`7.135` traced presents/s). Continued
unprofiled observation then showed a decline, so that trace is not treated as
steady state.

A repeat 30.695-second trace after longer residency recorded 163 presents
(`5.310` traced presents/s), 6,315 command-buffer submissions
(`38.74`/present), `77.0%` GPU-active union, 4.778–4.910 GiB Metal allocation
and Nominal thermals. Fifty-four subsequent ten-second viewer-log samples
covered more than 300 frames and had median `6.35` FPS, mean `6.322`, and range
`6.1–6.5`.

The closest immediate predecessor is working build `26.4.0.68515`, executable
SHA-256
`380759049e35d78f8314db4b4fed3645c8588919c8dd61c8b61a72c25f02c10f`.
It already contained base-light depth classification but predated both native
occlusion and live anisotropy. At the same `3456x2168` drawable, world position
within 0.000027 m and the same 19 common graphics controls, 21 settled log
samples had median `5.9` FPS, mean `5.833`, and range `5.6–5.9`.

The current median is therefore 7.63% higher and the mean 8.38% higher. This is
bounded directional evidence only: neither run captured camera transform,
the live scene is nondeterministic, these are ten-second viewer-log aggregates
rather than consecutive-frame CSV, and the predecessor was not retained as a
final source-exact release artifact. No isolated gain is assigned to either
occlusion or anisotropy, and no formal product performance claim follows.

## CPU attribution

A 30.718-second Time Profiler capture attributed 24,353 ms of running main
thread samples. Inclusive categories overlap and do not sum:

| Inclusive main-thread category | Sampled time | Share |
| --- | ---: | ---: |
| Sun/shadow generation | 13,733 ms | 56.4% |
| Native spot-shadow encoding | 8,519 ms | 35.0% |
| Hero/reflection-probe work | 7,822 ms | 32.1% |
| Main G-buffer capture | 4,414 ms | 18.1% |
| Cull traversal | 1,563 ms | 6.4% |
| State sort | 600 ms | 2.5% |
| Post sort | 585 ms | 2.4% |
| Occlusion-query mechanics | 104 ms | 0.4% |

The native query mechanism is not a material CPU bottleneck in this capture.
`UseOcclusion=1` and native support were live, but the run did not expose
issued/ready/culled counters, so it does not prove how many groups were
actually rejected or quantify query benefit. Visual popping was not reported
during the attended check, but a dedicated cull-counter/popping gate remains
open.

## Fully resident GPU attribution and priority correction

In the settled repeat, labelled main-view GPU channel time per present was:

| Main-view labelled channel | Time/present |
| --- | ---: |
| Opaque post-deferred | 55.307 ms |
| Atmospheric haze | 17.544 ms |
| Depth-classified base light | 17.267 ms |
| Light map | 5.365 ms |
| Projector light | 3.864 ms |
| Local light | 3.566 ms |
| SSAO horizontal / vertical | 3.014 / 2.966 ms |

Channel intervals can overlap and are not whole-frame time. Submission-table
encoder duration likewise is not GPU frame time; it nevertheless identifies
31.715 ms/present across 7.583 direct spot-shadow slice submissions/present,
14.219 ms/present for main ordered-sky G-buffer work, and exactly six hero
face transactions/present at 12.004 ms/present.

The earlier `68515` GPU trace began only about 32 seconds after launch. It
recorded haze at 7.883 ms/present, base light at 2.688 ms/present and opaque
post-deferred at only 0.579 ms/present. The current fully resident run's
approximately 55 ms/present opaque post-deferred work proves that the earlier
trace did not contain the final streamed workload. The previous conclusion
that haze and base light were necessarily the next largest targets is therefore
withdrawn for prioritization. Source-trace the original viewer's opaque
post-deferred behavior first; haze and base lighting remain significant but
secondary in this resident capture.

The native visibility-query code is encoded in the main G-buffer pass with
read-only depth and disabled color writes, not in opaque post-deferred, so the
large opaque-post difference is not explained by attaching query draws to that
channel.

## Evidence hashes

| Export | SHA-256 |
| --- | --- |
| `gpu-intervals.xml` | `c5127960a3e74c1609721bc535e64d7a166a9bd2815f522e92cc6b23f1d4a1db` |
| `presents.xml` | `007d43664a38bdc557363c99520b6a7ec8dd0cbbb8f7144bb224f59e68eceb7c` |
| `submissions.json` | `810281fe034d49ed854d1804741723f200873bc3d22bc5c51839beaa1964bd30` |
| `time-profile.xml` | `1e8508bb8f276da354c55c6a08260b1ad36b5b3a3d4458b593da2f46ba829209` |
| `gpu-intervals-repeat.xml` | `0d9d633c975fb8652cd66b118d32de71cdd81457e01a33ec501751e0014bfc8f` |
| `presents-repeat.xml` | `1ca7a0e7d057d36578f9532102009418d8f8d1b18ca903de628934f8766b4da7` |
| `submissions-repeat.json` | `0a79a8dfd2a549537e8e363e08bc6ba24a1f0bb0f834d78948fc3fec0f7b5e52` |
| `leap-final.json` | `9709203f754b7d4d1c3542db4acc85e35e926f932b1dca95953ab341e9d413c5` |
| `viewer-final.log` | `652b9032d8d432daeec7069f8127d96bd8b9df5cdb89c95ecabb26f55b1e5b19` |

## Next executable action

Trace the original viewer's fully resident opaque post-deferred pass and match
its content, state and ordering before changing Metal. Instrument native query
issued/ready/culled counts for a separate dense-scene correctness gate. Then
run a source-exact, camera-pinned, quality-equivalent 300-frame Metal/OpenGL
comparison; the 24 FPS objective remains open.
