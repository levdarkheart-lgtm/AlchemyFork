# HDR ordinary probe publication format — PPD-086

Status: investigating, actual owner-runtime failure reproduced in retained logs.
Registered before a corrective edit. Root owns canonical manager/pipeline/shared
format contract; independent agent owns a new bounded physical regression test.

Owner-candidateSky68965/PID12146 viewer.log (private exposure-comparison artifact)
records at14:46:01 and14:48:01UTC:
`Could not publish native reflection-probe face 5: Metal texture copy requires matching pixel formats`.
There are exactly two matching warnings. Native reset records at14:45:53 and
14:47:59 report no completed default retained. This is actual runtime evidence,
not a speculative source-only candidate or evidence from probe-only post gates.

Current producer ALMetalWorldGBufferRenderer::updateReflectionProbeFace at12738
chooses rg11b10_float forHDR; filterReflectionProbeRadiance at12965 preserves
sourceformat. Manager bootstrap382,publication852,reset2556 still choose
rgba16_float. ALMetalTexture::copyFrom requires identical formats. Need inspect
which publication copy fails and exact generation state before changing code.
Face5 may already set validfaces0x3f before array publication fails; retry
admission expects preceding0x1f, so scheduler can stall rather than recover.

A failed first/default generation can leave black radiance/zeroSH bootstrap.
That is a plausible missing cool environment-light source for owner PPD083,
but exact main boundSH/readiness and corrective owner comparison remain required.
Keep PPD081 pre-existing overexposure and intentional old/new sky-ray differences
separate. No source repair, closure or push at initial registration.
