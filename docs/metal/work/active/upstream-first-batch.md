# First upstream backport batch — 2026-11-13

Owner authorizes implementing the two First groups from the upstream review.
Do security/unsafe-read and shared crash/correctness repairs now; defer texture,
material, HDR/probe, geometry/sky modernization and other later groups.
The owner says the current app need not be preserved. Proposed fixes remain
local until owner testing and explicit approval to push.

## Source and ownership

- Integrator: `codex/upstream-first-batch-20261113`, initially
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
| PPD-017 | HTTPS redirects at login and HTTP transport | doing |
| PPD-018 | Independent local Xfer filenames and short-name handling | doing |
| PPD-019 | IM copied-size clamp and bounded bucket text | doing |
| PPD-020 | Packet ID domain guard | doing |
| PPD-021 | Object/particle/TE/extra/motion/layer/Xfer payload bounds | doing |
| PPD-022 | Mesh face/index work caps and skin weight reads/initialization | doing |
| PPD-023 | Retain empty world at disconnect; Scene Monitor guards | doing |
| PPD-024 | Thread-safe random UUIDs; deterministic hashes unchanged | doing |
| PPD-025 | Reliable packet queue, duplicate/ACK/history handling | doing |
| PPD-026 | Cache-entry removal lifetime prerequisite only | doing |
| PPD-027 | Same decoded floater name for policy and opening | doing |
| PPD-028 | Clear reused COLRv1 painter before each glyph | doing |

PPD-026 disk/extras pairing remains later. Its removal lifetime corrections
`fdda86251c` and `79057e0958` are necessary now because malformed cached input
must finish reading before removal frees its storage. Do not pull region
retirement or the larger object-roster modernization as incidental context.

## Validation and next action

Combine peer commits, review selected final upstream semantics and preserve
Metal-specific changes. Run the production tests for common/message/math/
primitive/HTTP/login/app-link/cache owners, focused unsafe-input controls,
doctor/configure/build/app verification, tools tests and documentation harness.
Source/build success is not owner verification; leave affected PPD records open
or ready_for_owner, retain failures and exact evidence, and do not push.
