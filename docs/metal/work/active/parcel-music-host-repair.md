# Packaged parcel-music host initialization repair

Owner reports parcel music enabled with adequate volume but no playback.
Actual68638 log contains three start requests reaching the LibVLC audio-only
host, followed by child exit1 after reporting its LibVLC version. Source maps
show FAudio correctly selects the existing media-plugin streaming fallback.
No missing Metal-specific music implementation or muted-setting cause was found.

## Confirmed packaged-library failure

Original reference1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 has the same audio,
parcel, streaming-wrapper and LibVLC source logic. This is an inherited packaged
host defect, not evidence of changed Metal camera/rendering behavior.
`initVLC` sets VLC_PLUGIN_PATH to ./plugins. Actual child working directory is
viewer Contents/Resources, but the package stores modules under its nested
media_plugin_libvlc.app/Contents/Frameworks/plugins.

Actual isolated subprocess A/B used the exact packaged core/LibVLC libraries,
original two arguments, actual Resources working directory, and no network URL:
relative ./plugins gives NULL libvlc_new; absolute packaged module path yields a
valid instance and clean release. Core preload reproduces the dedicated host's
RPATH dependency resolution. Preliminary ctypes-only attempt could not resolve
@rpath/libvlccore; preserved as a test-host limitation, not a viewer defect.
The A/B proves this package's initialization failure. Live crash instruction
was not captured: the host converts fatal signals to exit1 and suppresses normal
stderr, explaining why the parent log lacked a useful stack.

## Narrow repair

Darwin derives the module directory from the canonical native executable path,
not working directory or a shell command. Spaces and relocation are preserved.
Missing module directory or failed initialization publishes STATUS_ERROR. Play
requires a valid instance; media/player failures remain errors. Cleanup guards
null values and releases valid owned player/media/instance handles. Original
stream arguments, saved preferences, volume behavior and audio engine unchanged.
No stream URLs are included in new diagnostics. Root integrates/builds;
independent source review and actual dedicated-host playback checks are pending.

Private evidence: /private/tmp/alchemy-metal-vlc-init-probe-20261110.a1wzo_vu/
and /private/tmp/alchemy-metal-libvlc-child-audit-20261110.mvm5qy8g/.
No private URLs, account/location data or captures uploaded.
