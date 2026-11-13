This is an early, owner-directed Apple Silicon Metal prerelease. It is not a
general release or a claim that the product release gates are complete.

The download contains an ad-hoc-signed native ARM64 macOS application archive
and its SHA-256 checksum. The viewer uses the open-source dependency set; no
private Alchemy registry components are included.

This update includes native parcel music through Core Audio and AVFoundation,
with song metadata, playlist/reconnect handling, and preserved WebRTC voice and
video ownership. Renderer preparation happens during login to avoid the reported
black startup world. It also includes the owner-verified fix for water bands
that appeared when turning the camera. The accepted OpenHTJ2K texture decoder
is rebuilt from its pinned open-source revision instead of falling back to
another decoder.

It also retains prior native RLVa/developer controls, snapshot/SMAA/water fixes,
avatar preview generation repairs and media pixel-layout corrections.

The build gates strict owned-source OpenGL removal, ARM64 slices, code signing,
direct viewer/media-host graphics imports and archive checksums. Foreign browser
and media internals remain a separate audit scope. Complete feature/visual parity,
prolonged stability, independent release review and quality-equivalent performance
remain open product gates.
