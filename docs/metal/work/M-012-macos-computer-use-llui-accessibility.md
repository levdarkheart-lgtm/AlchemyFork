# Work item M-012: Optional macOS Computer Use / LLUI accessibility interoperability

**Status:** `done` — bounded Gate B proof integrated and verified; **not a product release gate**
**Owner / branch / worktree:** Codex integrator on `codex/apple-silicon-metal`; serialized in the primary worktree because the existing M-008 edits were preserved and the touched login/Cocoa files were disjoint.
**Source SHA / source paths:** Baseline `60793752c3406b738689cf3c64f307a877262a0f`; implementation in `indra/llwindow/llsdl_macos.{h,mm}` and `indra/newview/llpanellogin.{h,cpp}`.
**Dependencies / concurrent edit exclusions:** Existing distinct `Alchemy Metal Development` diagnostic identity and isolated profile; M-003/M-006 native window/UI work for context. **Never block or preempt M-007/M-008 visual parity, M-009 release validation, M-011 proven per-owner GL cutover, or performance work that is on the critical path.** `llwindowsdl.cpp`, `llview*`, `llrender*`, other shared UI/rendering files, packaging and canonical ledger are integrator-coordinated/serialized edits under `docs/metal/OPERATING_MODEL.md`.

## Completed result — 2026-10-31

- **Gate A: FAIL (app-side semantic control exposure).** Codex Computer Use could bind the exact isolated development bundle, capture its login screen and deliver keyboard input, but the accessibility tree contained only the Cocoa window/title/menu. LLUI controls were absent. Coordinate focus did not reliably provide password or login-button operation, so the missing semantic descendants directly blocked the planned repeated runtime validation.
- **Gate B: PASS.** The existing SDL Cocoa content view now gains three virtual `NSAccessibilityElement` children only while the development-channel login panel exists: username, protected secure password and Log In. The password has no getter in the C++ callback contract, returns an empty AX value, advertises secure/protected semantics and only accepts replacement text. Mutations and activation dispatch to the viewer main thread; geometry is queried from current LLUI screen rectangles; destruction deactivates callbacks and restores the prior children.
- **Actual acceptance client: PASS.** Codex Computer Use discovered stable identifiers `alchemy.login.username`, `alchemy.login.password` and `alchemy.login.connect`; set the owner-authorized test values; observed the button enable; activated login; reached `STATE_STARTED`; observed the upright authenticated world; invoked the existing quit flow; and left no viewer process. The protected field never exposed its contents in the accessibility tree.
- **Isolation/security: PASS.** The run used exact bundle-path binding, disposable `ALCHEMYNEXT_USER_DIR=/tmp/alchemy-metal-m012.UGPJCd`, direct executable arguments `--multiple --noaudio`, and a process census before launch, after binding, after login and after quit. Counts were zero, one, one and zero. The bridge is gated by exact channel name `Alchemy Metal Development`; Release channels receive no elements. No server, network listener, remote command surface or credential store was added. The owner explicitly authorized use of an existing test account; credential values and account/world-bearing raw evidence are not tracked.
- **Build/package: PASS.** `tools/metal/build.sh` completed source-exact Xcode Release build 67952 with warnings-as-errors and exit 0. `codesign --verify --deep --strict --verbose=2` and `tools/metal/verify_app.sh` both exited 0; 349/349 bundled Mach-O files are ARM64. Executable SHA-256 is `e0a3e70c11da95b3dca72b18d58f8c36c6cf334751ad488cd2b67dabb9953276`.
- **Post-review Retina correction: PASS.** Review caught scaled LLUI rectangles being divided by raw drawable-pixel dimensions. The implementation now normalizes with `getWindowWidthScaled()` / `getWindowHeightScaled()` before the Cocoa view maps to points. A complete RelWithDebInfo Ninja rebuild linked and signed; deep signing and 349/349 ARM64 verification pass. Actual Computer Use rediscovered all three protected elements in an unauthenticated disposable run and an element-directed username click landed on the visible control at Retina scale. No credentials were entered and `--quitafter 90` exited cleanly.
- **Runtime boundary:** the in-world screenshot and shutdown telemetry are useful smoke evidence, not proof of Metal-only rendering, parity or performance. The executable still retains OpenGL work. No general AX hierarchy is justified; add another control only if a named parity test is blocked.

