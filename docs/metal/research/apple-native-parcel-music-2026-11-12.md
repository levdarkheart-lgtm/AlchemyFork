# Apple-native parcel music investigation

2026-11-12. Investigation requested by owner; no production backend replacement.
Source inspected at `9f8d17583097d25a7d086296d9553858f6c9853b`; concurrent
documentation/publication checkpoint advanced to `627e735fa562510727b90df7ccd79a3bcbb327c2`.
Host: Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6, native ARM64.
Related defects: PPD-010 startup latency; PPD-011 inherited metadata delivery.

## Finding

Apple supplies viable native playback APIs. AVFoundation's `AVPlayer` is the
smallest integration candidate; Core Audio's `AudioFileStream` + `AudioQueue`
offers explicit packet buffering and playback control if AVPlayer's live-radio
startup is unsuitable. For a startup-focused implementation experiment, prefer
the small Core Audio packet/queue route: the matched synthetic MP3 probe showed
positive AudioQueue sample time in about0.4s; AVPlayer became ready/reported
playing at6.129s and reached0.5s media time at6.686s. Its first positive clock
timestamp was not recorded. This is grounds
for an implementation candidate, not an accepted real-station performance result.
Native API selection alone does not prove faster starts,
lower CPU, lower power or better fidelity. The stream's encoding still determines
its available audio information.

This can replace only land/parcel music. Existing sound effects, voice, parcel
video and web media have separate owners. VLC remains a video/media dependency
until its other uses are replaced; removing its music use does not remove VLC
from the bundle. AVFoundation may use Apple media services, so the benefit is
avoiding Alchemy's VLC music host, not promising zero helper processes.

## Verified source boundary

- `indra/llaudio/llaudioengine_faudio.h:150`: default streaming factory returns
  null. `indra/newview/llstartup.cpp:888–899` then installs
  `LLStreamingAudio_MediaPlugins`; the explicit
  `UseMediaPluginsForStreamingAudio` preference also selects this route.
- `indra/newview/llviewermedia_streamingaudio.cpp`: a nonempty start lazily
  creates a plugin with forced `audio/mpeg`; MIME mapping chooses VLC. Stop or
  empty URL deletes it, so the next start repeats host/library initialization.
  Its contribution to the owner's reported5s latency remains unmeasured.
- Implement `LLStreamingAudioInterface` in `indra/llaudio/llstreamingaudio.h`
  using a small Objective-C++ owner, and select it through the existing factory.
  `indra/llaudio/CMakeLists.txt` needs native source/framework linkage. Do not
  introduce a general media framework or alter unrelated viewer systems.
- Preserve original versus cleaned URLs, whitespace/legacy appended-label
  handling, stop/pause semantics, LLSD metadata updates and cancellation.
  `LLViewerAudio` owns existing music/master/mute gain and3s/2s fades;
  `LLViewerParcelMgr` owns autoplay/parcel transition/consent policy.
- Existing `isPlaying()` distinguishes stopped/requested-or-playing/paused,
  not audible output. Retain cancellation of pending play and expose native
  waiting/error/actual progress separately. A requested rate, ready status or
  playing flag alone is not a speaker-output receipt.
- Viewer output selection currently targets FAudio. Propagate the corresponding
  Core Audio UID explicitly; `AVPlayer.audioOutputDeviceUniqueID` supports it.
  Test device removal, system-default changes and user-selected output.
- Current compiled deployment floor is macOS26.0 in
  `indra/cmake/AlchemyTarget.cmake:20` and the dependency triplet. Do not base
  compatibility advice on old macOS audio-format limitations.

## Options

| Apple API | Role and tradeoff |
| --- | --- |
| AVFoundation / AVPlayer | Smallest native player; network loading, native decoding, transport states, volume, output UID and timed metadata. Needs real stream/playlist/recovery validation and startup measurement. |
| URLSession + AudioFileStream + AudioQueue | Native network/parser/output building blocks; explicit buffering. Viewer must own playlists, ICY extraction, redirects, queue sizing, reconnects and lifecycle. |
| AVAudioEngine | PCM processing/mixing/effects after decoding; not a complete internet-radio transport. No reason to add DSP merely to replace VLC. |
| AVAudioPlayer | File/data playback; unsuitable as the main indefinite-radio transport. |
| MusicKit | Apple Music catalog/library integration, unrelated to arbitrary parcel radio URLs. |

