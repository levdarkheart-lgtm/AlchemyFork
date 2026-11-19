/** PPD003: execute actual product-reset and lighting-settlement bodies.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Renderer outcomes are controlled CPU inputs; no GPU success is inferred.
 */
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

// Baseline controls can select the exact original extracted source sections.
// Production and normal tests include the same current bodies by default.
#ifndef AL_FRAME_PRODUCT_RESET_BODY
#define AL_FRAME_PRODUCT_RESET_BODY "../pipeline_frame_product_reset_native.inl"
#endif
#ifndef AL_LOCAL_LIGHTING_RESULT_BODY
#define AL_LOCAL_LIGHTING_RESULT_BODY "../pipeline_local_lighting_result_native.inl"
#endif
#ifndef AL_PROJECTOR_LIGHTING_RESULT_BODY
#define AL_PROJECTOR_LIGHTING_RESULT_BODY "../pipeline_projector_lighting_result_native.inl"
#endif
#ifndef AL_LIGHTING_BATCH_SETTLEMENT_BODY
#define AL_LIGHTING_BATCH_SETTLEMENT_BODY "../pipeline_lighting_batch_settlement_native.inl"
#endif

namespace
{
using U32 = std::uint32_t;
struct Target
{
    bool present = true;
    unsigned resets = 0;
    int storage = 0;
    explicit operator bool() const { return present; }
    int& operator*() { return storage; }
    void reset() { present = false; ++resets; }
};
struct Scene {};
struct Renderer
{
    bool local_ok = false, projector_ok = false, commit_ok = false, active = false;
    unsigned local_calls = 0, projector_calls = 0, commit_calls = 0;
    bool accumulateLocalLights(int&, const Scene&, std::string* error)
    {
        ++local_calls;
        if (!local_ok) *error = "controlled native local encoding failure";
        return local_ok;
    }
    bool accumulateProjectorLights(int&, const Scene&, std::string* error)
    {
        ++projector_calls;
        if (!projector_ok) *error = "controlled native projector encoding failure";
        return projector_ok;
    }
    bool commandBatchActive() const { return active; }
    bool commitCommandBatch(std::string* error)
    {
        ++commit_calls;
        active = false;
        if (!commit_ok) *error = "controlled native batch commit failure";
        return commit_ok;
    }
};
struct Fixture
{
    struct MetalWorldSimpleCapture
    {
        // The production pipeline constant remains unchanged at120.
        static constexpr U32 max_failed_frames = 120;
        Renderer* renderer = nullptr;
        bool frame_active = false, attempted = true, continuous_world_capture = true;
        U32 continuous_world_frame = 0;
        Target target, composed_light_target, opaque_post_deferred_target, display_post_target;
        Target presentation_target, world_ui_target, depth_of_field_target, bloom_target;
        Target base_light_map_target, pending_probe_light_map, main_lighting_ready, base_light_target;
        U32 display_post_frame = 0, presentation_frame = 0, world_ui_frame = 0;
        U32 depth_of_field_frame = 0, bloom_frame = 0;
        bool world_ui_chain_active = true, world_ui_chain_counted = true;
        bool world_ui_chain_snapshot = true, world_ui_chain_debug_color = true;
        bool local_lighting_attempted = false, projector_lighting_attempted = false;
        bool lighting_composition_attempted = false, opaque_post_deferred_attempted = false;
        bool base_lighting_attempted = false;
        U32 local_lighting_failed_frames = 0, projector_lighting_failed_frames = 0;
        U32 lighting_composition_failed_frames = 0, lighting_batch_commit_failed_frames = 0;
        U32 base_lighting_failed_frames = 0, failed_frames = 0;
        std::array<int, 2> projector_shadows{11, 22};
        U32 projector_shadow_frame = 4;
    } data;
    Renderer renderer;
    MetalWorldSimpleCapture* mMetalWorldSimpleCapture = &data;
    U32 gFrameCount = 1;
    bool snapshot_request = false, mScenePixelPending = false;
    bool refresh_scope = false, refresh_lens_flare = false, refresh_lens_dirt = false;
    bool request_world_ui_chain = false, lighting_batch_active = true;
    unsigned warnings = 0;
    std::ostringstream messages;
    Fixture() { data.renderer = &renderer; }
    std::ostream& warning() { ++warnings; return messages; }

