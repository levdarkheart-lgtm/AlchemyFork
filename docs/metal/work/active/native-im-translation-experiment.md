# Native Apple IM translation experiment

## Scope and ownership

Owner requested a local experiment on 2026-11-19, branched exactly from
`9e4654d84ac0b4161b78095678b1abbc4a77beb7` as
`codex/native-im-translation-experiment`, isolated at
`worktrees/native-im-translation`. The original checkout and its five untracked
research notes are untouched. No push is authorized for this experiment.

Goal: selectable outgoing language and opt-in incoming translation, independently
for each one-to-one IM conversation. Preserve originals, logs, RLVa restrictions,
normal transport, and IM lifetime. No renderer changes or automatic test IMs.

Integrator owns CMake, floater integration and canonical records. Independent
agents own the Swift/C ABI bridge, model hook, and pure C++ message/request policy.

## Feasibility

Public Apple TranslationSession installed-language initialization is available on
macOS 26+. LanguageAvailability enumerates supported languages, NaturalLanguage
detects source text, and a native SwiftUI translation task can obtain explicit
user permission to download models. No private APIs or remote translation
provider is part of the experiment. SDK and native runtime viability are verified; normal logged-in IM acceptance remains pending.

Primary sources: [TranslationSession initializer](https://developer.apple.com/documentation/translation/translationsession/init(installedsource:target:)),
[Translation overview](https://developer.apple.com/documentation/translation),
[Apple language download instructions](https://support.apple.com/guide/mac-help/translate-text-mchld5a9b3bb/mac).

## Execution evidence

- Start: branch source above; native arm64 macOS 26.5.2, Xcode 26.6,
  Apple M1 Max, 64 GiB. `tools/metal/doctor.sh` initially exits 1 because the
  sandbox denies sysctl; required unrestricted retry exits 0 and confirms Metal.
- Pure message/request policy: standalone clang++ C++17 with `-Wall -Wextra
  -Werror -fsanitize=address,undefined`; 46 checks, exit 0.
- PPD-498: initial standalone bridge link fails because private Swift C exports
  have local visibility; corrected export visibility and retest pending.

## Delivery state

Implemented locally; native approach is viable. Full Release build, native
Spanish/Japanese translation and protection/lifetime tests pass. Owner acceptance
using normal logged-in one-to-one IM remains pending; no test message was sent
to another person. The original checkout and preserved water candidate remain
separate. All new controls are per live chat, with defaults off.

## Implementation and corner cases

- Per-live-P2P-floater settings only: Send in (off by default), Receive in
  checkbox/target (off by default), Write in source (system default, explicit,
  or detect). Closing a conversation resets these choices; minimizing retains
  them. Group/conference and nearby chat remain outside this experiment.
- Incoming originals follow normal IM notifications/logging immediately.
  Translation is display metadata on the exact indexed message, including while
  minimized; it does not create another message or transcript entry. Replay
  preserves scroll, unread counts, activity ordering and alert masks. Native
  selection takes precedence over the existing global translator for that chat.
- One outgoing request per chat retains the original draft. Cancel, failure,
  changed target/source, modified draft, disconnected/restricted send, close and
  oversized output prevent sending. Actual delivery reuses current RLVa and
  existing UTF-8 transport splitting. No partial translated sends.
- The real C++ scanner protects whole URI tokens (including arbitrary schemes,
  SLURLs and action links), email, UUID, bracketed/Markdown links and labels,
  inline/fenced code, escapes, newlines and nonlinguistic fragments. Exact bytes
  never enter Apple Translation. Conservative ambiguous suffixes attached to a
  URI stay untranslated. There are no placeholders for the translator to alter.
- Native bridge translates prose segments in order within one cancellable
  request/session. Maximum 64 prose segments, 32 pending incoming requests per
  chat. Link-only/code-only messages are identity results without models.
- Native SwiftUI Languages setup prepares public Apple language models. The
  verified local test bundle showed English to Japanese and the successful
  “Languages are ready” state. English US/UK, Spanish and Japanese models were
  downloaded using the public System Settings interface for validation.

## Observed native validation

`/tmp/alnativeimtranslation_test --live`, unrestricted: 195 checks, zero failures,
exit 0. Actual translations include English→Spanish, Spanish→English with source
recognition, English→Japanese (`こんにちは、本日はお元気ですか？`) and Japanese→English
with automatic recognition. Short Hi/OK/こんにちは, emoji identity, regional English,
Japanese and Chinese script identity/distinction, cancellation/shutdown, protected
links and production-scanned Japanese multiline/emote messages pass. Raw direct
Apple translate may change line-break formatting; viewer scanning protects line
separators and the actual scanned integration checks exact newline count.

The earlier unrestricted probe returned 21 languages and missing-model errors;
the first live run produced valid translations but failed two regional-identity
checks (PPD-506), now corrected by script-aware language identity. Native tests
in the restricted sandbox report local-service unavailable and exit 77 rather
than claiming a translation pass. Swift 5 mode is explicit because current SDK
Swift 6 Sendable annotations reject awaiting LanguageAvailability on MainActor.

Policy: production C++23 ASan/UBSan, 699 checks, zero failures after the final
conservative-URI revision. Full viewer build initially stopped at 1590/2558 on
in-progress C++17 u8 literals (PPD-507); corrected Release build is progressing.
PPD-499–504 fixes are integrated; UI source compilation found/repaired callback
signature and participant-initialization ordering defects. Production history,
session-tab and model translation units independently compile. No owner IM,
network-delivery or runtime scroll acceptance claimed.

## Final callback correction and UI test limits

The first offline UI fixture exposed PPD-510, an actual new native callback bug:
LLHandle::getDerivedHandle reinterprets the UI base pointer, while this IM floater
has a voice-observer base before the UI base. LLDB stopped in LLComboBox::add
with a null receiver at the first supported-language entry. Source repair
`0cfff83edc` uses LLHandle<LLFloater> and an adjusting static_cast in all native
callbacks and the model hook. The corrected 69397 fixture survives language
population and local synthetic IM creation. A production HeadlessUI/LLFloater/
LLVoiceClientStatusObserver regression passes 14 assertions in two cases,
including destruction and reopened-chat isolation.

The diagnostic copy differs only in bundle identity/signature, temporary login
menu entries invoking the existing synthetic-message generator and conversations
floater, and disabled transcript defaults. Original and fixture `__TEXT,__text`
section digests match; signed full-binary digests differ as expected. Fixture
receipt: `/tmp/alchemy-im-offline-qa-20261119/fixture-handle-fix.json`. Native model
setup UI was separately observed to report English→Japanese languages ready.

The pre-login fixture cannot qualify normal chat operation: gIMMgr is initialized
only after login. Synthetic sessions time out without a server; dismissing that
fixture timeout reaches existing leaveSession(this=null), PPD-511, retained as an
invalid diagnostic environment. No production menu or session ownership change
was made for it. Do not report a logged-in UI, transcript, scroll, actual network
send/receive or live two-chat independence pass from this fixture. No logged-in
region/profile scene gate passed; only preflight UI was inspected. The fixture
and temporary native setup processes are stopped. Normal owner viewer was not
terminated or used to send an IM.

## Final validation summary

- App source checkpoint: `1573a3addcd448501a8b78187202f660e14e8bfc`,
  Release `26.4.0.69398`, indra tree `f9070b328471878f5ad7bfc1ee4d01366380fa61`.
  Main executable SHA256 `da6bb5a976d65bb45c54378c2f77e1f58b1916b729e6dd46c3d14adda8c5801a`.
  The handle correction is0cfff83edc; subsequent handoff edits are documentation only.
- Required doctor0 on M1Max/64GiB/macOS26.5.2/Xcode26.6. Configure and complete
  Release wrapper builds pass; first char8_t failure and callback crash remain
  recorded, not erased.
- Native real API: 195 checks/0 failures, including English↔Spanish,
  English↔Japanese, auto incoming recognition, short messages, regional language
  identity, Chinese script distinction, mixed protected text, and cancellation.
- Pure policy: 699 checks/0 failures, C++23 ASan+UBSan, exact conservative URI
  tokens, all protocols, email/code/UUID/Markdown/newlines and emotes.
- Nine focused CTest processes pass: native policy/bridge/live, real handle
  lifetime, and five existing chat/emoji/text/URL suites. The handle test has
  14 assertions/two cases; all counts overlap where noted.
- Tool suite183 passes/one existing optional skip, using isolated PyYAML6.0.3
  plus existing system packages. Initial missing-PyYAML and sandbox Darwin
  notification errors are retained; no repository tool behavior was changed.
- App346ARM64 Mach-O/zero invalid slices; deep strict signature passes.
  Strict source inventory3647files/925informational/zero blockers/errors.
  Harness33required-files passes. Main/owned-host direct imports are inspected
  separately; no renderer parity or performance claim follows from this work.

## Owner check when using the experiment

Open a normal one-to-one IM. Choose Send in, choose Write in explicitly when
needed, and enable Receive in with the desired reading language. Choose a
second chat to verify it retains independent defaults. Languages opens Apple's
model preparation; missing models never send the untranslated draft. Check
cancellation, error recovery, links, emotes and received originals/annotations.
Closing a chat resets its choices; minimizing retains them. Groups/conferences
have no native translation controls. Full owner acceptance and pushing remain
unapproved.

Final command/status/hash receipt is retained at
`artifacts/metal/native-im-translation-20261119/delivery-receipt.json`, with exact
argv and logs. Final nine-process CTest,346ARM64/deep signature, three owned
executable direct imports, doctor, strict source and harness all exit0.
No required live-chat observation was inferred from synthetic/native tests.

## Owner-requested launch — 2026-11-19

Exact normal candidate26.4.0.69398 launched with `open -n <verified app>
--args --multiple --set AutoLogin false`, exit0. Actual process4274 executable
path matches the delivery receipt; SHA256 still matches da6bb5a976d65bb4…5801a.
CUA observed window “Alchemy Metal IM Translation 26.4.0.69398”. No fixture,
profiler, capture or automatic IM is active; owner controls login and testing.
Receipt: `artifacts/metal/native-im-translation-20261119/owner-launch.json`.
No owner acceptance, logged-in observation, defect closure or push inferred.

## Owner follow-up — outgoing original and French preparation

Owner confirms live Japanese outgoing translation works on69398. Requests original
wording plus bracketed sent translation in local history (PPD-512), with recipient
payload still translated only. Owner's French attempt retains its draft and shows
not_installed; English is present and French was absent from downloaded models.
French public System Settings download completed; root synthetic native en→fr passes. No automation sends IMs.


### Follow-up implementation checkpoint

PPD-512: `local_original` is separate metadata through the floater/initialization
queue/model send. Only the local history uses `original\n[translation]`; packets
retain the target-only string. Identical/protected-only results do not duplicate;
matching `/me` markers are omitted inside brackets. Ordinary sends retain their
existing local echo/global translation behavior.

PPD-513: Close clears initialization messages. Queue drain copies and clears
before callbacks, rechecks current RLVa restrictions through sendMsg, and checks
base weak handle, session UUID and close generation before each queued send.
The close generation also prevents a close/reopen of the same UUID from resuming
old queued text or clearing a new draft.

PPD-514: Native pair-readiness endpoint keeps supported choices visible with
installed/supported/unsupported/identity status. UI adds Download needed labels,
retains selections, defers menu rebuilding while open, polls visible chats every
four seconds and refreshes on selection changes. Detect-source and unknown-peer
incoming choices do not claim availability for an unknown source. Native send
remains authoritative so protected-only text and freshly installed models work.

PPD-515: Independent actual-body sanitizer fixtures reproduce session deletion
and floater-owned UUID argument deletion during synchronous local echo. Retain
both failed runs. Correction reacquires the session and owns ID copies; floater
send/input callers reacquire base weak handles before accessing UI afterward.

Executed on the M1 Max/macOS26.5.2:
- `tools/metal/doctor.sh`: exit0, log `/tmp/alchemy-im-followup-doctor.log`.
- Standalone native probe `--live`:342 checks/0 failures; en→fr, en→ja,
  ja→en, protected production segments and missing en→ar-AE agree with readiness.
  `/tmp/alchemy-native-readiness-20261119/native-live.log`.
- C++23 policy with ASan+UBSan:716 checks/0 failures.
- Actual LLIMModel send body with ASan+UBSan:135 checks/0 failures, target-only
  transport/local original/links/code/emotes/split routing/callback deletion.
  `/tmp/alchemy-native-im-echo-argument-after`; earlier callback/argument UAF
  stacks retained in corresponding `*-before.log` files.
- Updated Release build/CTest/app verification completed below.

Owner closed69398 and requests launch of the checked replacement. Original app
preserved under ignored evidence `previous/Alchemy Metal IM Translation 69398.app`.
No automated IMs, login or push. Logged-in UI acceptance remains the owner's next
check. Rendering/performance release gates remain unchanged and incomplete.


### Follow-up delivery —69401

Source `323a21242e33fdb94ddb0a8b9a39d21e290eae97`, indra tree
`31096a53de88a1fedafdb95bbb2266580347c36c`. Cached CMake configure exits0;
required Release wrapper exits0,104 Ninja actions. Exact app remains
`build-native-im-translation/newview/Release/Alchemy Metal IM Translation.app`,
version26.4.0.69401, binary SHA256
`4e98de1c534bc2b29976a7e42bc46eeb16bf9e14a04c2ddb494f2d0435d3e411`.
`tools/metal/verify_app.sh <app>` and `codesign --verify --deep --strict <app>`
exit0;346 Mach-O files ARM64, zero invalid slices. Strict source audit exits0,
3647 files/925 informational/zero blocking/zero scan errors. Harness33 files pass.

Ten focused CTest processes pass: native bridge/live, policy, actual-send echo,
real UI handle, autocomplete, emoji, text parser, URL entry and URL matching.
Policy716 and actual-send135 sanitizer checks pass; native342 checks include
French and Japanese, installed/missing readiness, cancellation and protected
URLs/code/emoji. No claim of translation linguistic perfection or logged-in UI
acceptance follows from these checks.

Detailed local receipt and logs: ignored
`artifacts/metal/native-im-translation-20261119/followup/receipt.json`.
Both pre-correction callback/session and borrowed-ID UAF stacks retained beside
passing actual-send-body source/receipt. Previous69398 bundle remains preserved.

Owner explicitly closed the old viewer and asked for the checked replacement to
start. Exact69401 launched with `open -n <app> --args --multiple --set AutoLogin false`,
exit0, observed PID19174 with exact executable path. Owner controls login/chat
acceptance; no agent IMs, login, scene screenshots, profiling or push. PPD-512–515
remain ready_for_owner. Next owner check: outgoing original+bracketed translation,
receiver-only target text, download badge clearing, per-chat isolation, incoming
opt-in and close/reopen behavior. Overall renderer/performance release gates are
unchanged and are not claimed complete.


## Settings popup follow-up — PPD-516

Owner69401 screenshots show original-plus-bracketed French outgoing echo, then
report the permanent settings rows crowd the chat. Move configuration to the
P2P toolbar's compact A/文 button. Existing ALPopover handles placement, screen
clamping, Escape and outside-focus dismissal. Done closes settings; changes apply
immediately to this chat. Translation remains independent of popup visibility.

The persistent control panel moves out of main_stack into a hidden root child.
A close callback is installed before opening and returns it before the popup
can destroy its children. All async native status/cancel/setup access uses the
persistent panel pointer. Closing/hiding/reusing the chat closes its popup;
closing the popup alone keeps language choices and pending translation intact.
Unsuccessful translated Enter opens settings to explain its retained draft.
Button highlight and tooltip expose enabled state, send/receive choices/status.

Remove translation-specific minimum width/height overrides; upstream collapse
and participant resize behavior apply again. Original144px toolbar width is the
initial layout baseline. At narrow host/torn-off widths, five actions fit142px
(25px buttons,38px teleport,1px gaps), retaining every old action; otherwise
normal31/44px buttons and2px gaps use177px. Translation icon padding1px avoids
three-glyph clipping at compact width. Nearby/group/conference default hidden.

Doctor exits0 on M1Max/macOS26.5.2. Initial exact Release syntax failed on factory
rvalue params; corrected lvalue syntax exits0, old failure retained under
`/tmp/alchemy-im-popup-syntax.log`, corrected log `-syntax-after.log`. XML parsing
and root-panel/main_stack/minheight checks pass; independent source review finds
no ownership/geometry blocker. Updated full Release/widget regression/app
verification is next. No renderer changes, IMs, automatic login or push.


Pre-delivery focus review registers PPD-517/518 before fixes: delayed failure must
not open over a hidden/compact chat, and toolbar focus loss must not close then
reopen on the same click. Guard visibility/expansion and track close/press/release
frames, following the existing emoji helper's event-order precedent. A chrome-only
approach was rejected before rebuild because the parent floater can take focus
first. Initial eedc0db054 Release49-action build exits0, not delivered/launched;
widget placement/popover CTests2/2 pass. Final source-pinned rebuild follows.


### Popup delivery —69404

Final source `7d607c955ab8b027a745bf5406b537f0640d99bb`, indra
`6cfca54e0398bb26d97ac33c39941a3ead123512`. Cached configure and required
Release wrapper exit0,49 Ninja actions. Version26.4.0.69404 binary SHA256
`1a8758659155dfe6cb9015cc41b5184d2f12f85827cf8c2ff5ffcf7c90b2e068`.
Exact app remains `build-native-im-translation/newview/Release/Alchemy Metal IM Translation.app`.
App verification reports346ARM64/zeroinvalid; direct deep strict codesign exits0.
Strict source audit exits0 with3647files/925informational/zero blockers/errors.

Twelve registered CTest processes pass: ALPopover/ALPlace2 plus native bridge,
live translation, policy, actual-send-body sanitizer, real UI handles and five
chat/emoji/text/URL processes10. No new mirrored UI tests added. XML parse and
out-of-main_stack/hiddenpanel/original190minheight checks pass. Independent final
source review finds no ownership or focus blocker. Same-frame suppression follows
the existing emoji-helper precedent; two distinct close/open gestures within one
frame can be treated as one. Actual popup appearance, button legibility at narrow
width, detach/collapse and per-chat acceptance await the owner.

Receipt/logs: ignored
`artifacts/metal/native-im-translation-20261119/popup-69404/receipt.json`.
Previous69401 bundle preserved beside previous69398. Exact69404 opened with
`open -n <app> --args --multiple --set AutoLogin false`, exit0; observed PID34771
with exact executable path. No automated login, IM, scene capture or profiling.
Owner's earlier request to start the checked replacement remains in effect.

Next owner check: A/文 after Pay opens configuration, Done/Escape/outside click
or toolbar re-click dismisses, choices/translation persist, full chat area and
single-line collapse are restored. Compare narrow hosted and torn-off widths;
failed send in an inactive chat must retain its draft without taking focus.
PPD-516–518 stay ready_for_owner and local. Overall renderer/performance gates
remain unchanged; no push or product-complete claim.
