# Preference-qualified shadow checkpoint — 2026-11-02

This archive records the bounded fixed-Midday shadow observation and the owner
prerequisite that supersede reliance on earlier Debug-Settings-only evidence.

## Qualification and provenance

Both pinned viewers used isolated profiles. Shadows were enabled and saved via
Graphics Preferences, both viewers were cleanly quit and restarted, and the
restarted Advanced Graphics panels displayed `ShadowDetail=2`. Effective
settings capture reported `RenderShadowDetail=2` in both profiles, 416 settings
per profile, zero mismatches and zero one-sided keys.

Exact-path preflight passed for:

- reference Development `26.4.0.67607`, executable SHA-256
  `fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`;
- candidate Development `26.4.0.68089`, executable SHA-256
  `f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`,
  built from renderer source checkpoint
  `b0e0973898da8e3a275b8a29ed72e7ce7f4e9db5`.

The candidate was launched with `ALMetalWorldPresentation=true`. Its runtime log
proved `STATE_STARTED`, direct Metal directional-shadow slices, direct Metal
projector-shadow slices, and continuous exact native world plus ordered UI/HUD
publication without GL swap. Both viewers ended with `Goodbye!`, stopped status
and exit zero.

Persistent effective-setting artifacts:

- `artifacts/metal/m009-shadow-reference-after-preferences.json`;
- `artifacts/metal/m009-shadow-candidate-after-preferences.json`;
- `artifacts/metal/m009-shadow-settings-after-preferences-comparison.json`.

The controlled screenshots remain local because they contain authenticated
viewer state. Their SHA-256 provenance is:

- reference fixed Midday:
  `02a3d08afac728968fd84d4a337d74c124e80d6f779ac9ba37df32298c92853e`;
- candidate native fixed Midday:
  `2e915324b75ad97bc0e156422d83c382cbad997de220843b4289d664d22eaaf6`.

## Bounded result

The upright fixed geometry and directional shadow placement/edges are coherent.
Whole-frame raw SSIM is `0.933767`, average PSNR is `22.091122 dB`, and
half-size sigma-8 structural SSIM is `0.968037`. Avatar pose and streamed media
textures are temporal differences. This preference-qualified replay
corroborates the existing fixed-Midday directional-shadow result.

It does **not** close the full shadow row. The public Grignano fixture did not
isolate a visible projector contribution. Deterministic projector-visible,
alpha/avatar-caster, all-cascade and PCSS/quality coverage remain open. Direct
Metal projector-owner execution is provenance, not visible projector parity.
Earlier evidence that set shadows only through Debug Settings is not
independently admissible under the owner-required Graphics Preferences, save
and restart workflow.

## Strict GL inventory and next boundary

The refreshed inventory scanned 2,408 viewer-owned files and reported 3,351
informational findings: 2,497 shader tokens, 784 GL-function findings and 72
SDL-GL findings. Strict mode exits 1 as expected. `LLWindowSDL` still creates an
OpenGL 4.1 context and the candidate executable still links OpenGL and Metal;
removing the context while GL remains fallback authority is unsafe.

Continue M-009 by reconciling these findings against reachability and
presentation authority. Resolve the next concrete gate, including current
hero-mirror exact eligibility where reachable, using current source and
physical evidence rather than removing a guard from an old fixture alone.

Exact RLVa parity remains deferred non-release nice-to-have M-013; the durable
inventory is `docs/metal/work/deferred/rlva-parity.md`.
