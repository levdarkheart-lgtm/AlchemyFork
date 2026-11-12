# PPD-007 — HUD font position regeneration audit

Status: investigated; implementation not selected. Owner requested verification
of the x/y cache-invalidation, native allocation, inheritance and3.35% profile
claims. No rendering change, new build, launch or capture for this audit.

Source: a6b29f2e6d at start, c5c57a80bd during concurrent decoder work; inspected
font/HUD/render-storage files unchanged. Existing dirty global work-ledger
format correction and all other-chat source/build/cache work preserved.

[Source/profile investigation](../../research/font-position-cache-audit-2026-11-12.md)
confirms exact x/y mismatch→Position→full glyph-geometry regeneration, list
replacement and fresh native stores per successful nonempty captured flush.
Private actual-body decision probe12checks and placement-math probe12checks pass,
compile/run exit0. These do not render fonts or prove GPU/visual parity.

Exact68849 machine-code/raw-PC audit confirms7distinct regen call sites and
5.485CPU-s/3.34919%main CPU under name-label Position-selected ancestry. It is
not an avoidable-cost estimate; later keys may also differ, and glyph phase
changes require some regeneration. Local actual upstream65c39b7 and1eb44
inheritance verified; source/file/hash receipts retained.

Evidence: artifacts/metal/font-position-investigation-20261112/.
Next action: select completion-safe backing-store reuse or prove effective
placement-equivalence caching, preserving glyph phase, clipping, scale,
underline/emoji/fallback, right_x and resource generation. Keep PPD-007 open;
owner verification/push approval pending. No threading redesign required for
those bounded experiments, but benefit remains unmeasured.
