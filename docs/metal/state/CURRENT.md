# Current engineering state — review and Home handoff

Observed 2026-11-22, Apple M1 Max /64 GiB /macOS 26.5.2 /Xcode 26.6.
Branch `codex/apple-silicon-metal`; source base `d191bb062f` plus local review changes.
**Renderer code checkpoint:** `ec696aa615` (published renderer, before local diagnostics).
[Final review/Home report](../research/review-home-admission-20261122.md) is the
current numerical disposition; [canonical work](../work/active/metal-port.md) owns
remaining engineering. The register retains defect history and owner decisions.
**New scope has no push approval. Home FPS is not fixed.**

## Source and candidate

- Accepted Scheduling69450/source960bd3644a and Maintenance69445 remain preserved.
  Published PPD567/ec696aa615 and handoff d191bb062f are separate from this scope.
- Canonical source retains two main groups, original adoption and one-display capture
  controller. All 28 pinned source hashes match; private cap3/early/capture4 paths are
  absent from canonical code/tests. No new graphics preference or policy switch.
- Normal candidate: `~/Alchemy-candidates/Alchemy Metal Review Candidate 26.4.0d69454.app`.
  Executable SHA256 `7c4e45dce8e7066e7911d42dbee04291ad8cbccf29b09d5be6111ab965248dcd`;
  UUID `C1C910DE-5E27-3C74-93D6-0FD739075A62`, matching dSYM. Private identity
  `ppd567-review-20261122/identities/group2-final.json` records the exact dirty build.
- Native library SHA256 `6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.
  Actual OpenHTJ2K0.27 sole pixel decoding verified; OpenJPEG header/encoding separate.

## Last observed runtime and disposition

All measurement viewers quit normally. Final Club control PID88938 closed after its
100-second fixed view plus10-second drain. Four HTTP checks are complete; no further
policy, input-target or shader experiments are planned. The exact normal candidate
is now running as PID98258 at Club in native fullscreen. Login and rendered world
were observed through the UI; runtime OpenHTJ2K and executable identity match.
Diagnostics are off, with no LEAP helper or fixed-camera controller. It is ready for
owner testing; this runtime observation is not owner visual acceptance.

Cap3 removes main-policy waits but moves blocking to shadow admission and produces
worse frame-period tails. Earlier adoption moves a roughly11µs operation before the
gate without a material repeated frame gain. Both experiments stay out of production.
Exact captured dependency/resources, live limiter scope, input and sampled-memory
results are in the report, including incomplete gates and the separate un-attributed
completion tail. Qualified input counts are normal6, early10, cap3=12; the imposed
12 target was not met in two arms. Investigator ended repetition after owner feedback.

HTTP independent owner wake is implemented and measured. Club callback p95 bounds
improve from32.768–65.536ms(control1846) to2.048–4.096ms(candidate4621); sparse Home
samples and unequal populations stay explicit. This is a handoff-stage result, not
whole loading or FPS. Callback count/time bounds, cancellation and teardown remain.

## Completed engineering and actual qualification

| Scope | Executed proof / remaining owner boundary |
| --- | --- |
|582 water consumer | Real slot3/alpha1→3→1; focused3805/1 and integrated transparency1,829,148/13 pass API+Shader. Owner visual acceptance remains. |
|583 public tools |74new sanitized cases across qualification, closed Home and numeric memory helpers; final259tests/1optional skip0. Negative gates and original failures retained. |
|584 truth audit |211current-field corrections/44scoped accepted-removal closures; original histories retained.74per-ID missing-proof actions remain explicit, not74current bugs. |
|585 camera description | Request-time/non-atomic/radian/infinite-far description; behavior unchanged and integrated build0. |
|586 octree | Original diagnostic compile failure retained; fixed normal/diagnostic0, normal8264/7 and ASan/UBSan8331/8 pass. |
|587/588/589 HTTP | Independent bounded wake, exact-once nonthreaded service and response RAII; queued4783/21 and HTTP180/11 pass. Four live age checks complete. Owner testing remains. |
| Native diagnostics |572/12 API+Shader, CPU observers107/2 and native token140/2 pass. Diagnostic OFF avoids new clock/syscall/hash/allocation/callback work. |
| Build/app | Doctor, consistent integrated/restored full builds0; candidate346ARM64 components/signatures verified. Source hashes and exact symbols pinned. |

The final259-test run uses the existing requirements containing NumPy and PyYAML.
Two incomplete interpreter-environment attempts are retained; source was not changed
to hide them. Public memory reduction retains a global missed interval and actual
sample gaps; reference quantiles differ by at most1ULP of interpolation rounding.
Build/tests do not confer owner acceptance or resolve the Home performance incident.

## Complete-product advancement assessment

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual parity and representative performance. |
| M-011 | doing | Runtime/foreign-process, reproducible release and strict no-GL qualification. |
| M-012 | complete | Harness/build/verification tooling maintained and exercised. |
| M-013 | doing | Retained Alchemy/RLVa/media behavior and prolonged stability. |

Required independent build/review evidence and release gates remain open; component
or local build success does not establish product completion.

## Exact next actions

Report/register history checks and harness are complete. Preserve the coherent local
changes and hand off the running NORMAL fullscreen candidate for owner testing.
Do not push without new owner approval.
Next engineering is the measured main lighting/forward-alpha GPU cost (PPD319), not
more queue-depth trials. Other surviving requirements and the audit's missing-proof
records are linked from the canonical work list; accepted removals remain closed.
