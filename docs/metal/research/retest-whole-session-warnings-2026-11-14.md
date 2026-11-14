# Retest viewer warnings — whole session, 2026-11-14

The preserved68999 viewer session has **337 WARNING lines,0 ERROR/0 FATAL**.
The measured five-minute window has **11**:10HTTP timeouts and one avatar
complexity notification. The remainder is255before and71after measurement, covering startup, a later
teleport and shutdown. Several lines are stages of the same failure, not distinct assets.
The336DWARF warnings are separate Instruments messages about debug information.

All source interpretation uses compiled source
`4918fd7ce3976576eababff430910d73017c9bdb`, not a changing checkout. Capture is
10:19:21.899–10:24:23.666UTC. Private evidence remains in canonical ignored
`artifacts/metal/retest-busy-club-profile-20261114-101904/`: whole-session-warning-summary.json
and whole-log-{asset,native,other}-warning-audit artifacts. No new capture,
viewer interaction, settings changes or source repair occurred for this audit.

## Delivery and streaming

| Whole-session warning lines | Meaning |
| --- | --- |
|75HTTP policy|47HTTP403 forbidden,2HTTP404 not found,26libcurl Easy_28 timeouts. Only10timeouts during measurement. All report0HTTP-policy retries; callers can retry separately.|
|88texture follow-on lines|22fetch-failure episodes involving20texture IDs recur through HTTP failure, abort, fetch-failure and mark-missing stages. These are not88missing textures; the responses were403. None during measurement.|
|25glTF material callbacks|Failed material retrieval involving14IDs, generic status−1. This does not diagnose malformed glTF or failed Metal shaders. None during measurement.|
|31full texture-response lines|A full image arrived instead of the expected partial/range response. The viewer accepted it, replaced partial data and marked it complete. Possible extra traffic; no decode/submission failure established. All before measurement.|
|2immediate-downscale conditions|Before measurement, desired detail changed while loading. These do not count distinct physical GPU copies.|

These are real delivery failures where403/404/timeouts occur. They can leave
missing textures or default materials and change workload/appearance. Remote
permissions/content, expired resource access or a client request defect are not
isolated by this log. Counts from the different layers must not be added as if
they were independent failed assets.

PPD-122 records a logging-classification bug: _httppolicy.cpp intends to lower
4xx messages to DEBUG but checks HttpStatus::getStatus() instead of the HTTP code
held in getType(). The status member holds an internal success/error flag.
Thus403/404 remain WARNING. Correcting severity would not repair retrieval,
authorize content, change retry behavior or make these deliveries successful.

## Native rendering and mirror state

**37 scoped native entries:23before,0during,14after measurement.**

- 12mirror-capture-begin warnings occur before STATE_STARTED; native resources
are not yet ready. The ordinary world remains present and capture retries.
- 7incomplete face transactions:2initial renderer-setup cases and5during a later
teleport. Of the latter,1hits a PBR MASK/pass validation mismatch and4hit the
terrain validation compound. The precise terrain operand is not logged.
- 6face-not-published and5radiance-warming notices are downstream retry/unadvertised
mirror state. They are not additional independent GPU failures.
- 7float alpha-analysis warnings use format0x1406 (float). The inherited CPU
alpha/picking analysis declines that representation; this branch does not
reject the native texture or Metal render submission.

The post-capture teleport starts10:25:18 and arrives10:25:20–21. Actual quit is
10:25:51; cleanup starts10:25:53. The rejected faces at10:25:20–22 therefore
occur during a transition, not shutdown. PPD-125 retains this observed symptom
for coherent-state/lifecycle investigation; rejecting inconsistent input is a
protective branch, not by itself proof of a faulty validation rule.

First successful hero-mirror radiance publication is logged10:25:20. This proves
at least one successful publication, but does not establish final recovery after
all transition rejects. No eligible nearby hero/mirror owner is established in
the club window, so absence of first publication there is not proof of a broken
mirror. Saved mirror settings matching is not proof of equal active mirror work.
Existing PPD-088 SH-ticket failure and PPD-103 admission exhaustion are not
established by these warnings.

## Configuration, UI, certificate store and cleanup

- 6missing RenderSMAAUseSample setting reads default to false. Live readers exist
but no declared setting is found in the compiled defaults. PPD-123; actual AA
impact remains unproved.
- 5missing Pick.Enable callbacks are Picks-menu enable rules for Teleport, Info,
Edit, Map and Delete. Click handlers are separate; this does not prove all five
actions fail. PPD-124 tracks missing enable registration/state behavior.
- One generic XUI parameter parse warning has unverified resulting UI behavior;
PPD-127 retains it for exact parameter investigation.
- One CertExpired entry comes from loading the local CA store:120certificates
accepted,1expired entry rejected. It does not establish that a login/server TLS
certificate expired or that verification was bypassed.
- Shutdown logs one remaining volume group/LOD3reference, generating4lines.
PPD-126 retains that ownership-cleanup symptom; no live GPU leak is proved.
- Another shutdown warning reports509worker delete-list entries. Its source is
clearDeleteList(), which explicitly deletes and clears those workers. That
message alone is not evidence that509objects remain leaked after shutdown.

Other notices cover missing optional crash-settings/cache files, the custom
G2G5 channel being classified as Test, a previous-version duration marker,
signal-handler replacement, early capabilities/inventory requests, duplicate
capability announcements, stale appearance updates, missing landmarks and
object/inventory cleanup. They are not all renderer errors; some indicate
incomplete or stale state and must retain their context. The capture's one
AvatarRenderInfo complexity notification is unconditionally logged at WARNING.

## Assessment and record ownership

This session's warnings do **not** support a blanket claim that everything is
harmless or that every line is a native renderer bug. The delivery failures are
real; missing configuration/UI bindings and the HTTP severity test are concrete
code issues. Transition mirror rejects and remaining volume references need
further ownership/state investigation. None of the scoped native/UI/cleanup
warnings occurs inside the five-minute measurement, but asset availability and
actual mirror readiness still qualify cross-run rendering/workload equivalence.

This documentation-only audit is isolated on
`codex/retest-warning-audit-20261114` because the experiment checkout has
concurrent implementation work. It does not modify or advance that checkout's
HEAD, compiled app or code. New PPD-122–123 records are local audit records,
awaiting coordinated documentation integration; no fix/build/owner acceptance
or push is claimed. Preserve the recorded IDs/history and integrate the audit
records before allocating subsequent IDs. The active implementation checkout
is untouched by this audit.


Canonical coordination: branch-local audit IDs118–123 are retained as aliases
for canonical122–127, respectively. Canonical118–121 already belong to engine
integration defects; their records were not replaced. The source audit at
db4604df71 remains preserved. Fixes are isolated until root integration.
