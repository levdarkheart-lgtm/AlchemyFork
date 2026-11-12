# Rejected eight-worker burst experiment

**Status: reverted at the owner's request.** Owner reports slower gray-texture
resolution in the first heavy place and worse texture loading/FPS in the second,
closes the viewer and explicitly requests restoring the previous policy. The
three scheduler/worker test files are restored byte-for-byte to299e748^; the
entire indra tree matches98f41e652b again. The owner then explicitly approves
committing/pushing the restored4normal/6maximum adaptive decoder scope, then
explicitly expands approval to the named directional-shadow occlusion and
PPD-001 projector repairs and their tests.

2026-11-12. Owner explicitly requests implementing the proposed larger burst
ceiling, retaining normal4 on the M1 Max, then starting the viewer. Local scope
only; no push approval. Base290dd0907cb3b9604b4cdad5dcb9bd1047274d8c on
codex/apple-silicon-metal. Prior68880 adaptive-six and fixed-six68876 apps and
their populated cache must remain available.

## Trial policy (reverted)

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

## Build and launch evidence

Private artifacts: artifacts/metal/eight-burst-decoders-20261112/.
Doctor exits0. baseline-before.json pins68880/hash243b5787 and the populated
cache path. Release worker/image target build and all seven selected CTest
entries exit0:47 substantive cases/1500assertions. Real-worker tests admit
4→5→6→7→8, enforce8, and drain16requests exactly once; fixed8 drains12.
Pure controller tests pass326assertions/14cases with24topology rows and active
permit survival through backoff. Input hashes are unchanged during validation;
worker-tests/validation-summary.md records exact commands and fixture limits.
Read-only bounds review finds no defect; all four prior app hashes match.
Source299e748bcf1d5413211bbaa874c40e7af959b9b5 committed locally on
codex/apple-silicon-metal. Configure and wrapper build exit0, Release/O3/LTO OFF,
OpenHTJ2K0.27.0 decode retained. Exact commands in configure.json/build.json:

```sh
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Eight Burst' -DAL_BUILD_TESTS=OFF
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=3 nice -n 10 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Eight Burst.app'
codesign --verify --deep --strict --verbose=2 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Eight Burst.app'
python3 tools/metal/audit_gl.py --strict --output artifacts/metal/eight-burst-decoders-20261112/strict-source.json
```

All exit0.346Mach-O/zero invalid slices, signature valid, production source
3516files/912informational/zero blockers/errors. Same tested source hashes.
App26.4.0.68884 contains full299e748bcf source SHA; executable SHA256
b649705c2c88db0f5ce722fdde3cc895a1df90f1d1cb702333974543c55e5cfe.
build-identity.json and preserved-apps-after.json pin identity/four old hashes.

Exact app under build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/
launched PID61154 at19:08:41UTC. runtime-190659/launch.py and run.json record
argv/session settings/environment; no auto-login or graphics override.
Process path/hash verified; startup logs maximum8,normal4,adaptive1,valid8P/2E
topology and OpenHT codec; STATE_LOGIN_WAIT observed19:08:43UTC. lsof confirms
the populated retained cache open (33375files before launch); no clear/move/
migration/fallback. No recorder was active at launch verification19:08:58UTC.

## Captures and restoration

Owner explicitly requests recording. Both traces/TOCs save successfully(exit0):
runtime-190659/eight-burst/ records19:09:40.862–19:12:41.719UTC,180.856777s,
including login;332DWARF analysis warnings retained. other-place/ records
19:13:40.235–19:15:40.234UTC,119.999715s, ending automatically when owner exits
the viewer(return0). The log anchors second TP19:14:03/arrival05/finished06.
Owner confirms arrival later; receipt19:14:27.890 is not an exact TP timestamp.
The second recording starts before that teleport. Heavy CPU/GPU exports were
deferred throughout both captures; all analysis stops on the owner's rejection.
No causal speedup/regression percentage is claimed. Both private traces and
light first-phase queue observations remain preserved; no capture upload.

Restored exact68880 app/source98f41e652b/hash243b5787 starts PID63726 from
reverted-six-runtime-191704/launch.py. Strict signature passes again; hash and
process path verified. Startup normal4/max6/adaptive1 and STATE_LOGIN_WAIT
observed;33665retained cache files, no purge/migration or recording. This is the
previous validated app, not a freshly rebuilt reversion. The rejected68884 app
is preserved as evidence. Next: publish only the owner-approved4/6 decoder
implementation together with the two subsequently approved renderer repairs on
codex/apple-silicon-metal. Raw captures and five unrelated untracked research
notes remain private/local. The rejected8 change and explicit revert are
retained in history; the published source must match restored98f41 exactly.
