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
