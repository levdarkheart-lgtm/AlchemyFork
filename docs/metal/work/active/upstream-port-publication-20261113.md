# Full upstream-port publication — 2026-11-13

The owner explicitly says the reported colour issue is resolved and instructs
Codex to commit and push. The owner then clarifies: “entire work you did on
porting the changes from upstream, not just this fix”. This authorizes the
whole committed upstream-port series throughaf3558f689, including first/general
and remaining fixes, geometry, native sky modernization, and the diagnosed HDR
probe/colour repair, with their tests and verification records. It does not
authorize future unimplemented fixes or claim every retained defect is closed.

Publication target: origin/codex/apple-silicon-metal, an ordinary fast-forward
push. Remote fetched baselinef6452943952836742a644ebb342e464f439c9d9b;
80pending commits throughaf3558f689. The two earliest additional water/HUD
commits contain prior owner-test/publication documentation only. Canonical
renderer/workspace and running viewer are retained; no default-branch merge,
force-push, release publication or raw-capture upload is requested.

## Included checkpoints and retained validation

- First batch1f635d6b75: fullbuild/136affectedCTest/116tools; exact ARM64,
  signature, source/import checks. [Record](upstream-first-batch.md).
- Remaining batchaf5c263988: fullbuild/197CPU/22containedUI/245071GPUassertions,
  116tools and static checks. [Record](upstream-remaining-batch.md).
- Geometryb16fa27efa: fullbuild/192CPU/22UI/269602GPUassertions plus focused
  sanitizers and static checks. [Record](upstream-geometry-port.md).
- Skyefc3dc17d0: fullbuild/206CPU/24UI/512781GPUassertions and static checks.
  [Record](upstream-sky-port.md).
- Latest rendererb81a4645e0 /Sky26.4.0.68980: fullbuild/8407focused+relatedGPU
  assertions/9cases;346ARM64, deepstrictsignature, embeddedsource, ownedimports,
  strictsource3540files/0blockers/0errors. Owner clean comparison verifies083/086:
  same5materialpatch mean RGB error94.21%lower,15means within1/255 ofreference.
  [Record](hdr-probe-publication-format.md), [comparison](neutral-surface-colour-investigation.md).

These are retained executed results; no source implementation changed after
that validated renderer checkpoint. Documentation/harness validation runs again
for this approval record. No repeated broad test run is implied. Runtime,
performance and unrelated/open defect gates remain as individually recorded.
Preview087 acceptance is not fabricated by the scene-colour confirmation.

## Bounded pre-push review

Independent review of all80commits/net390paths found the source/test/docs scope
matches the authorization. No tracked binary captures, traces, logs, credential
files or high-specificity credential-pattern matches were found. Private
artifacts stay ignored; the five pre-existing untracked research notes remain
excluded. No blanket exposure/gamma/default change was introduced by the repair.

Push approval is recorded for committed scopes017–079,083/084/086/087, without
changing pending owner verification or partial/open status. Other symptom-only
and unimplemented follow-ups retain separate authorization requirements.


## Observed publication

Normal `git push origin HEAD:refs/heads/codex/apple-silicon-metal` exits0:
remote advancesf645294395→37989d9d66fetched via SSH origin. Independent
`git ls-remote --heads origin refs/heads/codex/apple-silicon-metal` confirms exact
37989d9d66 approval head, including80prior commits plus approval metadata.
No force-push or default-branch change. This receipt-only documentation follows
the already-published series and changes no implementation or viewer binary.
Private machine-readable receipt stays under ignored publication artifacts.
