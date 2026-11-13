# First upstream batch validation — 2026-11-13

The authorized security, unsafe-input and shared crash/correctness backports are
implemented locally. The combined Release candidate passes its build, affected
registered tests and static app checks. Owner in-world testing and permission
to push remain pending; later upstream batches are not implemented here.

## Exact candidate

- Branch: `codex/apple-silicon-metal`.
- Code/source checkpoint: `1f635d6b758af678a34b2c9e3a10eda7cf3dfcfe`.
  Later documentation commits do not change the tested code.
- Build: **26.4.0.68914**, channel **Alchemy Metal First Fixes**, Release,
  LTO OFF, native ARM64, Xcode26.6/macOS26.5.2/Apple M1 Max64GiB.
- App: `build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal First Fixes.app`.
- Viewer SHA256:
  `7727de0aa81e1c12f7aea06ce129eac09ea249206453b4f5a9bfb4c9f1687185`.
- The main executable embeds the exact source checkpoint above. Its bundle
  identifier is `org.alchemyviewer.viewer.metal-first-fixes`.

## Changes and retained boundaries

PPD-017–025/027–028: HTTPS login redirects at both application/transport layers,
independent local Xfer filenames, bounded IM text, packet-domain guard, bounded
object/particle/TE/extra/motion/layer/Xfer readers, bounded mesh allocations and
skin weights, disconnect world/monitor lifetime, concurrent UUID generation,
reliable queue/ACK/duplicate/history behavior, decoded app-link policy and
COLRv1 painter reset. PPD-026 contributes only the cache-removal lifetime
prerequisite; disk/extras pairing remains later.

Independent review caught omitted short Normal/TexCoord byte checks, added
from upstream `2a599a405c`. Their public-API test exposed the retained tangent
implementation's zero-normal NaNs. PPD-043 adapts upstream `unit3_or_zero`
semantics from `7a7296abfc` at the final CPU normal/tangent normalization
boundary, preserving valid nonzero arithmetic and tangent handedness.
PPD-044 separately retains the unmeasured optimizer cost report for later work.

Native Metal geometry, Scene Monitor texture ownership, native music, voice,
OpenHTJ2K decoding and OpenJPEG encoding are retained. No GL backend, new sky,
geometry/tangent replacement, texture-priority policy or HDR/probe rewrite is
imported as incidental context. Full provenance and focused red/green results
are in the [execution note](../work/active/upstream-first-batch.md).

## Observed final checks

All commands exit0. Detailed argv/status/source hashes and logs are in ignored
`artifacts/metal/upstream-first-20261113/`, particularly `final-state.json`,
`final-configure.json`, `final-build.json`, `affected-ctest.json`,
`app-identity.json` and `evidence-hashes.json`.

| Check | Observed result |
| --- | --- |
| `tools/metal/doctor.sh` | M1 Max unified-memory device, native arm64 and Xcode/Metal toolchain available. |
| Final Ninja configure + `tools/metal/build.sh build` | Release build passes at frozen1f635 source, using eight build jobs and existing dependencies with manifest installOFF. |
| Registered affected CTest selection | **136/136 entries pass**, 41.67s. Common/math/message/primitive/HTTP/login, standalone circuit, app-link/cache and color-font regression included. |
| Existing HTTP skips | Five pre-existing cases remain skipped: two pthread-cancellation cases and three header-replacement cases marked FIXME upstream. The new HTTPS redirect cases run. Stray-case entries intentionally require no unmatched cases. |
| Repository Python tools | **116 tests pass** in project virtual environment after adding missing NumPy. Initial missing PyYAML/NumPy environment failures are retained. |
| `tools/metal/verify_app.sh <exact app>` | **346 Mach-O files, zero non-ARM64 slices**. |
| `codesign --verify --deep --strict --verbose=2 <exact app>` | Valid on disk and satisfies designated requirement. |
| Owned viewer/CEF-host/VLC-host imports | No forbidden direct graphics libraries/symbols. Viewer source identity and executable hashes recorded. Foreign component runtime behavior is a separate product gate. |
| Compiled feature symbols | Native music23, WebRTC voice477, OpenHTJ2K61, OpenJPEG encoder1 defined-symbol matches. These are static retention checks, not playback/call verification. |
| Final strict source audit | **3526 production files, zero blocking findings/errors**, 912 informational compatibility rows. |
| Documentation/whitespace | `validate_harness.py`33requiredfiles and `git diff --check` pass. |

Focused old/fixed controls additionally reproduce the original mesh/IM heap
overflows, packet-ID timeout, concurrent UUID collisions and stale glyph surface.
The expanded production decoder plus instrumented SIMD kernels passes
**201 assertions/4 cases** under ASan/UBSan. The first zero-normal regression
failure is retained; it is not silently counted as a pass.

## Limits and next action

No viewer login, actual disconnect/region-cache incident, grid session, voice
call, visual comparison or performance benchmark was executed by this task.
Source/tests/builds do not close a PPD record. Owner tests the exact candidate;
keep it local until the owner agrees it works and explicitly approves pushing.
After that, take the next upstream batch separately under the retained audit.
