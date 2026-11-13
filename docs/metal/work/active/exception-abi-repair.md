# Standard exception ABI repair — PPD-091 / PPD-090

Status: corrected app built, staged and verified; ready for owner scene retry.
Branch `codex/apple-shadow-records-g1`. No viewer/audio-device launch, interactive
profiling, goal two or push. Preserve original G1/68987 and diagnostic68989 apps.

## Proved failure

Owner diagnostic retry20:23:26.8084 confirms J2C primary decode of a1024x1024
five-component input. Its active type is std::runtime_error but only catch-all
matches. That is not an ordinary malformed-texture result: its intended codec
handler is bypassed, and WorkQueue rethrows on mainloop to abort.

The executable contains private-local std::exception typeinfo while importing
std::runtime_error typeinfo from system libc++. A scan of192linked archives
identifies only two members of `libllwebrtc.a` as the source of the conflicting
standard exception RTTI: `alaudioechobuffer.cpp.o` and `llwebrtc.cpp.o`.

The wrapper uses `-fno-rtti` for vendor subclass compatibility. Apple Clang can
emit private exception typeinfo from such a translation unit when exceptions
are enabled. Changing visibility alone does not fix this. Enabling RTTI either
normally or with a tested cc1 option requires vendor class RTTI that does not
exist, so neither is an acceptable link fix.

Upstream reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` has the same
wrapper no-RTTI requirement. Its WorkQueue and codec contract is unchanged:
ordinary image failures are handled; unexpected exceptions are forwarded.

## Scoped correction

On Darwin only, compile the three llwebrtc wrapper sources with
`-fno-rtti -fno-exceptions`, PRIVATE to that target. This matches the actual
vendor boundary: generated WebRTC `obj/pc/audio_track.ninja` cflags contain
both options; its GN defaults enable no_exceptions/no_rtti and the vcpkg port
selects system libc++. Proof paths/hashes/excerpts are in the private receipt.

No voice code, codec, scheduling, shadow, cache or quality setting changes.
The wrapper has no explicit try/catch/throw statements. Callers, image codecs
and tests retain normal exception handling. This removes the conflicting type
information at its producer, without adding a catch-all conversion or RTTI shim.

Boundary limitation: allocation/container contract failures **inside** this
vendor-facing wrapper use no-exception standard-library paths, which may
fail-fast; C++ unwinding cleanup is not provided by this target. External
exceptions must not be relied upon to unwind this boundary. This is scoped
to the vendor boundary; do not describe it as disabling exceptions throughout
the viewer or unchanged allocation-failure semantics. Normal ring-buffer
behavior remains exercised in the tests.

## Actual-library regression

The permanent `alwebrtc_exception_abi_tests` target links real `llwebrtc` and
`llimage` together. It calls the actual echo ring and retains `&llwebrtc::init`
without calling it, so both offending production archive members are present.
No viewer, window, audio device or WebRTC session is initialized.

| Check | Baseline | Corrected wrapper |
|---|---|---|
| runtime_error matched by std::exception; original what() | fails | passes |
| std::string out_of_range matched by std::exception | fails | passes |
| Actual five-channel codec: valid primary+aux pixels, invalid-channel failure contained, valid recovery | exception escapes | passes |

The combined baseline fails all3cases and terminates SIGABRT. Corrected tests
pass3cases/34assertions; together with image/layout/scheduler/worker/OpenHT
suites,6CTest entries pass **50cases/1,311assertions**. The standalone actual
archive probe changes exit42 to0. The corrected archive has no standard
exception RTTI definitions; the joint executable imports them from libc++.

Four additional real cached1024-square five-component MCT1 bakes pass12primary
requests at discard0/1/3 in the existing contained corpus fixture. These are
representative inputs, not the exact282193byte image from the crash. That input
was not found on disk; cache writes follow successful decoding. The former
240-case extractor excluded >4components and is explicitly not coverage here.

This repairs the demonstrated abort mechanism; it does not identify the exact
codec rejection reason for that missing input or prove owner scene stability.
Both PPD records require owner confirmation before closure.

## Evidence and next action

Private local evidence:
`artifacts/metal/ppd-091-exception-rtti-20261113/` contains compiler trials,
vendor boundary proof, fixed tests, typeinfo inventories and later app receipts.
`artifacts/metal/ppd-090-g1-arrival-crash-20261113/rtti-proof/` contains the
actual-archive and combined baseline failures; retry evidence retains the
owner's exact log/IPS privately. Do not commit private inputs/session logs.

## Verified corrected app

Source `fef1e756fd3b0a988113c64c0e0e2bb5898fe955`,
**Alchemy Metal Alpha G1,26.4.0.68991**. Doctor/configure/Release wrapper build
exit0; full incremental build26.553s. The final build and staged main executable
import std::exception/runtime_error/logic_error/out_of_range/bad_alloc RTTI from
system libc++; **zero private standard exception typeinfos** remain.

Candidate:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd-091-exception-rtti-20261113/candidate/Alchemy Metal Alpha G1.app`.
Main executable SHA256:
`c188ebdfd45139677fcb7c45a9bfcacaa80f11867dcc9184090ba88a0dec9d08`.
346ARM64-only Mach-O files, deep strict signatures, source/version/native imports
and retained voice/media/decoder/render-record/diagnostic symbols/flags pass.
All7,841regular/symlink entries match after `ditto`. Frozen original and
diagnostic apps remain byte-identical. No viewer/audio/GPU launch.

Receipts in the PPD-091 evidence directory include `candidate-build.json`,
`compile-contract.json`, `fixed-tests.json`, `fixed-ctest-last.log`,
`candidate-build-static-verification.json`, `candidate-staging-receipt.json`
and `candidate/static-verification.json`. A private verifier regex typo was
corrected with its failed attempt retained; it was not an app/code failure.

Next: owner retries the same club with this corrected68991 app. Read any
remaining decode errors and confirm scene stability before closing PPD-090/091.
No automatic start of goal two, push or feature-parity/performance claim.
