# Temporal through-glass stability — PPD619

Starting branch `codex/aurora-native-temporal-20261126`, HEAD
`74c07274bb908df84648610b948259bb88c55004`; preserved69516 application source
`b6f18c24f4e32e5e8897f782c0e4b65bda521f8f`. Owner observes substantial live foliage
improvement but strong jitter through a glass fence. The actual fence material
is not identified by existing old69503 ROI records; those five draws were opaque
and cannot identify this new transparent producer. No local viewer session is active.

## Source contract and bounded reproduction

Generic standard-alpha and single-slot PBR are qualified for exact color/Temporal
coverage and reliable equal-motion layered history. Legacy materials and multi-slot
PBR are excluded and force rejection1. The Aurora adapter maps any positive input
to upstream composition, while transparency reactivity is independently generated
from genuine opaque/final HDR. This is a confirmed source candidate, not attribution
to the owner's particular fence.

Legacy coverage is max(diffuse.a,glare)*vertexAlpha, not diffuse alpha alone.
Original reference `upstream/develop`408fd6c014e6,
`class3/deferred/materialF.glsl`262/392 confirms the contract. Exact glare depends
on material/normal/specular data, lights, probes, shadows, SSR and hero inputs.
PBR coverage uses selected per-slot transformed base texture times streamed vertex
alpha; LLFace already bakes baseColorFactor. Tests must use those actual producers.

The focused fixture will render a tinted pane over detailed static geometry with
an unobstructed control, production jitter, renderTemporalMotion and native Aurora.
Retain raw masks, generated reactivity, motion/depth and consecutive final outputs;
measure temporal stability without treating intended tint as an error. Preserve
moving/animated/disoccluded protection, zero-contribution visibility and the
palm-quality baseline. No reconstruction shader algorithm changes are authorized.

## Demonstrated cause and correction

The valid stationary fixture reproduces full unsafe-history rejection (255/255)
for legacy material BLEND and two-slot PBR BLEND, with valid previous poses,
zero unjittered layer motion and **zero independently generated Aurora transparency
reactivity** in the measured region. Single-slot PBR produces rejection0 and is
an unchanged control. Actual opaque/final HDR inputs are bit-identical between
baseline and candidate across every retained phase, including pane tint/attenuation.

A localized **test-only** rejection override leaves all other inputs unchanged.
It lowers legacy stationary variation from.000901808 to.000686735 and raises edge
detail from.028473880 to.035373061. This demonstrates the composition-policy effect,
not an isolated algorithm time or proof about the owner's uninspected fence.
The production correction is not that override:

- Legacy color and Temporal replay share the actual shading/coverage helper,
  including max(diffuse.a,glare)*vertexAlpha, normal/specular UVs, reflection/
  shadow/SSR/hero/light data and exact minimum-alpha/water discards. Visible
  glare at zero diffuse alpha is represented correctly. The actual color
  calculation and intended glass appearance are retained.
- Validated PBR BLEND supports1–8 slots with the same per-slot transformed UV,
  streamed vertex alpha and populated-slot selector clamp as color (PPD620).
- The existing standard-blend visibility, culling and depth rules still apply.
  Exact-zero contribution preserves background inputs; full coverage can replace
  hidden background; partial layers keep unsafe visible contributions and require
  identical representable RG16 motion for history acceptance. Ordinary glass is
  not rejected merely for being transparent. Unmodeled families remain conservative.
- The shared producer signal is explicitly unsafe-history confidence. MetalFX
  consumes it as reactive weight; the native Aurora adapter explicitly maps it to
  the pinned algorithm's composition marker. Aurora's separate opaque/final
  transparency-reactivity calculation is unchanged. No global mask clearing,
  jitter freeze, opacity change, blur or reconstruction shader change is retained.

## Matched rendered result

108 warm worlds,54 stationary worlds (three complete18-phase cycles),18 camera-
movement worlds and36 stopped worlds. One fixed authored scene, camera, rational
2/3 scale and production mip/jitter policy. Means below measure successive output
RGB changes in the pane-covered region; lower means steadier, not closer in tint
to the unobstructed image. Edge detail is measured separately to reject blur.

