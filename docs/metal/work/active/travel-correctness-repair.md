# Repair the failed 68633 travel paths

Owner explicitly requested implementation after the failed live trial. Root
owns shared renderer integration/build/docs; agents own independent skinning,
fetched terrain binding and retained-background components/tests. No new launch,
account/cache/settings changes or performance-goal resumption during repair.
All prior HUD, upload, environment, avatar and performance repairs remain.

## Original contract and concrete changes

Reference: exact Alchemy/OpenGL `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
See [failed live evidence](travel-live-68633.md), not an inferred success claim.

- Original `llviewerdisplay.cpp::render_ui` and `swap` keep publishing interface
  after scene work. Native `beginMetalWorldUICompositionCapture` required a
  complete current presentation target; otherwise LLRender suppressed uncaptured
  UI and swap suppressed publication, leaving stale controls visible. New native
  owner retains only the committed PRE-UI BGRA8 world plus matching scene depth.
  Ordered fresh UI/HUD reads this immutable pair while scene inputs load. Exact
  snapshot/debug consumers still demand a current world. Device/surface/resize/
  reverse-Z mismatch uses a freshly cleared current-size color/depth pair, with
  far depth for ordinary HUD brackets. Never blend old combined UI into new UI.
  The redundant earlier main logical G-buffer clear was removed: the actual
  native G-buffer pass owns all MRT/depth/stencil clears. Otherwise the earlier
  clear could erase retained depth even when the new world later failed.
  Target leases pin pools; independent native aliases survive borrowed-wrapper
  retirement/resize. Future borrowed G-buffer writes exclude retained depth.
  A world/render-size mismatch now resamples encoded color with BilinearMirror
  and raw depth with PointClamp into one full UI-size pair, matching original
  final blit ownership. No Z conversion or offscreen row flip. Cached PSO and
  pool, completion-pinned inputs/output; one extra submission only when scaling.
  Existing final grain/dither still evaluates at world-size before this scale;
  exact noise/effect-coordinate parity under non-1 render scale remains open.
- Original `lldrawpoolterrain.cpp::renderFullShaderTextures/renderFullShaderPBR`
  texture binds revive fetched images and select Default then Null if pending.
  Direct terrain capture bypassed that selection. Resolve actual fetched inputs
  once per pool with strong owners, preserving ready sRGB vs raw placeholders
  and sampler behavior, for classic and PBR. Generated height/composition remain
  exact authoritative maps. Bounded diagnostics identify actual missing roles.
- Original `generateImpostor` clears the target even for a complete subject that
  emits no geometry. Native now accepts that transparent bake. Native publication
  returns a real validated quartet receipt; failed bakes keep `mNeedsImpostorUpdate`
  and do not advance cached dimensions/values/timestamp. Bounded diagnostics retain
  the first failing opaque/forward producer line across completeness resets.
- Original object palette uploads/CPU affineTransform consume twelve XYZ
  coefficients. Native-era stabilization checked all sixteen lanes and replaced
  a valid joint if only an unused W component was nonfinite. Validate the twelve
  consumed coefficients without caps or altering valid transforms. Genuine bad
  XYZ still receives precise bounded inverse/world/product operand diagnostics;
  its actual live upstream cause remains unknown, and no whole-generation policy
  change or arbitrary geometry gate was introduced.
- Original `idleUpdateNameTagText` intends to retry a display-name cache miss next
  idle. Distance/status lines set `mNameIsSet` again, defeating that retry. The
  same bug exists in the pinned original. Preserve pending status after adding
  ancillary lines, so a stationary avatar rebuilds when the cached name arrives.
  No avatar-loading gate, name format, privacy or RLVa policy change.

## Verification

Executed on the M1 Max in Release with Metal API+shader validation for GPU cases:

| Check | Assertions/cases | Result |
| --- | --- | --- |
| Affine palette coefficients / real affineTransform |201/3|pass|
| Production fetched resolver + actual classic/PBR terrain render |272/1|pass|
| Empty bake/quartet handoff/replay across eight formats/Z layouts |17070/1|pass|
| Retained UI, pool/borrowed lifetime, resize, scaled color/raw depth/HUD |29130/3|pass|

Initial retained-UI fixture failed because it selected the alpha-textured solid
shader without a texture; corrected to the nontextured uniform-color shader.
The corrected GPU run passed. Native source/helper changes and API/shader checks
were independently reviewed; root performed builds/GPU execution. Additional
existing UI/pool regressions and the canonical viewer build are being finished.
Private exact command/log records are under ignored
`artifacts/metal/private-travel-correctness-repair-20261110-01/`.
Backend tests cannot establish full-viewer live teleport/resize/visual success.
No FPS gain, complete shadow/sky/low-distance parity or resolution of every
reported geometry spike is claimed without the corresponding live evidence.
