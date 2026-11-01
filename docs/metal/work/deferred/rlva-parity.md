# Deferred RLVa parity backlog

**Work item:** M-013. **State:** deferred nice-to-have. **Owner decision:**
2026-11-01.

The owner explicitly accepts RLVa as a non-release parity class. RLVa must have
enough native implementation that enabling it does not block Metal authority,
viewer-owned OpenGL removal or ordinary viewer use. Exact reference-level RLVa
behaviour and image parity are nice-to-have follow-up work. This exception is
limited to RLVa; it does not relax the core Second Life, open-source Alchemy,
native-only, stability or performance gates.

## Test prerequisite

RLV must be enabled in Preferences in **both** comparison viewers, saved, and
both viewers restarted before an on/off or reference/candidate check is valid.
A run that changes the preference without restarting is inadmissible.

## Recoverable backlog

The minimum cutover column is closed at this checkpoint. Everything in the
rightmost column is deliberately retained so a later “what remains from the
nice-to-have?” audit can answer from the repository rather than memory.

| Surface | Minimum cutover status | Minimum native/no-GL state | Nice-to-have work left |
| --- | --- | --- | --- |
| `@setsphere` | complete at `9c141e0887` | Native Metal post owner implements blend, fixed blur, bounded variable blur, chromatic aberration and pixelation; it consumes retained depth and no longer makes valid native presentation ineligible. | Restart-qualified live checks; exact multi-effect/tween/reference parity; unusual or malformed-script cases; snapshot/cutout interaction. |
| `@setoverlay`, `@setoverlay_touch`, `@setoverlay_tween` | complete at `79356bfd4f` | The ordered UI-command mirror carries the existing textured `gUIProgram` draw, AnisoWrap sampler, tint/alpha and blend. Missing Metal texture state fails the complete transaction closed. Touch masking remains CPU-side. | Exact overlay sampling, tween timing, alpha hit-test/touch behaviour and reference comparison. |
| `@setcam_textures`, avatar-distance silhouette and camera visual modifiers | source-audited complete | Forced textures mutate the `LLFace`/`LLDrawInfo` texture before native batch capture. Silhouettes select the shared avatar-impostor/color route, which the native world owner consumes. No separate presentation rejection remains. | Exact texture replacement, silhouette thresholds and transition parity. |
| Camera position/focus/FOV/mouselook restrictions | source-audited complete | These update the shared CPU camera state before either renderer consumes it; no GL-only presentation side effect or Metal eligibility rejection was found. | Complete command/reply and edge-condition parity matrix. |
| Names, UI visibility, chat/IM, inventory/outfit, attachment, movement, interaction and teleport restrictions | source-audited complete | Existing CPU/UI policy paths remain in place and do not require a separate renderer. Re-audit only if later OpenGL removal exposes a concrete dependency. | Full RLVa behavioural regression suite and viewer-reference comparison. |
| Snapshot/cutout policy | ordinary Metal path unblocked | Ordinary snapshot owners remain functional. RLVa-specific cutout policy does not reject ordinary Metal presentation. | Decide and compare RLVa-specific cutout semantics. |

The source census is deliberately broader than renderer work: at this
checkpoint 211 `indra/newview`/`indra/llrender` C++ headers or sources mention
`RLVa`, `RLV_BHVR` or `RlvActions`. Refresh it rather than trusting this count:

```sh
rg -l 'RLVa|RLV_BHVR|RlvActions' indra/newview indra/llrender \
  --glob '*.{cpp,h}' | sort
```

The authoritative behaviour/modifier inventory remains
`indra/newview/rlvdefines.h`; the two explicit visual-effect owners are in
`indra/newview/rlveffects.*`. When M-013 is resumed, first re-audit those
sources at HEAD, then work only the rightmost nice-to-have column unless a new
concrete Metal/no-GL blocker has appeared.
