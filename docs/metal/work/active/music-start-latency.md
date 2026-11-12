# Parcel music startup latency investigation

2026-11-12. PPD-010. Owner reports5s+ before Alchemy music is audible versus
1–2s in Firestorm. This remains a reported comparison, not a controlled timing.
Keep it separate from the locally built PPD-009 button-state correction.

This build selects FAudio for viewer sounds. Its default streaming factory is
null, so LLStartUp installs LLStreamingAudio_MediaPlugins. start(nonemptyURL)
lazily creates a plugin source/process; stop and start(emptyURL) destroy it.
Later Play repeats host/LibVLC initialization, connection and buffering. The
current plugin has no explicit network-cache override. The7000ms stream-buffer
constant in LLViewerAudio is only sent to adjustable-buffer backends; this
plugin adapter does not implement that facility.

Pinned official Firestorm source176c8a16ced2d13d455e68b06c500c8def0e40d6:

- [macOS ReleaseFS enables FMOD](https://github.com/FirestormViewer/phoenix-firestorm/blob/176c8a16ced2d13d455e68b06c500c8def0e40d6/autobuild.xml#L3687).
- [Startup selects the engine-native stream implementation](https://github.com/FirestormViewer/phoenix-firestorm/blob/176c8a16ced2d13d455e68b06c500c8def0e40d6/indra/newview/llstartup.cpp#L1097), subject to engine/settings overrides.
- [FMOD creates native streaming](https://github.com/FirestormViewer/phoenix-firestorm/blob/176c8a16ced2d13d455e68b06c500c8def0e40d6/indra/llaudio/llaudioengine_fmodstudio.cpp#L454).
- [Default fade settings](https://github.com/FirestormViewer/phoenix-firestorm/blob/176c8a16ced2d13d455e68b06c500c8def0e40d6/indra/newview/app_settings/settings.xml#L23262) also use3s in/2s out; [fade/start ordering](https://github.com/FirestormViewer/phoenix-firestorm/blob/176c8a16ced2d13d455e68b06c500c8def0e40d6/indra/newview/llvieweraudio.cpp#L80) is optional/configurable there.

Our3s fade overlaps connection/buffering; it is not a3s playback hold. The
Firestorm default matches it, so do not attribute the difference to an extra
Alchemy-only3s delay. The owner's exact Firestorm build/settings are unverified.
Cold plugin lifecycle is a concrete structural difference, but its share of
the observed5s is unknown. Plugin timeout limits are not normal startup delays.

Next: measure request→host-ready→opening→playing→audible stages using the same
stream/network and volume/mute settings; compare cold versus retained-host
playback before changing backend, buffering, fades or automatic startup policy.
No latency source/settings change or new audio playback performed. Private
research: artifacts/metal/music-startup-indicator-20261112/firestorm-latency-comparison.md.
