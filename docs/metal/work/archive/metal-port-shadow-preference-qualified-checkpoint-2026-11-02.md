# Metal port execution — preference-qualified shadow checkpoint — 2026-11-02

## Completed bounded validation

Reference and candidate shadows were enabled and saved through Graphics
Preferences in isolated profiles, both viewers were restarted, and both
restarted Advanced Graphics panels displayed `ShadowDetail=2`. Effective
capture matched all 416 visual settings with `RenderShadowDetail=2` and no
one-sided keys.

Exact-path preflight passed for the pinned `26.4.0.67607` reference and
`26.4.0.68089` candidate. The candidate used
`ALMetalWorldPresentation=true`; its log proved `STATE_STARTED`, direct Metal
directional/projector shadow slices, and continuous native world plus ordered
UI/HUD publication without GL swap. Both runs shut down cleanly with exit zero.

In the fixed-Midday Grignano pair, aligned fixed geometry and directional
shadow edges remained coherent. Raw whole-frame SSIM was `0.933767`, average
PSNR `22.091122 dB`, and half-size sigma-8 structural SSIM `0.968037`. This is
accepted as preference-qualified corroboration of directional shadows. It is
not projector-visible parity: the public fixture did not isolate a visible
projector contribution. Alpha/avatar casters, all cascades and PCSS/quality
coverage also remain open.

The effective-setting artifacts are:

- `artifacts/metal/m009-shadow-reference-after-preferences.json`;
- `artifacts/metal/m009-shadow-candidate-after-preferences.json`;
- `artifacts/metal/m009-shadow-settings-after-preferences-comparison.json`.

Authenticated screenshots remain local. Reference and candidate screenshot
SHA-256 values are respectively
`02a3d08afac728968fd84d4a337d74c124e80d6f779ac9ba37df32298c92853e` and
`2e915324b75ad97bc0e156422d83c382cbad997de220843b4289d664d22eaaf6`.

Earlier Debug-Settings-only shadow evidence cannot independently close a
shadow requirement. Future shadow pairs must repeat the Graphics Preferences,
save and both-viewer restart workflow.

## Refreshed M-009 boundary

The informational audit scanned 2,408 files and reported 3,351 findings:
2,497 shader tokens, 784 GL-function findings and 72 SDL-GL findings. Strict
mode exits 1. `LLWindowSDL` still creates the fallback-authority OpenGL 4.1
context and the executable still links Metal and OpenGL, so context removal is
not yet safe.

Next, reconcile the refreshed inventory by reachability and presentation
authority. Resolve the highest-value remaining gate, including current
hero-mirror exact eligibility where reachable, with source and physical proof.
Do not remove its guard from old fixture evidence alone. Continue controlled
parity/stability work. Exact RLVa parity remains deferred nice-to-have M-013.
