# Ultra 68537 runtime and Metal binding cache — 2026-11-08

This checkpoint records the owner-attended `68537` Ultra measurement, the
exact-original binding discrepancy it exposed, and source-exact repaired build
`68539`. It is bounded runtime/source/build evidence, not a formal
quality-equivalent benchmark or a claimed performance improvement for the
repair.

## Provenance and gate

- Branch: `codex/apple-silicon-metal` on the physical M1 Max acceptance host.
- Measured renderer commit:
  `16820100044d46d66ed306dd77e8f0ca14797780`.
- Measured candidate: Xcode Release `26.4.0.68537`; executable SHA-256
  `60c6e2772eff0c51f76db3a5e36782c40d9ce2e3662db9fa77d39f77e9985c6b`.
- Drawable: `3456x2168`; AC power; Nominal thermal state.
- QA run:
  `/private/tmp/alchemy-metal-68537-projector-reuse-ultra-20261108-1`.
- Measurement run:
  `/private/tmp/alchemy-metal-68537-projector-reuse-ultra-measurement-20261108-1`.
- The live gate passed immediately before tracing: PID/hash, logged-in state,
  Spare Time position `(105,21,24.2146)`, and all 21 expected Ultra/diagnostic
  controls matched. The owner confirms the scene and fixed camera matched the
  settled `68532` run. The transform itself was not machine-recorded, so this
  is strong bounded owner-attested A/B evidence rather than the formal
  performance gate.

## Observed result

| Metric | settled `68532` | `68537` | delta |
| --- | ---: | ---: | ---: |
| Trace duration | 30.697 s | 30.843 s | — |
| Presents/s | 8.633 | 5.188 | -39.91% |
| Pretrace log median FPS | 9.9 | 6.0 | -39.39% |
| Submissions/present | 31.702 | 29.781 | -6.06% |
| GPU-active union | 80.52% | 57.52% | -23.00 pp |
| Allocation range | 4.661–4.795 GiB | 4.575–4.656 GiB | lower |
| Hero-face encoder time/present | 10.750 ms | 51.567 ms | +379.69% |
| All hero labelled GPU time/present | 33.172 ms | 27.497 ms | -17.11% |

The regression is real for this owner-confirmed unchanged scene/camera, but it
does not point to higher GPU saturation. The GPU-active fraction and total
labelled hero GPU work fell while main G-buffer/sky encoder time stayed stable
at `10.674` versus `10.780` ms/present. Time Profiler instead sampled more
main-thread work: `29,054` versus `19,947` ms, with hero-manager work `15,225`
versus `5,200` ms and standard `setFragmentTexture` stacks `6,117` versus
`3,305` ms.

Projector sharing worked structurally despite the end-to-end regression. Hero
direct-spot GPU interval count fell from `27.743` to `24.063` per present,
matching the expected six faces times two directional cascades times the two
labelled stages after cube projector caster renders were removed. All hero
labelled GPU work also fell 17.11%. Therefore the projector repair must not be
reverted to chase the CPU regression.

The texture-pressure correction also behaved as designed: discard bias fell
from `4` to `3.01812`; budget-accounted texture bytes held about
`2,838–2,927 MiB` against the `2,969.6-MiB` image target while driver total
allocation held `4,665–4,753 MiB` against the `59,392-MiB` global target.
Texture-wrapper churn fell from about `84.1` creates/destroys per second to
about `40.9/41.0`. The higher resident texture identity population is observed
renderer policy/state, not evidence that the scene changed.

## Exact-original discrepancy and repair

The earlier finding about redundant per-draw binding was partly already fixed:
current G-buffer material/PBR batches bind eight base diffuse slots rather than
the former 32-slot ladder. A separate discrepancy remained. Exact original
commit `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` caches texture identity per
slot in `ALTextureSlot::bindImpl()` and sampler identity in `bindSampler()`;
unchanged bindings do not enter the driver. `ALMetalRenderEncoder` forwarded
every texture, linear-view and sampler setter to Metal without an encoder-local
identity check.

Commit `f8af790f32` adds per-render-encoder native texture and sampler caches.
It compares actual native view handles, so linear and sRGB views of one
`ALMetalTexture` remain distinct. Caches start unbound with each encoder and
never cross a render-pass boundary. Invalid-device validation and fail-closed
behavior remain. No buffer, pipeline or depth-state cache is claimed.

