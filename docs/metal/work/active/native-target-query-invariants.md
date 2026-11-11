# Native target, query and font invariants — 2026-11-11

Branch codex/apple-silicon-metal, physical M1 Max64GiB/macOS26.5.2/Xcode26.6.
Code checkpoint7162dd92f86dc1df099bf2c27d53b8fdc070d2eb; goal active.
Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 inspected before fixes.
No viewer/appearance utility/window/menu/screenshot/user-cache action. Actual
offscreen Metal commands and synthetic native pixel readbacks are test evidence.

## Integrated invariant admission

3c2192c2ed extracts scoped NativeFont/NativeFontUI fixture (child69a6cda5f2).
eb0d2a9618 (child2f5fa5d669) maps all24 original registry/COLRv1/shaping/UIImage
cases, registered154165dfc8. Existing assertions retained/strengthened with
published native buffer and full image/palette/page/pixel checks. First22/24
failures were stale fixture constructor flags creating GL instead of Metal
atlases; corrected LLFontRegistry(false,true), no production edit. Peer review0
blockers. Actual integrated276assertions/24cases0 with API/shader validation.

bdfc3f86c8 (child5ce24b6f6b) ports FontGL38/TextCache23 and registers a separate
native UI process in fb7c4873a3. Original static default getters hold lifetime
pointers; supported reload mutates heads in place, terminal registry destruction
is shutdown. Preserve that contract, no speculative production lifetime fix.
Named image-lifetime guard allowance matches original shared-session fixture;
all other per-case guards remain, exact terminal image count must match.
Shared shutdown avoids Catch assertions after its context is destroyed.
First fixture-only EMPTY-vs-COMPLETE and teardown failures retained. Actual
integrated1862assertions/61cases0, no skips. Deliberate missing-assets child
variant113assertions/fivepassed56honestskips0; explicit SKIP alone waives draw
requirement. Scoped FreeType160/11 still passes. Independent review no blockers.

fb7c4873a3 also ports original four2D cases,76assertions0. Original rotation/
rectangle helper bodies byte-identical, old assertion lines retained; actual
published Metal buffers replace driver reads, native capture retains the exact
57600-pixel outline oracle (two sizes/locations and1x/2x UI scale). Peer review
no blockers. Receipt native-2d-utils-preservation.json. Sphere range native
admission is currently pending in root, not yet claimed passing.

## Removed dead ownership

657da5ce23 (childa53386a128) moves the exact39format isImageFormat table to
its sole live LLRenderTarget allocation consumer. bd13164b70 removes unused
ALImageSlot binding/refresh/manual-name/counter/LLRender storage graph. No
native encoder consumed it. Four old cases exercise only ambient driver-state
mechanics; actual target write-usage/mips retained, native sRGB/depth negatives
added. Sampled bindings and typed compute owners unchanged. Peer review clean;
native target-usage/font filter447assertions/34cases0. No remaining source refs.

ef34b9c424 (child6a59f6fbc2) retains only actual native command receipts in
ALGPUTimer. Original generic spans have no native producer; uncalled avatar
place/end wrappers and their three driver-only cases are removed. Existing
pipeline attach/read semantics unchanged, no fake zero or wait. Real copy and
exact receipt-time/no-submit/pending/abandon/65independent-profile/reuse tests:
5666assertions/threecases0 with API/shader validation. Four Release compile
checks0 in child, including avatar/pipeline; root registered build/run0.
f2db7c729e removes displaced write-only mAttachmentSpans; actual native
mMetalAttachmentProfiles remain. Root Release pipeline syntax0.

## Reproduced defects still under repair

menu_port owns llrendertarget.* and bounded almetalresource.* transaction/
label additions in /private/tmp/alchemy-native-depth-target-test-admission-20261111.
Against canonical d28: shared-depth rebase replaces colors and magenta pixels
with zeros; label rename/swap/clear leaves actual native labels stale:
46assertions/twocases16fail, exit42. Further growth baseline: addColorAttachment
replaces old color/depth/serial/data, allocateDepth replaces existing colors:
21assertions/twocasessevenfail, exit42. Preserve original resource identity/
contents and failure rollback. Native shared-depth leases must survive producer
release/rebuild without changing producer labels. Six depth/projection cases
737assertions0 in child; full native target admission/repair still pending.

gl_tail_audit owns alquerypool.* plus original/native query tests in
/private/tmp/alchemy-native-query-pool-cleanup-20261111, basef2db7c729e.
Release currently leaves mSerial unchanged, so reacquired slot0 aliases a stale
ticket. Original release advances generation. Red CPUthreefail/onecase and
actualGPU15fail/onecase(three sections), exit42: stale attach changes clipped0
to visible1. Keep actual visibility capture/ring/result/wait/diagnostics; remove
only producerless spans/statistics/conditional/driver strategy APIs after an
explicit original20case census and native lifetime admission.

## Scene Monitor in progress

Original/current compare and aggregate remain Windows-only; capture used the
now no-op window-framebuffer API. Menu/view/freezing/recording remain live.
rlva_port owns llscenemonitor.*, bounded native service/test and additive MSL
in /private/tmp/alchemy-native-scene-monitor-20261111, basebd13164b70.
Original point comparison reads cropped display RGB8, raw Bayer values despite
sRGB storage (PointWrap defaults SKIP_DECODE), RGBA8 quantizes differences,
BilinearMirror half-size aggregate compares strict RGBsum<tolerance. Preserve
numeric0.5 legacy normalization; original front/back double-count comment is
unsupported by actual two-triangle helper and must not invent duplicate work.

Root1c77fa745a/7162dd92f8 expose only exact current complete main pre-UI frame
leases, excluding snapshots/probes/cutouts/history/unsubmitted work. Reuse actual
ALMetalRetainedBackground surface normalization so reduced world resolution
matches UI pixels, with no duplicate same-frame publish. Release pipeline
syntax0, not a functional menu/feature result. Native submit must pin both
source leases until actual command completion, including reset/destruction;
pending/failed work must never record a successful zero. Implementation/tests
and high-level wiring remain in progress.

## Command receipts and limits

Root receipts artifacts/metal/feature-completion-20261111/:
native-font-ui-test-build/gpu.log, native-default-font-registered-build/gpu.log,
native-2d-utils-build/gpu.log, native-image-binding-cleanup-build/gpu.log,
native-timer-registered-build/gpu.log, native-attachment-span-cleanup-syntax.json/log,
native-scene-monitor-frame-owner-syntax.log and normalized-owner-syntax.log.
All0; initial font test build selected Xcode app-only tree and failed because
llrender_tests target absent, preserved wrong-build-directory log. Correct
registered Ninja test tree built0. Current targeted filters overlap, no totals
are summed. Last verified canonical/appearance binaries remain frozen d28/
26.4.0d68712; current source still needs a full build after active repairs.
Full source/runtime/feature/stability/release/performance gates remain open.
