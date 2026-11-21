# Maintenance candidate 69445 — 2026-11-21

Status: **ready for owner testing**.

Candidate: `/Users/dekstroza/Alchemy-candidates/Alchemy Metal Maintenance 69445.app`

Version: **26.4.0.69445**. Source: `a2aaaf29421dd49821606457923f246f73f77318`.
Indra tree: `6e3b13c1f4076c5244bfa708ce564d8e03cd6cdd`.
Executable SHA256: `0c5de0c90878f85c7e71296c279d303adda475be39a9c31923b10a942e2b9d89`.
Metal library SHA256: `6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.

Accepted 69417 remains separately preserved and its executable hash was rechecked.
No viewer launch, login, saved-setting change or live performance recording was
performed. All changes are local; pushing requires the owner's explicit approval.

## Implemented scope

- Connected texture-cache priority to fair request promotion; moved fast-cache
  reads and normal cache completion handoffs onto explicit worker ownership. Added
  bounded, generation-checked ready-result delivery and accepted-decode cancellation.
- Validated and logged worker configuration; replaced sleeping retries, coalesced
  housekeeping and protected shutdown/request lifetime. Repaired timed queue
  deadlines and earlier-work wakeup; foreground housekeeping admission is nonblocking.
- Removed ResourcePlan hit-path ownership construction, reused completion-aware
  lighting pages and prepared required point-light pipelines during renderer creation.
  Made required geometry ownership explicit, reply budgets live and removed the
  queue-count-based foreground file sleep.
- Removed eleven obsolete backend settings plus thirteen abandoned experiment
  controls, with exact old-profile retirement. Corrected native graphics gates/help
  and disconnected preference-panel callbacks on destruction. Retained quality,
  media, mesh, waves, HiDPI and other live controls.
- Removed abandoned world preparation, banks/rows/animated GPU records, world GPU
  visibility, orphan resource tables, rejected main-light overlap and rejected
  forced probe consumers. Their exclusive tests are gone; shared CPU, terrain,
  probe, shadow and rigged-output/lifetime tests remain and are explicitly registered.
- Made OpenHTJ2K an unconditional production dependency and removed unreachable
  alternate pixel decoding. OpenJPEG header parsing/encoding is preserved. Pinned
  dependency checks, actual decode tests, required licence and attribution remain.

## Qualification

Host: Apple M1 Max, 64 GiB, macOS 26.5.2, Xcode 26.6.

| Check | Observed result |
| --- | --- |
| Doctor and full normal Release build | Pass at the exact source above. |
| CPU / asset / settings selection | 55/55 pass, including actual OpenHT primary/auxiliary decoding. |
| Native rendering / lifetime selection | 56/56 scopes pass with API and shader validation; no skips. |
| Pipeline cache | Ordinary, fully instrumented and selective-instrumentation checks pass. |
| Tooling unit suite | 184 pass; one optional retained serializer-stream fixture skipped. |
| Strict source GL gate | Zero blocking findings and zero scan errors. |
| Staged application | Signature pass; all 346 Mach-O files ARM64-only. |
| Complete source/stage inventory | Equal: 7,839 regular files, two internal symlinks; no external symlinks. |
| Decoder/package identity | OpenHT decoder symbols, exact required licence, attribution and production engine test; no live viewer collection. |

The main executable has bundle-relative and system Swift search paths, with no
source/build-directory rpath or direct OpenGL/Vulkan framework dependency. This
is static qualification; runtime context and foreign media subprocess gates remain
separate. The candidate is for owner testing, with no whole-viewer FPS gain or
complete-product/release claim.

Receipts, inventories, exact commands and retained failed attempts:
`artifacts/metal/production-convergence-20261121/`. A compact copy is also at
`/Users/dekstroza/Alchemy-candidates/Maintenance-69445-receipt.json`.

## Remaining work

The [execution ledger](../work/active/production-convergence-20261121.md#explicit-remaining-engineering-actions)
contains concrete next actions and boundaries. In particular, PPD-556's conditional
full-capacity request/retry hazard remains open. Its coherent fix requires reusable
request-owned scheduling entries and explicit bounded admission across callers.
Increasing capacity or dropping accepted retries would not resolve that contract.

Other retained work includes completion/service-age instrumentation, frame-triggered
HTTP progress, cancellation-aware permit admission, General job separation, immutable
geometry/render preparation, safe reply service around GPU waits, measured credit
policies and upload CPU preparation. Memory and GPU-completion ownership bounds
remain in force. Owner runtime checks should cover loading/teleport/refinement,
preferences, shader reload and shutdown; broad parity and prolonged stability stay open.

[Defect register](../POST_PORT_DEFECTS.md) retains PPD-527–559 and all prior history.
Automated checks do not close owner incidents or approve pushing.
