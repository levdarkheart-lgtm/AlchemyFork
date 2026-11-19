# Warning-input runtime investigation — build69331

The owner reproduced broken floor sections after teleport (PPD470) and42missing bake masks (PPD471). Rigging fallback did not recur in this bounded run. Investigation and local repairs continue; this is not a port-correctness or performance acceptance.

## Exact run

- Source:96f10353af94981cdd70fb303be0525a60f9e22b; indra11bf6f5ae7ce1d192d330a4f0de4195d7753c660.
- Executable:26.4.0.69331, SHA128faa4ba39ddedca69db71f388e82e2ba7a3f5762f052f1a6d49e5eb302d9c7, PID74144.
- Mandatory pixel decoder: OpenHTJ2K0.27.0. OpenJPEG remains header/encoding support only.
- Existing AlchemyNext cache retained. No cache clear or performance recording.
- Owner controlled login, camera, teleports and logout. Starting scene was the club; later teleports were explicitly authorized. No Home interval was available.
- Log spans23:39:06UTC18November through00:00:19UTC19November; Goodbye retained and PID absent. Complete7,541,103bytes/SHA972950c7620bf531e969d839adc937fb42922e687c7ef889de2a1a15d66b575d.
-809WARNING rows at61source sites;0ERROR/FATAL. Counts exclude unrelated stderr and multiline continuation rows. Namespace-aware source parser leaves zero unparsed warning rows.

Private run root: `/Users/dekstroza/Alchemy-warning-inputs-wh3ienda`. Raw log, exact identity, observer/settings, immutable prefixes, closed-run-01 receipt, original source comparisons and owner images remain local. The two owner image hashes are578d7fec2fc57e684a5cf3b36e92cefd9ee6c9e76e879f6e5c925b9cc033cc6b and472e72de9d3623a70b4745591201e04e1166507ea3358a1eb9332b3985ae6304. Do not commit raw account, asset, location or request data.

## Findings and limits

PPD471:42consumer records contain valid primaryRGBA and no auxiliary pixels, needs_aux0, success1/final1. All29historical complete five-channel inputs decode successfully in87actualOpenHT requests. The private actual9body/realOpenHT test reproduces four cases/87behavior assertions: early aux succeeds, late aux can falsely complete, prior aux history requests recovery, and pause→publication→resume loses aux demand and falsely succeeds. This confirms inherited demand/readiness defects, while the exact live ordering and producer bytes remain unknown. A separate local repair must cover completed-worker admission and genuine maskless terminal behavior, preserving valid color/native generations.

PPD470/473: Owner explicitly confirms this club is high in the sky; the missing floor is mesh/prim object geometry, not terrain. Owner screenshots show missing/black/blue floor areas.230capture warnings identify the tree compound guard, not its individual predicate. One rejected tree clears all view draw collections and prevents G-buffer publication. Original tree binding uses a default image when requested GPU backing is absent; native tree shadow already resolves that default, but native world tree capture does not. This source-confirmed mismatch is separately PPD473; it is not yet proved to cause the floor screenshots. Geometry/transform/source-cache tracing continues.

PPD125: Later generated-height input was unavailable at23:56:17, aborting hero faces3and0 before post-deferred output. This follows the owner screenshots and does not establish their cause. The earlier null-buffer repair remains distinct from this generated-resource readiness observation.

PPD311/313: Zero structured skin records, skinning warnings, fallback/operand literals, broad nonfinite-rigging warnings and raw skin files through the final prefix. Absence is bounded nonreproduction; no fix or closure.

PPD472: Core handle reuse can suppress a later operation's context, so joins require policy/range/time validation. S8 component counts are raw control bytes, retained losslessly. Separate64unique category caps filled, including initial bake producer traffic; absence of a later producer record cannot prove absence of decode.44early map-tile403diagnostics are separate from20ordinary texture403failures in the asset-review prefix.19retained Easy28callbacks match the original expected eventqueue long-poll contract; this does not classify all historical timeouts. Material failures still lack per-asset transport attribution.

