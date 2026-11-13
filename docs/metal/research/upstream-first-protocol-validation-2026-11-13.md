# FIRST batch — protocol/layer backports — 2026-11-13

## Scope and source

- Worktree `/private/tmp/alchemy-upstream-first-protocol-20261113`, branch `codex/upstream-first-protocol-20261113`.
- Base `bbd5193f99a3dfea2b9ac6337e3909bba86dc290`.
- Local commit `f3b2d74c35b00cdd095a639db6f03bcd4e296a7f`.
- Apple Silicon arm64 host, macOS26.5.2; inherited repository instructions/continuity skill read, canonical PPD register read before repairs. Integrator owns canonical PPD-018/020/021/025 and handoff documents; this isolated work does not modify them.
- Final selected upstream source/tests are from `408fd6c014e6e3b2b90dfe61265f34df0628f450`, reviewed against original base65c39b7bad and current fork. Every selected file is exact final upstream except message.cpp removes three optional AL_SESSION_CAPTURE hooks, CMake only registers selected tests, and LLSurface retains the fork's two `hasGPUTexture` native admission calls.

## Imported provenance

- #617 circuit timing/ACK/zero-code/duplicate/copy-window chain: `f4cd6e34e6`, `475007ebd4`, `eb0cd7fe7c`, `6bb0ddd97e`, `a9fbfbe0ae`, `a30ca40506`, `d0f7aab185`, `3d37e9c2a0`, `406baace85`.
- #630 full queue and impairment-release tails: `f90c8559b3`, `670b85ad16`, `010f7371f4`.
- #636 packet-ID domain guard: `6687792a35`.
- #637 no-ping reliable-history bound: `d557bdde72`.
- #634 bounded layer/bitstream and #635 wind precursor: `a6c038538f`, `df1de743b5`, `83ebc5157c`, `34f22f08e3`. Include encoder/test coherence tail `a6cd1b444c`; valid existing calls remain source-compatible, the viewer sends no patches through that encoder.
- Xfer short header, failed start, cleanup/reentrant callback and final tests: `8a0c98ae13`, `a1382cb852`, `db24486fa2`, `0375b1c008`, `e18a7f95fd`, `c69565cc31`.
- #614 only bounded suffix checks and tests in llxfermanager/llxfer_file: `4fa42289ac`. Object peer owns local filename generation/client callback handlers.
- Include #623 `d728638e36` **llmessage-only** closeCircuit helper and regression tests. Region-retirement caller and new feature/settings are excluded.
- Exclude all aloracle/session recording, unrelated locks/network transport and object-parser peer paths.

## Actual verification

`git diff --check` exit0 before commit; commit created cleanly and contains exactly24 owned source/CMake/test files.

Run `python3 /private/tmp/alchemy-first-protocol-validate-20261113.py` from the worktree. It uses canonical Release compiler flags/Catch/dependency libraries, remaps source include paths to this worktree, compiles **current source** without old PCH, writes new objects/binaries entirely under `/private/tmp/alchemy-first-protocol-checks-20261113`, and links only selected tests plus unchanged support/library code. No canonical binary or object is overwritten.

-18 translation units compile exit0 under ARM64/macOS26.0 minimum, C++23, Release/O3/Werror: llcircuit/message/llpacketring/llpacketbuffer/llxfer_file/llxfermanager/patch_code plus unchanged-header-dependent lltemplatemessagereader; six protocol test units; bitpack; three real viewer consumers LLSurface/LLVLManager/LLWind.
- `protocol_tests` exit0: **20,599 assertions /52 cases**. Includes real message-system localhost socket duplicate/ACK/MTU/full inbound rings, Xfer short names/header/real file start failures/callback retries, bounded layer/end-code/partial input, zero-code capacity boundaries.
- `llcircuit_test` exit0: **49 assertions /11 cases** including invalid24-bit IDs, valid wrap gaps, ping timing, ACK collection, duplicate retention/copy window and65,536 no-ping history cap.
- `bitpack_test` exit0: **36 assertions /9 cases** including zero-fill past input and exact boundary consumption.
- Source-identical upstream test bodies remain unmodified; production source is committed f3b2d74c35.

Logs/argv/script evidence: `/private/tmp/alchemy-first-protocol-checks-20261113/{protocol_tests,llcircuit_test,bitpack_test}-{link.json,link.log,run.log}`, per-unit `.log` files, `compile-results.json`, validation script above. The compile manifest is generated per invocation and skips already-current objects on the second run; script and per-object logs retain reproducibility.

### Retained initial narrow-link failure

First manually linked protocol test crashed (-11) in LLTemplateMessageReader::decodeData while reading timing callback, at message-test duplicate delivery. That binary combined a new LLMessageSystem/embedded LLPacketRing layout with an **old unchanged** lltemplatemessagereader object from a prebuilt static archive. Recompiling that header-dependent production unit against current headers removed the ABI mismatch; same new protocol/test sources then passed all52cases. This was validation construction, not a viewer-source change or a proven defect in the imported upstream fix. The integrator's normal CMake dependency rebuild recompiles header consumers automatically. Initial output/OS crash report remains available at `/Users/dekstroza/Library/Logs/DiagnosticReports/protocol_tests-2026-11-13-090215.ips`.

## Limits and next gate

Integrator owns doctor/full combined build/ctest/app verification. No full app build, viewer launch, grid session, GPU check or owner verification performed by this workstream. No closure, user-visible performance claim or push authorization follows from these bounded tests. Changes remain local.

An additional source-isolated packet-domain negative control is recorded in `id-domain-red-control.json`: current llcircuit source with only the24-bit guard removed is compiled and linked against the exact same current test, then run with a3-second timeout. Actual negative control: unguarded test times out at3seconds; current guarded source completes in the passing11-case circuit suite. Compile/link exit0. Receipt retains argv, timeout and elapsed time; no viewer was involved.
