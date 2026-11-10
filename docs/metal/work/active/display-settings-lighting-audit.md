# HiDPI, HDR, emissive and Lightbox source audit

Owner requested original-versus-port support/toggle checks, then reported bright
sunlit areas, dark avatar/shadows, a change while using Lightbox and darkness
after disabling Auto Exposure. Exact reference
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; audit checkout
`98f2862ca91d32d1ebccd4458da67c0733e082dd`, renderer2c60e47b0e /running68643.
No renderer/settings/time-of-day changes, restart or new build in this audit.
Independent source audits plus root source review; no matched visual parity test.

## Support and settings findings

| Feature | Original and native contract | Finding |
| --- | --- | --- |
| HiDPI/Retina | RenderHiDPI, default on, visible Darwin checkbox, restart required | Connected; no changed sizing/toggle defect found. |
| Internal scene HDR | HDR and Emissive checkbox uses RenderDisableVintageMode to set RenderHDREnabled and RenderEnableEmissiveBuffer | HDR settings/resource/format/exposure/post path connected; emissive OFF defect below. |
| Physical display HDR/EDR | Separate from internal floating-point scene lighting | Current production surface is BGRA8 +sRGB +EDR false. Original also tone-mapped to SDR; optional original10-bit SDR differs from native8-bit precision. |
| Opaque/masked PBR emissive OFF | Original HAS_EMISSIVE suppresses deferred emission | **Incorrect native toggle:** contribution remains enabled. |
| Legacy Glow ON | Runtime latch must accept the active executable pass owner | **Incorrect native toggle:** callback requires GL handles from native metadata programs. |

HiDPI: settings.xml7864→llappviewer.cpp606→llwindowsdl.cpp288 preserves the
original HIGH_PIXEL_DENSITY window property. Metal surface uses SDL pixel size
as drawableSize, without multiplying contentsScale again; resize updates precede
viewer reshape and actual drawable mismatch is rejected. Original logical/pixel
coordinate and UI-scale functions match. UIScaleFactor and world resolution
divisor/multiplier are separate controls. Inherited HiDPI tooltip still says
OpenGL. No live off/on/restart/multiple-display test was performed.

Scene HDR: advanced graphics checkbox at XUI746 is connected through
llviewercontrol.cpp304–318 and1003–1006 to normal render-input reconciliation.
Native formats, environment lighting, exposure textures, bloom and tone mapping
consume the HDR latch. Surface defaults (almetalsurface.h34–42), production
creation (llwindowsdl.cpp363–368), and CG color space/layer configuration show
that this is not a monitor HDR switch. Original optional RenderGLContext10bitSDR
is forced off on Darwin at llappviewer.cpp573; this concerns SDR precision.

## Confirmed conversion defects

1. Original class1/deferred/pbropaqueF.glsl144–146 and gbufferUtil.glsl29/48
   condition opaque/masked emission on HAS_EMISSIVE. Native PBR fragment
   almetal_builtin.metal5321–5343 always writes texture times emissive factor;
   native base12902/13058 always samples/adds it. G-buffer/base scenes carry HDR
   but no independent emissive-enabled input. Thus checkbox OFF changes HDR
   storage but does not reproduce original deferred emissive suppression.
   Enabled texture/factor/UV/color views are present. Original PBR alpha and HUD
   add emission inline regardless of HAS_EMISSIVE; a blanket suppression would
   incorrectly change them. Repair must propagate the actual emissive setting
   through affected deferred producers/consumers and preserve those exceptions.
2. llviewercontrol.cpp301 still computes sRenderGlow using both glow programs'
   isComplete(). LLGLSLShader::isComplete (h539) tests mProgramObject, while
   configureNative (cpp997–1030) sets native metadata without a GL program name.
   isAvailable (h541) includes native configuration. Turning Glow on therefore
   leaves native glow off until shader reload restores the setting-based latch.
   Use native-aware availability here. HDR bloom is a separate original path;
   this does not establish the reported bright scene's cause.

These are source-confirmed defects, not newly executed GPU/UI toggle failures.
No repair applied in this requested check. Follow-up validation should cover
HDR/emissive independent combinations, opaque/mask static/rigged PBR, terrain/
impostor/resource invalidation and unchanged alpha/HUD behavior; Glow should be
tested off→on without a shader reload in its non-HDR owner.

## Lightbox, apparent time and brightness

All18 scoped Lightbox/Look/EEP/control/XUI/Neutral files match original. Opening
Lightbox does not load Neutral or change sky/time; actual Look selection or
Revert can load aesthetic settings. The Look whitelist excludes environment,
time and HDR/emissive. Explicit Sky Freeze/day-slider/day-preset/Restore callbacks
can select a local sky. Programmatic combo/slider display updates do not commit.
No source-proved new opening side effect was found. Neutral label alone is not
proof that opening applied it. User interactions/live values were not recorded
as a controlled before/after event sequence, so their exact cause is unknown.

About Land's apparent-time label (llpanelenvironment.cpp926–968) derives a
fraction from corrected time, day length and offset. Actual selected environment
and its sky's sun rotation drive rendering (llenvironment.cpp1476–1520;
llsettingssky.cpp1352). These files match original. A local override or authored
day-cycle keyframes can differ from a wall-clock expectation; the9:19PM label
does not alone prove a sun-direction defect or validate this scene's lighting.

Auto Exposure OFF uses manual Exposure ×1 in both renderers. Default ON can
amplify a dark scene up to8× (minimum EV−3). Luminance coefficients, log/EV sign,
histogram bins/range/percentiles, masks, adaptation, min/max and tone-map ordering
match in the bounded audit. An initially suspected no-fade/reset discrepancy was
withdrawn after tracing the original live caller: both live histories startEV0.
No settled Auto Exposure math regression proved. The screenshot's manual1 and
ACES mix0.70 plus darker OFF output are not a matched original comparison.

Sunlight/ambient/probe packing refreshes each view. Original PBR3×, classic
1.35/1.1, sRGB decode/store, linear HDR accumulation and haze transfer were traced
without finding a large extra scale/gamma factor. This is negative source
evidence, not visual acceptance. The owner's overbright/dark-lighting complaint
remains OPEN; capture matching effective Look/graphics, active EEP/sky rotation,
exposure state and original/native output before tuning renderer brightness.

## Evidence and runtime limits

Ignored `artifacts/metal/private-hidpi-hdr-emissive-lightbox-audit-20261110-01/`
contains copied hashed audits, exact environment source-diff receipt and four
owner screenshots. Raw account/location data stays private. No uploads.
CUA attempted CmdP to inspect Preferences: first rejected as inactive control;
later batched call timed out/reset. Refreshed state showed Lightbox. No checkbox
state was successfully read, no value/sky/Look control was changed by root, and
the final CUA observation phase was completed. Saved-profile absence of an
override was not promoted to live-toggle proof. Viewer remains owner-controlled.
