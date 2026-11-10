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
No stream URLs are included in new diagnostics. Independent source review
approved the patch, including null-safe shared-memory removal after init failure.

## Executed playback and build checks

Source commits `d497d2122e` and `2c60e47b0e` are included in the verified/signed
canonical26.4.0.68643 Release bundle. Doctor/configure/build/verify_app/deep strict
signature all exited0;349 Mach-O files passed ARM64 inspection. Actual child SHA
`a74462307afa55f9dec7648b622b6e960e838ba82842d5836777208aa98ba424`.

Root ran the exact rebuilt packaged host with its real Resources working
directory, a local fake parent speaking the real LLSD plugin protocol, and a
generated3-second all-zero WAV whose path contains a space. Volume0, looping off,
localhost IPC only, no external media URL/account/preferences changes. The host
completed hello/init, media init, Playing,13 time updates through3seconds, Done
and normal exit0, with empty stdout/stderr and no audio-output errors. No parent
signals were sent. Shutdown used the existing child goodbye timeout (~12s).
This verifies real compiled dedicated-host playback, not just library loading.

Private receipts: ignored
`artifacts/metal/private-classic-avatar-position-w-20261110-01/vlc-real-host/`;
wrapper commands/exits and final build identity are in its parent directory.
The private stdlib harness SHA is
`9e595b2fd65b0cce8c9a28079e0118e3fdda1b6347f46e462e606d54e22600cf`.
An earlier sandboxed CoreAudio probe could not enumerate output devices and is
preserved as failed; the exact subsequent unsandboxed local test passed.
Remote parcel/audible music, station switching and actual account acceptance
remain untested. Viewer stays closed; no claim that every audio issue is fixed.

Private evidence: /private/tmp/alchemy-metal-vlc-init-probe-20261110.a1wzo_vu/
and /private/tmp/alchemy-metal-libvlc-child-audit-20261110.mvm5qy8g/.
No private URLs, account/location data or captures uploaded.
