# G1 busy-scene termination — PPD-090

Status: investigating. Branch `codex/apple-shadow-records-g1`, HEAD
`694e59760a6b5f9ecb675165bb66657def1a3ca3`; delivered app code
`e21fe9c21832e5af88971ae0cb4f1ec2d892c36e`, G1/68987.

Owner reports the experiment viewer died shortly after arriving at the busy
club used for earlier shadow measurements. Runtime acceptance has failed;
the earlier headless timing result does not override this failure. Goal two
remains unstarted. No agent viewer launch, login or interactive profiling.

## Observed before repair

The current AlchemyNext log ends at 2026-11-13 19:59:23 UTC with
`WorkQueueBase::callWork ImageDecode` capturing an uncaught exception, its
owner-side rethrow, and `std::terminate: No diagnostic information available.`
This establishes an image-decode exception path, not the underlying exception
type or a demonstrated relationship to the new shadow records. The same decode
implementation was retained from the baseline; attribution remains open.

The new viewer dump directory contains only `static_debug_info.log`. A macOS
IPS report arrived during investigation at20:00:44, recording the19:59:23.5246
SIGABRT. Its main stack is `on_terminate` / `__cxa_rethrow` /
`WorkQueueBase::callWork` at workqueue.cpp244 / `runUntil` / viewer idle.
Its executable UUID `8A937B22-A193-3010-990B-3EDE50377553` exactly matches the
frozen G1 binary SHA256
`278f8ea009fe71976a3c509fb1afffe635ff9d3ab135ebd6478ebfd9aec76594`.
No original worker throw stack, exception type/message, minidump or core is
available. Six other threads were decoding OpenHTJ2K; that snapshot does not
identify the throwing operation. Private raw artifacts are retained locally.

## Source and contained reproduction

The decode/scheduler/queue/callback sources are byte-identical to Alpha6 and
the G1 link uses the exact accepted OpenHT archive SHA256
`27a396b30668fc311ee461cee8349b25c8e36d12fcd5636027081603d98ec1d3`,
source `72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2`.
Inspected library parser failures use std-derived exceptions, already handled
inside `LLImageJ2COJ::decodeImpl`. Metadata still uses OpenJPEG, outside that
catch. Allocation, error-message copying and responders are also possible
escaping stages. No throwing stage has been proved for this incident.

Original reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` and the
pre-OpenHT integration retain the header/error contract: ordinary bad codec
input becomes a failed image request, while an unexpected WorkQueue exception
is forwarded to the owner. A responder can already have changed cache/request
state before throwing; blanket conversion/retry would change that contract.

The current cache was copied read-only using the existing extractor:64complete
and16partial inputs, each at discard0/1/3. Cache header hashes were unchanged.
The new hidden `[g1-decode-corpus]` fixture executes real LLImageJ2C and six
production workers, pumps/catches mainloop exceptions and associates missing
callbacks with bounded serial reruns. On this selection: **240successes,
0ordinary failures,0escaped exceptions,0missing/duplicate callbacks**,0.615s,
exit0. It does not recreate live arrival, texture-fetch responders, every
possible input/format, memory pressure or the exact offending request.

## Diagnostic change, not a claimed crash repair

`ImageRequest` now saves scalar context under its image lock and a sticky stage
before metadata, allocation, primary/aux decode, error-copy, accounting and
responder calls. The existing worker job catches only to log request/stage,
codec/size/dimensions/discard/aux, native exception type and `what()`, then
rethrows the original exception. Logging is best-effort/noexcept, makes no
fresh image calls/locks, and cannot replace the original failure. No codec,
concurrency, image-failure policy, shadow or cache setting is changed.

Actual worker tests cover a standard exception, a nonstandard marker/payload
and a responder exception: stage/type/message retained, original mainloop
rethrow retained, admission released and no duplicate callback/retry.
Narrow build and five CTest entries (worker/image/layout/scheduler/OpenHT) pass0.
Doctor passes0. No agent viewer launch or interactive profiler.

## Work and evidence

Root owns production diagnostics/CMake/registration/integration. Independent
agents preserve/symbolicate artifacts, audit source and own disjoint worker
tests/private-corpus fixture. Diagnostic tests pass; app packaging is next.
The actual crash remains unresolved and PPD-090 stays investigating.

Local private evidence directory:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd-090-g1-arrival-crash-20261113/`.

Next action: build and verify a separately staged diagnostic G1 app, then owner
retries the same arrival once. The new log is needed because the original
exception details are absent from the preserved report. Do not claim a fix or
start goal two; use the captured stage/type/input context for a contained repair.
