# PPD-001 — projector admission belongs to each light

2026-11-12. Owner selected PPD-001 in the defect register. Base source
`7a49b990ffd3b62b4ba8c4641d09388c1a42bb57`, local branch
`codex/apple-silicon-metal`. No push approval for this repair.

## Original mapping and chosen contract

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `pipeline.cpp`
ordinary-light loop11928–12023, projector loops12052–12081/12154–12180 and
void `setupSpotLight`12479: projectors draw independently after ordinary lights.
A problematic projector cannot veto remaining lights or world publication.
Original `deferredUtil.glsl`343–352/393 and native
`almetal_builtin.metal`12363–12365/12416–12418 use signed
`(distance-focus)/range` for cookie LOD. Preserve finite nonzero signed range
and the original forward projection with its [-1,1] cookie Z remap.

Concrete valid case: FOV pi/2, scale(1,1,2), physical radius1.1/effective1.65,
identity rotation: near.5, far.15, range-.35. A receiver at(0,0,-.75) lies
inside the cookie and light volume and can receive legacy/PBR light. Positive
near/range is not the original contract. FOV0 and range0 are singular;
nonfinite packed parameters likewise cannot be submitted to Metal.
The tools UI admits FOV0; network light parameters are not UI-clamped.

Chosen repair: exclude only the offending singular/nonfinite per-light math,
continue collecting valid neighbors, and share packed-parameter validation
with the backend to avoid a later all-light rejection. Keep cookie-resource,
shadow readiness, G-buffer/target compatibility and actual encoder failures
distinct and fail-closed. Publish an empty projector stage when no eligible
lights remain, as for an ordinary empty list. Do not bypass composition flags.
No shader math, draw budget, texture fallback or shadow ownership change.

## Evidence and limits

Local evidence: `artifacts/metal/ppd001-reproduction-20261112/`.
`python3 artifacts/metal/ppd001-reproduction-20261112/run.py` compiles the actual
collector/composition bodies with API spies: baseline compiles0, run1,15/34
failed checks. Negative-range B must retain A/B/C; zero-range or FOV0 B must
retain A/C; each successful stage must allow current-frame composition.
Valid-only, empty, excluded attachment and real renderer/shadow failure controls
pass at baseline. This proves CPU routing, not shader pixels or the cause of
the owner's busy-region freeze PPD-002. Physical GPU regression and candidate
build are the next verification boundary. Owner verification remains pending.