## Ledger row

The active work board records the completed bounded result as:

```markdown
| M-012 | Optional developer-only Computer Use / LLUI accessibility interoperability | macOS Cocoa/SDL window + existing `LLUI` control hierarchy; scoped diagnostic checks | M-003, M-006; safe diagnostic identity; no conflicting UI/window edits | done | Gate A proved missing LLUI semantic descendants; Gate B exposes only development-channel username, protected password setter and Log In controls, verified end to end with actual Codex Computer Use. Release channels receive no bridge. |
```

Completing M-012 does not change the product critical path, create a product release gate or authorize broad UI automation work.

## Exact behavioral contract

**Outcome sought:** When appropriate, allow Codex to operate a genuine, separately identified Alchemy Metal diagnostic viewer—click visible controls, focus and type into nonsecret fields, navigate settings and menus, and observe rendered results—so repeated feature-parity and rendering checks need less owner involvement. Prefer the existing **Codex Computer Use plugin** for visual observation and OS-level input; do not create a competing general desktop-control system.

**Non-goals:** Changing Alchemy's graphics architecture, replacing SDL3 or LLUI with AppKit/SwiftUI, rewriting the UI, porting desktop input handling to a new framework, implementing a remote-control RPC server, enabling persistent automation in release builds, circumventing macOS TCC permissions, automating Second Life authentication without owner authorization, automatically making purchases/uploads/account changes, or treating GUI automation as evidence of Metal correctness on its own.

**Priority/scheduling rule:** Treat M-012 as a convenience/capability investigation, not a mandatory step on the path to the Metal-only viewer. Do not pick it while an unblocked high-impact product implementation or required manual-equivalence investigation is available. It becomes eligible for a **bounded feasibility pass** only when (a) a real GUI automation limitation demonstrably impedes an already-planned parity/diagnostic activity, or (b) the integrator has a natural idle/dependency gap and a nonconflicting isolated task. If not eligible, keep `todo` and return immediately to the primary Metal ledger. The owner should not have to manage another phase.

**Definition of done:** Either (1) prove that Codex can already operate the chosen diagnostic UI without source changes, (2) prove one working Mac accessibility bridge for a button plus one nonsensitive input using actual Codex Computer Use and integrate the minimum required adapter without harming viewer behavior, or (3) document a precise, reproduced incompatibility and stop instead of escalating into a UI rewrite. Do not call the work `verified` merely because a source file compiles or Accessibility Inspector exposes elements.

## Existing source facts to confirm at HEAD

- `indra/llwindow/llwindowsdl.cpp` already obtains an SDL3 Cocoa window using `SDL_GetWindowProperties()` and `SDL_PROP_WINDOW_COCOA_WINDOW_POINTER`; verify ownership, actual `NSWindow`/content `NSView`, existing `CAMetalLayer`, window recreation and lifecycle before attaching anything. Never replace SDL's content view or interfere with drawable/presentation setup just to install accessibility.
- `LLView` exposes names, child traversal, bounds, visibility and focus-related behavior (`indra/llui/llview.h`); `LLButton` has existing pointer/commit behavior (`llbutton.*`); `LLLineEditor` and `LLUICtrl` expose editing, focusing, validation, value and commit interfaces (`lllineeditor.*`, `lluictrl.*`). Verify actual runtime dispatch before calling anything; a direct `setValue()` or `onCommit()` is **not automatically equivalent** to user input.
- `indra/newview/llpanellogin.cpp` uses stable control names such as `connect_btn`, `username_combo` and `password_edit`. These are examples for locating controls—not authorization to read saved account data or attempt an authenticated login. Choose an unrelated harmless button and a nonsecret editable field for the initial proof if possible.
- Alchemy's SDL/LLUI widgets are custom-drawn, so a Cocoa window can be visible to macOS Accessibility without individual LLUI controls being discoverable. But missing AX children is **not proof** that screenshot-based Computer Use cannot click or type. Diagnose the actual failure first.

## Feasibility sequence — minimal, stop between gates