This is the smallest source-correct response to the measured binding stack. It
has not yet been remeasured, so no FPS or CPU reduction is attributed to it.

## Validation and repaired candidate

- `tools/metal/doctor.sh`: exit 0; M1 Max/Apple family 7 selected.
- Full real-device Metal suite: 52,870 assertions pass across 59 cases.
- Warnings-as-errors Xcode Release build: success after commit `f8af790f32`.
- Source-exact candidate: `26.4.0.68539`.
- `tools/metal/verify_app.sh`: 349 Mach-O slices, zero invalid non-ARM64 slices.
- `codesign --verify --deep --strict`: exit 0.
- Executable SHA-256:
  `427414330ab546c034536fc27227c1bad335726290bc963a1eb0087fe631a6cb`.
- Packaged Metal library SHA-256:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.

Build `68539` was launched owner-attended under the same Ultra manifest as PID
`38044`. PID/hash, Spare Time coordinates and all 21 controls passed before
each trace. The first Metal capture was rejected: discard bias was still
converging from `1` toward `2.98`, and it contained 1,062 image scale-down
submissions (`6.359`/present) versus 107 (`0.669`/present) in settled `68537`.

After bias and resource counts stabilized, the repeated 30.623-second trace
recorded 184 presents (`6.009`/s), 5,508 submissions (`29.935`/present), 56.36%
GPU-active union, 4.917–4.998 GiB allocation and Nominal thermals. Twenty
pretrace log samples had median/mean `7.1`/`7.005` FPS and range `6.2–7.4`.
Scale-down work returned to `0.598`/present, proving the repeat was outside the
rejected convergence burst.

Relative to `68537`, traced presents/s rose 15.82%, log median FPS rose 18.33%,
and submissions/present were effectively unchanged (+0.52%). Sampled standard
`setFragmentTexture` inclusive time fell from 6,117 to 4,688 ms (-23.36%);
G-buffer renderer time fell 9.69%. Hero-face, direct-spot and main-G-buffer
encoder time per present fell 21.26%, 13.57% and 19.65%. These matched
directions establish a measured partial recovery from the binding cache.

It is not a full recovery. `68539` remains 30.40% below `68532` in traced
presents/s and 28.28% below it in log median FPS. Hero labelled GPU interval
count remains structurally similar (`214.19` versus `219.24`/present), and
labelled hero GPU time is only 7.76% above `68532`, but hero-face CPU encoder
time remains 277.7% above it. Standard texture-binding samples remain 41.85%
above `68532`. The next optimization must therefore explain remaining
per-present hero/shadow CPU encoding rather than undo projector sharing or
claim that this cache closed the regression.

## Raw evidence integrity

The `68537` measurement summary SHA-256 is
`9c725f836e2cf4e7dbb4f7d794d363a8451fb365f67220f7de5cfa6cc17364f7`;
the settled `68539` summary is
`a67d1d2c0772a6edb83c429a6c6d24c36635a3c275f0576460f8a59cd1c0ccc1`.
Principal export hashes are: presents
`1ea62cf266943212bb1195488e971150bb371230d28987a9b7ff972637049cb2`,
GPU intervals
`66d7494afd67e2bddca1e5816fad5641ef61b06c087a668628e1e79452737721`,
allocation
`ce36fe31de29e657ddbfbe42f630f202d8e82a90259fb5c5fc5f0c589d68cdf6`,
submissions JSON
`0e4381a47c3cbe602e958f19742b2db918cba755ce3d87d1aa9dda3fe63feb09`,
and Time Profiler
`b09f59a6c9e4125061b7e5c2072d6776cb2383e1fa18ee76cfe011eea7df64f4`.
Settled `68539` presents/GPU-interval/allocation/GPU-state/submission/Time
Profiler hashes are respectively
`e62e6d8f785c0046460b7407a3deb48016884cbbd9da01ebe4334a352c351c77`,
`848fa761b7f977c5c7f80921a9041214d401d26ba66bcf65fcc0cbda8bcf641c`,
`b9b5d4b4a1e0727e3bda9ae7334e917a938f37a708d7ebd3c3a3fd6ee3639c72`,
`aa3c9ab570e9e7ae4baace69c920ef0003f4b923fff6b685ae17e5ad145eb901`,
`3569cc2a5c7ab4a8ea57926cbd374039295af6b655f25e8f5c3cfa7508cb743e`
and `3ed55f5d38f8939d717e06463970af8b1ac231c2242286b0e7661338d9681425`.
