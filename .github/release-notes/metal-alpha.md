Alchemy Metal Alpha 7 is an early Apple Silicon prerelease for **macOS 26 or later**.
It includes the current native Metal engine and the owner-tested loading, water
and IM translation changes. Overall feature parity, prolonged stability and
quality-equivalent performance gates remain open.

Changes since Alpha 6:

- Apple Silicon engine work covering scene preparation, resource publication,
  lighting and shadow submission. Production defaults are retained; optional
  alternatives follow their individual controls.
- Reworked texture streaming with hardware-aware adaptive OpenHTJ2K decoding,
  memory-bounded decode ahead and background native texture uploads.
- Texture/cache, bake auxiliary-data, object admission and native upload fixes
  addressing loading problems seen during login and teleporting.
- Resource-readiness and snapshot-result corrections, plus the owner-verified
  fix for the camera-relative dark/grainy water band.
- Optional per-chat native Apple IM translation, with language preparation,
  compact settings popup and preserved original outgoing text in local history.
  Language models are prepared on the user's Mac with their approval; they are
  not bundled into the download.

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
