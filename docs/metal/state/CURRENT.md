# Current engineering state — classic-avatar and music repairs

**Observed:** 2026-11-10, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `2c60e47b0eac61f3a62b504163504516d1729bed`.
All previous renderer/HUD/loading/UI/environment/performance repairs retained.
Earlier20FPS campaign stays paused. Translator stopped; Xcode replay stopped.
Owner closed68638. At the owner’s next explicit request, exact68643/hash was
launched viaCUA, PID48817; login window/title observed. No login performed.
No account/cache/settings/camera/region changes during this launch.

## Confirmed avatar contract repaired

[Shader repair and actual GPU proof](../work/active/classic-avatar-position-w.md).
Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` declares classic-avatar
positions vec3 and explicitly constructs vec4(XYZ,1). Poly-mesh loading/morph/VBO
copying never guarantees stored SIMD padding W1. Native shared skin/cloth
helpers instead consumed raw float4 padding, omitting or scaling translations.
Repair restores W1 at those two helper entries, covering main opaque/mask,
forward alpha, cloth and preview consumers. Rigid/spot-shadow already normalized.
No object-skin change, palette clamp, hidden geometry or occlusion workaround.

Owner screenshots show skin/head fans originating from one avatar with coherent
clothing. This is a confirmed matching shader defect, but not proof that every
live attachment/loading artifact shares its cause. The previous68638 live result
remains [failed](../work/active/travel-live-68638.md), now preserved in the
[indexed archive](archive/CURRENT-continuous-avatar-corruption-68638-2026-11-10.md).

Actual M1 Max Release API+shader-validation tests:

| Check | Result |
| --- | --- |
| Same classic pose fixture with exact previous68638 metallib | exit42;256 assertions,10 failures |
| Repaired shader, independent original-style CPU affine oracle | exit0;256 assertions/1case |
| Existing four-MRT world/avatar-impostor regressions | exit0;48568 assertions/2cases |

Pose fixture covers indexed geometry,15 distinct joint affine transforms,
fractional endpoint weights, W0/arbitrary padding, all4 MRTs/rawD32, both Z modes
and nonzero cloth wind. Initial fixture used wrong shadow target type; corrected
reruns above are decisive. Independent source reviews approved shader/test.
Potential dirty-VBO publication/fence seams remain private/unapplied, not proved
live causes; do not silently integrate them into this narrow repair.

## Parcel music repaired; camera and voice audited

[Music host repair](../work/active/parcel-music-host-repair.md). Actual68638 music
requests reached LibVLC child then exited1. Its inherited ./plugins path does not
match nested packaged modules. Exact-library A/B: old relative path gives NULL
initialization; canonical absolute module path succeeds. Repair resolves native
executable location, validates modules, reports init/play failures and guards
cleanup. Original settings, volume/backend/arguments retained.
Actual rebuilt dedicated host passed local3s silent WAV playback, volume0:
hello/init→Playing→13 time updates→Done→normal exit0, empty stderr, no audio-output
errors or parent signals. Remote parcel/audible playback remains unverified.

[Camera and voice source audit](../work/active/teleport-camera-voice-audit.md):
teleport/reset/interpolation handlers match original; no guessed camera change.
Native retained world imagery can produce held-frame/jump behavior; continuous
fly-in cause is not proved. Voice client/channel/WebRTC/ADM/device contracts and
permission/package requirements match original. ARM64 in-process WebRTC and
microphone entitlement are present in68643. No voice-specific incorrect port
found; microphone/TCC, actual calls and audible/spatial voice are untested.

## Exact build and evidence

Canonical Release **26.4.0.68643** built from checkpoint2c60e47 with clean source.
Doctor/configure/build/verify_app/deep strict codesign all exit0. ARM64 inspection
checked349 Mach-O files, zero invalid slices. Root ran build/GPU/audio checks;
peers source-reviewed. This is not an independent fresh-checkout release build.
App: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256: `421fc603583e07f54a397baa5d38d51c814c18303bfd13d3a92fba5f13170790`.
Metallib SHA256: `52d5aec4853511526cc5198f077fdb289d566c602271464f57c3f6174314e36c`.
Ignored evidence: `artifacts/metal/private-classic-avatar-position-w-20261110-01/`.
Its final-build-identity.json pins exact source, artifacts and command receipts;
vlc-real-host/result.json pins compiled playback host and protocol observations.
No private logs/screenshots/URLs/cache uploaded. Code commits29d0b148e7,
d497d2122e and2c60e47b0e are local; no new push or release claimed.

## Exact next actions

68643 is now running at login for the owner. Next live acceptance must
verify the previously identified avatar and outfit/loading/teleport geometry,
HUD transparency, UI/movement/resize responsiveness, names and real parcel music.
Capture exact-build logs; bounded backend success does not close those symptoms.
If camera fly-in persists, correlate actual camera pose with published world
source-frame/region before changing original interpolation policy. Voice needs
an attended functional test before parity is claimed. No current source proof
justifies voice or camera changes. Do not resume the pausedFPS campaign.

## Complete-product advancement assessment

| Milestone | State | Evidence /remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Original avatar shader contract repaired with failing-old/passing-new GPU proof; live parity remains. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new CUA phase. |
| M-014 Ultra20FPS campaign | deferred | Owner-paused; no new FPS or speedup claim. |

Prior loading/UI/terrain/impostor/name repairs remain included; see
[correctness report](../work/active/travel-correctness-repair.md). Full sky/interior/
shadow/low-DD parity, crowd ART, original loading-sphere producer, non1-scale
final-noise coordinates and prolonged stability remain open. Cold cache backup
8978files/1703059096bytes remains private/intact; do not restore over active cache.
[Paused campaign](archive/CURRENT-20fps-owner-stop-2026-11-10.md): last controlled
current-source9.745FPS;20FPS unmet, no hardware ceiling proved. Full parity,
multi-scene performance, strict no-GL and independent build/review evidence remain
open release gates. Product is not complete.
