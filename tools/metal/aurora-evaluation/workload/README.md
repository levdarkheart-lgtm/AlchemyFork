# Target-size cost inputs

`input_workload.c` is one shared producer linked into both runners, compiled with Apple clang at `-O3 -ffp-contract=off`. It creates no disk image dataset. It is a reconstruction **cost workload**, not another quality qualification.

The fixed grid is 2304×1446 with a 3456×2168 output. Eighteen positive image-displacement Halton phases match the existing two-thirds policy. For every input pixel the producer evaluates the static scene at `(x+0.5-jitterX,y+0.5-jitterY)`.

A right-handed infinite reverse-Z perspective has near plane0.1m, vertical FOV90° and the output aspect ratio. Frontoparallel geometry carries textured stone at12m, an HDR/glow panel at4m and fine opaque/masked foliage at2m; uncovered sky has clear depth0. Color/depth coverage is authored together. Motion is zero in previous-minus-current input pixels, excluding jitter, because the scene and camera are static. A scene projected onto these planes supplies the represented image footprint; the projection is not invented for the old identity-projection fixture.

Color is finite linear HDR RGBA16Float; alpha is current glow. Depth is R32Float; motion is RG16Float. Both before-transparency and final color are identical; no transparency or animated-texture marker is supplied. MetalFX uses a zero reactive map. Aurora generates its own masks after the established R32→Depth32 and RG16pixel→RGBA16UV conversion. Exposure is1. This deliberately samples the opaque/MASK workload implicated by the retained quality comparison; it does not represent arbitrary transparent, disoccluding or moving-content cost.

Each runner uploads the same eighteen immutable phase images once (~915MiB shared input storage). It GPU-copies the selected phase into fixed input textures before the timed reconstruction command. That copy simulates a fresh producer and is **excluded**: the viewer's render passes already produce those fixed inputs. The two methods therefore receive matching cache preparation. The actual Aurora format/sign-unit adapter is included in reconstruction time. Native MetalFX also includes Alchemy's required glow-alpha resolve; Aurora's unchanged graph includes its display-depth pass.

The transient CPU fill buffers are reused between phases and released before warm-up. Per-phase sequential FNV-1a fingerprints cover color, depth and motion bytes (initial14695981039346656037) and must match between runners. Fingerprints are identity checks, not cryptographic authenticity. Raw workload images are not exported.
