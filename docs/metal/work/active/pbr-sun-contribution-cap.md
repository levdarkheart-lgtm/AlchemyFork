# PBR sun contribution cap — PPD-085

Status: open, source-confirmed missing upstream boundary. No repair selected.

Original1eb44bb deferredUtilF:1048 evaluates
`clampRadiance(nl*(diffPunc+specPunc))*sunlit*3*scol`, cap65504.
Current native almetal_builtin.metal:13233–37 uses the same ordering and
65504 helper at11872–74. Classic finalSun at13223–26 likewise uses65504.
Final upstream408 deferredUtilF:1040 evaluates the complete contribution inside
`clampRadiance(...*sunlit*3*scol,MAX_PUNCTUAL_RADIANCE)`, punctual512;
classic finalSun also512. Ordering change07094f82fe6097f6e3b34d099dd7552f2f742624;
limits977e51afe94e80045ad75140e5d4248147fb74ff.

Native completed-base bound32752 at13346–49 is already present and matches
final softenLightF:151–159. It prevents nonfinite storage, but under extreme sun
could ratio-scale ambient/IBL together instead of bounding only the sunlight's
share before combination. Need actual native extreme-input physical reproduction
and original/final oracle before changing shared shader ownership.

This does not explain PPD083 by itself: old1eb reference and native use the same
sun ordering. Ordinary pavement inputs and relevant intensity are unmeasured.
Private source excerpts/hashes/report retained under
artifacts/metal/colour-cause-20261113/alchemy-surface-arithmetic-audit-20261113/.
Root owns canonical pipeline/shared shader. No code edit, closure or push.