    void resetProducts()
    {
        auto& capture = data;
#include AL_FRAME_PRODUCT_RESET_BODY
    }
    void fresh(U32 frame)
    {
        gFrameCount = frame;
        data.continuous_world_frame = frame - 1;
        data.attempted = true; data.frame_active = false;
        resetProducts();
        // Successful G-buffer/base producers are external inputs to these
        // CPU settlement bodies. No shader or encoding policy is mirrored.
        data.target.present = data.base_light_target.present = true;
        data.continuous_world_frame = frame;
        data.attempted = true;
    }
    void local()
    {
        auto& capture = data;
        Scene scene;
#define LL_WARNS(category) warning()
#define LL_ENDL '\n'
#include AL_LOCAL_LIGHTING_RESULT_BODY
#undef LL_ENDL
#undef LL_WARNS
    }
    void projector()
    {
        auto& capture = data;
        Scene scene;
#define LL_WARNS(category) warning()
#define LL_ENDL '\n'
#include AL_PROJECTOR_LIGHTING_RESULT_BODY
#undef LL_ENDL
#undef LL_WARNS
    }
    void composedInputs()
    {
        data.base_light_map_target.present = data.pending_probe_light_map.present = true;
        data.main_lighting_ready.present = data.base_light_target.present = true;
        data.composed_light_target.present = true;
    }
    void settleBatch()
    {
        std::string lighting_batch_error;
#define LL_WARNS(category) warning()
#define LL_ENDL '\n'
#include AL_LIGHTING_BATCH_SETTLEMENT_BODY
#undef LL_ENDL
#undef LL_WARNS
    }
};
}

TEST_CASE("Native lighting retry streaks survive121 successful G-buffer transactions", "[lighting-retry-counters]")
{
    Fixture f;
    for (U32 frame = 1; frame <= 121; ++frame)
    {
        CAPTURE(frame);
        f.fresh(frame);
        CHECK_FALSE(f.data.local_lighting_attempted);
        CHECK_FALSE(f.data.projector_lighting_attempted);
        f.local(); f.projector();
        CHECK(f.data.local_lighting_failed_frames == frame);
        CHECK(f.data.projector_lighting_failed_frames == frame);
        CHECK(f.data.local_lighting_attempted == (frame >= 120));
        CHECK(f.data.projector_lighting_attempted == (frame >= 120));
        CHECK(f.warnings == (frame < 120 ? 0u : 2u));
        CHECK((f.data.projector_shadows == std::array<int, 2>{}));
        CHECK(f.data.projector_shadow_frame == std::numeric_limits<U32>::max());
    }
    CHECK(f.renderer.local_calls == 121);
    CHECK(f.renderer.projector_calls == 121);
    f.fresh(122); f.renderer.local_ok = f.renderer.projector_ok = true;
    f.local(); f.projector();
    CHECK(f.data.local_lighting_failed_frames == 0);
    CHECK(f.data.projector_lighting_failed_frames == 0);
    CHECK((f.data.local_lighting_attempted && f.data.projector_lighting_attempted));
    f.fresh(123); f.renderer.local_ok = f.renderer.projector_ok = false;
    f.local(); f.projector();
    CHECK(f.data.local_lighting_failed_frames == 1);
    CHECK(f.data.projector_lighting_failed_frames == 1);
    CHECK(f.warnings == 2);
}

TEST_CASE("Native retry state stays with each main or probe capture owner", "[lighting-retry-counters]")
{
    Fixture main, probe;
    main.data.local_lighting_failed_frames = 22; main.data.projector_lighting_failed_frames = 33;
    probe.data.local_lighting_failed_frames = 77; probe.data.projector_lighting_failed_frames = 88;
    main.fresh(1); main.renderer.local_ok = main.renderer.projector_ok = true;
    main.local(); main.projector();
    CHECK(main.data.local_lighting_failed_frames == 0);
    CHECK(main.data.projector_lighting_failed_frames == 0);
    CHECK(probe.data.local_lighting_failed_frames == 77);
    CHECK(probe.data.projector_lighting_failed_frames == 88);
    probe.fresh(1); probe.local(); probe.projector();
    CHECK(probe.data.local_lighting_failed_frames == 78);
    CHECK(probe.data.projector_lighting_failed_frames == 89);
    CHECK(main.data.local_lighting_failed_frames == 0);
    CHECK(main.data.projector_lighting_failed_frames == 0);
    // A new product transaction is not an attempted lighting encode.
    probe.fresh(2);
    CHECK(probe.data.local_lighting_failed_frames == 78);
    CHECK(probe.data.projector_lighting_failed_frames == 89);
    CHECK((probe.renderer.local_calls == 1 && probe.renderer.projector_calls == 1));
}

