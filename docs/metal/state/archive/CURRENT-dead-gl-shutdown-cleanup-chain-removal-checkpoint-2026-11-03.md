# Dead GL shutdown cleanup-chain removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `3ebe982748` (`Delete dead GL shutdown cleanup chain`),
after `27d519fde0` (`Delete empty particle GL shutdown hook`)

## Implemented cutover

- `LLVOPartGroup::destroyGL()` and `LLBox::cleanupGL()` were empty and had one
  shutdown caller each. Their calls, declarations and definitions are deleted.
- `LLViewerWindow::shutdownGL()` calls `gSky.cleanup()` before `stopGL()`;
  `cleanup()` nulls both sky owners. The later `gSky.destroyGL()` call therefore
  could not enter either cleanup branch. That call and the now-unreferenced
  `LLSky`, `LLVOSky`, `LLSkyTex`, `LLVOWLSky` and `LLDrawPoolWLSky` cleanup
  methods are deleted.
- `LLCubeMap::destroyGL()` had no caller after the dead sky chain was removed.
  It is deleted; normal object/member destruction retains resource ownership.
- Both bump shutdown wrappers called `LLBumpImageList::clear()` and then cleared
  the standard maps a second time. The duplicate standard `shutdown`/`destroyGL`
  methods are deleted, pipeline cleanup calls `clear()` directly, and the later
  post-shutdown `stopGL()` call is removed.
- Together the two commits remove 120 lines and add one direct `clear()` call
  across 18 files. Live font, avatar, dynamic-texture, manipulator and texture
  generation releases remain in their established order.

This is dead/redundant shutdown deletion, not runtime no-GL proof. The viewer
still creates, uses and shuts down a GL context and still links OpenGL.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on `MacBookPro18,2`: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` completed the warnings-as-errors Xcode Release build at
  committed source `3ebe982748`. It linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68200` (`26.4.0.68200`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O files as ARM64.
- Viewer executable SHA-256:
  `61f2a1cf0d87e62c35fe0aa0cfa9bfdb689a9a234294983ecd905c54f78311d5`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API, GPU, shader and load-action validation enabled, the complete
  physical Metal suite passed 52,414 assertions across 55 cases.
- Full `alrender_tests` passed 557,312 assertions across 137 cases.
- The exact `newview_tests` binary self-reported build 68200 and passed 98,973
  assertions across 743 cases: 742 passed and the established CRT case skipped.
- Exact packaged-binary inspection finds none of the deleted particle, sky,
  box or cube cleanup symbols. The only restore symbols remain the three live
  bump/manipulator initialization methods.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-dead-shutdown-cleanup.json` exited 1 as expected at
  2,408 scanned files / 3,253 findings. The count is unchanged because these
  wrappers were not audit-counted GL calls. Report SHA-256:
  `709a8061bdc372bb6d7f0b05b2f0aa90476039d9087c3e8fe8a402da5beae0b6`.
- `otool -L` still finds both OpenGL and Metal in the packaged viewer. Strict
  linked and runtime no-GL gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Dead
shutdown cleanup creates no visual question, so no queue row was added.
Existing parity questions remain in the owner-attended daily queue.

## Next executable action

Port the ordinary and joint manipulator grid textures from manual GL allocation
and per-mip upload to a shared Metal-native authored-mip producer. Replace
GL-name binds with texture-object sampling, add physical content/mip/lifetime
coverage, then rebuild and audit.