### Gate A — reproduce and classify the actual failure **without viewer code edits**

1. Confirm the current Codex Computer Use plugin and skill are installed/enabled for the running Codex app, that the diagnostic application is approved, and that macOS Screen Recording and Accessibility permissions are granted **to the relevant controller**. Do not change unrelated security/privacy settings or assume availability where the plugin is disabled by account, region or organization.
2. Verify screenshot visibility of the isolated diagnostic app, keyboard focus and entry in a harmless field, one coordinate click, one semantic/AX lookup or press if the plugin exposes it, and one scroll/menu action if needed. Record whether operation is through screenshot/coordinates, AX tree, keyboard, or another supported mechanism; do not assume a specific unpublished plugin API.
3. Classify the failure precisely: plugin unavailable/approval; screenshot absent; Cocoa window not discoverable; LLUI child absent from AX; coordinate mismatch (including Retina scaling, view origin, framebuffer vs logical pixels, display transforms); click arriving at SDL but not LLUI; focus/keyboard event issue; or protection from another running viewer/session.
4. Distinguish failure of a specific Codex integration from general macOS AX capability. Use Xcode Accessibility Inspector or a minimal local AX inspection only as supporting diagnostics. **The acceptance client is Codex Computer Use itself**, not merely an AX tool.
5. If Codex can already reliably click and enter nonsecret text using the existing plugin, record the tested steps and evidence, mark the item resolved with **no code** and resume the product work. If the observed failure is external, record it and do not paper over it with invasive viewer changes.

### Gate B — only if A demonstrates missing semantic LLUI access, prove the smallest Cocoa AX bridge

1. Create a diagnostic-build-only Objective-C++ adapter using the macOS `NSAccessibility` APIs (candidate: `NSAccessibilityElement` virtual descendants of the existing SDL/Cocoa view). Validate the **supported actual attachment mechanism** against Apple's API and the source at HEAD. This document intentionally does not dictate that an unverified `NSView` override or SDL subclassing trick will work.
2. Represent **one visible, enabled and harmless `LLButton`** as an AX button with a stable, non-sensitive identifier/label, accurate screen frame, visible/enabled/focused state and the press action. Route the AX action onto the viewer's UI/main thread through the existing LLUI event/activation semantics, preserving hit testing, focus, mouse-down/up ordering and callbacks. Never invoke random callbacks from an AX background thread.
3. Represent **one nonsecret `LLLineEditor`** as an AX text field with correct focus, insertion/selection and commit behavior. Input must respect the existing editor validation and focus/keystroke/commit contract. Verify that typing via Computer Use actually changes the on-screen field and triggers the expected interactions. Do not expose password value, saved login state, session tokens or protected field contents through AX; password fields must use secure semantics if ever included.
4. Keep geometry correct across LLUI-origin-to-Cocoa-screen conversion, HiDPI/Retina scale, window movement and resize, dynamic visibility, clipping/scrolled containers, z-order, modal dialogs, control destruction and SDL window recreation. Virtual AX elements must not outlive their backing LLUI control; avoid persistent raw pointers to destroyed views.
5. The diagnostic bridge must have **zero effect** on Metal presentation, SDL input behavior and performance when disabled. Build-gate it using the repository's actual development-only conditions and diagnostic bundle identity; do not invent a nonexistent compile macro. The release viewer must not gain an automation surface.
6. Prove that **Codex Computer Use**, operating the built diagnostic application, can discover/activate the button, focus/type into the ordinary field, visually verify the result, and exit. AX Inspector-only success is insufficient. Stop at the proof if the plugin cannot make use of the new elements; record the incompatibility instead of generalizing untested code.

### Gate C — only if B is useful, generalize narrowly

Extend the generic LLView-to-AX mapping to *currently needed* controls: buttons, nonsecret line editors, checkboxes and combo/list/menu items, with correct roles/actions and stable identifiers. Add further widgets only when a named parity-test scenario is blocked by one. Avoid publishing the entire scene graph or arbitrary UI data, and avoid exposing passwords or private account/location data. No second UI hierarchy with divergent state; the AX representation is a thin, query-on-demand view of existing LLUI ownership.

