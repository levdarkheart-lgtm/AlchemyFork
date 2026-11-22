# Current engineering state — PPD-556 bounded scheduling

Build observed2026-11-21; owner acceptance2026-11-22.
Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
Branch: codex/apple-silicon-metal, canonical checkout.
**Renderer code checkpoint:** `960bd3644a`
Indra tree: `7d8c095d360bd3f77ac64d1b69e23c21840e0011`.
Scheduling **26.4.0.69450 is owner accepted; commit/push approved**.
PPD-556/560–565 are verified, owner verification passed and push approval granted.
Accepted Maintenance 69445 remains preserved and previously pushed at629984ee37;
the new explicit2026-11-22 approval now covers Scheduling69450 as well.

## Current implementation and evidence

[Candidate report](../research/scheduling-candidate-69450-20261121.md) records exact
source/app hashes, executed validation and retained limitations.
[Current execution ledger](../work/active/ppd556-bounded-scheduling-20261121.md)
retains commands, original failures, independent reviews and residual actions.
[Previous accepted maintenance](../research/maintenance-candidate-69445-20261121.md)
retains PPD527–555/557–559 owner acceptance. All records survive in the
[defect register](../POST_PORT_DEFECTS.md).

- Local a27fb1a57e separates reusable request-owned scheduling from bounded callback
  admission. Retry/wake/promotion cannot self-post into a full service queue.
  Coalesced native owner notification avoids cross-owner request-data locking.
  Registered requests remain capacity charged until reaping, including terminal results.
- WorkerClass/cache/LFS/audio handle busy/closed admission explicitly, without
  fabricated completions. WAV finalization is once-only. Texture-priority posts
  retain exact lifecycle identity and report actual admission.
- Local960bd3644a retires seven orphan settings, the old automatic OpenGL GPU
  classifier and its child startup mode. All66live ordinary graphics bindings,
  quality presets and the manual native Metal benchmark remain. Pacing help and
  human-readable About diagnostics describe the native implementation.
- Actual baseline tiny-capacity reproduction fails two expected cases. Modified
  scheduler/worker ASan/UBSan:793assertions/18cases pass. Final integrated
  CPU/asset/settings/OpenHT58/58 and API/shader-validation native56/56 pass, no skips;
  pipeline-cache ordinary/instrumented/selective modes pass separately.
- Full Release build01, deep/strict source+stage codesign,346ARM64-only Mach-O
  files, strict GL source audit (0blocking/0scanerrors),15modified XML parses and
  harness (33required files) pass. Source/stage inventory equals7839regular files
  plus2internal symlinks, no external links. Required OpenHT licence/attribution
  and actual decoder identity/primary+auxiliary pixels pass. No _opj_decode symbol;
  required OpenJPEG metadata/encoding support remains.
- Original invalid-zero-capacity worker fixture, ad-hoc PCH-filter syntax error,
  readonly-shell receipt wrapper and overly broad symbol-prefix check are retained
  with their corrections. No failure was hidden or converted into a pass.

## Ownership and recovery

All implementation/review agents are frozen. Root completed canonical integration,
full build, final tests, staging and exact owner-requested launch. Existing native-IM
and other worktrees remain untouched. Private evidence: artifacts/metal/ppd556-20261121;
exact baseline/sanitizer locations are linked in the current execution ledger.
No build, validation driver, recording or capture remains running.

Owner-tested candidate: /Users/dekstroza/Alchemy-candidates/Alchemy Metal Scheduling 69450.app
(executable SHA256 df28cec46e833d8eba36c38e45385a4ed469964cc2ae84a753a78a86ccd47335).
PID31214 was observed at its exact executable path. Owner requested launch after
validation; AutoLogin=false is a temporary session override. No login, saved
settings edit, scene/profile collection or visual-parity assertion was performed.
The unchanged accepted69445 executable retains SHA256
0c5de0c90878f85c7e71296c279d303adda475be39a9c31923b10a942e2b9d89.
Older accepted69417 remains untouched.

Owner launch preference2026-11-22: preselect the appropriate explicit Home or club
destination on the login screen. Use the private destination map under
artifacts/metal/private-testing-destinations.json; pass its canonical SLURL with
--url as the last option. Use Home for quiet/startup/loading baselines and club for
busy-scene tests. Preserve graphics preferences and do not infer auto-login or
recording authorization. Exact locations stay outside tracked documentation.

## Complete-product advancement assessment

| ID | State | Evidence / remaining boundary |
| --- | --- | --- |
| M-008 | doing | Bounded scheduler/caller repair owner accepted; broad feature acceptance remains. |
| M-011 | doing | Final static app/source gates pass; runtime/foreign subprocess qualification remains. |
| M-012 | complete | Existing harness/build tools retained, maintained and exercised. |
| M-013 | doing | Alchemy/RLVa/media features retained; broader runtime qualification remains. |

No product/release completion or FPS improvement is claimed. PPD319 completion/
performance, broad parity, loading, prolonged stability and foreign subprocess
qualification remain open. Reproducible distinct open-source release, strict no-GL
and independent build/review evidence remain required. Component checks and process
launch do not establish whole-viewer runtime/performance or visual acceptance.

## Exact next actions

Owner reports "looks good..commit and push" on2026-11-22. Record acceptance,
commit documentation and publish the approved scope with a normal push to
origin/codex/apple-silicon-metal. Verify exact remote/local identity; private push
receipt retains the result. Built source/app identities are unchanged. No tag,
release, force push, default-branch merge or private evidence upload.

[Outstanding priorities](../work/active/metal-port.md#outstanding-work-in-priority-order)
start with asset service/cancellation and General jobs, then immutable geometry
preparation, safe GPU-wait reply service, streaming policy, compilation ownership
and broader qualification. LFS sleep is fixed; asset handoffs have bounded
replacements plus explicit residuals; geometry scheduling and earlier GPU-wait
accounting remain unfinished despite their narrower accepted repairs. Future fixes
need their own executed checks and applicable owner publication approval.