PPD474: Three inherited preferences XML bindings have no declared settings or consumers. Preserve intended behavior; do not conceal the issue with inert settings. PPD340 repeats one cache/index mismatch followed by existing sampled recovery, not evidence for clearing the cache.

## Current work and acceptance

Root owns `pipeline.cpp`, defect register and ledgers. Separate PPD471 agent owns texture/fetch changes in a persistent worktree. Independent source reviews cover tree binding and floor source/transform lifetime. Complete warning-site explanation reconciliation continues from the [69277 audit](log-warning-port-audit-20261118.md) plus newly observed sites. Repairs remain local, unaccepted by owner and unpushed; full builds and actual tests cannot close visual defects by themselves.


## Complete final-run warning source map

All809rows are accounted for at61sites below. Line numbers are pinned to runtime96f10353af, not the current edited checkout. Earlier explanations are reconciled from the69277audit and asset-live-02-final; final counts are independently regenerated from the closed raw log. Seven additional inherited bodies were compared against original1eb44 and are byte-identical; native mirror/terrain publication has no original Metal equivalent and is traced through its current owner. Site explanation is not a blanket cause/fix verdict.

| Runtime site | Rows | Trigger and immediate code outcome |
| --- | ---: | --- |
| `llcharacter/llkeyframemotion.cpp:2476 onLoadComplete` | 4 | Asset callback has nonzero retrieval status; marks the existing motion ASSET_FETCH_FAILED. Four load failures, not decode failures. Upstream fetch failure handling is unchanged; transport/cause per asset remains unknown. |
| `llcorehttp/_httppolicy.cpp:453 stageAfterCompletion` | 59 | 66 transport timeouts exhaust/disallow internal retries; final failed result goes to caller. Endpoint and caller recovery unknown. |
| `llcorehttp/httpstats.cpp:107 dumpStats` | 1 | Unconditional shutdown HTTP aggregate printed at WARNING severity; it is a statistics receipt, not another request failure. |
| `llfilesystem/lldir.cpp:781 findSkinnedFilenames` | 1 | Loading... contains two dots, so skin lookup rejects it. Caller identity unknown; retain traversal guard. |
| `llmessage/llexperiencecache.cpp:339 requestExperiences` | 2 | Empty capability delays request without consuming queue; later experience requests/results occur. |
| `llrender/llimagegl.cpp:3025 calcAlphaChannelOffsetAndStride` | 10 | GL_FLOAT unsupported by byte CPU alpha/pick analysis; conservative defaults. Upload is separate. Exact3images unidentified. |
| `llui/llmodaldialog.cpp:144 stopModal` | 3 | mModal is true but dialog pointer absent from static modal stack. |
| `llui/lltrans.cpp:264 findString` | 1 | Server supplied the literal ejection sentence as a lookup key. No matching translation template; findString returns false and caller can use the literal server reason. This does not prove a missing product translation. |
| `llui/lluictrl.cpp:594 setControlName` | 3 | UI XML names a setting not found by the context control lookup. |
| `llxml/llcontrol.cpp:1079 LLControlGroup::loadFromFile` | 1 | Optional user crash-policy override cannot open; previously loaded defaults remain. Absence versus I/O unknown. |
| `llxml/llcontrol.cpp:793 LLControlVariable::setValueFromNotation` | 1 | LEAP shell command fails notation parsing, then command parser preserves it as a string and processes it. |
| `newview/alassetblocklist.cpp:87 load` | 1 | Cannot open optional account asset blocklist; no persisted entries imported. Absence versus I/O unknown. |
| `newview/alderenderlist.cpp:168 load` | 1 | Cannot open optional saved derender list; no persisted entries imported. Absence versus I/O unknown. |
| `newview/llagent.cpp:4350 LLAgent::handleTeleportFailed` | 2 | Teleport failure handler invoked. |
| `newview/llappearancemgr.cpp:4178 LLAppearanceMgr::serverAppearanceUpdateCoro` | 1 | Skip duplicate/stale COF appearance request; prevents redundant POST. Does not certify loaded appearance. |
| `newview/llappviewer.cpp:1585 doFrame` | 2 | Signal handler changed and was restored; loop continues. Changed handler/owner/crash implication unknown. |
| `newview/llappviewer.cpp:2756 initLoggingAndGetLastDuration` | 1 | Prior log stat unavailable, so previous execution duration becomes unknown. No current-run crash inference. |
| `newview/llattachmentsmgr.cpp:429 expireOldAttachmentRequests` | 19 | Idle housekeeping finds attachment request timer >30seconds. |
| `newview/llattachmentsmgr.cpp:464 onAttachmentArrived` | 19 | No matching request timer remains and startup has passed wearable-wait. |
| `newview/llavatarrendernotifier.cpp:146 displayNotification` | 4 | Unconditional complexity/visibility notice logged at WARNING; does not measure actual attachment visibility. |
| `newview/llcontrolavatar.cpp:575 LLControlAvatar::updateAnimations` | 1 | Control avatar has no root volume during shutdown; warns once and returns before rebuilding signaled animations. No runtime floor or animation corruption follows from this row. |
| `newview/llgltfmateriallist.cpp:558 onAssetLoadComplete` | 38 | 19 failures across15materials call materialComplete(false). Referenced failed entry has no immediate blanket retry; cause/objects/recovery unknown. |
| `newview/llheroprobemanager.cpp:448 updateProbeFace` | 2 | Native hero face was not published; mirror remains unadvertised/retries. The later capture records generated-height input unavailable, separate from the high-altitude mesh/prim floor report. |
| `newview/llheroprobemanager.cpp:482 generateRadiance` | 4 | Native hero/mirror radiance readiness expression false. |
| `newview/llhudmanager.cpp:72 sendEffects` | 2 | Dead HUD effect remains in list at send traversal; warns and skips it. No invalid network send occurs; lifecycle ordering/emitter is not identified. |
| `newview/llinventorymodel.cpp:921 LLInventoryModel::findCategoryUUIDForTypeInRoot` | 8 | 8 login category misses return null; later inventory completion for each folder not established. |
| `newview/lllandmarklist.cpp:200 processGetAssetReply` | 1 | Landmark asset callback status is LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE after earlier retry/no-cap/alreadybad guards. |
| `newview/lllandmarklist.cpp:241 onRegionHandle` | 6 | 6 landmarks lack resolvable global position even after handle response; callbacks erased. Real navigation failures; inputs/recovery unknown. |
| `newview/llmediadataclient.cpp:613 LLMediaDataClient::Handler::onSuccess` | 1 | After successful HTTP media response, request is dead because owning media client is gone or underlying object is dead. |
| `newview/llmutelist.cpp:1107 onRegionChanged` | 1 | Current mute list is degraded fallback and the one allowed region-change retry has not occurred. |
| `newview/llmutelist.cpp:1147 loadFromFile` | 1 | Optional account visual-mute file cannot open; no saved settings imported. Absence versus I/O unknown. |
| `newview/llmutelist.cpp:252 updateLoadState` | 1 | Mute list is neither loaded nor failed, state requested, and requeststart+30seconds has elapsed. |
| `newview/llmutelist.cpp:264 clearCachedMutes` | 2 | Unconditional cache-clear lifecycle followed immediately by successful server-cached mute load. |
| `newview/llmutelist.cpp:324 tryLoadCacheFallback` | 1 | First fallback attempt and loadFromFile(cache,MLS_FALLBACK_CACHE) succeeds. |
| `newview/llmutelist.cpp:979 processUseCachedMuteList` | 1 | Server sends UseCachedMuteList after region retry flag set. |
| `newview/llpanelobjectinventory.cpp:1385 ~LLPanelObjectInventory` | 1 | Shutdown destructor finds no matching idle callback to remove; postBuild registers it. Warning alone cannot distinguish a never-built panel from callback-list cleanup or repeated removal. |
| `newview/llprogressview.cpp:444 LLProgressView::initStartTexture` | 1 | Saved bitmap load fails before decode/upload; same function selects explicit black image fallback. |
| `newview/lltexturecache.cpp:1857 purgeTextures` | 1 | Startup1/256sampled cache validation finds stored body size differs from index mBodySize. |
| `newview/lltexturefetch.cpp:1745 doWork` | 58 | 22 fetches have no usable formatted bytes, terminate DONE and release HTTP slot; precede missing texture state. |
| `newview/lltexturefetch.cpp:2205 onCompleted` | 58 | 22 HTTP403 responses mark fetch unsuccessful. Request/capability/content/server cause unresolved; no GPU upload reached. |
| `newview/lltexturefetch.cpp:2426 callbackHttpGet` | 15 | 5 full responses replace obsolete partial data after range mismatch. Recovery branch continues; final pixels unknown. |
| `newview/lltoastalertpanel.cpp:197 LLToastAlertPanel::LLToastAlertPanel` | 3 | Alert panel constructor logs notification message unconditionally during text/layout construction. |
| `newview/llversioninfo.cpp:162 getViewerMaturity` | 2 | Custom channel misses maturity regex and falls back to TEST_VIEWER; no shader failure. |
| `newview/llviewermessage.cpp:3315 process_agent_movement_complete` | 1 | AgentMovementComplete before avatar validity warns then continues; no teleport abort in this branch. |
| `newview/llviewermessage.cpp:6470 process_teleport_failed` | 2 | Incoming simulator TeleportFailed message dispatch. |
| `newview/llviewermessage.cpp:6490 process_teleport_failed` | 2 | Incoming simulator TeleportFailed message dispatch. |
| `newview/llviewermessage.cpp:6543 process_teleport_failed` | 2 | Incoming simulator TeleportFailed message dispatch. |
| `newview/llviewerobjectlist.cpp:1912 clearAllMapObjectsInRegion` | 9 | 1032/861 residual live map objects swept and marked dead at region removal. Counts may overlap; underlying reason/stale draw unknown. |
| `newview/llviewerparcelmgr.cpp:2112 processParcelAccessListReply` | 2 | Reply parcel ID differs from currently selected parcel; warns once and returns without applying stale access-list data. |
| `newview/llviewerregion.cpp:3588 setSeedCapability` | 8 | Duplicate seed starts comparison request and retains primary capability state; not itself a failed seed. |
| `newview/llviewerregion.cpp:3734 getCapability` | 18 | GetExperienceInfo queried before ready; joins early ExperienceCache delays and later processing. |
| `newview/llviewerregion.cpp:506 requestBaseCapabilitiesCompleteCoro` | 8 | Duplicate capabilities have107versus106entries; smaller response does not replace original. Differing keys unknown. |
| `newview/llviewertexture.cpp:2171 LLViewerTexture::processFetchResults` | 58 | 22 fetches lack GPU/saved CPU data and are marked missing after no-data failure. Native upload recovery is separate PPD-467. |
| `newview/llviewertexture.cpp:2543 LLViewerTexture::setIsMissingAsset` | 58 | 22 distinct textures enter persistent missing state; outstanding fetch removed. Eventual unflag/recovery not demonstrated. |
| `newview/llviewertexturelist.cpp:1367 LLViewerTextureList::updateImagesCreateTextures` | 7 | Loaded GPU level finer than changing demand queues maintenance;2throttled rows imply at least10incidences. Acceptance/outcome unknown. |
| `newview/llviewertexturelist.cpp:452 LLViewerTextureList::getImageFromFile` | 2 | unknown and Loading... local images fall back to IMG_DEFAULT/UI priority; specific widget/HUD unknown. |
| `newview/llvoavatar.cpp:10613 LLVOAvatar::processAvatarAppearance` | 12 | 12 equal-version self appearance updates skipped to prevent rollback; accepted texture completion is separate. |
| `newview/llvoavatar.cpp:11054 LLVOAvatar::onBakedTextureMasksLoaded` | 42 | 29 successful bake callbacks lack required1-channel auxiliary mask, skip CPU morph application. Disjoint from22missing textureIDs; actual mask content unknown. |
| `newview/pipeline.cpp:16665 describe_missing_native_terrain_input` | 1 | Generated terrain height backing is absent; classic terrain capture rejects the current view. It accompanies hero-face failure after the owner floor screenshots and does not identify missing club floor objects. |
| `newview/pipeline.cpp:17318 endMetalWorldGBufferCapture` | 230 | Native captured view is incomplete and failed-frame count has reached120. |
| `newview/pipeline.cpp:2337 endMetalHeroProbeFaceCapture` | 2 | Hero capture has no post-deferred target and returns unpublished. Both records first fail at classic generated-height resource validation, before lighting/post. No weakened completeness guard or proof of floor causality. |

