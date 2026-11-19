# PPD003: bounded retained-world input evidence

The closed 69343 run confirmed fresh interface publication over an older retained
world image. Existing messages did not identify the current producer state at
that boundary. This change adds optional evidence there; it does not resolve the
world-freeze incident or change its attribution.

## Implementation and limits

With `ALCHEMY_WARNING_DIAGNOSTICS=1`, the existing stale-background branch in
`LLPipeline::beginMetalWorldUICompositionCapture()` records one structured
`world_retained_inputs` event per distinct view, readiness mask and first
incomplete source line. The existing process-wide gate now has eight categories;
the new world category admits at most 64 keys. Frames, ages, epochs and runtime
identities do not enter the key. The schema contains no account, asset, URL or
location fields. Without the exact opt-in, collection, key construction and
logging are skipped. Collection, admission and emitter exceptions are isolated.

The snapshot includes background/current frame correspondence, capture and
lighting flags, attempted stages, display/presentation frame stamps, exposure
frame and epoch correspondence, and stamped exposure/post policy facts. Policy
facts are captured at their real decision sites and have their own current-frame
qualification. The snapshot does not query environment settings later to invent
the policy that an earlier producer used.

`earliest_missing_product` identifies an absent or explicitly stale required
product when the observed state permits it. It is not a causal or GPU-completion
claim. Target presence alone does not establish current or completed output.
Separate light-map/base targets can be optional, and fused staging can already
have been consumed. Those lighting cases report
`unknown_with_lighting_readiness`; a snapshot with no qualified missing product
reports `unknown_with_readiness_flags`. The raw flags remain available. An
unknown post-policy stamp does not manufacture an exposure requirement.

Existing rendering, input, admission guards, failure counters and capture
behavior are unchanged. In particular, the existing incomplete-capture counter
reset/threshold limitation remains open under PPD003. These bounded diagnostics
can exhaust 64 distinct states and are not a complete frame history. An emitter
failure consumes its admitted key, preserving the bounded failure behavior.

## Offline validation

Base: `a3553def891b28827b02b613d2de3b0c11a8a64c`. Tests include the actual
production collection fragment and real shared gate; the CPU fixture substitutes
only capture values and the logger. It does not model the viewer or GPU.

Using Apple Clang with C++20, `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer:

- The original boundary body fails all three structured-schema assertions.
- The repaired actual collector passes disabled/enabled/classification/schema
  modes: 3/16/18/3 checks, zero failures. Controls cover no collection or allocation
  while disabled, 10,000 changing frames/epochs sharing one key, view/source-line
  distinctions, conservative lighting classifications, unknown/future age,
  exception isolation and recovery, and the 64-key bound.
- The real eight-category gate passes disabled/not-one/enabled modes:
  21/21/1048 checks, zero failures. These retain cross-translation-unit shared
  state, concurrent exactly-one admission, per-category limits and allocation
  failure controls.
- The complete `pipeline.cpp` translation unit passes ARM64 Release syntax
  checking and object compilation with the retained configured include/define
  environment and no PCH. `tools/metal/doctor.sh` and
  `python3 tools/metal/validate_harness.py` also pass.

The prerelease classifier was tightened after source review of optional lighting
targets and consumed fused staging; no runtime producer failure is inferred from
that correction. The final tests require conservative labels for those cases.

Exact argument arrays, outputs, negative controls, source hashes and compile
receipts are retained locally under
`/Users/dekstroza/Alchemy-functional-69343-n6ifzso_/analysis/ppd003-offline-01/`.
No viewer was launched, settings/cache changed, full application built or push
performed for this change. Full integration and owner runtime acceptance remain
pending.
