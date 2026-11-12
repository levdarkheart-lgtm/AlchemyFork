# Native parcel music replacement

2026-11-12. PPD-010 startup latency / PPD-011 inherited ticker metadata.
Owner requests implementing the researched Apple-native backend, conditional on
preserving voice. Proposed replacement remains local pending owner testing and
explicit push approval. A separate session owns the black-screen startup fix.

## Ownership and source

Isolated branch `codex/native-parcel-music`, worktree
`/private/tmp/alchemy-native-parcel-music-20261112`, starts at4a3ebace81.
Integrator owns FAudio factory/device integration, Core Audio output-name mapping,
llaudio CMake, app HTTP transport policy, build/integration and canonical records.
Backend peer exclusively owns new llstreamingaudio_coreaudio.h/.mm; test peer
owns its native test and loopback fixture server. No startup/renderer/voice source
edits. Unrelated research notes and main-checkout black-screen work stay untouched.

## Acceptance

Native Core Audio owns parcel music only, retaining existing gain/fade/mute,
autoplay/parcel policy and metadata signal contracts. Exercise actual production
MP3/AAC/HE-AAC/Ogg decoding, playlists/redirects, ICY boundaries/encodings,
stop/pause/replacement, buffering/reconnect and destruction. Output remains
independent of voice and never changes system default input/output or microphone
settings. Test native queue coexistence and inspect final WebRTC linkage.
Build/signature/source/tests do not alone prove actual voice-call parity or
audible latency; owner session verification remains explicit.

## Verified initial facts

Voice is enabled (`AL_USE_WEBRTC=ON`); Release ARM64 voice objects and the prior
68889 executable contain LLWebRTCVoiceClient. WebRTC's native audio-device module,
separate capture/render selectors and voice/mic volume are independent from VLC
and the music gain path. Voice effects/morphing are not implemented by this
module; do not infer support for every historic voice effect.

FAudio persists display names as output IDs, not native UIDs. Resolve uniquely
matching output-capable Core Audio device names and trim trailing spaces as SDL
does; empty/unavailable/ambiguous choices follow system output. Never guess a
duplicate physical device or alter global HAL defaults.

Extra standalone probes show native AudioFileStream/AudioQueue plays Ogg/Vorbis
and Ogg/Opus on macOS26.5.2, including variable-frame packet descriptions. Legacy
ICY200 receives response/data through NSURLSession but requires bounded raw
status/header normalization before the audio parser. The diagnostic alone has no
such normalization and fails type detection; the implementation/test must cover
that path. Plain HTTP station compatibility needs NSURLSession ATS policy; the
AVFoundation-only media exception does not apply to this networking transport.

## Progress

- doing: production serialized/cancellable native stream owner, bounded buffers,
  protocol/metadata handling and native HLS path as required.
- doing: actual-backend native loopback regression suite and output coexistence.
- todo: isolated configure/build, tests, static/ARM64/signature/voice evidence,
  independent lifecycle review, local integration and candidate handoff.

Evidence root in canonical checkout:
`artifacts/metal/native-parcel-music-20261112/`. Prior investigation remains in
[Apple-native parcel music](../../research/apple-native-parcel-music-2026-11-12.md).

## Initial candidate failures, PPD-012

Doctor0. First isolated configure1 because the new worktree lacked Dullahan;
local clone at the worktree's pinned submodule ba893db repairs setup, configure0.
First native object compile1 from const-char AudioFile info keys incorrectly
bridged as NSString; corrected to NSString literals. First native test-target
build1 because integration accessed a private base member; corrected to its
public getter. Current Release native target then builds0 with warnings as errors.

Independent review records callback errors overwritten by parser success,
partial queue allocation admission, stale reconnect progress, first-only playlist
selection and main-thread polling waits; repairs are retained before final
verification. AudioQueue output callbacks free compressed storage before the
sound necessarily finishes; remaining-duration accounting must use enqueued
frames minus native sample progress. Cached metadata/state snapshots and gain
updates must not stall the rendering loop behind network or device work.

Actual first MP3 native playback test exits42 after10s, state reconnecting,
queue_start:-66680 (InvalidDevice). Full initial suite is intentionally terminated
(SIGTERM, helper241) after retaining its failures, request/provenance receipts and
partial results. HLS via AVFoundation and ICY tag transport succeed in that run;
those observations do not establish direct Core Audio playback correctness.

