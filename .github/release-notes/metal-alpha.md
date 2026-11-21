Alchemy Metal Alpha 8 is an early Apple Silicon prerelease for **macOS 26 or later**.

This release fixes camera-dependent breakup of layered rigged mesh surfaces.
Opaque, transparent and glow passes now keep those surfaces aligned as the
camera moves, including the retained animated mesh paths. The correction passed
native GPU regression checks and owner testing in candidate 69417.

It retains Alpha 7's native Metal engine, hardware-aware adaptive OpenHTJ2K
texture streaming, loading and water corrections, and optional per-chat native
Apple IM translation. Language models are prepared on the user's Mac with their
approval; they are not bundled into the download. Overall feature parity,
prolonged stability and quality-equivalent performance gates remain open.

The download is an **ad-hoc-signed ARM64 application ZIP**, with a SHA-256
checksum and build-provenance JSON. It is not Developer ID signed or notarized.
Extract the ZIP and copy the application to Applications. No engineering launcher,
Python observer or diagnostic command-line options are required by the product.
A clean-profile/Finder launch of this particular release artifact is a separate
runtime check; build/signature verification does not establish that result.

The package uses the open-source dependency set and the pinned OpenHTJ2K pixel
decoder. Native parcel music, WebRTC voice/video and prior upstream-port features
are retained. Existing HTTP/material asset failures and other tracked unresolved
issues remain under investigation; this release does not claim they are all fixed.

Build gates check source identity, owned-source OpenGL removal, ARM64 slices,
strict signatures, direct viewer/media-host graphics imports and archive checksums.
Foreign browser/media internals remain a separate audit scope. Source and exact
build identity are recorded by the release tag and accompanying provenance.
