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

| Surface | Minimum native/no-GL state | Nice-to-have work left |
| --- | --- | --- |
| `@setsphere` | Native Metal post owner implements blend, fixed blur, bounded variable blur, chromatic aberration and pixelation; it consumes retained depth and no longer makes valid native presentation ineligible. | Restart-qualified live checks; exact multi-effect/tween/reference parity; unusual or malformed-script cases; snapshot/cutout interaction. |
| `@setoverlay`, `@setoverlay_touch`, `@setoverlay_tween` | Confirm the existing UI-command mirror carries texture/tint/alpha output and remove any remaining dependency on executing the GL draw when GL authority is retired. This is a minimal cutover task, not a reference-pixel gate. | Exact overlay sampling, tween timing, alpha hit-test/touch behaviour and reference comparison. |
| `@setcam_textures`, avatar-distance silhouette and camera visual modifiers | Audit the native world census so an active modifier cannot invalidate or bypass native publication; implement only missing native data needed for cutover. | Exact texture replacement, silhouette thresholds and transition parity. |
| Camera position/focus/FOV/mouselook restrictions | CPU camera state is shared with the retained native world route; ensure no GL-only side effect is required at authority removal. | Complete command/reply and edge-condition parity matrix. |
| Names, UI visibility, chat/IM, inventory/outfit, attachment, movement, interaction and teleport restrictions | Preserve existing CPU/UI policy paths while removing renderer-owned GL. No separate Metal renderer is expected unless source tracing finds one. | Full RLVa behavioural regression suite and viewer-reference comparison. |
| Snapshot/cutout policy | Ordinary snapshot owners remain functional; source currently blocks cutouts while sphere or overlay is active. This policy must not block ordinary Metal presentation. | Decide and compare RLVa-specific cutout semantics. |

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
sources at HEAD, then close only minimum native/no-GL blockers before spending
time on the nice-to-have parity column.
