---
name: metal-port
description: Implement and integrate actual Alchemy rendering code changes for a native Metal-only macOS ARM64 viewer. Use for editing renderer resources, SDL Metal presentation, shaders, UI text, scene passes, compute, readback or replacing owned OpenGL calls; not for generic product planning.
---

# Native Metal source work

Read `docs/metal/PRODUCT.md`, `ARCHITECTURE.md`, `VALIDATION.md` and the active execution ledger first. Verify the checkout's real symbols, state and call graph before applying changes; source files may differ from upstream observations.

1. **Modify actual renderer code.** Keep existing non-GPU viewer behavior. Implement native Metal C++/Objective-C++ integration with `.metal` shaders and Apple/SDL3 presentation. No GL shim, translation layer, fallback engine or general renderer framework.
2. Determine resource ownership and render passes before changing their implementation: who produces/consumes color/depth, camera matrices, materials, shader variants, CPU/GPU memory, readback and synchronization. Record required API semantic differences, not a generic GL-to-Metal renaming table.
3. Use platform-supported APIs on M1 Max, with runtime feature checks for later GPUs. Manage `MTLRenderPipelineState`, `MTLDepthStencilState`, samplers, resource binds, command buffers and in-flight lifetime explicitly. Preserve actual PBR and shader math unless correctness demands change.
4. When rewriting shader paths, build a variant/binding census from `llviewershadermgr.cpp` and GLSL includes; cover material alpha, depth, sRGB/gamma, winding, mip levels, texture sampling and uniforms. Verify output against upstream reference frames.
5. Protect shared files: `pipeline.cpp`, `llviewershadermgr.cpp`, `llrender.cpp`, `llwindowsdl.cpp` and shared shader managers have one integrator at a time. Delegate independent units with exclusive paths and acceptance tests only.
6. Build and run on real macOS hardware (`tools/metal/build.sh`, `verify_app.sh`), record Metal validation/capture evidence and update feature matrix. Handle compile/runtime defects rather than stopping at scaffolding. Continue on remaining work.
7. After meaningful implementation, update active work ledger and recovery snapshot with source SHA, exact tests and next executable changes. Product is complete only when full release gates pass.

Do not mark a subsystem complete because it draws a first frame. Do not claim a speedup without `gpu-validation` and paired measurements.
