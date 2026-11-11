This is an early, owner-directed Apple Silicon Metal prerelease. It is not a
general release or a claim that the product release gates are complete.

The download contains an ad-hoc-signed native ARM64 macOS application archive
and its SHA-256 checksum. The viewer uses the open-source dependency set; no
private Alchemy registry components are included.

This update completes additional native RLVa and developer controls, retires
displaced rendering code and resources, and fixes snapshot/SMAA/water behavior.
Further corrections preserve debug feature masks, refresh avatar preview morphs,
retry current preview buffer generations, and honor media pixel layout and alpha.

The build gates strict owned-source OpenGL removal, ARM64 slices, code signing,
direct viewer/media-host graphics imports and archive checksums. Foreign browser
and media internals remain a separate audit scope. Complete feature/visual parity,
prolonged stability, independent release review and quality-equivalent performance
remain open product gates.
