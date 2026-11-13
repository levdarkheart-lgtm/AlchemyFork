# Neutral surface colour — PPD-083

Status: investigating. Owner confirms the same framing in the supplied comparison;
only the Metal exposure dialog occludes part of that image. Owner now identifies
a hue in the lower grey pavement/road areas of Metal14:08:20 versus more neutral
looking grey in original14:31:18. Keep separate from general brightness PPD081
and do not explain it away with the withdrawn camera/framing claim.

Existing raw image pair and exact byte hashes retained under ignored
artifacts/metal/exposure-comparison-20261113/. Need corresponding same-material
regions, lighting/shadow classification and image ICC/transfer inspection before
claiming a numerical colour shift. A pavement tile and asphalt patch are not
interchangeable neutral targets merely because both look grey. No analysis or
repair is established from differing arbitrary corners alone.

Owner requests restarting our Metal viewer to capture a cleaner image. Existing
reference PID10889 already normal-exited0/state10 at14:44:11UTC. Exact candidate
Sky26.4.0.68965/executable3f629d75890f1bed93f933f6f3f54e685e4c80a47d114b45d419808fc45ba777
launched underLLDB at14:45:37/PID12146, observed exact window logged in.
New immutable candidate-hdri profile/config deliberately sets split.5/EV-3 before
launch, retaining full agreed Look, graphics and Lev1233 camera controls. Actual
EXR resource is process-local; owner reloads it and captures. No current viewer
or display controls manipulated by agent; no new build/shader changes/push.

Next: inspect image metadata and same-material colour samples in the original
pair while owner supplies the new Metal image; use new image once available.
Root owns canonical docs and diagnosis. Keep exact colour measurements separate
from possible causes (lighting/probes, material, tone mapping, grading, colour
space, screenshot conversion); source-only earlier audit found no duplicate
exposure or sRGB encode. No defect closure or colour-parity claim.

Owner provides clean Metal macOS screenshot14:48:40 (3456x2234, matching reference dimensions), saved unchanged as owner-metal-clean-144840.png, and an in-viewer Snapshot PNG14:49:55, saved as owner-metal-viewer-snapshot-144955.png. Keep capture methods distinct: the requested side-by-side uses clean Metal14:48 and reference14:31 macOS screenshots; viewer export is additional evidence for isolating screenshot colour conversion. No pixel edits or alignment warps. Read-only corresponding-region/profile analysis delegated; results pending.

## Measured clean OS-image pair

Read-only analysis of Metal14:48:40/reference14:31:18 uses unchanged3456x2234
RGBA8images and direct same-coordinate same-material patches. Both contain the
EXACTsame4064-byte embedded Display ICC profile, SHA256
5662d0b79d03ba06449be9e660ba7a573d38aeaa4aff5c4ae8d1376978746579.
This excludes differing embedded-profile bytes as the explanation; it does not
prove identical live app/window colour transforms or raw lighting inputs.

Representative sunlit asphalt box[x600,y2010,x640,y2050],top-left pixel origin:
Metal meanRGB67.72/63.34/60.17 versus original74.51/71.67/70.67; medians67/63/60
versus74/71/70. Sunlit lower-left concrete[x35,y1920,x115,y2000] meanMetal
152.13/139.41/122.99 versus original158.93/148.23/135.01. Two concrete andthree
asphalt patches consistently show darker and relatively warmer/less-blue Metal
encoded pixels; both concrete samples already warm, so do not assume a neutral
albedo or claim whole-scene white balance from them. A common UI menu-grey patch
is31/31/31 in both. Initial proposed patch landing in vegetation was excluded.
No resizing, geometric alignment, image editing or resampling was performed.
Raw method/coordinates/measurements/report under comparison artifact/pavement-analysis/.

The in-viewer Metal SnapshotPNG3456x2168 contains no iCCP/sRGB/gAMA/cHRM chunk.
Keep its capture method/profile handling distinct from the two OS screenshots;
it is additional evidence, not a drop-in identically-profiled reference image.

Disposition: image-level colour difference is now measurable; renderer cause
still unproved. Small numerical/AA/filter differences are possible in a native
port; a systematic material-colour shift is not excused merely by API choice.
Intentional landed upstream fixes or differing runtime inputs/state can also
change output, because the original reference predates the new sky changes.
Correctness target is equivalent appearance for corresponding intended behaviour,
not forcing newer corrected algorithms to reproduce an older bug. Next localize
first differing producer: material/albedo, linear lighting/environment/haze,
then exposure/tonemapping/grading, then presented/exported image. Do not apply a
blanket colour/exposure correction before proving that boundary.
