# PPD-125: cause of the twenty 69258 startup warning rows

The twenty hero/mirror warning rows in each successful 69258 Home run have two
source-supported readiness causes. They are distinct from the historical
terrain/null-buffer teleport failure under PPD-125. The owner subsequently
confirmed that mirrors work later in the viewer. That positive functional
observation narrows this correction to startup readiness; changed-source
runtime acceptance and the historical transition case remain separate.

## Scope

Isolated branch `codex/ppd125-startup-cause-20261118`, worktree
`/private/tmp/alchemy-ppd125-startup-cause-20261118`, base documentation commit
`b34d82d38e29ec9809b324fdf2d8a45a7d8b3d3b`. Exact recorded builder
`afc9278d23d7fa5e800e322a7e1fa913e21d1070`, production
`cfc20af4ba19c7df9d1cf406cc07a5eba4c7eda6`, build 69258. Inspected pipeline,
hero-manager and display blobs match that builder.

The initial diagnosis read existing logs and source only. The subsequent local
repair below uses the same isolated worktree. No viewer launch, login, GPU replay
or performance recording occurred. The concurrent engine worktree and its
documentation remain untouched.

## Sixteen rows: manager starts before the native gate opens

`LLHeroProbeManager::update()` and `renderProbes()` permit `STATE_PRECACHE`
(`indra/newview/llheroprobemanager.cpp:83,218`), but
`LLPipeline::beginMetalHeroProbeFaceCapture()` rejects states before
`STATE_STARTED` (`indra/newview/pipeline.cpp:2197–2209`).

The ordered startup-state scan places all twelve "could not begin" rows and
all four "radiance is still warming" rows per run within `STATE_PRECACHE`.
A selected live owner was present (`owner_ready=1`); native capture cannot
begin then. The failed-face flag also prevents `generateRadiance()` from
attempting publication (manager:463–466). ONCE messages are throttled; twelve
rows do not mean twelve attempts.

## Four rows: the separate probe renderer is compiling

After `STATE_STARTED`, the face-begin guard checks the ready main renderer,
then swaps to the separate probe capture owner (`pipeline.cpp:2212–2225`).
That new owner has no renderer yet. `beginMetalWorldGBufferCapture()` posts
asynchronous construction and returns while `capture.renderer` is absent
(`pipeline.cpp:4109–4110`).

Two face transactions finish during this warm-up per run. Each produces an
end-capture warning and its parent face-not-published warning. All six observed
end-capture rows have `renderer=0`, `gbuffer=0`, `batches=0`, `post_attempted=0`
and terrain predicate `not_evaluated`. They never reached usable native face
rendering or terrain preflight. Successful asynchronous probe-renderer
completion is logged afterward in all three runs.

`first_incomplete_line=0` follows from the new owner's initial incomplete
state: `markIncomplete()` records a line only on a complete-to-incomplete
transition (`pipeline.cpp:1272–1284`). The warm-up return precedes
`resetCompleteness()` and frame activation. Zero does not identify a terrain
failure here.

| Run suffix | PRECache / Started lines | Empty face / parent lines | Probe build completed: line / worker time |
| --- | --- | --- | --- |
| uphzzchc | 585 / 917 | 936 / 937; 1001 / 1002 | 1005 / 0.0723392 s |
| 77ib0k3e | 515 / 860 | 879 / 880; 944 / 945 | 951 / 0.320761 s |
| sowsq5_l | 520 / 851 | 870 / 871; 935 / 936 | 943 / 0.342276 s |

Worker build duration is not a measured whole-viewer stall or complete
readiness interval. Log-line ordering avoids assumptions from whole-second
timestamps.

## Original contract and remaining boundary

