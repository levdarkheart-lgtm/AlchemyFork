# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `011ede6cf9` (`feat(metal): retain native reference still`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. A source-led post audit found the
native base-light, local-light, atmosphere/water-fog, map-decode and exposure
contracts aligned with their active GLSL owners. It did find one concrete
production gap: Lightbox reference stills were GL-owned, so an active wipe made
the otherwise implemented native final-presentation pass fail closed.

Commit `011ede6cf9` adds an exact-format, exact-size GPU-only copy of the
post-AA/CAS display source. It joins the renderer's active command batch,
publishes only after commit, invalidates stale peers on recapture/resize/failure,
and supplies the existing Metal wipe/side-by-side presentation shader. GL
remains authoritative while broader parity is incomplete.

Actual Computer Use on build 68016 authenticated in-world, opened Lightbox,
grabbed a still, observed the 0.50 wipe seam, switched to visibly duplicated
side-by-side views, cleared the still, restored the original isolated-profile
Look and logged out cleanly. No native capture warning, Metal validation error
or command-buffer error appeared; `Goodbye!` and a zero-process census followed.

The prior controlled authored-mirror pair remains valid evidence: post-run
manifests prove 416/416 identical effective visual settings (quality `3`,
mirrors enabled, resolution `512`) before any image metric is accepted. Full,
mirror-frame and mirror-interior scores were respectively SSIM `0.987409`,
`0.988105`, `0.991375`; that one fixture rejects the alleged large blur defect
but does not close broader mirror/probe parity. Comparisons now fail closed on
any mismatched, missing or unilateral visual setting.

## Build and test evidence

- `tools/metal/doctor.sh`: macOS 26.5.2 arm64, MacBookPro18,2, Apple M1 Max,
  64 GiB unified memory, Xcode 26.6; Apple7 and unified-memory checks passed.
- Warnings-as-errors RelWithDebInfo build 68016 linked and staged. Deep strict
  signing passed; `tools/metal/verify_app.sh` verified 349/349 ARM64 slices.
- Executable SHA-256:
  `ce435cf3030c93b179b24d5db08d03d5994c4e1b2b66ffb58000d06f29aec9d0`.
- Focused physical final-presentation validation passed 1,850 assertions. The
  complete Metal validation suite passed 44,871 assertions across 50 cases.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 tests in 242.67 seconds.
- Exact app/profile/version/hash preflight passed before Computer Use bound the
  build. Semantic login, Grab, wipe, side-by-side, Clear and confirmed Quit all
  succeeded without reading or recording credentials or account-bearing media.
- `validate_harness.py` validates 31 required files. All 24 tooling tests pass,
  including rejection of stale skill-relative paths. Python bytecode, diff
  checks and the corrected `docs/metal/benchmarks/scenarios.json` route pass.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies repository-declared `llsd 1.2.4` and PyYAML;
  `llsd` is Linden Research's `secondlife/python-llsd` package, not Bison.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-008 native renderer | doing | Broad native world/UI owners and physical regressions exist; advanced-family parity remains incomplete. |
| M-008 advanced views/effects | doing | Native reference wipe/side-by-side now owns a coherent GPU still; broader post, alpha/PBR, probes and advanced views remain open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path preflight and semantic login/in-world/control/logout operation are verified. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A real retained Alchemy editing feature
now has a coherent native Metal data owner at its exact presentation boundary,
with physical GPU coverage and actual in-world UI acceptance. This is bounded
M-008 progress, not broad feature parity, Metal-only completion or a performance
claim.

**Deferred critical work:** remaining post/alpha/PBR/probe/advanced-view parity;
full feature-matrix and stability coverage; viewer-owned OpenGL removal;
independent build/review evidence; and paired quality-equivalent M1 Max measurements.
These remain requirements, not waivers.

**Evidence-based priority:** the inspected base/local-light/fog contracts and
the matched mirror fixture do not justify more activity there. Continue the
source-led M-008 pass graph at the next unverified post or alpha/PBR boundary;
implement only the first demonstrated contract gap and add an asymmetric
physical control. Require identical post-run graphics manifests before scoring
any new reference/candidate images.

## Exact next actions

1. Audit the next open post and alpha/PBR owners against active GLSL call sites,
   resource formats, state and pass ordering; record the first concrete gap.
2. Add an asymmetric physical negative control, implement the bounded owner,
   then run focused and complete Metal validation plus the neutral test gate.
3. Keep strict no-GL, feature parity, stability and performance gates open until
   their documented release evidence exists.
