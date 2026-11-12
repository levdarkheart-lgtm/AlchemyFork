# Font position cache: source and profile audit — 2026-11-12

The reported mechanism is grounded in code, and the 68849 capture's **5.485 CPU
seconds /3.349% of main CPU** is independently confirmed as ancestry through the
Position-selected regeneration call site. It is not a measured saving. Position
is the first failing cache condition, other keys may also differ, and some
fractional movement genuinely changes the required glyph output.

This is investigation only. No font implementation, viewer build/launch or new
capture was performed. The issue is retained as **PPD-007** in the mandatory
[defect/performance register](../POST_PORT_DEFECTS.md). Existing concurrent
six-decoder work and the dirty global work-ledger correction were preserved.

## Exact mechanism

1. `LLHUDNameTag::renderText` (`indra/newview/llhudnametag.cpp:351–411`) draws
   each segment through `LLHUDTextScope::draw` with its persistent segment cache.
   Moving camera/avatar/label position can change the projected screen point
   without changing the string.
2. `llhudrender.cpp:114–129` projects that point and splits x/y into integer
   translation and fractional residual. The integer part is applied to the UI
   matrix at138. The explicit cache origin at143 is `frac_x, 1 + frac_y`.
3. `LLFontTextCache::renderImpl`, `llfonttextcache.cpp:286–288`, compares exact
   float x/y after checking valid capture and font environment. Any unequal
   coordinate selects `RegenReason::Position`; case362 calls `genBuffers`.
   There is no epsilon or glyph-placement equivalence test.
4. `genBuffers` clears its retained shadow/foreground lists at391–392, builds
   new geometry and capture records at408–428, and correctly records the new
   `mLastX/Y` at486–487. This is not a stale-key bug. A later identical position
   replays the result.
5. Each successful nonempty captured `LLRender::flush` calls `genBuffer`
   (`llrender.cpp:2424`). At2588–2589 it creates a new LLVertexBuffer and vertex
   storage. Native allocation is `llvertexbuffer.cpp:759`→`535–550`→
   `ALMetalBuffer::create`, with fresh CPU staging plus a shared write-combined
   Metal store. The first upload uses that store rather than allocating twice.
   Empty/fully clipped captures can regenerate without allocating a buffer.

There is no one-buffer-per-character rule. Atlas changes, quad-batch limits and
shadow/foreground boundaries determine nonempty flushes. Retained replay uses
current UI matrices (`llfonttextcache.cpp:558`), so integer/current-origin motion
already moves cached geometry without rebuilding it. Explicit x/y is different.

A private CPU probe compiles the **verbatim production renderImpl body** and its
actual cache-state assignment block with generation/replay/resource spies.
**12 checks pass**: unchanged text/same position replays; a one-ULP change in
x or y regenerates; repeating the new position replays; integer movement with
identical fractional residual replays; existing color and environment controls
behave as expected. This verifies cache decisions, not real glyph rendering or
GPU allocation. Native allocation is established separately by the source chain.

## The 3.35% attribution survives a machine-code audit

The retained three-minute capture is PID31451, viewer26.4.0.68849, main sampled
CPU163.771 seconds. Exact executable SHA256
`4d04fdd723f9cebb3433e8ac0ed59cf69c5b929f75f1803f79e59b16c1959b88`;
UUID `225F5551-4B76-3596-8403-3E5BD20628DC`. Its dSYM and raw-recorded load
address were independently checked in the earlier recovery audit.

Checking source line362 alone would be insufficient: optimized code can merge
branches. Disassembly of this exact binary instead shows **seven distinct
regeneration call sites**. The Position block at preferred addresses
`0x101c3c1cc–0x101c3c218` is reached only with reason register `w20==3`; the x/y
mismatch sets that value. Other invalidation reasons have separate call sites.

A fresh raw-PC stack audit agrees with the previous source-line classifier on
all weighted stacks: **5.601 CPU seconds** under this Position-selected path
across main CPU, **5.485 seconds** with name-label ancestry, or **3.34919%** of
all main CPU. Of the latter,5.484 seconds contain the saved caller return PC
`0x101e4c218`;0.001 second is at a local Position-block instruction. The loaded
addresses use the recorded0x210000 slide.5.477 seconds also include a named
`genBuffers` frame; absence in the remainder is not proof of local-only work.

