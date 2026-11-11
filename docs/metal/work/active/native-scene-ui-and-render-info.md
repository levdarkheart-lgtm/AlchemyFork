# Native Scene Monitor, sampled UI and RenderInfo — 2026-11-11

Renderer checkpoint0b219b9728b7dda9864357e1546a7e7337ce274c on
codex/apple-silicon-metal. Physical M1 Max64GiB/macOS26.5.2/Xcode26.6.
Original source1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 inspected before fixes.
Goal active; no viewer/appearance/window/menu/screenshot/user-cache action.
Actual offscreen native GPU commands/pixel readbacks are bounded test evidence.

## Scene Monitor ownership and original behavior

22e648236a/9e4c7eb894 (children9d7da20e26/14e508eea8) replace Windows-only
comparison/query rendering and the native no-op window-copy caller. Root frame
getter1c77fa745a/7162dd92f8 accepts only exact current complete main-world
transactions, excluding snapshots/probes/cutouts/warm-up/uncommitted work.
Reuse ALMetalRetainedBackground surface normalization (actual BilinearMirror
color/PointClamp depth) so reduced world resolution matches pre-UI window bytes.
Two source leases are pinned through actual command completion; dropping the
ticket/service during blocked GPU work cannot recycle pooled source handles.

Preserve original lower-left RGB8 crop, PointClamp comparison into full diff,
raw4x4Bayer channel cutoff, RGBA8 quantization, BilinearMirror half-size sample,
strict RGBsum<tolerance and numeric .5 legacy normalization. Original alpha is
constant1 from RGB8 capture, so input alpha differences are ignored. Raw Bayer
bytes remain raw despite sRGB storage because original sampler SKIP_DECODE.
Original double-face count comment is unsupported by actual two-triangle quad;
no duplicate work is invented. Any finite tolerance is accepted; negative/all
and above3/zero outcomes match original comparisons, NaN/inf fail honestly.

Initial cutoff test had14 failures: actual normalized n/255 texture read and
fast reciprocal expression differ one ulp. Independent actual38assertion read
control proves round(read*255) recovers exact authored bytes. Use integer RGB8
delta/BayerByte*15 comparison without epsilon or widened tolerance. Do not
claim bit identity with incidental old driver floating ties. Same GPU oracle
passes after correction. Final registered root513assertions/sevencases0 with
Metal API/shader validation: channels/alpha/equality, bilinear threshold, crop/
orientation, odd extents/normalization, finite tolerances/invalid inputs and
actual blocked completion/source-pool lifetime.

Viewer retains cadence, independent nonblocking result fetch and trace data;
failed/unavailable measurements never record successful zero. Keep last
completed debug result while pending. Restore only state actually frozen by
monitor (prior FreezeTime/render flags/particle budget/avatar pauses), and
refreeze active monitor on public/teleport reset. Disable/shutdown guards remain.
Visible draw retains original ALWAYS/no-depth-write bracket. Policy is source/
Release checked, not a live viewer result. Root shader-manager/monitor syntax0.

c50db7562f registers native service/tests, removes displaced two shader globals/
Windows loaders/four GLSL files, and removes uncalled LLRenderTarget window-copy
stub. Only corresponding old driver case removed; all other old state bodies
compare identical. Native crop/current-owner/real-output tests replace its
meaningful source-selection boundary; no live window test claimed. Reserved
uniform indices remain unchanged.

## Raw and target texture admission

634bbd02c5 (child00c56d1c23) fixes existing bindNative raw identity never reaching
authoritative UI texture/sampler admission; mirror mode also lacked explicit
texture forwarding. Original manual bind flushes preceding work and applies
sampler changes. Native hook does that explicitly; optional typed draw argument
keeps cached LLImageGL replay independent of current raw slot. Invalid slot/
type/integer/depth/usage is rejected; image display lists cannot retain raw
borrowed owners and invalidate instead of storing an unowned pointer. Existing
begin/end saved state restores exact image/raw/type/sampler bindings. Caller
retains wrapper through flush; actual commands retain encoded native resources.

