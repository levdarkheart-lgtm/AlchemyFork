# Accepted decoder changes — 2026-11-12

The owner tested the local OpenHTJ2K experiment, accepted it and explicitly
requested committing and pushing these changes to `codex/apple-silicon-metal`.
Approved scope: Apple Silicon decode-worker sizing, the CPU texture benchmark
and tests, OpenHTJ2K decoder integration, unchanged disk-cache identity, focused
regression tests, dependency attribution and sanitized measurements.

Publication is based on remote `0f7b488264fc60e177e30d445502c8008d4fd37a`.
Unrelated unapproved local shadow-renderer changes and governance/research
commits are excluded. The decoder implementation files match the owner-tested
`db398cc3a0` candidate byte-for-byte. The tested full viewer included the then-local
renderer; no identical-whole-tree or causal FPS comparison is claimed for this
publication base. A separate source checkout validates the published image
integration and harness without replacing the owner's running app.

## Accepted evidence

- Local viewer26.4.0.68860, ARM64/Release, four ImageDecode workers using
  OpenHTJ2K0.27.0. Full local build,346-file ARM64/signature checks and736 image
  assertions/21 cases pass. Integrated actual-body corpus/synthetic checks are
  in the [implementation note](openhtj2k-local-experiment.md).
- [Controlled CPU corpus benchmark](texture-decoder-workers-and-codecs.md):
 192 complete and48 partial actual SL textures. Correcting two to four workers
  reduces isolated OpenJPEG batch wall time46–48%; OpenHTJ2K reduces another
 13–20% at equal four-worker concurrency. These are not viewer loading times.
- [Owner-triggered profile](../../research/openhtj2k-profile-2026-11-12.md):
 181.755495s,70.591 summed decode-worker CPU seconds, about41.76 presentedFPS
  over available intervals. Recording began34s after arrival; controls/workload
  differ from earlier captures, so no several-fold codec/FPS speedup is established.
- Raw cache data, account/location metadata, captures, binaries and private
  receipts remain ignored/local; only sanitized source and notes are published.

## Enabling the accepted experiment

The accepted integration remains opt-in: `AL_EXPERIMENTAL_OPENHTJ2K=ON`.
The normal build keeps its existing decoder when the option is off. OpenHTJ2K
is statically linked for decoding; OpenJPEG still encodes and parses metadata.
The separate cache compatibility identity preserves existing JPEG2000 cache
bytes and tiny decoded previews across codec changes.

This local experiment imports the exact benchmarked ARM64 Release archive.
It does not download a dependency or commit a binary. Supply explicit absolute
`AL_OPENHTJ2K_SOURCE_DIR` and `AL_OPENHTJ2K_LIBRARY` paths. The source must be
OpenHTJ2K0.27.0 revision `72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2`, with clean
public headers; the archive SHA256 is
`43190158ea345322dcdc89175d5ec36e8212933fdf83236b3f8fb668dd225182`.

The tested dependency was built with these options:

```sh
cmake -S <OpenHTJ2K-source> -B <OpenHTJ2K-build> -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DBUILD_SHARED_LIBS=OFF -DOPENHTJ2K_BUILD_APPS=OFF \
  -DOPENHTJ2K_BUILD_TESTS=OFF -DOPENHTJ2K_RTP=OFF -DOPENHTJ2K_QUIC=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_TIFF=ON -DENABLE_ARM_NEON=ON
cmake --build <OpenHTJ2K-build> --parallel 4
```

An independently rebuilt archive can have a different hash. Validating another
archive and deliberately updating the experiment's pin is a separate change;
the accepted source retains the artifact actually tested on the M1 Max. Full
BSD/ThreadPool notices stage through the existing attribution system. This is
acceptance of the bounded experiment, not whole-product release readiness.

## Publication-base validation

The isolated publication source configures successfully and builds the actual
`llimage_tests` target on the M1 Max. All five CTest groups pass (including
progressive quality, primary/auxiliary channels, identity and failure recovery),
as do all56 tooling tests, the32-file harness and diff checks. All19 changed
implementation/tool files match the accepted working source byte-for-byte.
The first temporary-worktree configure found its Dullahan submodule uninitialized;
initializing that existing dependency resolved it without a source change.
This validates the decoder integration on the publication base; the full
owner-tested app receipt remains tied to its original source/binary above.
