# Dependencies, licences and independent build

## Baseline
Use the documented upstream **open-source-only** `xcode-os-arm64` CMake preset. It does not require Alchemy's private vcpkg registry. Source/build dependencies still must be inventoried for architecture and redistributability. Alchemy's public GitHub Actions has completed ARM64 macOS viewer and test builds; this does not prove the user's local Mac build or GPU tests.

## Known functional alternatives in Alchemy
- Physics: open-source physics extensions / V-HACD rather than licensed Havok wrapper. Do not claim Havok-specific binary parity.
- JPEG 2000: OpenJPEG rather than proprietary Kakadu.
- Audio: FAudio and OpenAL alternatives to FMOD; compare sound/stream behavior against the *open-source* Alchemy baseline.
- Voice: WebRTC. Media: CEF/VLC as applicable to macOS, including their separately managed processes.
- Optional proprietary Discord integration, crash reporting and other SDK integrations must not be silently enabled. Audit source and vcpkg manifests at fork HEAD.

## Release and private use
- Respect LGPL-2.1 and every linked third-party licence, including source/notice obligations for redistribution.
- Use a distinct channel/name/bundle identifier/settings path and accurately identify the fork. Check Second Life Third Party Viewer Policy before connecting/distributing. Do not impersonate Alchemy or Linden Lab or reuse their trademarked identity as though official.
- Run `lipo` on app-owned Mach-O executables and loaded non-system frameworks. Do not claim an ARM64-only package based on the main binary alone.
- Local private-use authorization does not automatically grant distribution rights to separately licensed third-party components.
- Never auto-fetch private registries, scrape licensed packages, use unlicensed Havok/KDU/FMOD binaries or publish credentials.

## Remaining evidence needed
Codex must record resolved vcpkg package versions, macOS deployment target, actual link graph, codesign/notarization needs if distributing, and verified media/voice functionality in the feature matrix. Do not assume that deleting Havok/KDU/FMOD means full equivalence to a proprietary release.
