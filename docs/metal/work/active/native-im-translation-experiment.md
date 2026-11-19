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
provider is part of the experiment. SDK and runtime validation are in progress.

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

## Outstanding

Build complete viewer, run native translation without sending an IM, verify
bundle and controls, exercise failure/cancel/lifetime, and document actual
language availability. Owner runtime IM acceptance remains distinct from builds.

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