Default-device audit first checked only an unstarted queue: null assignment0
and getter AQDefaultDevice. That was insufficient. An actual muted PCM comparison
finds unset/start0, null/start-66680, explicit current HAL UID/start0 and symbolic
AQDefaultDevice/start0; successful cases advance native samples. Backend repair
must use actual-start evidence and preserve following the system default.

Evidence files under implementation/: configure.log/configure-retry.log,
submodule-setup.json, backend-first-compile.log, native-test-build*.log,
actual-native-tests-first-failure.json, actual-native-tests-initial.json and
actual-native-tests-initial-stopped.json. Full app build is in progress in the
isolated directory; no candidate launch or source completion claimed yet.

## Default-device correction and integration boundary

Repaired actual-production format run exits0:31assertions/1case, MP3/AAC/HE-AAC/
Vorbis/Opus all explicitly report CoreAudio and advance the output clock. This
is an interim binary; final snapshot/gain/tail/budget fixes and14-case tests still
need the final rebuild. No audible viewer-latency or real-call result inferred.
Both owned source/test peers are now frozen pending runtime findings.

Full viewer build was deliberately interrupted130 to retarget the failed music
playback quickly; completed objects retained. The combined-target resume is
again deliberately interrupted before integrating the now-committed startup
correction (226c334be9, with63af048d20/b0585e4c1a). The music candidate must include
that owner's accepted correction rather than present an older startup path.
No renderer/startup implementation is authored by this music change.

## Source-pinned runtime retest

Music branch is rebased onto completed owner-accepted startup correction at
226c334be9; source checkpoint c07d9ca9ad. Source-pinned native build0. Full14-case
run exits42:13cases/267assertions pass,1case/1assertion fails (268assertions total).
The failing finite4sMP3 reaches>=3.9s native media time and>=3.5s walltime, but
never transitions to stopped within the next4s. Queue completion must account
for actual native drain, not nominal encoded-frame counts/gapless padding.

Passing cases include all5CoreAudioformats, HLSviaAVFoundation, HTTPredirect/
PLS/M3U/tokenizedURL/alternatefailover, ICYASCII/Latin1/legacyheaders, gain/pause,
replacing/canceling requests, reconnects, persistent-connection underflow,
bounded malformed/alternatecycleplaylists, destruction and independent-output
coexistence. ActualUID/default/invalidUID switching preserves HALdefaultinput and
output and both muted outputs advance. No microphone or real voice call opened.
All before/after binary and source hashes match for this full run; retained as
implementation/actual-native-tests-final.{json,log} and provenance.

Full app target build resumes at six low-priority jobs after a recorded SIGINT130
parallelism change; completed objects retained. Focused native-test artifact
builder is being prepared to retest the completion repair without mutating the
app build graph or its test executable. The source/body fix and final candidate
still need their own clean source pin, complete runtime pass and bundle checks.

## Native EOF repair

MP3fixture has155packets/178560nominalframes versus176400validframes plus576
priming/1584remainder. Encoded queue estimates cannot identify final output
completion. Frozen backendc25f7872a6df uses one asynchronous AudioQueueStop(false)
after EOF/pending submission, then native IsRunning completion after actual
start; no premature input-buffer callback or nominal-frame EOF decision.

Independent artifact builder uses the actual Release compile/link flags and
current production backend/test objects, without stubs, ahead of the unchanged
existing static archive. It leaves canonical build outputs alone. Focused finite
tail test exits0:11assertions/1case, actualmedia>=3.9s andwall>=3.5s, stopped/error
empty. Binary0963b19c675ff3ea2eed56ca922985af53386a5cb7babcd47a2a4b00290d5737;
112stable link inputs and source/binary hashes unchanged across compile/run.
Evidence implementation/focused/{build-receipt,finite-tail,finite-tail-provenance}.json.
Full14repaired-source suite now running against that immutable binary.

## Ready for owner testing

Final application source f826eea1351bdc496722044b14e4147390d009e8, version
26.4.0.68898 / CFBundleVersion26.4.0d68898, channel Alchemy Metal Native Music.
Local canonical branch integrates c07d9ca9ad/137dfbf25c/f826eea135 without touching
startup/renderer/voice source relative to accepted226c334be9. Native music builds
independently of the sound-effects engine; both legacy/default startup routes
select it. Retain video/CEF/VLC dependencies for their existing media owners.

Exact candidate: `artifacts/metal/native-parcel-music-20261112/Alchemy Metal Native Music.app`.
Executable SHA2569d862876b866fd4ab2cc4bdc2e2422182d9e185f44369e76bf98cbb259b2360e;
copy/source hashes equal. Full final configure0 and tools/metal/build.sh build0,
Release/O3/LTOOFF, OpenHTJ2K experimentON (approved decoder preserved), WebRTCON.
verify_app0:346Mach-O slices/zero invalid; codesign deep strict0. Strict production
source3520files/912informational/zero blockers or errors. Doctor0, tool units76pass,
harness0. Exact logs/argv/provenance in implementation/ under the evidence root.

