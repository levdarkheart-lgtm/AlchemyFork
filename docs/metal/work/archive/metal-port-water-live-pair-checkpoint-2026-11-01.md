# Matched live-water execution — 2026-11-01

**Work item:** M-008, still `doing`.  
**Candidate source:** `fd0fa7cea56e3a9ba933f1405a6be68e9099374e`.  
**Reference source:** `8a68422d107600f0074923eb2191f915e742e8e8`.

## Executed comparison

1. Rejected the manual-flight attempt and used actual Computer Use to fly and
   Map-teleport both isolated viewers to `Grignano (128, 228, 74)`.
2. Selected Midday, opened the viewer Snapshot flow, and captured fresh
   2048x1476 PNGs with identical visible Snapshot controls.
3. Captured both post-exit effective graphics contracts. The fail-closed tool
   compared all 416 settings with zero mismatches, missing requirements, or
   one-sided settings.
4. Verified camera/focus X/Y identity within serialized precision, about 1 cm
   Z difference, pinned candidate/reference source and executable provenance,
   and clean viewer/helper shutdown.
5. Retained raw SSIM 0.870878 and PSNR 26.392383 dB as temporal descriptive
   metrics. Human inspection found the frames materially the same; structural
   blurred whole-frame SSIM was 0.993616 and the bounded water/sky crop was
   0.998397.

## Priority decision

The accepted pair is bounded live evidence for the corrected default-probe
water fixture. Repeating it would add activity without advancing another
release criterion. Continue source-led M-008 work through the first
demonstrated probe/mirror or projector/cascade-shadow divergence, then
transparency/material replay, non-default post, and advanced output. Require an
asymmetric physical control before another live replay. Full feature parity,
strict no-GL, stability, independent release proof, and qualifying paired M1
Max performance remain open.
