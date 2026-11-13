# First upstream backport batch — 2026-11-13

Owner authorizes implementing the two First groups from the upstream review.
Do security/unsafe-read and shared crash/correctness repairs now; defer texture,
material, HDR/probe, geometry/sky modernization and other later groups.
The owner says the current app need not be preserved. Proposed fixes remain
local until owner testing and explicit approval to push.

## Source and ownership

- Integrator: canonical `codex/apple-silicon-metal`; isolated preparation was
  `codex/upstream-first-batch-20261113` at
  `/private/tmp/alchemy-upstream-first-batch-20261113`.
- Baseline `bbd5193f99` reconciles accepted publication `f645294395` with parked
  local notes `5a403168af` and the retained upstream audit/register.
- Upstream source snapshot `408fd6c014`; actual original/current paths inspected
  before backporting. Native Scene Monitor texture ownership is retained.
- Objects worktree owns object/primitive/data-packer/name-value/particle readers
  and safe local task/mute transfer targets. Protocol worktree owns other
  message/circuit/ring/Xfer/terrain/wind readers. Foundation worktree owns mesh
  decode bounds, UUID generation and color-font painter. Integrator owns
  login/HTTP/IM/app-link/disconnect/cache lifetimes and integration.

## Executable scope

| Defect | Change | State |
| --- | --- | --- |
| PPD-017 | HTTPS redirects at login and HTTP transport | ready_for_owner |
| PPD-018 | Independent local Xfer filenames and short-name handling | ready_for_owner |
| PPD-019 | IM copied-size clamp and bounded bucket text | ready_for_owner |
| PPD-020 | Packet ID domain guard | ready_for_owner |
| PPD-021 | Object/particle/TE/extra/motion/layer/Xfer payload bounds | ready_for_owner |
| PPD-022 | Mesh face/index work caps and skin weight reads/initialization | ready_for_owner |
| PPD-023 | Retain empty world at disconnect; Scene Monitor guards | ready_for_owner |
| PPD-024 | Thread-safe random UUIDs; deterministic hashes unchanged | ready_for_owner |
| PPD-025 | Reliable packet queue, duplicate/ACK/history handling | ready_for_owner |
| PPD-026 | Cache-entry removal lifetime prerequisite only | partial; disk pairing later |
| PPD-027 | Same decoded floater name for policy and opening | ready_for_owner |
| PPD-028 | Clear reused COLRv1 painter before each glyph | ready_for_owner |
| PPD-043 | Preserve finite zero attributes after old tangent normalization | ready_for_owner |

PPD-026 disk/extras pairing remains later. Its removal lifetime corrections
`fdda86251c` and `79057e0958` are necessary now because malformed cached input
must finish reading before removal frees its storage. Do not pull region
retirement or the larger object-roster modernization as incidental context.

## Integrated corrections and focused evidence

Source checkpoint `1f635d6b75` includes selected object/protocol/foundation
commits, then `d1b8554520` short normal/UV bounds from upstream `2a599a405c`
and `1f635d6b75` zero-safe normalization from upstream `7a7296abfc`.
Independent review found the short-array omission before finalization. Its
public compressed-input test then exposed inherited final `normalize3(0)` NaNs:
18 checks failed. PPD-043 was registered before adapting the exact upstream
`unit3_or_zero` boundary to the retained old tangent algorithm. Ordinary nonzero
normalization and tangent handedness remain unchanged. A second review found
no actionable issue. PPD-044 retains the unmeasured high-valence optimizer cost
report for the later geometry work; this batch does not bound every decode cost.

Retained focused evidence:
[objects](../../research/upstream-first-object-validation-2026-11-13.md),
[protocol](../../research/upstream-first-protocol-validation-2026-11-13.md),
[foundation](../../research/upstream-first-foundation-validation-2026-11-13.md),
[integrator peer review](../../research/upstream-first-peer-review-2026-11-13.md).
Text logs/receipts are copied into ignored
`artifacts/metal/upstream-first-20261113/peer-evidence/`.

- Object parsing: 7 registered entries, 88 cases/1451 assertions pass.
- Protocol: 20599 assertions/52 cases; circuit49/11; bitpack36/9 pass.
  Removing only the packet-domain guard reproduces the timeout.
- UUID: 37136 assertions/25 cases pass, including 200000 concurrent distinct
  IDs and unchanged deterministic name/asset vectors. Original produces
  140214 distinct of200000 in the recorded run.
- Color font: 15 assertions/1 case pass, actual painter failure injection;
  original stale surface width116 versus clean18 is retained as a failing control.
- Mesh: first public decoder/profile174/4 and sanitized141/3 pass;
  expanded final decoder plus instrumented SIMD kernels passes201/4. Short-array
  unguarded control aborts with heap-buffer-overflow; initial zero-normal test
  failure remains under `mesh-attributes-initial-zero-normal-failure/`.
- IM: exact production bounded-text body passes9 checks under ASan/UBSan;
  original string-constructor control aborts with heap-buffer-overflow.
- Tool tests: 116 pass in the project Python environment. Homebrew Python first
  lacked PyYAML; the project environment then lacked NumPy. Installing NumPy
  into that existing virtual environment resolves the environment failures;
  no test was skipped or weakened.
- Initial doctor/configure pass. Production strict source3526files has
  zero blockers/errors at the initial checkpoint; final audit follows the
  zero-safe correction. Harness33requiredfiles and diff whitespace checks pass.

Full build of `cc0313e507` was intentionally interrupted with130 to include
the additional array bounds. Cached objects were retained. Resumed build at
`d1b8554520` continues while zero-safe source `1f635d6b75` is integrated; its
result is an intermediate build. `final-validation.py` waits for that build,
then configures/builds the frozen1f635 checkpoint, runs affected registered
tests and static bundle checks. Only that final result qualifies the candidate.

## Final validation and next action

[Final candidate report](../../research/upstream-first-batch-validation-2026-11-13.md):
frozen1f635/26.4.0.68914 build and136affected CTest entries pass, with five
pre-existing HTTP cases still skipped.116tools tests pass;346ARM64files, strict
deep signature, exact embedded source, owned imports and retained compiled
media/voice/decoder features pass. Strict production source3526/0blockers/0errors
and harness33requiredfiles pass. Viewer SHA256
`7727de0aa81e1c12f7aea06ce129eac09ea249206453b4f5a9bfb4c9f1687185`.

Exact candidate: `build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal First Fixes.app`.
No viewer launch/login, actual grid-cache/disconnect incident, voice call or
visual/performance test. Owner tests the candidate; local-only pending owner
acceptance and explicit push approval. Later scope remains separate; PPD-026
disk pairing and PPD-044 cost report are not closed by this batch.


2026-11-13 publication authorization: owner confirms the colour issue resolved
and explicitly approves pushing the ENTIRE committed upstream-port series,
including this work and its tests/records. [Publication scope](upstream-port-publication-20261113.md).
Earlier local-only statements are historical; unverified/partial defect status
and independent release/performance gates remain unchanged. No future unrelated
fixes, raw captures or release artifacts are included in this approval.