Real red two-case baseline1153failed/1578assertions: raw stripes white, mirror
draw incomplete. Child final core7209/8GPU0 includes retained-image replay after
caller image release despite unrelated current raw texture. Five ReleaseTUs0.
9e27977324 (child6881ce2fd8) fixes same bug in target convenience overload:
red772/798 (white pixels plus falsely accepted depth/unknown sampler), final
8007/9GPU0. Preserve resolved ALSamplerDesc exactly (implicit BilinearMirror,
PointClamp and custom S-repeat/T-clamp), not a reconstructed bit mask. Normal
world target/depth metadata remains unchanged. Independent reviews no blockers.

abc91e6dec/32894ca519 (child885eabe439) actual Scene Monitor filtered texture ->
authoritative UI test: red80assertions/eight white-pixel failures, unchanged
oracle fixed80/onecase0 with API/shader validation. Actual asymmetric BGRA
red/green/blue/yellow pixels, replace/alpha blend over retained black+depth0 and
numeric ticket destruction afterflush/beforeUIcommit all proved. No viewer hook.

## Native RenderInfo

9814944134/0b219b9728 (child1545967f56) restore missing typed-world contribution
to DebugShowRenderInfo. Original LLVertexBuffer counted issued draws and input
elements (including instances); native world/shadow/water/post owners bypass
that generic path. ALMetalRenderEncoder now records actual emission sites,
including joined-index flushes, direct/static, instanced and patch draws. Publish
once at encoder end, independent of timing/commit/completion; generation reset
discards older encoders. Default disabled, no GPU query/wait/readback/IO.

Count every native UI emission too. Earlier encoder-wide UI opt-out missed
background/HUD/selection; actual red four failures/two cases while all pixels
passed. Final actual mixed background/foreground/resume/HUD gives4calls/12
elements with pixel oracles. HUD uses native totals alone; accepted LLRender
UI batches are a separately labeled unit. No per-draw opt-out framework.
ICB only reports range/slot metadata and explicitly unknown total draws/elements;
no guessed GPU contents. Exact direct records count their actual indices.
Overflow saturates/marks unknown. Function-static state follows synchronous
encoder owners; no speculative heap lifetime sink or timing-sink change.
Child actual11455assertions/16cases0 includes six new counter cases and existing
join/indirect/timing, API/shader validation. Seven ReleaseTUs/link0, peer reviews
clean. Final root registered rebuild/run still required at0b checkpoint.

## Buffer tail and receipts

b7433702b0/31e07a06d8 (childbd97c56add) remove unconsumed ALGPUStream and zero
getGLName stub. Five old storage invariants retain/expand actual native transfer,
partial read/nonzero clear/static+selfcopy/coherent map/GPU readback/move/bytes/
queued-copy retirement; three stream cases test an API with zero original and
current product users. All retained ALGPUBuffer bodies compare unchanged;
uniform/frame owners untouched. Root registered108assertions/fourcases0,
API/shader validation. Independent review no blockers. Full wrapper retirement
awaits separate actual consumer/diagnostic audit after uniform cleanup.

Root artifacts/metal/feature-completion-20261111/: native-buffer-invariant-*,
native-scene-monitor-registered-*, native-scene-monitor-viewer-syntax.json,
native-scene-monitor-tail-preservation.json, native-raw-ui-integrated-build.log,
native-scene-monitor-ui-integrated-build.log, native-ui-sampled-target-build.log,
native-scene-render-info-final-test-build.log. Child receipts:
/private/tmp/alchemy-native-scene-monitor-validation-20261111.json,
/private/tmp/alchemy-native-scene-monitor-ui-validation-20261111.json,
/private/tmp/alchemy-native-ui-raw-texture-validation-20261111/validation-receipt.json,
target-followup-receipt.json in same directory,
/private/tmp/alchemy-native-render-info-validation-20261111.json,
/private/tmp/alchemy-native-gpu-buffer-validation-20261111/validation-receipt.json.

Historical filters overlap and are not summed. Last verified canonical app is
still d28/26.4.0d68712; frozen0b native suites/canonical/appearance build and
import/signature/architecture checks are next. Goal is not complete: actual
RLVa silhouette RGB/zero-forward stamp gap is under repair, unconsumed uniform/
stencil graphs and old fence invariant admission remain; full parity/stability/
runtime no-GL/independent release review/performance gates remain open.