The final-run literal-string lookup failure is the server ejection reason, not evidence that the renderer changed. Animation retrieval failures, actual asset403s, modal-stack and callback-removal warnings retain unresolved per-instance causes; protective discard/return behavior alone does not prove their producers correct. The final source map accounts for every row without turning warning counts into unique affected objects or shader/GPU errors.


## PPD473 local repair checkpoint

The function-local tree body is shared with durable actual-body tests. Resolve the requested image after the existing deleted-texture revival, then choose a resident default only when its own native backing is unavailable. Retain the resolved image lease rather than the absent requested generation. All unrecoverable pointers, required attributes, publication, nonzero extents and valid matrices still reject the whole current view. The local bounded diagnostic records first predicate and view (main1/hero2/probe3/other0); the historical untagged warning could arise from main or direct probe traversal.

Private red18cases/112checks fails4; proposal18/122passesASan/UBSan. Durable current-body tests pass6cases/288assertions both privately underASan/UBSan and in actual Release llrender_tests. Build/test commands: `cmake --build build-Darwin-warning-audit --config Release --target llrender_tests --parallel 4` exit0; `build-Darwin-warning-audit/llrender/Release/llrender_tests '[ppd473]' --reporter compact` exit0. Doctor on M1Max/macOS26.5.2/Xcode26.6 exit0. No app/GPU/owner verification yet; the club mesh/prim floor incident remains open.


