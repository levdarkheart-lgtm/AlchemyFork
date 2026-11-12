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

Doctor/configure/build/ARM64/signature/strict source checks pass on Apple
M1 Max/64GiB/macOS26.5.2/Xcode26.6. Build68876 fromc5c57a80bd, SHA256
`6558ad34136c5deac313a17c0025503d230296d0b6c5c97d19d38770b72dd0d8`.
Six-worker selection observed; actual-body sizing probes pass16cases each for
accepted/current policies. Prior apps remain byte-identical. Local evidence
root: `artifacts/metal/six-decoders-20261112/`; validation.json records commands
and exits, including a corrected documentation-harness failure.

First [cold login analysis](../../research/six-decoder-cold-profile-2026-11-12.md):
PID8113, verified empty dedicated cache,182.128547s normal capture; six workers
use184.944CPU-s, first5s after STATE_STARTED average5.33cores. No controlled
four-to-six speedup or measured time-to-sharpness. Preserve this trace/cache.
The owner felt loading was quick and requested another run at a never-visited
login location. New independent `cold-unvisited-171832/isolated-cache` contained
zero files/bytes before launch; PID10691 is the same exact68876 binary, six
workers and active cache path verified. Unsupported observer query and startup
cache-initialization timing corrected in this launcher's private copy.
Owner explicitly requested recording now; xctracePID10817 saved normally(exit0).
The [second cold analysis](../../research/six-decoder-unvisited-cold-profile-2026-11-12.md)
covers181.216215s,17:19:32.032–17:22:33.248Z, with world arrival+7.968s and
STATE_STARTED+14.968s. Six workers use62.173CPU-s; peak5s averages2.425cores,
last30s has no sampled decoding. World cadence45.031FPS; shadows40.66% of
post-started main CPU. This scene is lighter; no worker-count speedup inferred.
TOC/CPU/sixmetrics exports and analyses pass. Live snapshots are late-only
because a collector timestamp-field mismatch was corrected; full limitations
and exact commands/hashes are in the report/private receipts.

Next: owner-coordinated warm repeat with this second run's same populated cache
and exact68876binary, after the owner finishes the current logged-in session.
Private `warm-unvisited-plan.json` pins both paths and requires no concurrent
session or purge. `leap_probe_history.py` records future live history directly
to avoid sampler-field/timing gaps; syntax compiled, not yet run. No warm run,
owner acceptance of six workers or push. Prior cache/trace remain preserved.

An empty application cache at launch does not remain empty after startup/login.
The target busy place should be first visited during recording for its cold
arrival. The later run reuses that populated cache. Earlier four-worker traces
used different cache/arrival phases; isolating a worker-count speedup requires
matched conditions. No faster loading or owner acceptance claimed in advance.
