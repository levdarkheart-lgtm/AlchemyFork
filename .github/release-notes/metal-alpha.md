Alchemy Metal Alpha 9 is an early Apple Silicon prerelease for **macOS 26 or later**.

This release brings the optional world-resolution and reconstruction work into
this Metal branch. **Native remains the default.** MetalFX Spatial and MetalFX
Temporal offer 50%, approximately 67% and 75% modes where supported. Experimental
**67% Aurora Temporal**, sharpening OFF, uses a native-Metal port of Aurora's
WGSL reconstruction algorithm; the viewer has no wgpu runtime dependency.
Explicit scaled modes preserve output-based shadow budgets and native-resolution UI.

The owner accepted build 69531 in the affected scene: the reported background-water
blinking now looks fixed. Its SSR correction preserves reflected detail and normal
water animation while excluding the water receiver from reflection hits. Accepted
tile-grain, glass-fence and foliage improvements are retained. This acceptance is
for that tested appearance, not a guarantee for every water scene. The retained
mixed comparison and receiver-band approximation remain documented limitations;
they are not reasons for further visual tuning without a reproducible defect.

Actual GPU cost of the water correction remains unmeasured. Shader work counts
are not GPU timings or viewer FPS. Temporal recovered-area/settling quality bounds
and other tracked limitations remain open; these optional modes are experimental.
Use Advanced Graphics → World resolution to select a mode and check its effective
status. Unsupported cases report fallback. Use macOS screenshots/screen recording
for displayed Temporal quality; the independent viewer snapshot path can differ.

Alpha 8's rigged-overlay correction and earlier native Metal, hardware-aware
OpenHTJ2K streaming, Apple IM translation, parcel music, WebRTC and media features
are retained. The pinned OpenHTJ2K implementation remains the sole texture pixel
decoder. Overall feature parity, prolonged stability and quality-equivalent
performance gates remain open.

The download is an **ad-hoc-signed ARM64 application ZIP**, with SHA-256 checksum
and build-provenance JSON. It is not Developer ID signed or notarized. Extract the
ZIP and copy the application to Applications. No engineering launcher, Python
observer or diagnostic command-line options are required. The hosted artifact has
its own exact source/executable/shader identities; it is not relabelled build 69531.

Build gates verify source identity, owned-source OpenGL removal, ARM64 slices,
strict signatures, direct viewer/media-host graphics imports and archive checksums.
Foreign browser/media internals remain a separate audit scope. No raw private
captures, profiles, credentials or account data are included in release assets.