## Cached prim admission isolation (PPD475)

A read-only copied region cache contains one approximate object bounding box beneath both screenshot positions. Actual production unpackers consume its complete381byte update: an ordinary six-face box prim with valid24vertices/36indices, no sculpt/mesh or glTF assignment. Top scalar alpha is1; actual diffuse alpha and three legacy materials are unavailable offline. Side transparent constants decode correctly with actualOpenHT. Eight relevant full material/alpha functions are byte-identical to original1eb44. This is a likely platform candidate, not exact broken-frame floor identity.

Independent actual-body cache-admission tests find a separate concrete liveness defect: with the real static front/back schedule, one of16LODhash phases never admits an object over2048frames after camera-inside query avoidance or repeated native Expired results. Other phases eventually progress; known-positive and known-zero query controls behave correctly.112controlledscenarios run underASan/UBSan. A naive initial fixture and a nonstarving acquisition-refusal case are retained; no broad claim about all unavailable results is made. PPD475 is registered before repair, and its connection to the owner floor remains unproved.


PPD476 is a prerelease integration finding in the PPD471 callback-only path: actual whole processFetchResults/upload-queue tests preserve normal color queue and duplicate controls (14cases/113checks), but the new early return precedes the normal4096full-dimension guard. Additional controlled oversized data reaches a callback; OpenHT success alone does not impose that consumer limit. A narrow parity correction is required before candidate delivery. Auxiliary groups with multiple channels or different sampling dimensions are not declared invalid without an applicable source contract.


