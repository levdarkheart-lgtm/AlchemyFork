# Startup media indicator and silent parcel music

2026-11-12. Owner reports top-bar play/pause media control initially appears
active while music is silent. First click shows stopped; second click shows
active and music starts several seconds later; subsequent pause works. Owner
identifies the top-bar control, not the volume-panel streaming checkbox or
Nearby Media parcel-audio row. Registered as PPD-009 before any repair.

## Observed source boundaries

LLStatusBar::refresh polls every0.2s and sets the inverse toggle from the union
of LLViewerMedia::isAnyMediaPlaying, isParcelMediaPlaying and
isParcelAudioPlaying. XUI describes this as Start/Stop All Media (Music, Video,
Web pages). A visible non-time-based in-world media implementation also counts
as playing. This is an aggregate control, not a music-only playback indicator.

The plugin-backed streaming implementation reports MEDIA_LOADING as playing,
as well as MEDIA_PLAYING. Its indicator therefore covers pending connection/
buffering, not only audible output. LLViewerAudio also owns a3s fade-in and2s
fade-out; those delays must remain separate from a startup failure.

An independent source comparison against1eb44bbd finds statusbar, viewer audio,
parcel manager, Nearby Media, streaming adapter, plugin host and FAudio engine
unchanged. Same-URI requests are skipped after mNextStreamURI is set; a failed
lazy backend start can leave a requested URI latched until OFF clears it, but
that mechanism is not yet tied to this report. VLC lifecycle review and
owner-session state/log checks continue. No source fix or reproduced cause yet.

Root owns canonical docs and any eventual implementation. Peers inspect source/
reference read-only. No viewer restart, stream toggle, settings change or new
recording is authorized by this investigation. Keep any fix local for owner
testing; the previous decoder/renderer push approval does not cover this issue.

Private evidence directory: artifacts/metal/music-startup-indicator-20261112/.
Next: distinguish non-audio media/pending playback from a failed initial stream,
inspect current versus original plugin creation and transport transitions, then
repair the earliest demonstrated incorrect state without changing all-media
button semantics or starting media against the owner's autoplay preferences.

## Current findings

Owner confirms initial silence lasts at least30seconds until OFF/ON. Read-only
whitelisted saved/default settings at19:29:10UTC show AudioStreamingMusic=1,
AudioStreamingMedia=1, ParcelMediaAutoPlayEnable=0 and MediaTentativeAutoPlay=1.
Thus streaming is enabled but automatic parcel music startup is disabled. These
are current file values, not a live incident snapshot. The restored68880 is
still at LOGIN_WAIT, so its log does not reproduce the world-entry report.

Both saved68884 world-session logs have zero nonempty Starting parcel music,
Starting internet stream or Playing stream requests. They show backend fallback
to media plugins, and NULL/stop during shutdown, without corresponding VLC
startup/playback warnings. These recordings support music never being requested
there; they are not guaranteed to be the precise reported incident.

The source explains the reported sequence without a failed audio engine:
autoplay-off leaves music stopped, another non-UI web/media instance can make
the aggregate button active, and OFF/ON explicitly starts parcel music without
requiring the autoplay setting. The3s fade and buffering follow that manual
request. This matches original behavior. No wrong initial boolean or plugin
failure is demonstrated; do not change aggregate semantics or autoplay settings.
The synchronous lazy-init path already has fade-driven retries; only a later
asynchronous failure with a retained URL could fit the earlier retry hypothesis.
That remains unobserved, so no generic same-URI retry fix is justified.

No production code, preference or stream state changed. Keep PPD-009 open as a
reported symptom unless owner confirms the explanation; if it recurs with
autoplay explicitly enabled, capture actual aggregate branches/backend status
and start-request ordering at that moment. Raw logs/settings receipts remain
private. Previous approved fixes are published at802250e82c; this new report
is local and outside their push scope.

## Owner-requested correction

Owner explicitly rejects the previous explanation as a resolution and requests
that active indication agree with parcel music, without an OFF/ON cycle. The
OR selector is a concrete UX mismatch under that contract: stopped enabled
parcel music plus active MOAP renders Pause; the first click stops everything.
This is an intentional correction to inherited UI policy, not a demonstrated
VLC transport failure or a rendering regression.

When AudioStreamingMusic is enabled and the parcel music URL is present, use
parcel-audio transport state for the top-bar indicator. Otherwise preserve the
existing all-media aggregate. Keep the global start/stop callback, autoplay
preferences and backend loading-as-active cancellation semantics. Initialize
the icon to Play until the first state refresh; clarify the tooltip in music
mode. Do not change master/music mute, fade timing or URL validation policy.

Root owns llstatusbar.cpp, English panel_status_bar.xml, canonical docs, local
commit/build and candidate. Peer owns only the private extracted-production
regression harness. Independent review confirms disabled-music/no-URL fallback,
inverse toggle sense and pending-loading cancel behavior. Build separately from
the preserved68880 baseline; no push until owner accepts this new correction.

## Candidate evidence

Local source9f8d17583097d25a7d086296d9553858f6c9853b produces26.4.0.68889,
Alchemy Metal Music Startup Fix.app in the shadow-occlusion Release staging
directory. Binary SHA2562670e59c1b85fa4323bf4e50070346df8a3f96efaed57f3fe8037d7494f6b873;
full source SHA embedded, staged English XUI byte-identical. Doctor0, configure0,
Release/O3/LTO-OFF wrapper build0, verify_app346Mach-O/zero invalid, deep strict
signature0 and production strict audit3516files/912informational/zero blockers
or errors. Prior68880 executable243b5787 remains unchanged. At build validation the candidate had not
been launched; control-flow checks do not establish audible playback.

Private repro/validate_music_button.py extracts actual production refresh,
initialization/click, LLButton toggle, global media action and backend mapping
bodies, stubbing only external media/parcel/audio boundaries. Same22checks:
baseline967c6db9 fails7 and takes2clicks; candidate passes22 and starts music
on first click. Active/paused/loading cancellation, disabled/no-URL aggregate
fallback and tooltip restoration pass; autoplay remains0. All five input hashes
stay unchanged. This is a control-flow regression, not real audible playback.

Receipts: configure.json/build.json, build-identity.json, arm64/signature/
strict-source-receipt.json, staged-ui-proof.json and repro/summary.json under
the private evidence root. Exact build commands:

```sh
tools/metal/doctor.sh
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Music Startup Fix' -DAL_BUILD_TESTS=OFF
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=3 nice -n 10 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Music Startup Fix.app'
```

Owner test is next; all changes remain local. The newly reported5s start delay
is separately tracked as [PPD-010](music-start-latency.md), with no latency fix
included in this candidate.

## Owner launch and publication approval

Owner explicitly requests the new viewer. Exact68889/source9f8d175830/hash
2670e59c starts PID77975, runtime-194511/run.json, LOGIN_WAIT verified. Populated
cache30365files retained with equal CacheLocation/NewCacheLocation, no migration
or purge. Runtime4normal/6maximum/adaptive1 and OpenHT0.27.0 observed. No media
preferences/graphics overrides or profiling introduced; owner controls login.
Owner then explicitly requests committing/pushing the music fix. The only two
application files changed are llstatusbar.cpp and English panel_status_bar.xml.
Any new native music backend and black-startup repair remain separate.
