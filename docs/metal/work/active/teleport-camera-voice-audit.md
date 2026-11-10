# Teleport-camera and voice source audit

Owner requested comparisons against exact original
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, while the avatar and music defects were
being repaired. Current source checkpoint
`2c60e47b0eac61f3a62b504163504516d1729bed`. Read-only audits; no camera/voice
changes, viewer launch, call, microphone capture or permission changes performed.

## Teleport camera

Original and current LLAgentCamera, LLAgent, LLViewerMessage, progress floater
C++/XUI and update_tp_display function match. Movement-complete handler changes
coordinate frame, setFocusOnAvatar(true,false), slamLookAt, updateCamera, then
arrival/new avatar position. First arrival resets view. Startup animation stop,
reset and saved-login-pose restoration remain original. Main display camera uses
original setup3DRender; native probe-only math setup preserves those matrices.

Both original and native render world behind the small420x90 teleport progress
floater; the original "skip render" comment does not establish a hidden-world
contract. Original setFocusOnAvatar(true,false) stops animation only when focus
state changes, so its handler's "do not animate" comment is not unconditional.
No Metal-only reset/interpolation omission was found or guessed fix applied.

Native presentation can retain a prior pre-UI world image while current world is
incomplete, while CPU camera continues updating. This can yield a held image/jump;
it does not prove the reported continuous fly-in/zoom cause. If repeated, capture
actual camera pose versus published source-frame/region across that teleport,
and compare the same action against the original executable before changing
camera policy. No matched live camera comparison was obtained in this audit.

Private proof: `/private/tmp/alchemy-metal-teleport-camera-source-20261110.ndg2fz82/`.

## Voice audio and call ownership

29 scoped files match original byte-for-byte: llvoiceclient, llvoicewebrtc,
llvoicechannel/call handlers, device preferences, speaker/mute moderation,
UI call dispatch, voice visualizer source, entire llwebrtc implementation and
WebRTC dependency selection. Six registered voice/PTT/device settings match;
registered defaults are not evidence of current saved user settings.

LLWebRTCVoiceClient still selects spatial/nonspatial sessions, devices and mute.
Group/P2P state machines remain intact. VoiceWebRTC queue is serviced by
doOnIdleRepeating; LLAppViewer still runs idle callbacks. SDL event pumping is
retained. Native WebRTC creates its platform-default AudioDeviceModule on its
worker thread and preserves recording/playout/device-switch/enable paths.
Metal window/surface changes do not replace that audio module.

Integration check also found the voice visualizer call boundary retained:
render_ui_over_world calls LLHUDObject::renderAll inside authoritative native UI
capture, which dispatches LLVoiceVisualizer::render. Its gUIProgram textured
triangle strips, matrices/blend and read-only depth have native UI consumers,
including reverse-Z remapping. SDL key-down/up and middle-button events still
reach viewer global bindings and mic press/release/toggle callbacks. No missing
dedicated voice-symbol producer was identified. Actual symbol rendering and
PTT interaction remain untested; source presence is not live acceptance.

Current canonical68643 package retains original microphone/local-network usage
text, audio-input entitlement, hardened runtime, ARM64 static/in-process WebRTC,
and CoreAudio/AudioToolbox/AVFoundation linkage. A separate SLVoice process is not
required by this WebRTC configuration. Info.plist has unrelated native fork
document/SLURL identity substitutions; its voice permission strings are unchanged.

Three selected historical logs show capture/render device enumeration, mute and
shutdown callbacks, nine selected voice rows each, no selected warning/error.
This proves limited initialization activity, not any call or audio TX/RX.
No changed incorrect audio/call/device/permission contract was found. This is
separate from the confirmed LibVLC parcel-music repair.

Microphone TCC authorization, capture/playout, spatial/group/P2P calls,
hotplug/device switching, PTT/mute/moderation, reconnection and teardown remain
functionally untested. No claim of full voice parity or audible success.

Hashed source/package/log evidence, with raw user/device strings omitted:
`/private/tmp/alchemy-metal-voice-retention-audit-20261110/summary.json`, SHA256
`8de8e5e363865f7d3b55e4f119223a4238b3e8bd0a9fd45eba4dfdc50c57e980`.
Independent package evidence:
`/private/tmp/alchemy-metal-voice-package-audit-20261110/summary.json`.
Copies of these reports and camera proof are preserved under ignored
`artifacts/metal/private-classic-avatar-position-w-20261110-01/source-audits/`.
