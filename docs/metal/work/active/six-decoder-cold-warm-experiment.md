# Six-decoder cold/warm loading experiment

2026-11-12. Owner explicitly requests six decoders, a built/launched viewer,
an empty-cache busy-scene profile, then another profile using the populated
cache. Base source `a6b29f2e6d3b00baab056d6e3e9945481cfae964`, local branch
`codex/apple-silicon-metal`. This scope is local; no push approval.

The preceding [retained-cache analysis](../../research/warm-cache-profile-analysis-2026-11-12.md)
finds nearly four busy decoder cores initially, but no per-texture loading
latency census. The owner wants to test a higher static worker count before
adaptive concurrency. This is a controlled setting candidate, not an
implemented adaptive burst policy or proof that six is optimal.

## Minimal change and retained behavior

`LLAppViewer::initThreads` changes the existing reserve from six to four cores.
Detected/effective ten-core M1 Max therefore selects six ImageDecode workers.
Hardware-count handling, EmulateCoreCount cap, 2–16 worker bounds, ThreadPool
ownership, request/callback semantics and OpenHTJ2K's one inline lane per image
remain intact. Other hardware can select a different count under the formula.
No renderer, codec, upload-worker or graphics-setting change.

The four-worker Projector Fix68869 and owner-accepted OpenHTJ2K68860 apps must
remain preserved. Build a separate `Alchemy Metal Six Decoders` channel.
Use a new dedicated cache with session-only CacheLocation/NewCacheLocation
overrides set to the same path, avoiding migration/purge of the existing cache.
Keep this cache for the later warm run; do not purge it between those runs.

## Verification and next action

Doctor passes on Apple M1 Max/64GiB/macOS26.5.2/Xcode26.6. Build, core-count
admission checks, ARM64/signature validation and exact launch evidence are next.
Local evidence root: `artifacts/metal/six-decoders-20261112/`.
Confirm runtime six-worker selection and actual new cache directory before
profiling. Owner controls login and location. Start recording only on the
owner's signal, before login/teleport loads the target scene. Capture live
settings, request/arrival timestamps and source/PID/hash provenance.

An empty application cache at launch does not remain empty after startup/login.
The target busy place should be first visited during recording for its cold
arrival. The later run reuses that populated cache. Earlier four-worker traces
used different cache/arrival phases; isolating a worker-count speedup requires
matched conditions. No faster loading or owner acceptance claimed in advance.