| Path | Covered RGB change, before→after | RMS change, before→after | Edge detail, before→after |
| --- | ---: | ---: | ---: |
| Aurora legacy BLEND |.000901808→.000710947 (−21.16%)|.001645373→.001172547 (−28.74%)|.028473880→.034480371 (+21.09%)|
| Aurora multi-slot PBR BLEND |.000900528→.000707894 (−21.39%)|See exact linked results (−29.51%)|.028634528→.034682233 (+21.12%)|
| Aurora single-slot PBR control |.000710783→.000710783|Unchanged|.034622804→.034622804|
| MetalFX legacy BLEND |.012875387→.001980242 (−84.62%)|See exact linked results|.026212885→.034898981 (+33.14%)|

Aurora gradient-change variation falls22.95%/23.26% in legacy/multi-slot panes.
The unobstructed Aurora control stays.001000411, with identical pixels. This
establishes a bounded through-glass stability/detail improvement; it is not live
owner-scene acceptance or FPS. Both Aurora baseline/candidate already meet the
existing1/255 mean-change bound, so that bound alone did not catch this defect.
The regression additionally checks the real producer mask, unchanged color and
output/detail agreement with the localized causal diagnostic.

[Exact metrics/per-frame observations](media/aurora-glass-20261126/glass-comparison.json),
[artifact identities](media/aurora-glass-20261126/manifest.json),
[stationary legacy comparison](media/aurora-glass-20261126/legacy-production-stationary54-12fps.mp4),
[multi-slot comparison](media/aurora-glass-20261126/pbr-multi-production-stationary54-12fps.mp4)
and [legacy move-stop sequence](media/aurora-glass-20261126/legacy-production-move18-stop36-12fps.mp4)
retain the output. Clips are explicitly12fps playback of fixture worlds, not viewer
frame periods. PNGs preserve the stated HDR-clamp/gamma display conversion;
private float inputs/output remain authoritative.36 stopped worlds permit
phase-matched adjacent-cycle drift checks, not an independent mature-reference
settling claim. Original recovered-area/settling failures remain unchanged.

## Correctness follow-through and limitations

The first two fixture attempts were invalid: a nonzero unused weight/cloth stream
violated the batch contract, then a Y-sign/winding mistake culled the geometry.
Both logs are retained and excluded. Mandatory actual foreground contribution and
background-detail gates now prevent accepting those empty scenes. Valid baseline03
fails14 assertions; it is not replaced by the earlier invalid attempts.

Candidate01 passes all quality/alpha/MetalFX cases. Three initial overlap checks
incorrectly demanded rejection throughout an ROI only62.17% covered by the new
unsafe layer. That oracle is replaced with same-jitter actual visible-contribution
checks; no image-quality bound is relaxed. Final regression/build results follow
in the delivery receipt.

The actual owner fence's family/slot layout is still unconfirmed. Perspective
foreground/background motion disagreement intentionally remains protected; this
repair does not make all layered transparency representable by a single vector.
Animated UVs, invalid poses, material/content changes and real disocclusions must
retain conservative history treatment. PPD621 closes a discovered same-allocation
image-content identity gap using the existing authored source revision; it does
not add another history or resource-retirement system.

Legacy coverage reuses the full lighting-dependent shader during the existing
motion pass. It introduces extra shading work for visible legacy glass, two
pipeline variants and constant bindings, **no new render pass or full-frame
target**. Its live cost is unmeasured; no Home FPS claim is made. Native/Spatial,
existing MetalFX modes/defaults, output-based shadows, native UI, successful
foliage reconstruction and the pinned Aurora algorithm are preserved.

Stopped output is steadier, but accepting useful history also restores detail
more gradually: Aurora legacy adjacent-frame change is.001108→.000703 in stopped
worlds1–18 and.000890→.000675 in worlds19–36, while same-phase cycle-to-cycle drift
increases.002109→.005129 as detail recovers. Multi-slot PBR shows the same tradeoff.
This is **not faster settling**; no mature-reference/trail-fraction claim is made.
Owner live review must judge that response, pane edges, reflection/transparency
and any remaining jitter in the original fence scene.

Final package identity, focused safety regressions, content-publication checks,
Release/signing/architecture results and confirmed delivery will be appended
before declaring a new review build ready.69516 remains preserved.
