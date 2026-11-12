# Hardware-aware eight-worker burst candidate

2026-11-12. Owner explicitly requests implementing the proposed larger burst
ceiling, retaining normal4 on the M1 Max, then starting the viewer. Local scope
only; no push approval. Base290dd0907cb3b9604b4cdad5dcb9bd1047274d8c on
codex/apple-silicon-metal. Prior68880 adaptive-six and fixed-six68876 apps and
their populated cache must remain available.

## Bounded policy change

With valid topology, maximum admission is the smaller of fastest-tier physical
count and effective enabled total minus2, bounded2–16. Subtraction saturates;
the inherited minimum2 is preserved for missing/very small effective totals.
Normal admission is ceil(maximum/2), bounded2..maximum. M1 Max8P/2E therefore
selects normal4/maximum8. No cores are reserved or pinned by those budgets.
EmulateCoreCount still caps effective capacity. Unknown/inconsistent topology
keeps the old conservative total-minus4 and two-thirds-normal fallback.

Existing occupancy/age ramp, CPU/thermal/power backoff, quiet/recovery timing,
ticket ownership, callbacks, drain behavior, codec and upload paths remain
unchanged. A sustained backlog can progress4→5→6→7→8; increasing the ceiling
does not change normal admission to6 on this machine. This is a cap experiment,
not proof of optimal sizing or linear scaling across other Macs.

Root owns the production scheduler header, canonical docs, source commit,
separate app build and launch. Peers exclusively own pure-policy tests and
actual-worker integration tests; another peer reviews bounds/preservation.
No renderer fix is included; PPD-008 late native preparation remains separate.

## Evidence and next action

Private artifacts: artifacts/metal/eight-burst-decoders-20261112/.
Doctor exits0. baseline-before.json pins68880/hash243b5787 and the populated
cache path. Release worker/image target build and all seven selected CTest
entries exit0:47 substantive cases/1500assertions. Real-worker tests admit
4→5→6→7→8, enforce8, and drain16requests exactly once; fixed8 drains12.
Pure controller tests pass326assertions/14cases with24topology rows and active
permit survival through backoff. Input hashes are unchanged during validation;
worker-tests/validation-summary.md records exact commands and fixture limits.
Read-only bounds review finds no defect; all four prior app hashes match.
Build a distinct
Alchemy Metal Eight Burst app from a committed source SHA, validate ARM64 and
signature, then launch the exact executable with retained cache location and
adaptive1. Verify runtime maximum8/normal4 and recording-off status. Owner
controls login/destination/profiling timing; do not auto-login or clear cache.
No candidate launch or eight-worker performance observation yet.