Apple documents [AVPlayer's playback model](https://developer.apple.com/documentation/avfoundation/avplayer),
[transport states](https://developer.apple.com/documentation/avfoundation/controlling-the-transport-behavior-of-a-player),
[ICY StreamTitle](https://developer.apple.com/documentation/avfoundation/avmetadataidentifier/icymetadatastreamtitle),
[metadata output](https://developer.apple.com/documentation/avfoundation/avplayeritemmetadataoutput)
and [output UID](https://developer.apple.com/documentation/avfoundation/avplayer/audiooutputdeviceuniqueid).
For the lower-level route, see [Audio File Stream Services](https://developer.apple.com/documentation/audiotoolbox/audio-file-stream-services)
and [Audio Queue Services](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioQueueProgrammingGuide/Introduction/Introduction.html).

`preferredForwardBufferDuration=0` means system-chosen buffering, not zero
buffering. [Automatic waiting](https://developer.apple.com/documentation/avfoundation/avplayer/automaticallywaitstominimizestalling)
trades startup against stalls; disabling it requires explicit recovery policy.
The APIs do not supply a free latency improvement through one setting.

## Standalone native playback evidence

Private reproducible artifacts: `artifacts/metal/native-parcel-music-20261112/`.
`probe.m` links only Apple Foundation/AVFoundation/CoreMedia, compiles with
`xcrun clang -fobjc-arc`, and mutes AVPlayer. `run_probe.py` serves generated
four-second fixtures repeatedly over paced localhost HTTP without Content-Length
or an initial burst. LAME generates MP3, Apple's afconvert generates AAC/HE-AAC,
and ffmpeg generates Ogg fixtures; these are fixture tools, not playback backends.
No public station, account, viewer preference or running-viewer playback changed.

`commands.json` records fixture/compiler argv, output and exit status;
`results.json` records native readiness/state/time/metadata/errors and probe
exit status. `requests.json` records localhost requests. A positive result means
the muted native playback clock advanced at least0.5s. It does not establish
audible onset, long-run continuity, real station compatibility or a VLC advantage.

| AVPlayer case, default waiting | Observed result; first playing seconds |
| --- | --- |
| Direct MP3 | Clock advances;6.129s |
| Direct AAC-LC ADTS | Clock advances;1.853s |
| Direct HE-AAC ADTS | Clock advances;1.484s |
| MP3 with ICY headers/metadata | Clock advances;6.152s; StreamTitle and StreamUrl received |
| Legacy ICY200OK response + metadata | Clock advances;6.222s; metadata received |
| HTTP302 to MP3 | Clock advances;6.167s |
| PLS URL list to MP3 | Clock advances;6.194s |
| Ordinary M3U URL list | Fails with CoreMediaErrorDomain -12642; HLS is a separate format |
| Ogg/Vorbis | Clock advances;7.373s; encoder metadata received |
| Ogg/Opus | Not ready within9s; inconclusive, not proof of unsupported format |

These are single synthetic probes, not a controlled production benchmark. Offered
chunk timing, no initial server burst and repeated file framing affect startup;
Opus was offered at9000B/s, which may be below fixture encoding rate. Do not
equate these timings with the owner's parcel stream or audible five-second report.
The important finding is that native AVPlayer can handle radio, while default
live MP3 startup is not automatically fast. Ordinary M3U should be explicitly
resolved; PLS acceptance in one fixture does not establish full playlist support.

One deliberately early `play()` with automatic waiting disabled reported playing
around0.022s but the playback clock stayed at zero throughout9s. Readiness only
arrived at6.102s. This illustrates why state alone cannot prove output and why
the native backend needs careful readiness/start sequencing.

Waiting until readiness and then using `playImmediatelyAtRate` still took6.124s
to readiness/playback, with a progressing clock. A0.25s preferred forward buffer
still took6.200s. Thus neither tested buffering/start setting removed this
fixture's AVPlayer MP3 preparation delay. These additional cases are appended in
`results.json`; their source is retained in the final `probe.m`.

### Lower-level Core Audio candidate

Independent `coreaudio_probe.m` uses only Apple Foundation/AudioToolbox:
NSURLSession -> AudioFileStream -> AudioQueue, volume0, with a deliberately
small0.25s application prebuffer. The same fixtures and256-byte localhost pacing
were used. `coreaudio-results.json` retains exact compile/run argv and exit
status, stage timestamps, formats, hashes, host/source identity and prior attempts.
The three final MP3/AAC/HE-AAC cases compile/run successfully, report no errors
and advance the AudioQueue sample clock by at least1s. MP3 reaches first sample
time advancement in about0.4s. This precedes AVPlayer's recorded readiness and
0.5s media-time milestones, and establishes native startup-control feasibility,
not audible latency or an equal-endpoint benchmark.

The requested AAC/HE-AAC encoder bitrates exceed their measured fixture bitrates
(about35kbps/15kbps for these easily compressed tones), so initial nominal128/64kbps
server pacing overfeeds the decoder. Final AAC/HE-AAC retests use fixture bytes
divided by4seconds; both pass again. Prior overfed runs are retained separately.

| Core Audio case | Request to first sample-clock advancement | Offered bytes/sec |
| --- | --- | --- |
| MP3 | 0.392s | 16000 |
| AAC | 0.333s | 4465.25 |
| HE-AAC | 0.335s | 1959.5 |

MP3 nominal128kbps pacing is the stronger direct comparison with AVPlayer.
The probe initially selected HE-AAC's base
AAC22050Hz format; selecting FirstPlayableFormatFromList correctly uses
HE-AAC44100Hz/2048frames per packet. Retain that invariant in any implementation.

This disposable diagnostic is not production-ready: no complete teardown,
playlist/ICY handling, reconnect/stall policy or output-device integration is
implemented. Its small prebuffer is an experiment, not a selected shipping
policy. Native AVPlayer's broader protocol handling remains useful evidence;
the lower-level candidate must earn that compatibility before replacing VLC.

Initial fixture setup failed because installed ffmpeg lacks libvorbis; its native
Vorbis encoder with experimental mode worked. afconvert Ogg/Vorbis encoding also
failed ('fmt?'), which says nothing about decoding. Initial probe linking omitted
CoreMedia, then succeeded with explicit linkage. Failed commands are retained;
none represents a viewer regression.

## Inherited metadata defect, PPD-011

Current VLC `updateTitle()` sends `name_text{name}` only
(`indra/media_plugins/libvlc/media_plugin_libvlc.cpp:279–283`). The other sender
only names the plugin. No VLC sender supplies structured metadata.
`LLPluginClassMedia` only assigns `mMediaMetadata` when that message has a
`metadata` field (`indra/llplugin/llpluginclassmedia.cpp:1343–1351`). The music
adapter reads this field at line138, while `alpanelmusicticker.cpp:159–166`
expects ARTIST/TITLE and station keys. Consequently this VLC route cannot
populate the ticker's structured tags. Original1eb44bbd has the same message gap;
adapter/plugin-wrapper files are unchanged. A particular station's tag contents
have not been inspected. No repair attempted or closure claimed.

Native ICY metadata delivery worked in the standalone probe. Mapping it into
LLSD can improve the ticker, while respecting combined artist/title strings and
missing tags. Station headers and UTF-8/legacy tag encodings need explicit tests.

## Integration acceptance and remaining work

Keep implementation local for owner testing. Before selecting the final backend,
compare native candidates and current VLC on the same station/network: request,
connection/first bytes, first parsed packets, ready/output start, audible onset,
steady playback, stall/reconnect and stop/restart. Include cold and warm starts.
Validate direct MP3/AAC/HE-AAC/Ogg, HTTPS, redirects, playlist variants, ICY tags,
MIME mistakes, network loss, parcel transitions, device changes, fade/mute/gain,
shutdown and simultaneous voice/sound effects. Packaged-app HTTP/ATS behavior
needs separate testing; localhost command-line probes do not establish it.

Product advancement: source boundaries and real native radio/metadata viability
are established. No production backend replacement, PPD-010 latency repair,
PPD-011 ticker repair, fidelity/CPU/power improvement or full-product completion
is claimed. Runtime stream compatibility and owner acceptance remain open.

Environment doctor exits0. Existing unchanged68889 app `verify_app.sh` evidence
is retained separately; it does not validate an integrated native music backend.
The standalone probes were built, but no viewer rebuild was needed for this
source-read/research-only investigation. Final doctor0, existing-app ARM64
verification0 (346slices/zero invalid), documentation harness0 (33required files)
and `git diff --check`0. No application or repository tool implementation changed.
Independent report review corrected the unequal startup endpoints before handoff.