## Combined repair qualification before application build

Local integrated code throughde0ce5052d contains PPD313/471/472/473/475/476 repairs. CMake registers durable avatar, auxiliary callback, whole fetch-consumer and cached-admission cases. Source-body tests exercise actual production functions with explicit decoder/queue/GPU boundaries; they are not in-world acceptance. PPD476 records the repair's oversized callback validation gap and retained failed/mutation cases rather than hiding it. Existing native query/texture provider tests pass1369assertions/9cases under Metal API/GPU validation. Actual llprimitive target passes1489/99; tree body6/288, callback30/67, avatar32, cache admission5920 and whole fetch consumer22/244 all pass. OpenHT-only engine/channel tests pass3/56.

The first tools-suite invocation used systemPython withoutPyYAML and failed to import one test module (181reported,oneerror/oneoptional skip). Reusing the already prepared toolvenv resolves that environmental failure:183tests pass/oneoptional skip. Original failure and corrected receipt are retained. Earlier runtime-doc harness line-limit failure was corrected without dropping history; current harness/diff/strict-source checks pass.

Original observed69331 app is preserved under private candidates/69331-observed.app with exact executable hash before build output reuse. An intermediate tree/skin-only application build completed0; it is not the final combined candidate. Final source-pinned build, bundle signature/slice/decoder checks and owner club teleport validation are still required. No new viewer is launched by these unit checks.


