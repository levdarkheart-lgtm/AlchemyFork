# Continue failed-travel correctness repairs

Read AGENTS.md and CURRENT.md completely. Branch codex/apple-silicon-metal.
Renderer checkpoint 0cbb48f104ad2f7d6e358266c1abf32d4d487a63.
Owner explicitly requested implementation; code committed, canonical app68638
built/verified/signed and left closed. Do not launch an older app or resume the20FPS campaign.
Translator fully stopped; Xcode replay off. All previous CUA phases completed.
Old68633PID95871 was confirmed exited before native GPU tests. No new launch,
account/cache/settings/camera/region change by root during repair.

Read docs/metal/work/active/travel-correctness-repair.md. New code:
- Native ordered UI can render over retained PRE-UI world color+depth when
  current world is incomplete. Typed pool leases+native aliases, resize/device/
  Z compatibility, fresh cleared backing if needed. Early redundant main logical
  G-buffer clear removed; later borrowed writes exclude retained depth.
- Reduced/supersampled world resamples to full UI color+depth with original
  BilinearMirror encoded color and PointClamp raw depth. No Z conversion/flip,
  cached PSO/pool and completion-pinned command. Matching sizes add no GPU work.
- Classic/PBR fetched terrain Default/Null/immediate-revival, raw/sRGB and sampler
  behavior restored; generated height/composition remain authoritative.
- Complete empty main views continue through sky/lighting as original; explicit
  completeness/resource/query checks remain. Complete empty impostor bakes clear transparent; failed publication keeps
  dirty retry and old dimensions/values/time, with exact producer diagnostics.
- Object skinning validates twelve consumed XYZ fields, preserves valid data
  with nonfinite unused W. Actual bad XYZ operands get bounded diagnostics.
- Display-name pending state survives distance/status lines (inherited original
  bug), allowing stationary avatars to gain newly cached names.

Actual M1 Max Release tests passed: skin201/3; terrain272/1; impostor17070/1;
retained UI/resize/scale/rawdepth/HUD29130/3; existing UI/pool/impostor17964/4;
world four-MRT/clear-only31498/1.
GPU cases used Metal API+shader validation. Initial UI fixture selected textured
solid_color without a texture; changed to uniform_color and rerun passed.
Root performed builds/GPU checks; independent agents source-reviewed and caught
an actual earlier-clear lifetime problem, now fixed. This is not independent
fresh-checkout build evidence or full live acceptance.

Private exact commands/logs under ignored
artifacts/metal/private-travel-correctness-repair-20261110-01/.
Doctor/configure/build/final configure+build/verify_app/deep strict codesign all0.
Verified349 Mach-O files, no invalid nonarm64. Canonical26.4.0.68638 executable
SHA6942629c44614828ed327e563536376191c992c0d2af9ead44f2c819fc8c0eb0;
metallib bca357a8868e3c4a82c9e8eb21cc3b27fe783806e83651742e2919b4671c3f0d.
Exact build-final.json is in that private evidence directory. No source edits
remain beyond committed0cbb48f104; final handover docs can have a later SHA.
Next owner-driven travel should exercise UI input/resize, missing-world loading,
teleport, name arrival, HUD alpha, attachments and earlier lighting/terrain/water.
No automatic launch just for further source audit. Prior live68633 FAILED:
see travel-live-68633.md and indexed CURRENT-failed-travel-68633 archive.
Runloop active in10sCPUtrace did not prove visible publication/input delivery.

Full sky/interior/shadow/low-DD parity and all geometry spikes remain unverified.
Actual bad XYZ producer not observed; unused-W repair isn't proof of every spike.
Non1-scale final grain/dither still evaluates at world size before scaling.
Original particles-disabled avatar loading sphere lacks native producer. Prior
native avatar profiling/ART dispatch still needs actual requested live receipts.
All earlier97dd825 avatar resources/profile/body placeholders, cold-start/EEP/
probe continuity, HUD/alpha/upload and performance changes remain included.

Original8978files/1703059096bytes cache remains preserved privately under
artifacts/metal/private-empty-cache-start-20261110-01/original-cache;
do not restore over active cache. Never upload private captures/logs/cache.
