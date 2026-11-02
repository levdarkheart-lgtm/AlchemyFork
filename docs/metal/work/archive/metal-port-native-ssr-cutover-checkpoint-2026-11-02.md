# Native SSR cutover execution checkpoint — 2026-11-02

Commit `53de84c79c` retires Metal's GL framebuffer/PBO SSR bootstrap. A first
native frame after startup/resize now warms coherent color/depth history while
remaining offscreen and ineligible for UI composition, snapshots or drawable
publication. Metal consumers no longer inspect GL target dimensions.

The source-exact RelWithDebInfo app build, 349-slice ARM64 verifier and physical
52,190-assertion Metal suite pass. Strict inventory falls by 14 to 3,337 and
still exits 1. The unfiltered 574-entry gate records 568 passes, six GL-host
failures and five skips; focused reruns reproduce three unique failures in
untouched GL tests. Exact commands, hashes and failure details are in
`docs/metal/state/archive/CURRENT-native-ssr-cutover-checkpoint-2026-11-02.md`.

Owner direction changes visual validation cadence: no unattended viewer driving
or snapshot capture. Visual questions are collected in
`docs/metal/work/active/visual-review-queue.md` for one owner-attended daily
batch. Engineering continues between reviews.

Next: remove the next synchronous GL/PBO ingress, beginning with the avatar-
impostor bake boundary. Keep strict audit, build, physical GPU and failure
evidence source-exact; do not revive broad draw suppression.