## Final cache producer and ownership integration

PPD340 now has a concrete original-equivalent producer reproduction: replacing a body with a shorter complete prefix via nontruncating LLFile::write leaves old tail bytes beyond the newly indexed size. The local repair opens only that UUID body with truncation and rejects positive short header/body writes. Actual producer bodies plus real LLFile synthetic-file tests show five expected baseline failures, then pass shrink/grow/same-size/header-only/open/error controls. Existing active cache files were never edited by this investigation. The historical29225/18786entry's exact write sequence remains unknown.

PPD477 first restored deferred failed-write cleanup, but independent review and an added overlap case prove that late failedA retirement can erase newer sameUUIDB (PPD478). That initial patch337c9647c8 is rejected for delivery and retained in history. Correctivec5102a03b0 invalidates completed failed writes in finishWork, on the serial cache worker before responder delivery and the next queued write; handled-state0 makes late endWork inert. Genuine abort/read policy and optional cache completion remain unchanged. Two overlap cases, null responder, real LLPointer ownership and ordinary read/success/abort controls are covered. Final26cases/153checks passASan/UBSan; rejected completion baseline has6expected failures. Source and lock-order review found no remaining scoped writer issue; no general concurrent cache-generation safety or exact runtime attribution is claimed.

Root adds an emit-only mode to the durable script and a normal generated CMake executable linked to current llcommon/al::flags with sanitizers. This removes generator-specific metadata discovery from normal test registration; the manual configured-Ninja red/green path is retained. The normal Release target and registered CTest pass26/153 here. Xcode generation itself has not been executed. A final cache delta/application identity refresh follows the successful23047d5f7dfull build.

PPD474 history confirms the three orphan controls once had real archived consumers. The attachment option can reuse an existing live policy; wearable/cinematic restoration needs real behavior, rather than dummy declarations. That open UI issue is inherited before1eb44 and is kept separate from this floor/bake/cache candidate.


## Final regression registration correction (PPD479)

Final69342build,346ARM64slice check, strictsource3626files/0blockers/0errors and deep strict signature pass. The targeted CTest aggregate initially reports12/13: the stray guard runs69existing included cases successfully (443396assertions) but correctly fails because no normal per-file entry owns them. Catch's fragment filename tags differ from the including .cpp filename. An exact listing maps those69ordinary cases to18fragments; add them as HEADER_FILE_ONLY SOURCES rather than disabling the guard or duplicating compilation. All18per-fragment entries plus the now-empty stray guard pass19/19. Eight hidden-only cost fragments and10hidden cases within mixed fragments stay manual.

The69342bundle/sourceceed0ef5fb and its failed-aggregate receipt are preserved, never launched. A metadata-only application refresh follows this CMake correction; runtime renderer/cache/texture code is unchanged. The final artifact identity and qualification receipt are saved privately under candidates/. This is a test coverage repair, not evidence of a new visual defect.
