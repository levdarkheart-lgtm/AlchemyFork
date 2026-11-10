# Owner-driven live 68638 — continuous geometry corruption

Observed2026-11-10 after owner explicitly requested starting the viewer and
observing logs. Root launched the exact canonical Release26.4.0.68638 app via
CUA full bundle path. Login window/title observed; process27603 exact executable
path checked. Executable SHA256 remained
`6942629c44614828ed327e563536376191c992c0d2af9ead44f2c819fc8c0eb0`.
Renderer checkpoint0cbb48f104; later handover commits change documentation only.
No launch overrides, settings/cache/account edits or agent-driven login/travel.
Owner subsequently logged in. Earlier20FPS campaign remains paused.

## Visible result — FAIL

Owner directed root to inspect the screen and clarified that corrupted shapes
keep dancing across it: continuously rendered junk, not a frozen covering image.
Root independently inspected two CUA screenshots of this exact running viewer.
Large skin-colored and dark triangles spanned the world in substantially different
arrangements. Ordinary world surfaces, avatars, name labels and HUD elements were
also visible. No agent camera/input/settings action between observations.
These were diagnostic observations, not reference/candidate parity captures.
CUA tool images are retained in the conversation; no separate saved image-file
artifact is claimed. All CUA observation phases completed normally.

The unused-W affine-classifier fix is not proof of the cause of this failure.
Actual live geometry corruption remains unresolved. Advancing publication
receipts and changing rendered shapes distinguish this from a static old-frame
presentation symptom; they do not establish all UI/input/resize behavior passed.

## Fresh log evidence and limits

Ignored evidence directory:
`artifacts/metal/private-68638-owner-log-watch-20261110-01/`.
Preflight records exact app/hash and old log identity before launch. New log has a
different inode from68633; launch/startup snapshots, incremental windows, summaries
and full corruption-visible log snapshots are saved privately.

- Startup reached STATE_STARTED. New native pre-UI-world/fresh-UI receipt appears
  during loading; complete ordered UI and surface-publication receipts continue.
- Entire fresh log through22:12:27UTC has ZERO nonfinite-XYZ operand receipts and
  ZERO old nonfinite-palette fallback warnings while corruption is visible.
  Also no terrain-missing-role or impostor-publication-deferred receipts observed.
- Initial bounded audit22:02:27–22:04:14UTC has12 early expected-cloud-layer
  incomplete warnings, all at22:03:55. Subsequent observed windows were quiet for
  that family. Historical wording says retained GL authority, but the actual
  viewer continues using native-only publication; it is not a GL fallback.
- Startup also includes failed material-asset requests and texture-fetch warnings.
  Their relationship to the changing geometry is unknown. Do not assert causality.
- No corresponding Metal validation/command-buffer-error or ERROR/FATAL signature
  was observed in these snapshots. This normal launch did not inject Metal API/
  shader-validation environment variables. Quiet logging does not validate finite
  vertex/index/palette data or identify the draw that produces the corruption.

Receipt counts are LOG ROWS, never frame/present counts. LL_INFOS_ONCE coalesces
repetitions. No resize/scaler-success receipt exists; whether it ran is unknown.
No controlled FPS comparison or shadow/environment parity result was obtained.

## Next investigation

Keep the owner-driven scene intact. Identify the actual draw/resource generation
emitting corrupted vertices and compare its native vertex/index/weight/palette/
transform inputs to original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45.
Read-only source audit is in progress; no new cause or repair is established.
Do not add magnitude caps, omit avatars/HUDs or declare visual acceptance from
quiet logs or the previous isolated GPU tests. No source edits/build/restart
occurred during this observation. Viewer remains running for the owner.

## Subsequent repair checkpoint

The preceding sections preserve the failed live observation as recorded. The
owner later closed68638. Source comparison then found a specific classic-avatar
position-W shader ABI defect; [repair and GPU red/green evidence](classic-avatar-position-w.md)
are included in new verified build68643. This supersedes the investigation's
"no cause established" status for that defect, but does not convert68638's failed
live result into a pass or prove all loading/attachment artifacts repaired.
