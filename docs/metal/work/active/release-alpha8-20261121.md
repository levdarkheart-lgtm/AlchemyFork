# Alpha8 — owner-verified rigged overlay release

Owner2026-11-21 requests triggering a GitHub release after accepting the69417
camera-motion overlay repair and explicitly approving commit/push. Baseline
feb57d835481ce6aafec24fd05281c7ff74dfd86 is published on
codex/apple-silicon-metal, revision69420. Its application tree
05091a15a707be1694ac37495414c42d90cbe951 is exactly the owner-tested69417 tree.
PPD-522/523 are verified; retain the source/GPU/owner evidence in the
[overlay ledger](bom-overlay-rendering-20261121.md). No new engine change.

Fresh GitHub reads show Alpha7/26.4.0.69412-alpha7 is the latest prerelease,
published2026-11-19; the latest manual release run44626887693 succeeded and no
recent release run is active. Default main remains65c39b7bad928e165f1acdc0cbf39478e017ef1d.
A connector read rejects the encoded-slash Metal branch path; use GitHub CLI
for branch identity and dispatch (connector exposes reads but no dispatch tool).
No failed release/build is inferred from that read-tool argument limitation.

Prepare Alpha8 notes and this ledger; run ordinary harness/whitespace and confirm
unchanged application tree, then commit/push the preparation. Pin the full
preparation SHA in workflow source_ref and choose a new version/revision-alpha8
tag matching that source's actual commit count. Use tag-release.yaml explicitly
from codex/apple-silicon-metal, never the older default main workflow. Do not
reuse or overwrite Alpha7 or publish raw captures, logs, profiles or credentials.

The existing manual workflow checks adequate Apple SDK and actual Swift source,
source identity, harness/tool tests and strict GL source gate; it builds the
sole OpenHTJ2K decoder from pinned72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2,
compiles an ARM64 Release viewer and verifies package/signatures/source/checksum
before automatic prerelease publication. This is ad-hoc signed, not notarized;
macOS26runtime minimum remains. Full product gates and existing asset failures
remain separate. PPD-520 downloaded-license receipt portability remains open;
that supplementary verification gap does not bypass hosted package/license checks.

No new local engine build/test is required for notes-only preparation: full
69417Release/346ARM64/signature,765exact-depth checks,13focused GPU CTests and
owner acceptance already qualify identical application source. A fresh hosted
artifact receives its own pinned identity; do not relabel69417 or claim the
future hosted artifact has already been built, published or independently run.

Next: fresh remote/tag/run checks, normal preparation push, dispatch exactlyonce,
and verify the returned new run's source/inputs/status. Preserve command receipts
under ignored artifacts/metal/release-alpha8-20261121. Report the trigger result
with the actual Actions URL; success/publication remains pending until observed.


Preparation checks pass: ordinary harness33required files, whitespace0,
unchanged application tree, independent scope/privacy/notes/workflow review.
Fresh remote Metal branch matches feb57; no queued/in-progress/requested/waiting/
pending manual release runs and no existing Alpha8 tag are found. Workflow is
active. Owner authorization covers required notes/record commits and normal push
as part of this release request. The next commit's actual full SHA/count, rather
than a predicted revision, determines the dispatch source and release tag.


## Dispatch verified

Preparation commit9ef1da4d37919f3b300ba6d777374bb021143abe (revision69421)
was pushed normally and independently matched on the remote Metal branch.
Exactly one workflow_dispatch request returned0, explicit workflow ref
codex/apple-silicon-metal and inputs source_ref full9ef1da4d37919f3b300ba6d777374bb021143abe,
alpha_number8, release_tag26.4.0.69421-alpha8. The workflow run is
[44888382375](https://github.com/levdarkheart-lgtm/AlchemyFork/actions/runs/44888382375),
created2026-11-21T09:11:23Z. GitHub CLI discovery and independent connector reads
agree on the exact source SHA, branch, workflow_dispatch event and Alpha8 title.
Run moved fromqueued toin_progress; job136798580790 set-up passed and source
checkout was in progress at the first job observation. Build, package and
publication were not yet observed complete. No duplicate dispatch was sent.

The existing workflow will publish the ad-hoc signed ARM64 ZIP, checksum and
provenance as a prerelease only after its gates pass. Actual run/dispatch/private
receipt data remains in ignored artifacts/metal/release-alpha8-20261121. User
requested a trigger; report this verified running Actions URL without claiming
that a downloadable release or any new runtime qualification already exists.
The immutable existing Alpha7 release and default branch are unchanged.
