# Apple-first architecture study — 2026-11-12

Status: analysis complete; implementation not selected. Owner requested a deep
dive using accumulated code/profiles and Apple CPU/GPU/UMA/Metal4/ANE features.
No viewer source change, build, launch or new capture for this study.

Source ef4290723c35; Apple M1 Max32GPU/8P+2E/64GiB, macOS26.5.2/Xcode26.6.
Doctor0; capability-only ObjC++ compile/run0, no submitted GPU commands. Metal4
family and queue/allocator/CB/table/residency creation verified; MetalFX support
flags observed. DenoisedFX runtime/table discrepancy retained as unknown, not a
functional/performance pass. Separate decoder experiment/dirty global ledger and
unrelated research files preserved.

[Architecture report](../../research/apple-first-renderer-architecture-2026-11-12.md)
selects retained generation-qualified render data, bounded detached CPU jobs,
compact Metal4 emission/residency, then GPU indexed visibility/draw execution and
tile-aware depth/lighting. UMA already exists; caches/ICB/NEON/GPU skinning and
resource retirement are real foundations. Preserve native tessellation during
Metal4 migration. ANE has no current supported/benchmarked viewer workload, so
no ANE integration is proposed. Existing PPDs remain open/owner-qualified.

Evidence: artifacts/metal/apple-first-deep-dive-20261112/ with official Apple
sources, source hashes, hardware/capability receipts and three scoped peer audits.
Three read-only peer reviews checked GPU/API, CPU/ANE/pacing and memory/source
claims. Clarified storage policy versus required lifetime, sparse mip fallback,
known deployment floor and the integrated MVP. Harness validator and diff checks
pass; no viewer tests/build were required by this documentation-only study.
Next action, if implementation is selected: freeze the attachment geometry/
material/pose generation boundary for one real main+shadow path; retain source
parity and compare complete direct-packet cost before adding workers/Metal4/ICB.
Prototype metrics must include fresh world frames, total CPU/GPU cost, queue
latency, bytes and quality. No FPS gain, corrective verification, push or release
approval inferred. Keep proposed changes local under the owner rule.