This is inclusive ancestry through the selected invalidation path, including
geometry construction, native allocation/publication and sampled descendants.
It is not3.35% exclusive time comparing floats, a regeneration event count,
or an FPS estimate. Later Span/Style/ShadowGate/Color conditions are not checked
after Position wins. The profile does not prove every selected rebuild could
otherwise be skipped.

## Inheritance is checked against actual local upstream history

The recorded local `upstream/develop` is
`65c39b7bad928e165f1acdc0cbf39478e017ef1d`, dated2026-10-23, and is the immediate
parent of reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`. The five audited
font/HUD files are byte-identical across that boundary. This is a local source
history fact, not a claim about today's remote branch tip.

That ancestry includes the exact-position text-cache policy, eight-phase glyph
rendering, fractional HUD placement and persistent HUD caches. Relevant recorded
changes include phases `8be68791a0`, fractional HUD placement `ce07d0108f`,
Position-reason reporting `0de54c32ce`, and HUD cache introduction `d230d79950`.
The inspected reference also creates fresh display-list vertex buffers on
regeneration. Current font/HUD source is unchanged from captured cb840/68841
through this investigation's c5c57a80bd checkpoint. Thus the invalidation policy
is inherited, while the backing-storage mechanism is now native Metal.

## Why deleting the position check would be unsafe

Origin movement does not itself change HarfBuzz glyph IDs/kerning. It can change
which cached rasterized glyph phase is selected. `place_glyph`,
`llfontgl.cpp:182–205`, chooses each glyph's integer pixel and phase together.
The selected slot supplies its own atlas rectangle, width and bearing at956–985.
Integer carry, per-glyph marks, clipping/ellipsis, scale and underline placement
can also change. Merely translating the previous quads preserves none of those
phase-dependent changes automatically.

A second private CPU probe uses verbatim placement/rounding code: **12 checks
pass**, including a counterexample to a simplistic eight-bin origin cache.
Origins0 and3/64 have the same origin phase; adding a glyph offset2/64 selects
phase0 versus phase1 for that glyph. One global origin phase is therefore
insufficient. Vertical marks are rounded separately, and `right_x` can vary even
when emitted geometry does not; a revised key must preserve that output too.
These are arithmetic checks, not a full geometry or visual-equivalence test.

Existing shaping, glyph bitmap/atlas, CPU batch arrays and deferred metadata
caches already work independently. Position regeneration does not mean new
FreeType rasterization or new HarfBuzz shaping on every draw.

## Bounded opportunities and limits

The least disruptive first experiment is **reuse of font backing storage during
necessary regeneration**, retaining current placement/phase behavior. Keeping
compatible logical LLVBs can preserve CPU staging and existing completion-aware
native-generation reuse. Alternatively, a bounded pool can accept completed
native stores when global retirement currently deletes them.

Use completion fences, capacity/layout/resource identity and exclusive capture
ownership. A refcount check, CPU age or fixed three-buffer ring is insufficient:
a cache may be rebuilt repeatedly inside one still-open frame. Fall back to
allocation if no completed compatible store exists; cap idle retention.

There is a separate native amplification: font atlas reassertion at
`llfontgl.cpp:758` reaches authoritative `bindMetalUITextureCapture`, which
unconditionally flushes at `llrender.cpp:1024` even for unchanged image/sampler.
This splits consecutive120-quad same-atlas batches into additional native stores.
An equal-effective-state fast path is worth examining, while preserving real
atlas switches, resource publication, default-image and failure behavior. Its
individual cost and safe guard have not been measured or implemented here.

Reducing unnecessary geometry invalidation is another bounded option, but it
needs a complete effective-placement equivalence/validity interval, including
per-glyph phase/carry/UVs, scale, clipping, underline, right_x and font/atlas
versions. No smaller generic cache key or speedup is established by this audit.

Private evidence: `artifacts/metal/font-position-investigation-20261112/`, with
`cache-probe/receipt.json`, `geometry/history-source-receipt.json`,
`geometry/place-probe-receipt.json`, `geometry/findings.md`,
`profile/font-pc-audit.json`, `profile/control-flow-proof.json`, and
`resources/audit.md`. Raw captures/symbol maps remain at their original paths.
The next step is selecting and validating a local experiment; no new capture,
rendering correction, owner acceptance or push approval is implied.
