# Matched mirror and reference-still execution record — 2026-11-01

**Renderer checkpoint:** `011ede6cf9`

The invalid unmatched mirror run was corrected by comparing complete effective
graphics manifests after both sessions. The accepted pair matched all 416
visual settings; preset, mirrors, mirror resolution and every captured
per-feature setting were identical. The one-fixture mirror metrics and visual
inspection rejected the suspected large blur, so priority moved back to the
broader source-led M-008 pass graph.

That audit rejected false positives in base/local lighting, energy
compensation, atmosphere/water fog, map decoding and exposure initialization.
It found a concrete final-presentation gap instead: the existing native
reference shader had no production GPU still and therefore failed closed on an
active Lightbox comparison.

Commit `011ede6cf9` implemented the bounded owner and tests. Validation:

- Focused physical final-presentation case: 1,850 assertions passed.
- Complete physical Metal validation: 44,871 assertions / 50 cases passed.
- Architecture-neutral CTest: 484/484 passed in 242.67 seconds.
- RelWithDebInfo build 68016, deep signing and 349/349 ARM64 checks passed.
- Harness: 31 live files; 24 tooling tests passed.
- Actual Computer Use: authenticated login, Grab, wipe, side-by-side, Clear,
  Look restoration and confirmed clean Quit passed; zero processes remained.

This moved one retained Alchemy post feature toward the product. Remaining
advanced-family parity, prolonged stability, strict no-GL, independent review
and qualifying M1 Max performance evidence remain open. The next executable
action is the first demonstrated contract gap in the next unverified post or
alpha/PBR owner, with an asymmetric physical control before implementation.
