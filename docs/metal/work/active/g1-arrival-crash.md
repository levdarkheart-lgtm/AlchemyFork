# G1 busy-scene termination — PPD-090

Status: investigating. Branch `codex/apple-shadow-records-g1`, HEAD
`694e59760a6b5f9ecb675165bb66657def1a3ca3`; delivered app code
`e21fe9c21832e5af88971ae0cb4f1ec2d892c36e`, G1/68987.

## Second owner crash — concrete exception ABI failure

The owner retried diagnostic68989/efd. Its new IPS matches the frozen binary
and records SIGABRT at20:23:26.8084. The new worker diagnostic identifies
primary-decode/J2C,282193bytes,1024x1024,5components,discard0,aux0. The active
thrown type is `std::runtime_error`, yet the catch-all executes rather than
`catch(const std::exception&)`. This explains both the codec failure escaping
its handler and the former uninformative terminate diagnostic.

Binary inspection found private-local `std::exception` type information in the
app alongside imported system `std::runtime_error` type information. Of192actual
linked archives, only `libllwebrtc.a` members `alaudioechobuffer.cpp.o` and
`llwebrtc.cpp.o` define the conflicting private standard exception RTTI.
The wrapper uses `-fno-rtti` for vendor subclass compatibility and the fork's
hidden visibility flags. [PPD-091](../../POST_PORT_DEFECTS.md) tracks this
source/link defect separately from the observed PPD-090 crash.

A small headless executable calling the actual echo-buffer archive member
reproduces the failure: a thrown runtime_error misses its std::exception base
catch, exit42. No fake RTTI, viewer, voice device or GPU is involved.
Visibility/compiler correction trials and an actual-library regression are
in progress before a production fix is selected.

**Repair update:** [PPD-091 correction](exception-abi-repair.md) now passes the
actual joint-library regression. Darwin llwebrtc privately matches the vendor's
no-RTTI/no-exception boundary; callers/codecs keep exceptions enabled. The
combined baseline fails3cases/SIGABRT; corrected6CTest entries pass50cases/
1,311assertions. This removes the demonstrated exception-matching failure;
full-app verification now passes for corrected fef1e756fd/G1/68991, staged in
the PPD-091 candidate directory. Owner scene retry remains pending. The older
diagnostic app/path below is historical; use the corrected app in the repair note.

The first cache extractor excludes components>4; its240passing requests did
not cover five-component bakes. A direct read-only scan found1387five-component
entries,416at1024square, but not the exact282193byte input. Cache writes occur
only after successful decode, so the throwing input may not have reached disk.
Four representative real five-component bakes are retained privately for
12requests at discard0/1/3; do not label them the exact offending asset.

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
A diagnostic recorder that deliberately throws also cannot replace the original
failure. Narrow build and five CTest entries (worker/image/layout/scheduler/OpenHT)
pass0:47cases/1,277assertions. Doctor passes0. No agent viewer launch or
interactive profiler.

Diagnostic source `efd0367ca446c4be368cbb8edd45c7cf9f662f4a`,
version26.4.0.68989, configured and full Release wrapper-built successfully
(28.085s incremental build). The initial development-bundle static check passes,
including the compiled diagnostic marker, source/version and unchanged native
renderer/media/voice/decoder configuration. Separate staging/reverification is
complete; the crashed G1/68987 and Alpha6 apps are preserved.

Verified diagnostic app:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd-090-g1-arrival-crash-20261113/diagnostic/Alchemy Metal Alpha G1.app`.
Main executable SHA256:
`1d423bd711caf6530f0a3a6520b23350f7861f0cb29467493cc10d59cad357eb`.
Build and staged static checks pass0:346ARM64-only Mach-O files, deep strict
signatures, exact code/version, native imports, retained features and compiled
diagnostic marker. All7,841regular/symlink entries match after `ditto`; the old
crashed bundle's executable hash remains unchanged. Receipts:
`diagnostic-staging-receipt.json`, `diagnostic-build-static-verification.json`,
`diagnostic/static-verification.json` and `diagnostic/bundle-content-manifest.json`
in the private evidence directory below. No viewer was launched.

## Work and evidence

Root owns production diagnostics/CMake/registration/integration. Independent
agents preserve/symbolicate artifacts, audit source and own disjoint worker
tests/private-corpus fixture. Diagnostic build/tests/staging verification pass.
The actual crash remains unresolved and PPD-090 stays investigating.

Local private evidence directory:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd-090-g1-arrival-crash-20261113/`.

Next action: owner retries the same arrival with the **corrected G1/68991** in
the [ABI repair note](exception-abi-repair.md), then verify stability/ordinary
decoder errors. The diagnostic retry supplied the evidence used for this repair;
do not reuse the older68989 diagnostic app as the current candidate. No goal two.