TEST_CASE("Native product retirement keeps attempt gates and generic G-buffer behavior", "[lighting-retry-counters]")
{
    Fixture f;
    f.data.local_lighting_failed_frames = 9; f.data.projector_lighting_failed_frames = 10;
    f.data.lighting_batch_commit_failed_frames = 11;
    f.data.failed_frames = 45; f.data.base_lighting_failed_frames = 33;
    f.data.lighting_composition_failed_frames = 34;
    f.data.local_lighting_attempted = f.data.projector_lighting_attempted = true;
    f.data.lighting_composition_attempted = f.data.base_lighting_attempted = true;
    f.data.frame_active = true; f.resetProducts();
    CHECK((f.data.local_lighting_attempted && f.data.target.present));
    CHECK(f.data.failed_frames == 45);
    f.data.frame_active = false;
    f.data.continuous_world_frame = f.gFrameCount; f.resetProducts();
    CHECK((f.data.local_lighting_attempted && f.data.target.present));
    f.snapshot_request = true; f.resetProducts();
    CHECK_FALSE((f.data.target.present || f.data.composed_light_target.present || f.data.base_light_target.present));
    CHECK_FALSE((f.data.local_lighting_attempted || f.data.projector_lighting_attempted));
    CHECK_FALSE((f.data.lighting_composition_attempted || f.data.base_lighting_attempted));
    CHECK(f.data.failed_frames == 0); // Deliberately preserve the existing G-buffer contract.
    CHECK((f.data.base_lighting_failed_frames == 0 && f.data.lighting_composition_failed_frames == 0));
    CHECK((f.data.local_lighting_failed_frames == 9 && f.data.projector_lighting_failed_frames == 10));
    CHECK(f.data.lighting_batch_commit_failed_frames == 11);
    CHECK(f.data.presentation_frame == std::numeric_limits<U32>::max());
    CHECK_FALSE((f.data.world_ui_chain_active || f.data.world_ui_chain_snapshot));
}

TEST_CASE("Actual lighting batch commit failures reach120 independently of CPU composition", "[lighting-retry-counters]")
{
    Fixture f;
    for (U32 frame = 1; frame <= 121; ++frame)
    {
        CAPTURE(frame);
        f.fresh(frame); f.composedInputs(); f.renderer.active = true;
        f.settleBatch();
        CHECK(f.data.lighting_batch_commit_failed_frames == frame);
        CHECK(f.data.lighting_composition_failed_frames == 1); // Existing transaction status retained.
        CHECK(f.warnings == (frame < 120 ? 0u : 1u));
        CHECK_FALSE((f.data.base_light_map_target.present || f.data.pending_probe_light_map.present));
        CHECK_FALSE((f.data.main_lighting_ready.present || f.data.base_light_target.present || f.data.composed_light_target.present));
    }
    CHECK(f.renderer.commit_calls == 121);
    f.fresh(122); f.composedInputs(); f.renderer.active = false;
    f.settleBatch(); // Prior constituent owner deliberately canceled the command.
    CHECK(f.renderer.commit_calls == 121);
    CHECK(f.data.lighting_batch_commit_failed_frames == 121);
    CHECK(f.data.lighting_composition_failed_frames == 0);
    CHECK(f.warnings == 1);
    f.fresh(123); f.composedInputs(); f.lighting_batch_active = false;
    f.settleBatch(); // This frame never requested that batch owner.
    CHECK(f.renderer.commit_calls == 121);
    CHECK(f.data.lighting_batch_commit_failed_frames == 121);
    CHECK(f.data.composed_light_target.present);
    f.fresh(124); f.composedInputs(); f.lighting_batch_active = true;
    f.renderer.active = f.renderer.commit_ok = true; f.settleBatch();
    CHECK(f.data.lighting_batch_commit_failed_frames == 0);
    CHECK(f.data.composed_light_target.present);
    f.fresh(125); f.composedInputs(); f.renderer.active = true; f.renderer.commit_ok = false;
    f.settleBatch();
    CHECK(f.data.lighting_batch_commit_failed_frames == 1);
    CHECK(f.warnings == 1);
}

TEST_CASE("Actual main and probe batch commit streaks recover independently", "[lighting-retry-counters]")
{
    Fixture main, probe;
    main.data.lighting_batch_commit_failed_frames = 17;
    probe.data.lighting_batch_commit_failed_frames = 29;
    main.fresh(1); main.composedInputs(); main.renderer.active = main.renderer.commit_ok = true;
    main.settleBatch();
    CHECK(main.data.lighting_batch_commit_failed_frames == 0);
    CHECK(probe.data.lighting_batch_commit_failed_frames == 29);
    probe.fresh(1); probe.composedInputs(); probe.renderer.active = true;
    probe.settleBatch();
    CHECK(probe.data.lighting_batch_commit_failed_frames == 30);
    CHECK(main.data.lighting_batch_commit_failed_frames == 0);
}
