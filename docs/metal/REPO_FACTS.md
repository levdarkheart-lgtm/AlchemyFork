# Repository observations, verification scope and provenance

These are findings from public `AlchemyViewer/Alchemy` **develop** branch inspected 2026-10-24, NOT a verification of the inaccessible fork's current HEAD. Codex must verify every path and assertion locally before implementing.

| Verified in public upstream | Engineering relevance |
| --- | --- |
| `doc/BUILD.md` documents `xcode-os-arm64` and `ninja-os-arm64` open-source-only builds | No access to private vcpkg registry needed for baseline |
| `.github/workflows/build.yaml` contains xcode arm64 builds and tests | CI build foundation, but its macOS GPU/GL tests are disabled |
| `indra/llwindow/llwindowsdl.cpp` creates SDL_GL 4.1 macOS context and swaps | Concrete window-present integration path to replace |
| `indra/newview/pipeline.cpp` has `LLPipeline::shaderInputsFromSettings` checking `GLSL >= 430` for compute | Existing Mac GL 4.1 misses GL compute-specific paths |
| `indra/alrender/aldepthpyramid.cpp` dispatches compute only when available | Metal compute may re-enable functionality |
| `indra/alrender/alrenderresourceplan.h` has pure render resource descriptors for Main / Probe / Hero | Retain plan semantics, rewrite allocation/commands |
| `indra/llrender/llvertexbuffer.cpp` defines `LLAppleVBOPool` | M1-specific OpenGL workaround to remove with Metal buffers |
| `indra/newview/app_settings/shaders` contains 273 direct GLSL files (count on inspected tree) | Many source fragments and variants, not 273 distinct PSOs |
| `llviewershadermgr.cpp` declares many shader handles (154 static `LLGLSLShader g...` declarations on inspected tree) | Need a deterministic shader/pipeline binding census |
| Existing CI had passing `xcode-27` arm64 Viewer, Tests, and sign/package jobs on 2026-10-24 | Build demonstrated in upstream CI, not yet locally on fork |

Source locations: https://github.com/AlchemyViewer/Alchemy/tree/develop , https://github.com/AlchemyViewer/Alchemy/blob/develop/doc/BUILD.md and https://github.com/AlchemyViewer/Alchemy/blob/develop/doc/ARCHITECTURE.md . The reference build event is https://github.com/AlchemyViewer/Alchemy/actions/runs/40087359751 .

At the first local session record fork HEAD, actual default branch, local Xcode version, preset list, vcpkg lock/manifest, dependency architecture and whether host supports necessary Metal capture functions; update this table only with observed evidence.
