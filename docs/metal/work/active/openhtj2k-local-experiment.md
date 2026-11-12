# Local OpenHTJ2K viewer experiment — 2026-11-12

Owner requests a quick OpenHTJ2K integration, build and exact candidate launch.
Owner controls login and teleport, and will explicitly signal when recording
should start just before teleporting to the busy location. Do not record before
that signal. All changes, binaries and captures stay local; no push approved.
This follows PPD-005 and the [codec comparison](texture-decoder-workers-and-codecs.md),
not an owner-confirmed fix or an approved permanent codec replacement.

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
Actual viewer texture correctness, teleport stability and performance remain
unverified until the owner session and recording occur.
