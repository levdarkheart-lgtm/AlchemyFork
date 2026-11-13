# FIRST-batch integrator source review — 2026-11-13

Read-only review of integrator commit765bc2d3fc9cdca7b7b7f5268162b05f730c1e51 in canonical worktree, observed combined HEADcc0313e507 plus protocol21b0f814e8. Baseline publicationf645294395. Compared actual diffs and the matching upstream commit bodies, then traced current consumers. No files edited or tests rerun by this review.

**No actionable findings in the reviewed integrator changes.**

Review boundaries:

- HTTPS downgrade protection: login's current request URI is checked before reposting credentials to next_url, accepts case-insensitive HTTPS and refuses absent/plain schemes after TLS; XML-RPC sets the transport option for initial TLS request. llcorehttp sets redirect protocols to HTTPS only on reset curl handles (`HandleCache::getHandle` resets), preserving default policy for other requests. Existing MAXREDIRS10 remains. Application-level XML-RPC `indeterminate` redirects have no attempt cap before or after this change; that is unchanged behavior, not a new backport regression or a claim of bounded application redirect count.
- App links: blacklist and opened floater share the same once-unescaped name, while exact-one-parameter admission remains enforced. Current native/renderer code is untouched.
- IM bucket: copy-size clamp matches the actual fixed UDP buffer, bounded text helper handles empty/nonterminated HTTP buckets and first-NUL semantics; the three changed readers preserve their old behavior within the copied bytes.
- Deferred cache release: LLVOVolume requests removal rather than invalidating the packet's backing cache during decode; LLWorld drains requests before region idle walks; region probe/full-update gates avoid consuming/repopulating a cache scheduled for deletion. releaseAll detaches tree/octet entries and invalidates each while references remain retained, empties raw-pointer sets, and clearVOCacheFromMemory then clears transient waiting list/cursor. Deferred cleanup retains ownership until safe later release. The source maps to fdda86251c plus required79057e0958 tail; test coverage of LLViewerRegion/LLVOVolume/LLWorld behavior still belongs to the integrator's app validation, as recorded.
- Disconnect: LLWorld is created before frames at llappviewer1341, now retained after reset until normal cleanup2287. Scene Monitor visibility/draw/render_ui avoid recreating a deleted monitor and preserve the current native ALMetalTexture consumer.

This is a bounded source review, not evidence of actual login/grid/cache-clear/disconnect runtime verification, owner confirmation, or release readiness.

## Additional decoder review

A second read-only review checks `d1b8554520` and `1f635d6b75`: short
normal/UV length predicates, final `unit3_or_zero` adaptation and the public
compressed-input test. No actionable findings. The helper matches upstream,
preserves nonzero normalization arithmetic and saves/restores tangent handedness.
Full/short normal-by-UV combinations inspect final optimized attributes and
therefore exercise downstream fallback handling, not only parser predicates.
