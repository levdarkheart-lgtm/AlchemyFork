# Optional MetalFX spatial prototype — 2026-11-24

Base: clean usual-branch7911e382ce. Separate control and implementation worktrees
preserve earlier research work. PPD591 owns implementation and qualification.
No source push or production-default change is authorized.

The accepted private Review69454 split supports reducing main-image-dependent work
while retaining the original shadow budget. Those53/55-probe recordings do not
substitute for measurements on the newly built executable. Native default remains.

The new fixed-half mode explicitly separates scene, output and shadow-reference
extents. Native preserves existing divisor/multiplier/shadow semantics. Half/MetalFX
use half output extents, full output-sized shadow reference, and native UI. Global
shadow settings are never compensated. Probe/hero allocations remain independent.

MetalFX reconstruction is limited to ordinary main views with completed FXAA or
SMAA. With AA off or MetalFX unavailable, the same half scene uses linear scaling
and normal CAS. An admitted MetalFX pass replaces CAS sharpening without changing
its saved value. Debug/special main views also use half-scene linear fallback with
the full shadow reference; explicit snapshots retain their existing independent
size override. Final lens/print effects and native UI remain after reconstruction;
scopes and reference capture retain the original internal-image boundary.

Classic MTLFXSpatialScaler is new native functionality with no upstream equivalent.
The nearest original contract is1eb44 display/AA/CAS/final-blit/UI sequencing.
Apple recommends antialiased, tone-mapped perceptual input and reuse of scaler
instances: [MetalFX spatial guidance](https://developer.apple.com/videos/play/wwdc2022/10103/),
[perceptual mode](https://developer.apple.com/documentation/metalfx/mtlfxspatialscalercolorprocessingmode/perceptual).
Actual supported formats/usages, command ownership and color/alpha boundaries must
be validated on this M1 Max. No exact-quality or throughput result is claimed yet.