Actual source-identical backendc25f7872/test9c285cab passes ALL14cases/271assertions,
including native MP3/AAC/HE-AAC/Vorbis/Opus, native HLS, protocols/metadata,
reconnect/underflow, finite tail, cancellation/destruction, UID selection/default/
invalid fallback and simultaneous independent output. Both global HAL default
input/output remain unchanged. Test binary0963b19c... and all source/helper hashes
are unchanged across the run; actual-native-tests-eos-fixed-full.{json,log} plus
provenance retain the complete results. No test was weakened to accept the EOF
failure; focused11assertion EOS correction also passes.

Final executable contains23native music and477WebRTC voice symbols; imports
AVFoundation/AudioToolbox and no direct OpenGL/Vulkan. Voice/microphone permission
strings are preserved. Git diff against226c334be9 for llwebrtc, all llvoice files
and black-startup files is empty. Source/link/output coexistence evidence does
not establish an actual microphone/voice-call result. No viewer launch, saved
preference/cache change, account action, capture or push performed.

PPD-010/011/012 ready_for_owner, verification/push approval still pending. Owner
next tests real parcel music start/stop, fades, ticker, output selection and voice
calls in this exact candidate. Duplicate display-name device mapping remains a
baseline-preserving system-default fallback, not a verified unique selection.
Product advancement: native music/protocol/metadata implementation and isolated
voice ownership are verified at the stated boundaries; audible latency, real-call
acceptance and broader Metal product release gates remain open.

## Owner-requested launch

2026-11-12T20:53:23UTC: owner asks to start the viewer. No other Alchemy viewer
process was running. Opened the exact copied68898 app; open exits0 and native
proc_pidpath confirms PID26265 at its recorded executable, stable for3s with
unchanged SHA2569d862876b866fd4ab2cc4bdc2e2422182d9e185f44369e76bf98cbb259b2360e.
Receipt implementation/launch-205323/run.json. No launch arguments for login,
settings, graphics, cache or profiling; owner controls login and playback testing.
Launch is observed, not a claim of audible music or voice-call verification.

## Owner acceptance and scoped publication

2026-11-12: after launching exact68898/PID26265, owner states:
"verified, works. you can commit and push this to github".
PPD-010/011/012 owner verification passed and push approval granted for this
native-music replacement. This records overall candidate acceptance, not an
invented controlled latency figure or a specific microphone/voice-call result.

The separate session has already published the approved startup correction as
3ea6170bc0. This music publication starts at that remote commit, retains its
PPD-008 approval/publication history and all six startup source files exactly,
and copies only the13music implementation/test paths plus music records. The
entire tracked non-documentation tree is byte-identical to testedf826eea135.
No new rebuild/relaunch is needed for the identical source; the accepted68898
binary remains SHA2569d862876b866fd4ab2cc4bdc2e2422182d9e185f44369e76bf98cbb259b2360e.
Unrelated research notes, captures and app binaries remain outside Git scope.

The original relative-latency report PPD-010 retains reported confirmation and
ready_for_owner status because no matched comparative timing/reproduction was
recorded. Owner verification of the native candidate and push approval are
passed/granted. PPD-011/012 have confirmed corrective evidence and are verified.

Publication validation: first documentation check correctly rejected closing
PPD-010 while its relative-latency confirmation remains reported. Status restored
to ready_for_owner with actual candidate acceptance/push approval retained; final
harness33files exits0, diff check0, and staged full non-documentation equality
against testedf826 exits0. Independent scoped review passes all18music paths,
retained PPD-001–009/black-publication history and absence of private artifacts.

## Publication observed

Owner-approved native music commit8f11241c947a019ea096460e71774c7344173f08 is
pushed normally to origin/codex/apple-silicon-metal; git ls-remote confirms the
exact SHA. Published parent3ea6170bc0 retains the separate startup fix. Canonical
checkout is aligned with the scoped publication; original local tested history
including its later startup-publication record is preserved on local branch
codex/native-music-owner-tested-history-20261112 at8e52d2f510. The five unrelated
untracked research notes retain identical SHA256s. No force push, release upload,
viewer restart, preference/cache mutation or capture publication occurred.
Private receipts: implementation/publication-{scope,reconciliation}.json.
