# HDRI preview exposure — PPD-080

Status: investigating. Canonical10b1ab3e72, renderer sourceefc3dc17d0,
owner testing Sky26.4.0.68965. Five pre-existing untracked research notes remain
untouched. No source change, viewer restart, UI manipulation or push authorized
by this investigation. Owner controls the current viewer.

Owner images13:48:26/13:49:29/13:50:20 show HDRI split.5 with a washed-out left
sky, severe bloom around foliage and ordinary-sky cloud detail on the right.
The debug settings screenshot confirms RenderHDRIExposure0.000. Personal
Lighting HDRScale is an ordinary environment term, not this separate EV control.
Owner says no clouds are visible. Whether a subsequently suggested-2EV was
applied or changes the preview is pending owner response; do not assume it.
Images copied to ignored artifacts/metal/hdri-preview-exposure-20261113/.
The downloaded asset was recommended as Kloppenheim06(Pure Sky),2K EXR; exact
loaded file/hash/sample data and a reference same-file runtime are unverified.

Native ALSky::captureNativeView reads RenderHDRIExposure, sets transfer[0] to
powf(2,EV); modern shader samples linear HDRI, multiplies transfer.x and bounds
the final floating store. It preserves independent split and common postprocess.
Final408 ALSky::bindComposite sets SKY_HDRI_SCALE to the same powf(2,EV).
A visible mismatch does not alone prove that this correct-looking source path
executes correctly with the owner's actual texture, settings and exposure.

Next: inspect final upstream composite/meter/post boundary and actual EXR if
available, confirm live EV response, preserve evidence and choose a concrete
fix only if an incorrect producer is identified. Do not lower global exposure,
change metering/lighting policy or clamp away HDR merely to hide the symptom.
No renderer fix, runtime repro beyond supplied screenshots, closure, owner
acceptance or publishing permission is claimed.

## Owner retests and source audit

Owner Midday images confirm global exposure1.0,ACES,mix.70,auto exposure enabled,
HDR/emissive andSSR enabled. EV-2 screenshot13:52:18 shows some clouds;
EV-3 screenshot13:52:46 shows clear cloud/blue detail. This answers the pending
EV response question positively; the EXR is not simply missing cloud content.
Native source retains upstream linear EXR decode, mip0 sample times2^EV once,
no WindLight fake-HDR gain on the HDRI branch, optional classic1.1, sky alpha0,
luminance bloom then common exposure/tonemap. Meter sky weight0, if configured,
excludes both sky backgrounds and makes autoexposure follow geometry, as upstream.
No additional HDRI gain/color-space mismatch identified in source. No physical
same-file/reference rendering comparison has run. Actual loaded filename is
unknown: Downloads contains three EXRs, so presence alone does not identify it.
No asset decode or GPU behavior is invented from file existence.

Owner then clarifies ordinary sky and overall scene looked overexposed before
these changes: PPD-081 tracks that broader report separately. EV-3 improves this
file's preview, not a corrective verification of global rendering. Source
unchanged; no settings altered by agent and no viewer restart/closure performed.
