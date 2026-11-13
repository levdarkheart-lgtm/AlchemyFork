# Alchemy upstream changes to backport — 2026-11-13

**Yes: important upstream fixes are missing from the Metal fork.** Start with
login/network/asset safety, disconnect lifetime, UUID generation and color-font
painting. Then bring texture/material correctness and adapt the HDR/probe fixes
to native Metal. Treat the geometry and sky rewrites as separate integration
projects with their final corrective followups.

## Exact comparison

- Repository: [AlchemyViewer/Alchemy](https://github.com/AlchemyViewer/Alchemy),
  default branch `develop`; checked through GitHub and a fresh
  `git fetch --no-tags upstream` on 2026-11-13.
- Last shared upstream commit:
  `65c39b7bad928e165f1acdc0cbf39478e017ef1d`, 2026-10-24 UTC.
  The engineering reference `1eb44bbd1b` is our first harness change, **not**
  an upstream revision. The Metal fork began on 24 October.
- Upstream snapshot: `408fd6c014e6e3b2b90dfe61265f34df0628f450`,
  2026-11-13 05:11:02 UTC, merge of graphics-preset PR #667.
- Audited fork: `5a403168af62b5960126bf5bcd3c410ca791e3b4`,
  `codex/apple-silicon-metal`. Local publication-related documentation changes
  and five earlier research files were already present and were preserved.
- **572 upstream-only commits: 472 non-merge commits, 100 merges, introduced
  by 78 first-parent landings.** See the complete
  [landing inventory](upstream-landing-inventory-2026-11-13.md) and
  [upstream comparison](https://github.com/AlchemyViewer/Alchemy/compare/65c39b7bad928e165f1acdc0cbf39478e017ef1d...408fd6c014e6e3b2b90dfe61265f34df0628f450).
  Topic branches with earlier author dates count when they landed after the
  fork. Unmerged topic branches are excluded. Upstream `main` is entirely
  ancestral to our fork and adds no other missing commits.
- Patch-ID comparison found no identical non-merge patches between the two
  sides. This alone does not establish missing behavior: native equivalents
  were checked separately. In particular `indra/llmath` and `indra/llprimitive`
  production trees are still identical to the fork base.

## First backport batches

Priority here is integration order, not a vulnerability severity score. These
findings are source-confirmed inherited mechanisms; this audit did not reproduce
an exploit or attribute them to the owner's existing freeze/HUD/water reports.

| Order | Change and upstream source | Why we need it | Metal work |
| --- | --- | --- | --- |
| 1 | [HTTPS login redirects, #637](https://github.com/AlchemyViewer/Alchemy/commit/cc2838504c417fb8d3ba3965a11f81ed22a78f10) | Both login `next_url` and HTTP 307/308 handling can resend credentials from HTTPS to HTTP. | Shared CPU/network backport; take both redirect protections and tests. |
| 1 | [Untrusted Xfer filenames, #614](https://github.com/AlchemyViewer/Alchemy/commit/4fa42289ac) | Remote mute-list/task-inventory names currently help choose local cache files. | Shared CPU backport: independent local temporary names, remote identity unchanged. |
| 1 | [Packet-ID hang, #636](https://github.com/AlchemyViewer/Alchemy/commit/6687792a35f19954dd34550cfef21ce05b6b069d) | A 32-bit wire number outside the 24-bit packet domain can make the main thread loop indefinitely. | Small shared protocol correction. |
| 1 | [IM payload bounds, #645](https://github.com/AlchemyViewer/Alchemy/commit/9063a7dfe83285b83760b373bd3b2ef01845fbe3), [object decoders #619](https://github.com/AlchemyViewer/Alchemy/pull/619), [terrain/Xfer #634](https://github.com/AlchemyViewer/Alchemy/pull/634) | Truncated, expanded or unterminated network payloads can overrun buffers; name-value parsing also uses unbounded static storage. | Shared CPU parsing. Preserve complete corrective sets and add meaningful bounds regressions. |
| 1 | [Mesh decoder #570](https://github.com/AlchemyViewer/Alchemy/pull/570) plus [`efd275c864`](https://github.com/AlchemyViewer/Alchemy/commit/efd275c864) | Missing influence-byte checks, uninitialized trailing weights, and absent per-face/LOD work caps. | CPU geometry fix can be isolated from the larger geometry rewrite. |
| 1 | [Disconnect lifetime #569](https://github.com/AlchemyViewer/Alchemy/commit/9e06c2d585) | Disconnect deletes `LLWorld` while later code in the same frame still uses it. | Retain world owner, clear its contents; preserve native frame cleanup. |
| 1 | [COLRv1 glyph painter #600](https://github.com/AlchemyViewer/Alchemy/commit/a55fd369594c79a49627484c6b260a205774d31b) | Failed painting leaves stale surfaces that a later smaller glyph can read beyond. | Shared CPU font fix; no MSL rewrite. |
| 1 | [Concurrent UUID generation #596](https://github.com/AlchemyViewer/Alchemy/commit/9ebf45f009210a8e1aea1a17528efd922b55fc0c) | Current generator shares unsynchronized time/sequence state; upstream reproduced duplicate UUIDs under concurrency. | Use upstream system-random v4 implementation for random IDs; preserve deterministic asset-ID/name hashing. |
| 1–2 | [Reliable inbound ring #630](https://github.com/AlchemyViewer/Alchemy/pull/630), [circuit reliability #617](https://github.com/AlchemyViewer/Alchemy/pull/617), [reliable-list cleanup #637](https://github.com/AlchemyViewer/Alchemy/pull/637) | Full queues discard already-acknowledged reliable packets; duplicate/ACK bookkeeping and never-pinging peers have further correctness and growth issues. | Shared protocol backport, with final queue/ACK followups together. |
| 1–2 | [App-link floater restrictions](https://github.com/AlchemyViewer/Alchemy/commit/53654b47811edd09884c0a9e967a7d71256da93e) | Blacklist checks escaped names, while the actual opener decodes them; encoded spelling can bypass the check. | Compare the resolved name using the same policy as the opener. |
| 2 | [Large texture readback](https://github.com/AlchemyViewer/Alchemy/commit/45007f232defcd25eca93dce11beecc3a8276f0e) | Our native readback still fatally rejects valid images over 2048px before reaching its Metal implementation, despite the 4096px accepted image limit. | Adapt `MAX_IMAGE_SIZE` validation/nonfatal failure to native readback; skip GL PBO machinery. |
| 2 | [Nonfinite quaternion #609](https://github.com/AlchemyViewer/Alchemy/pull/609) | Infinite/overflowing magnitudes yield invalid transforms instead of identity. | Small CPU header/test backport; preserve guard under fast-math/LTO. |

## Texture, material and native rendering correctness

| Change | Assessment and integration boundary |
| --- | --- |
| [#601 alpha-mask analysis](https://github.com/AlchemyViewer/Alchemy/pull/601): `bcce2bce3c`, `d66e1f4852`, `830be4b4f4` | **High value.** Asynchronous alpha analysis changes classification without rebuilding faces that used a provisional answer. Bring the complete owner-notification/rebuild sequence; preserve native texture publication. |
| [#533 PBR bake proxies](https://github.com/AlchemyViewer/Alchemy/pull/533) | **High value.** Fix local appearance composites, per-object material copies, base-color-only body hiding, refresh and shadow-cache invalidation. Our fetched-only texture slots drop dynamic composites. Resolve textures in every native world/alpha/HUD/shadow/selection consumer, not just upstream GL bind sites. |
| [#633 gained mesh faces](https://github.com/AlchemyViewer/Alchemy/pull/633): `93c320b7eb` | **High value.** Newly gained mesh faces currently inherit the last placeholder face's texture/material instead of their already-received entries. Coordinate with full-length TE parsing `444eb52a3e` from #619. |
| [#640 unknown-region material override](https://github.com/AlchemyViewer/Alchemy/pull/640) | Retain queued glTF overrides until the region/object is available. Shared material-state correction; inspect with the object/TE batch. |
| [#595 HDR, bloom, exposure and probe safety](https://github.com/AlchemyViewer/Alchemy/pull/595): `07094f82fe`, `977e51afe9`, `74589add5f`, `9c451dc4a2`, `b929675d9c` | **High value; MSL adaptation.** Native guards are partial: intensity multiplication and additive writes can exceed half-float bounds; bloom, exposure history and SH paths lack the final upstream finite-value boundaries. Specify native pass headroom and prove old/new behavior on actual GPU inputs. |
| [#595 placed-probe occlusion](https://github.com/AlchemyViewer/Alchemy/pull/595): `fb1a3f1b71`, `f410863587` | Preserve placed-probe lighting through stale occlusion, avoid off-frustum queries, restore updates when returning to view. Shared manager policy applies; preserve native completion-gated query lifetimes. Actual flicker duration on Metal remains unmeasured. |
| [#578 scoped texture boosts](https://github.com/AlchemyViewer/Alchemy/pull/578), [#601 release/retirement](https://github.com/AlchemyViewer/Alchemy/pull/601), [#622 fetch floor](https://github.com/AlchemyViewer/Alchemy/pull/622) | Fix sticky boosts, stale visibility/priority state, retired-region demand and fetches started below the renderer's discard floor. Reconcile GPU accounting against existing native descriptor-based bytes; avoid double counting and preserve accepted adaptive decoder policy. |
| [#615 default sky angles](https://github.com/AlchemyViewer/Alchemy/commit/4364fdcdba561ac91252a2cb76e2fc387c8e81c7) | Direct CPU fix for reversed azimuth/altitude arguments, still present here. Deliberately changes the default sun/moon positions; verify the expected visible result. |

## Shared utilities, features and performance candidates

- **URI concurrency and parsing:** [#604](https://github.com/AlchemyViewer/Alchemy/pull/604)
  includes `584b2cea53`, replacing a racy first-call sort with guarded static
  initialization; also fixes malformed escapes, scheme detection and empty
  query type. Current production file is identical to the base. Port early.
- **LLSD/XML and date/string contracts:** [#605](https://github.com/AlchemyViewer/Alchemy/pull/605),
  #607–612,
  `8000ca483b`, `1d410c39b9`, `15fd4bac40`, `2ed88ce337`,
  `4847b8c56d`, `58a1362221`, `0ec7fe6246`, `5d268213f2`, `58f471334a`.
  Fix failed/empty LLSD decompression reported as success, single-line
  deserialization, empty XML keys, malformed DOCTYPE/XML acceptance, URI quote
  escaping, date/timezone round trips and separator handling. The empty-target
  replace loop is a latent defensive fix: upstream found no current caller
  passing that input. These are shared CPU corrections, not Metal work.
- **Coroutine-safe locks and script correctness:** [#591](https://github.com/AlchemyViewer/Alchemy/pull/591)
  replaces thread-blocking locks reached by coroutines; also fixes published
  process state and LSL builtin-list ownership. Take per-owner contracts and
  regression tests, preserving current native locks. [#593](https://github.com/AlchemyViewer/Alchemy/pull/593)
  and [#606](https://github.com/AlchemyViewer/Alchemy/pull/606) contain Script
  Studio analysis/recovery/navigation/save-publish fixes worth a separate batch.
- **Worker scheduling:** [#598](https://github.com/AlchemyViewer/Alchemy/pull/598)
  replaces HTTP 2ms polling with socket waits, fixes retry ordering/cancel,
  queued-thread pause/shutdown, and wakes texture fetches directly on callback
  completion. Include `104dd0a607`, `dae2182667`, `5d4a951300` and the final
  wake/pause fixes. This intersects our decoder lifecycle; benchmark loading,
  idle CPU and shutdown. [#597](https://github.com/AlchemyViewer/Alchemy/pull/597)
  adds precise waits but its originating commit explicitly says macOS was not
  built; validate Apple behavior before adopting pacing policy. Windows-only
  sleepy-robin/window/timer results are not M1 performance evidence.
- **Geometry/LOD/uploader:** take the coherent final stack
  [#567](https://github.com/AlchemyViewer/Alchemy/pull/567),
  [#573](https://github.com/AlchemyViewer/Alchemy/pull/573),
  [#571](https://github.com/AlchemyViewer/Alchemy/pull/571),
  [#572](https://github.com/AlchemyViewer/Alchemy/pull/572),
  [#576](https://github.com/AlchemyViewer/Alchemy/pull/576),
  [#575](https://github.com/AlchemyViewer/Alchemy/pull/575),
  [#577](https://github.com/AlchemyViewer/Alchemy/pull/577).
  It improves prim/sculpt normals and stitching, planar UV/tangent agreement,
  invariant-LOD reuse, mesh ordering, and seam-aware uploader simplification.
  It needs the final custom meshoptimizer 1.3#5 registry version and all
  corrective followups; several later fixes repair new algorithms absent from
  our older stack. Preserve native vertex ownership and preview rendering.
- **CPU scene costs:** #568's octree capacity fixes `6ff9523744`/`df5199689b`
  apply independently of GL. Aggregate region-frustum culling `6cf56ecc2e`,
  probe pair precomputation `045e8c3d16`, probe reach/slot hysteresis
  `bbf3754dc8`/`c417d6ba98`, object-creation budgets and palette reuse are good
  profiling candidates. Map the last to native retained buffers, not GL UBOs.
- **Sky modernization:** [#584](https://github.com/AlchemyViewer/Alchemy/pull/584)
  replaces old sky objects/drawpools with ALSky, ray-evaluated layers, star
  binning and far-plane composition. This is a substantial feature/performance
  port, including its water/exposure dependencies. Implement native MSL/passes
  after urgent fixes; do not merge the GL pipeline wholesale.
- **Other viewer fixes:** login XML-RPC parsing (#620), teleport
  progress/local teleport/maturity/draw-distance (#637/#650 and `bc5cddb248`), region
  retirement (#623), cache extra-data pairing/flush (#602/#630), inventory IDs
  and links (#613/#647), friends/mutes/IM state (#642/#644/#645), and camera,
  graphics-preset and mirrors settings (#655/#660/#665/#667) retain user value
  without shader changes. FAudio attenuation #599 affects spatial sound, not
  our new parcel-music backend; F13–F24 #580 and MU poses #579 are optional
  feature additions. See the detailed general review for exact subcommits.
- **Build/dependency work:** builtin vcpkg baseline `5b6c62bf11` and stb
  attribution `c32de66abf` deserve a dependency audit, not a blanket update.
  No named CVE was established by these commits. Preserve our public dependency
  replacements, ARM64 enforcement, Metal attribution and manual-only CI.
  #590 Windows test packaging, #592 constexpr units, and dev-only
  oracle/replay/golden-capture infrastructure are lower product priorities.

## Changes we should not transplant

NVIDIA/GL driver stamps, GL fences/UBO/unpack rings, shared GL upload contexts,
buffer-name pools, GL query-age delays and macOS multithreaded GL setup do not
belong in this fork. Preserve their behavioral goals where relevant through
native Metal resource and completion ownership.

One concrete already-equivalent fix: #595 `c37563fa63` corrects the last probe
mip allocation in GL. Our native descriptor already allocates
`round(mMaxProbeLOD)+1` mips for the used chain. It needs no GL backport.
Native byte accounting and readback owners also already exist, but that does
not imply their surrounding CPU lifetime/priority behavior matches upstream.

## Evidence, retained defects and next action

Detailed source maps and prerequisite sets:
[general viewer](upstream-general-review-2026-11-13.md),
[renderer](upstream-renderer-review-2026-11-13.md),
[geometry/math/dependencies](upstream-geometry-review-2026-11-13.md).
Missing mechanisms and reported followups are retained as PPD-017–042 in the
[mandatory defect register](../POST_PORT_DEFECTS.md); upstream fixes have not
been applied or verified here. PPD-017–029 cover the first hardening/crash batch,
PPD-030–034 shared data/geometry, PPD-035–040 texture/rendering behavior, and
PPD-041–042 the remaining reported viewer/lock families.

Commands: `git fetch --no-tags upstream`, graph/range/patch-ID queries,
`git show`/`git diff` of actual changes and current production paths, and
`tools/metal/doctor.sh` exited 0 on Apple M1 Max/64GiB/macOS26.5.2/Xcode26.6.
`python3 tools/metal/validate_harness.py` exits 0 (33 required files). No application
source change, build, viewer launch, runtime reproduction or performance
measurement was made. No existing defect is closed and no push is authorized.

**Next implementation:** create an isolated local worktree from the current
accepted publication tip; first backport the security/memory-safety batch above
with upstream regressions and bounded native tests where shared owners differ.
Keep renderer and common-parser ownership separate. Then run doctor/build/app
verification, request owner scene testing for visible fixes, and retain all
proposed fixes locally until the owner explicitly approves pushing them.
