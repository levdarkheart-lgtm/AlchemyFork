# Probe projector-shadow sharing and finding review — 2026-11-08

This checkpoint records the source review and repair for the owner-supplied
hero/probe shadow assessment. It is bounded source/build/test evidence, not a
visual-parity or isolated-FPS claim.

## Provenance

- Branch: `codex/apple-silicon-metal`.
- Implementation commit:
  `16820100044d46d66ed306dd77e8f0ca14797780` (`Reuse projector shadows
  across Metal probe views`).
- Exact original viewer: commit
  `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
- Exact original `pipeline.cpp` SHA-256:
  `bb52adcb83cf493d57246b6410294bb8579f429454b024aeae1ae66efa23231b`.
- Exact original `shadowUtil.glsl` SHA-256:
  `a197eb4ccaaccc5d6712b38f1001e8de75c2cec97841106c5fcd191e40baee76`.
- Implementation `pipeline.cpp`, `almetalworld.cpp` and Metal GPU-test source
  SHA-256 values are respectively
  `15803fde0d0bfec7fb699c2edfa1cf4be2bca6496f0ce591fbbcf81ca7704eaf`,
  `a96ae31210c8db21864520590c2d145be47127ac2ea9b9203655386291f52ebb`
  and `196a5190cee8b98bd5c3f26bd52bacf2d0089bd2433852ada442fb7bc29a5ccd`.

## Finding classification

### Performance attribution

Confirmed as an evidence limitation, not a new renderer defect. The recorded
5.9-to-6.35 FPS comparison lacked a pinned camera and source-exact retained
predecessor. The later `68532` trace measured a combined change set. Neither
can isolate a projector-shadow, query, anisotropy or other individual gain.

### Projector maps rebuilt in cube views

Confirmed. Exact original `generateSunShadow()` always recomputes the two
eye-to-light matrices, then guards the projector caster camera, cull, cache and
draw with `!gCubeSnapshot`. Its two retained `mSpotShadow` maps are shared by
the main view and cube receivers.

Current Metal code kept the target/fade update guard but left planned map
selection, `sortShadow()`, caster hashing and native slice rendering reachable
for every hero or ordinary-probe face. The native capture-owner swap also gave
those views an independent projector texture, so this was real repeated CPU
and GPU work rather than only duplicated metadata.

Commit `1682010004` restores the original division of work:

- cube faces still compute and publish their own eye-to-light matrices;
- only an ordinary view can enter projector caster cull/cache/render;
- a cube face borrows the retained main-world R32 projector array and its
  validity bits from the capture owner left in the main-owner slot; and
- directional cascades remain view-owned, because the original does rebuild
  those for the probe camera.

The borrowed texture is accepted only on the same Metal device. Missing or
invalid main-world maps remain explicitly unbuilt and read lit; there is no GL
fallback.

### Missing projector probe single-tap flag

Confirmed. Exact original `shadowUtil.glsl` places `cube_snapshot` in the
shared sun/spot filter before PCSS, so both directional and projector shadows
take the cheapest single tap in cube captures. Metal already encoded and
tested this flag for directional shadows, and the shared MSL filter consumed
it, but the projector producer emitted only quality `0..3`. Projector
validation also rejected the flag and still required a positive Ultra PCSS
scale.

The projector producer now ORs `probe_single_tap` during cube capture. Its
validator masks the same shared filter configuration as directional shadows
and exempts only the flagged Ultra single-tap case from the PCSS-scale
requirement. A real-device regression proves that the projector path accepts
Ultra plus the flag with a zero PCSS search scale.

### Broad capture labels

Confirmed as an instrumentation-interpretation issue. The encoder opened for
opaque post-deferred replay also contains later families until a haze boundary;
the encoder opened for haze continues with later replay work. Labels now say
`post-deferred pre-haze` and `water/atmospheric-haze and later replay`, so a
capture does not present the whole interval as haze alone. This does not create
per-family GPU timing or split the pass.

### Unconditional query-state restore

Confirmed and repaired. `encode_visibility_at()` was called before every
retained G-buffer batch and unconditionally forwarded depth/fill restoration
to Metal even when no query matched that offset. It now returns immediately
when no visibility query changed render state, while preserving the existing
restore after one or more query draws. This is a bounded CPU-encoding cleanup;
it does not establish query effectiveness or a frame-rate gain.

### Discard-capable opaque variants and Apple HSR

Grounded only as an unproven risk. Opaque material families do include
discard-capable alpha-mask variants. Source inspection does not prove that
those variants disabled a useful hidden-surface path for the measured scene,
nor that specializing more pipelines is a net win. Preserve coverage and
derivative semantics until a matched Metal capture shows fragment occupancy or
overdraw attributable to this choice. No speculative shader change was made.

## Validation

- `tools/metal/doctor.sh`: exit 0 on MacBookPro18,2, M1 Max, 64 GiB, macOS
  26.5.2 and Xcode 26.6; selected device is Apple family 7.
- Focused native world/deferred/projector case: 1,991 assertions pass.
- Focused native visibility-query case: 10 assertions pass.
- Complete real-device Release Metal suite: 52,870 assertions pass across 59
  cases.
- `tools/metal/build.sh build`: warnings-as-errors Xcode Release build/stage
  succeeds after the implementation commit.
- Source-exact candidate: `26.4.0.68537` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
- `codesign --verify --deep --strict`: exit 0.
- `tools/metal/verify_app.sh`: 349 Mach-O slices checked, zero invalid non-ARM64
  slices.
- Candidate executable SHA-256:
  `60c6e2772eff0c51f76db3a5e36782c40d9ce2e3662db9fa77d39f77e9985c6b`.
- Packaged Metal library SHA-256:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.
- `python3 tools/metal/validate_harness.py`: all 32 required files pass.

## Open runtime proof

No visual or performance conclusion follows from the build and GPU tests.
Owner-attended `68537` should smoke projector-shadow continuity in the main
view, ordinary probe faces and hero mirrors, then record a settled capture that
counts projector caster traversals/draws by view. Cube views should have none.
Only a matched capture may quantify the saving. A later HSR experiment needs a
separate capture-derived hypothesis and must preserve alpha coverage.