Original reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` also allows
precache updates (`llheroprobemanager.cpp:78–84`), but `initReflectionMaps()`
synchronously allocates cube-array storage (`548–601`) before rendering faces.
The native owner adds the stricter startup gate and asynchronous per-owner
renderer readiness. Those states need coordinated scheduling while preserving
complete-face publication, ordinary-world continuity, owner identity, clipping
and retry semantics.

No complete recorded log contains the once-per-process six-complete-face hero
radiance success message. Later mirror selection, occlusion and eligibility
are not continuously recorded. The owner's subsequent observation establishes
that mirrors do work later; the logs alone did not establish that. The
historical terrain null-buffer repair still needs its transition/runtime
acceptance. No FPS attribution follows from this startup diagnosis.

## Local readiness repair

Source commit `a7696a3c1d070346e49676e8084aaa8c10bd599d`:

- Leave `update()` CPU discovery/origin maintenance and its existing shader
  reset workaround unchanged; start native `renderProbes()` at STATE_STARTED.
- Prepare/poll the actual separate probe owner before optional admission,
  mirror-pass flags, target swaps or any face/radiance transaction. Use the
  existing asynchronous renderer builder and callback, without a wait or worker.
- While pending, retain prior coherent radiance, valid faces, owner/epoch and
  continuation ownership; maintain the current CPU owner/origin and retry.
- Retain preparation errors and all real face/publication diagnostics. Require
  the correct-device probe renderer at face entry and remove the displaced
  in-transaction allocation. All face completeness and six-face guards remain.

The shared production readiness body is exercised by five focused CPU cases:
early eligibility gates, missing/mismatched device, cold independent owner,
pending/ready publication identity, and retained visible preparation error.
Standalone ARM64 execution passes 116 assertions/5 cases. Two independent
read-only source reviews found no actionable regression. The tests observe
device/build boundaries; they do not claim to execute the asynchronous compiler
or prove live mirror behavior. The full isolated Release build passes, as does
the same 116-assertion/5-case filter in the built `llrender_tests` binary.

## Contained qualification

Source remains `a7696a3c1d070346e49676e8084aaa8c10bd599d`; production-file diff
since that commit is empty. Host doctor exits 0: Apple M1 Max, 64 GiB,
macOS 26.5.2, Xcode 26.6, native ARM64. Configuration uses `ninja-os-arm64`,
the existing read-only installed dependency tree, tests ON, LTO OFF and Release
for the build. Exact argv/environment/output are in the private receipts.

| Check | Observed result | Private receipt |
| --- | --- | --- |
| CMake configure | exit 0 | configure.json / configure.log |
| `tools/metal/build.sh build` | first attempt intentionally interrupted (130) to adjust parallelism; unchanged-source resume exits 0 | build-01.json / build-02.json and logs |
| Standalone actual-body CPU tests | 116 assertions / 5 cases, exit 0 | hero-readiness-tests.json |
| Built `llrender_tests [hero-capture-readiness]` | 116 assertions / 5 cases, exit 0 | project-readiness-tests.json |
| Strict source audit | 3,614 files; 0 blocking / 0 scan errors | strict-source.json |
| `tools/metal/verify_app.sh` | 346 Mach-O / 0 invalid non-ARM64, exit 0 | app-verification.json |
| `codesign --verify --deep --strict` | exit 0 | app-verification.json |

Built app: `/private/tmp/alchemy-ppd125-startup-cause-20261118/build-Darwin-ppd125/newview/Release/Alchemy Metal Development.app`,
observed version 26.4.0.69279. Executable SHA-256
`795f592d33d4679fc8553d58511bb91c675fef65c99588d99c62a2666687c588`.
No launch, login, live warning retest, mirror comparison, GPU/performance claim
or integration into the concurrent engine worktree follows from these checks.
The code and qualification remain local; no push.

Next runtime acceptance is a cold startup followed by an eligible mirror and a
transition: expected readiness should defer without false face warnings;
complete mirror publication and real rejection diagnostics must still work.

## Evidence

Private receipt:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd125-startup-cause-20261118/cause-evidence.json`.
SHA-256 `7f2695663be8107152eb15ddf7e9b7e2b2eb308ae52610308d1faa5f3515ccec`.
The ordered scan exits 0: stable whole-log re-reads/hashes, exact 12/4/2/2 split
and startup states, sanitized rows/warm-up events, and exact source-blob checks.
The initial diagnosis had no build or runtime validation; subsequent repair
commands/results are retained beside it in the same private evidence directory.
The first docs harness check rejected the two recovery files exceeding their
line limits by one; joining existing paragraph wraps preserves the text and
restores the limits. Exact command/status/output history is retained beside
the evidence receipt in `validation.json`.
The repair's final docs check also rejected free text in the defect register's
Owner verification enum. Its required `pending` value is restored; the positive
owner observation remains in dated history. The failed receipt is retained as
`final-qualification-failed-owner-enum.json` before the corrected final rerun.