For actual 3D world/camera interaction, prefer the plugin's screenshots, pointer events and keyboard navigation. AX is for semantic UI, **not** for mapping every world polygon to an accessibility element. If coordinate delivery through SDL is independently proven defective, investigate platform/input scaling and existing SDL dispatch before considering an additional **diagnostic-only** event-injection fallback. Such a fallback requires a separate justification, must preserve normal SDL/LLUI input semantics, must not offer a network/listening control channel or enable remote arbitrary commands, and must not be treated as true OS-input end-to-end evidence.

## Integration and validation

- Record baseline source SHA, worktree state, diagnostic executable identity, macOS/SDL versions, Computer Use availability/permission state, exact actions attempted and outcomes. Do not store screenshots or logs containing account names, passwords, messages, avatar/region identity or session material in tracked files.
- Build and test with the harness's existing Apple Silicon ARM64 diagnostic tooling. Run the smallest applicable new native/LLUI lifecycle tests, full relevant `llrender`/`newview` tests if shared UI/window code changes, `git diff --check`, and the normal harness/tool validation required for documentation or tooling edits. Validate signed ARM64 bundle when app packaging changes. Report exact commands and exit codes; mark anything not executed `NOT RUN`.
- Include at least one regression for focus/press/edit callback order, safe destruction/recreation of controls, hidden/disabled control behavior, ordinary input still working with AX disabled, and absence from the production bundle where relevant.
- Run one attended or safely isolated real-app smoke **through Codex Computer Use**, capturing reproducible evidence of a harmless click plus nonsecret text input and no release-viewer interference. Do not test against protected Release PID or reuse an uncertain authenticated profile. If login/authentication is genuinely required later, ask for owner participation; do not solicit or expose credentials through chat, environment logs, test fixtures or version control.
- Never claim that desktop automation proves actual Metal rendering quality or FPS. Keep existing camera/scene parity captures, GPU validation, Metal traces and matched 300-frame benchmarks as authoritative separate gates.
- Self-review source ownership and ensure no edits conflict with ongoing renderer/performance work. Follow `AGENTS.md`, `PRODUCT.md`, `VALIDATION.md`, `OPERATING_MODEL.md` and `.agents/skills/agent-continuity`.

## Decisions, problems and resolution

- **Decision:** Treat existing Codex Computer Use as the preferred controller. Fix only the proven app-side deficiency; preserve SDL3 and native Metal UI composition.
- **Decision:** Do **not** introduce accessibility/UI instrumentation merely to tick a box. The benefit must be demonstrated against the real plugin.
- **Decision:** This is an *optional engineering-productivity item*, not a product milestone or release gate. It must never delay the strict Metal-only, visual-parity and performance acceptance work.
- **Resolved:** The acceptance plugin's screenshot and keyboard paths worked, while LLUI controls were absent from AX. The three-element bridge made the same plugin complete authorized login and quit end to end, identifying missing semantic descendants as the actionable app-side gap.
- **Kill conditions:** Existing plugin already works; plugin unavailable due to policy or permissions; button/text POC is not consumable by Codex; integration requires replacing SDL UI, broad changes to `llwindowsdl.cpp` while it is owned by another task, exposing private data, circumventing user consent, or measurable rendering regression. Record exact findings and return to the primary ledger instead of expanding scope.

## Evidence / deliverables if scheduled

1. A short failure classification with repro, source SHA, Mac/toolchain/Computer Use state, and sanitized observations.
2. Either a no-code verified Computer Use interaction recipe; a small proven diagnostic-only source change with focused tests and actual Codex click/type evidence; or a concise reproducible blocked/unsupported explanation.
3. If source changes are justified, an isolated, reviewed commit with clear ownership and no overlapping edits or new product dependencies. Update the M-012 ledger row, link this work item and update `state/CURRENT.md` only if a substantive checkpoint/handoff warrants it.

## Next executable action

M-012 is complete at Gate B. Keep the bridge limited to the development login controls and return to M-008: compile and validate the existing ordinary depth/depth24 snapshot extension, prove its native-path marker in an isolated run, then continue the required advanced/media parity matrix. Do not generalize accessibility coverage without a specific blocked validation scenario.
