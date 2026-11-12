# Local OpenHTJ2K viewer experiment — 2026-11-12

Owner requests a quick OpenHTJ2K integration, build and exact candidate launch.
Owner controls login and teleport, and will explicitly signal when recording
should start just before teleporting to the busy location. Do not record before
that signal. The owner accepted the experiment and explicitly approved committing/
pushing the decoder scope on2026-11-12. Binaries and captures remain local.
This follows PPD-005 and the [codec comparison](texture-decoder-workers-and-codecs.md).
The accepted implementation remains an explicit opt-in experiment.

## Original mapping before implementation

Reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; current base4ec6579592.
`LLTextureFetch` queues `LLImageJ2C`/`LLImageJ2COJ` decode through ImageDecode.
Preserve fresh per-call decode, partial-byte limits, safe reduction, channel
slices including baked auxiliary channels4+, bottom-up interleaved U8 output,
the raw/base image locks and failed-decode lifecycle. Encoding, metadata and
codestream layout remain OpenJPEG. Four outer workers remain from160a5f45bb;
each OpenHTJ2K decoder uses one inline execution lane.

The original and current texture cache use the engine label as an encoder tag;
a label change purges the disk cache and >31-byte tags are invalid. Separate
cache compatibility identity from decoder display before changing the label.
Keep the exact existing OpenJPEG encoder tag, cache1.71/address64 and cached
compressed data/previews. This experiment must not create a cold disk cache.

## Implementation and validation

Use pinned OpenHTJ2K0.27.0/72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2,
BSD-3-Clause, from the already built local ARM64/NEON Release static archive.
An explicit compile-time experiment opt-in selects OpenHTJ2K decoding; no
runtime OpenJPEG decode fallback. Keep OpenJPEG encoding/header metadata.
Stage the full dependency license through existing attribution machinery.

Actual integrated `decodeImpl` matches the earlier OpenHTJ2K output for all960
real SL cases (768complete/192partial). Synthetic real-J2K inputs pass94,488
checks across1–8 components, primary/aux slices, signed/unsigned8/16-bit data,
Y flip, reduction clamp, byte limits and errors. The actual `LLImageJ2C` tests
pass all five registered CTest groups, including existing progressive quality
and new56-assertion/three-case identity/channel/failure coverage. Initial
registration omitted the new source from CTest's source list, causing only its
stray-case guard to fail; corrected registration and rerun pass5/5.

Run actual `llimage_tests`, including progressive-prefix quality tests, focused
channel/lifecycle checks, doctor, full Release build and app/signature checks.
Preserve the worker-only68855 candidate as an explicit reference. Launch the
new exact app path/hash only after successful checks. Owner manages location
and graphics; no preset, automatic login or teleport is inferred. Prepare
Instruments/LEAP readiness but wait for the owner's recording signal.

Evidence directory: `artifacts/metal/openhtj2k-experiment-20261112/`.

## Observed build, launch and recording

Source db398cc3a0, local26.4.0.68860. Full build,346 ARM64 files/zero invalid,
deep strict signature, doctor and production source audit0blockers/errors pass.
Exact app: `build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal OpenHTJ2K Experiment.app`.
Executable SHA256 `6df603e39c6505634594f123f150cf5f1dde717da6f811fc2bf0788358b34807`;
PID67814. Startup four-worker/decoder identity and actual decode receipt
observed. Cache tag/format remain unchanged;68855 reference hash preserved.

Owner signaled “profile now”;181.755495s Game Performance Overview saved,
TOC/CPU/metric exports and analysis exit0. Recording is finished; viewer remains
owner-controlled. No push/upload. [Profile findings and limitations](../../research/openhtj2k-profile-2026-11-12.md):
four actual OpenHTJ2K workers70.591 sampled CPU seconds; shadows46.27% of main
CPU; about41.76 presentedFPS over covered intervals. The teleport completed
34s before recording, and shadow controls differ from the prior capture;
therefore no teleport-transition or causal codec-speedup claim.

## Owner acceptance and scoped publication

On2026-11-12 the owner accepted the experiment and explicitly requested commit
and push. Record PPD-005 as owner-passed with scoped push approval. Publish the
worker fix, codec benchmark/tools and OpenHTJ2K implementation/evidence on
`codex/apple-silicon-metal`, based on the remote branch before unrelated local
shadow changes. Keep those renderer changes and raw/private artifacts local.
The published feature stays opt-in and requires the pinned local dependency;
acceptance does not establish whole-product release readiness or a causal FPS gain.
