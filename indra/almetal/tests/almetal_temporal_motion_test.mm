/** Temporal motion/coverage fixtures through the production G-buffer and motion replay. */
#include "almetal_world_inputs_fixture.h"
#include <cmath>

namespace
{
struct MotionPixels
{
    std::vector<std::array<std::uint16_t, 2>> motion;
    std::vector<std::uint8_t> reactive;
    std::vector<float> depth;
    float x(unsigned x, unsigned y) const
    { __fp16 value; const auto bits = motion[y * side + x][0]; std::memcpy(&value, &bits, 2); return float(value); }
    float y(unsigned x, unsigned y) const
    { __fp16 value; const auto bits = motion[y * side + x][1]; std::memcpy(&value, &bits, 2); return float(value); }
};
ALMetalWorldTemporalPreviousDraw previousDraw(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldTemporalPreviousDraw p;
    std::copy_n(batch.modelview_projection, 16, p.modelview_projection);
    std::copy_n(batch.texture_matrix, 16, p.texture_matrix);
    if (batch.skin_palette) p.skin_palette.assign(batch.skin_palette, batch.skin_palette + batch.skin_palette_float_count);
    p.skin_joint_count = batch.skin_joint_count;
    std::copy_n(batch.avatar_wind, 4, p.avatar_wind);
    std::copy_n(batch.avatar_sin_wave, 4, p.avatar_sin_wave);
    std::copy_n(batch.avatar_gravity, 4, p.avatar_gravity);
    std::copy_n(batch.avatar_cloth_depth, 4, p.avatar_cloth_depth);
    p.valid = true; return p;
}
MotionPixels motionPixels(Fixture& f, ALMetalWorldSimpleBatch& batch,
    const ALMetalWorldTemporalPreviousDraw& previous, float camera_shift = 0.f,
    float jitter_x = 0.f, float jitter_y = 0.f,
    const ALMetalWorldOpaquePostDeferredScene* forward = nullptr, float water_z = 0.f, bool reverse_z = false, float wave_margin = 0.f)
{
    auto world = f.scene(); world.batches = &batch; world.batch_count = 1; world.reverse_z = reverse_z;
    world.temporal_jitter_pixels[0] = jitter_x; world.temporal_jitter_pixels[1] = jitter_y;
    auto gbuffer = f.renderer->render(world, &f.error); INFO(f.error); REQUIRE(gbuffer);
    ALMetalWorldTemporalMotionScene scene;
    scene.width = scene.height = side; scene.opaque_scene = &world;
    scene.depth = gbuffer->depthStencilTexture(); scene.reset = false; scene.reverse_z = reverse_z;
    scene.previous_opaque = &previous; scene.previous_opaque_count = 1;
    scene.forward_scene = forward;
    scene.jitter_pixels[0] = jitter_x; scene.jitter_pixels[1] = jitter_y;
    identity(scene.clip_to_previous_clip); scene.clip_to_previous_clip[12] = camera_shift;
    identity(scene.inverse_projection); scene.reject_water = water_z != 0.f;
    scene.eye_water_plane[2] = 1.f; scene.eye_water_plane[3] = -water_z;
    scene.water_rejection_margin = wave_margin;
    auto motion = f.renderer->renderTemporalMotion(scene, &f.error); INFO(f.error); REQUIRE(motion);
    MotionPixels result; result.motion.resize(side * side); result.reactive.resize(side * side); result.depth.resize(side * side);
    REQUIRE(motion->colorTexture(0)->readback(*f.device, 0, 0, {}, result.motion.data(), side * 4, &f.error));
    REQUIRE(motion->colorTexture(1)->readback(*f.device, 0, 0, {}, result.reactive.data(), side, &f.error));
    REQUIRE(motion->colorTexture(2)->readback(*f.device, 0, 0, {}, result.depth.data(), side * 4, &f.error));
    return result;
}
}

TEST_CASE("Temporal rigid object motion excludes camera-independent background and raster jitter", "[almetal][temporal][motion]")
{
    Fixture f;
    auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[12] = .25f;
    for (auto jitter : std::array<std::array<float, 2>, 3>{{{0, 0}, {.25f, -.375f}, {-.25f, .125f}}})
    {
        auto pixels = motionPixels(f, batch, previous, 0, jitter[0], jitter[1]);
        CHECK(std::abs(pixels.x(18, 16) + 4.f) < .02f);
        CHECK(std::abs(pixels.y(18, 16)) < .02f);
        CHECK(pixels.reactive[16 * side + 18] == 0);
        CHECK(std::abs(pixels.x(1, 30)) < .02f);
        CHECK(pixels.reactive[30 * side + 1] == 255); // no finite sky surface
    }
}

TEST_CASE("Temporal camera projection has analytical current-to-previous input-pixel displacement", "[almetal][temporal][motion]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[12] = .125f;
    auto pixels = motionPixels(f, batch, previous, -.125f);
    CHECK(std::abs(pixels.x(16, 16) + 2.f) < .02f);
    CHECK(std::abs(pixels.x(1, 30) + 2.f) < .02f);
    CHECK(std::abs(pixels.y(16, 16)) < .02f);
}

TEST_CASE("Temporal attachment and avatar palettes produce independent deformation motion", "[almetal][temporal][motion][avatar]")
{
    for (unsigned family : {4u, 6u})
    {
        CAPTURE(family);
        Fixture f; auto batch = f.batch(family); auto previous = previousDraw(batch);
        if (family == 4) f.attachment_pose[7] = .125f; // joint column0.w
        else for (unsigned joint = 0; joint < 15; ++joint) f.avatar_pose[joint * 4 + 3] = .125f;
        auto pixels = motionPixels(f, batch, previous);
        CHECK(std::abs(pixels.x(17, 16) + 2.f) < .02f);
        CHECK(std::abs(pixels.y(17, 16)) < .02f);
        CHECK(pixels.reactive[16 * side + 17] == 0);
        previous.valid = false;
        auto invalid = motionPixels(f, batch, previous);
        CHECK(invalid.reactive[16 * side + 17] == 255);
    }
}

TEST_CASE("Temporal alpha-tested legacy and PBR coverage agrees with production depth", "[almetal][temporal][motion][alpha]")
{
    for (unsigned family : {0u, 1u, 2u})
    {
        CAPTURE(family);
        Fixture f;
        const std::uint8_t texture[16]{255,255,255,255, 255,255,255,0, 255,255,255,0, 255,255,255,255};
        const auto* coverage = f.texture(ALMetalPixelFormat::rgba8_unorm, 2, texture, 8);
        f.geometry.uv[0][0] = 0; f.geometry.uv[0][1] = 0;
        f.geometry.uv[1][0] = 1; f.geometry.uv[1][1] = 0;
        f.geometry.uv[2][0] = .5f; f.geometry.uv[2][1] = 1;
        f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
        auto batch = f.batch(family); batch.diffuse_textures[0] = coverage;
        // Legacy material masks are selected by material_alpha_mode; the generic alpha_mask bit is illegal
        // for that family. Keep the production validator and the per-family coverage assertions unchanged.
        batch.alpha_mask = family != 1; batch.alpha_cutoff = .5f;
        if (family == 1) { batch.material_alpha_mode = 2; batch.materialSlots()[0].alpha_cutoff = .5f; }
        if (family == 2) batch.pbrSlots()[0].alpha_cutoff = .5f;
        auto previous = previousDraw(batch);
        auto pixels = motionPixels(f, batch, previous, 0, .25f, -.125f);
        unsigned covered = 0, uncovered = 0;
        for (unsigned i = 0; i < side * side; ++i)
        {
            if (pixels.depth[i] < .99f)
            {
                ++covered;
                CHECK(pixels.reactive[i] == 0);
                const unsigned x = i % side, y = i / side;
                CHECK(std::abs(pixels.x(x, y)) < .02f);
                CHECK(std::abs(pixels.y(x, y)) < .02f);
            }
            else ++uncovered;
        }
        CHECK(covered > 20); CHECK(uncovered > 100);
    }
}

TEST_CASE("Temporal animated UV and blended content conservatively reject history", "[almetal][temporal][motion][reactive]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.texture_matrix[12] = .25f;
    auto animated = motionPixels(f, batch, previous);
    CHECK(animated.reactive[16 * side + 16] == 255);
    batch.texture_matrix[12] = 0.f;
    auto blended = batch; blended.pre_water_alpha = true; blended.pre_water_minimum_alpha = .004f;
    ALMetalWorldOpaquePostDeferredScene forward; forward.batches = &blended; forward.batch_count = 1;
    auto pixels = motionPixels(f, batch, previous, 0, 0, 0, &forward);
    CHECK(pixels.reactive[16 * side + 16] == 255);
    CHECK(std::abs(pixels.x(16, 16)) < .02f); // transparent rejection does not replace opaque motion/depth
}

TEST_CASE("Temporal cloth motion uses prior submitted procedural deformation parameters", "[almetal][temporal][motion][avatar]")
{
    Fixture f;
    for (auto& clothing : f.geometry.cloth) { clothing[0] = .2f; clothing[3] = 1.f; }
    f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
    auto batch = f.batch(6); batch.avatar_cloth = true; batch.cloth_offset = offsetof(Fixture::Geometry, cloth);
    auto previous = previousDraw(batch);
    // Unit +Z source normal, gravity .1 gives .15 cloth displacement factor; .2*.15=.03 NDC, .48 input pixels.
    batch.avatar_gravity[2] = .1f;
    auto pixels = motionPixels(f, batch, previous);
    CHECK(std::abs(pixels.x(16, 16) + .48f) < .025f);
    CHECK(std::abs(pixels.y(16, 16)) < .025f);
    CHECK(pixels.reactive[16 * side + 16] == 0);
}

TEST_CASE("Temporal water rejection is bounded by the opaque eye-ray intersection", "[almetal][temporal][motion][reactive]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    auto behind = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .5f);
    auto front = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .1f);
    CHECK(behind.reactive[16 * side + 16] == 0);
    CHECK(front.reactive[16 * side + 16] == 255);
    CHECK(std::abs(front.x(16, 16)) < .02f);
    CHECK(std::abs(front.depth[16 * side + 16] - .625f) < 1.e-6f);
    auto crest = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .5f, false, .4f);
    CHECK(crest.reactive[16 * side + 16] == 255); // nearer wave bound intersects before the opaque surface
    auto near_water = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .1f, false, .2f);
    CHECK(std::all_of(near_water.reactive.begin(), near_water.reactive.end(), [](auto value) { return value == 255; }));
}

TEST_CASE("Temporal reversed depth retains motion sign and device depth", "[almetal][temporal][motion][depth]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[13] = .125f;
    auto pixels = motionPixels(f, batch, previous, 0, .25f, -.25f, nullptr, 0, true);
    CHECK(std::abs(pixels.y(16, 17) + 2.f) < .02f);
    CHECK(std::abs(pixels.x(16, 17)) < .02f);
    CHECK(std::abs(pixels.depth[17 * side + 16] - .25f) < 1.e-6f);
    CHECK(pixels.reactive[17 * side + 16] == 0);
    CHECK(pixels.depth[31 * side + 1] == 0.f);
}

TEST_CASE("Temporal reconstruction consumes production rigid and avatar motion with jittered color", "[almetal][temporal][motion][integration]")
{
    for (unsigned family : {0u, 6u})
    {
        CAPTURE(family);
        Fixture f;
        auto scaler = ALMetalTemporalScaler::create(*f.device, &f.error); INFO(f.error); REQUIRE(scaler);
        ALMetalTemporalScalerDescriptor descriptor;
        descriptor.input_width = descriptor.input_height = side;
        descriptor.output_width = descriptor.output_height = side * 2;
        ALMetalTemporalScalerRequirements requirements;
        REQUIRE(scaler->prepare(descriptor, requirements, &f.error)); INFO(f.error);
        REQUIRE(f.renderer->prepareTemporalMotion(&f.error));
        auto hdr = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
        auto reconstructed = f.renderer->prepareTemporalTarget(side * 2, side * 2, true, &f.error);
        auto resolved = f.renderer->prepareTemporalTarget(side * 2, side * 2, false, &f.error);
        REQUIRE(hdr); REQUIRE(reconstructed); REQUIRE(resolved);
        auto batch = f.batch(family);
        auto previous = previousDraw(batch);
        std::array<float, 2> centroids{};
        for (unsigned frame_index = 0; frame_index != 2; ++frame_index)
        {
            if (frame_index)
            {
                if (family == 6) for (unsigned joint = 0; joint != 15; ++joint) f.avatar_pose[joint * 4 + 3] = .125f;
                else batch.modelview_projection[12] = .125f;
            }
            const float jitter_x = frame_index ? -.25f : 0.f;
            const float jitter_y = frame_index ? .16666667f : -.16666667f;
            auto world = f.scene(); world.batches = &batch; world.batch_count = 1;
            world.temporal_jitter_pixels[0] = jitter_x; world.temporal_jitter_pixels[1] = jitter_y;
            REQUIRE(f.renderer->beginCommandBatch("Production avatar Temporal fixture", &f.error));
            auto gbuffer = f.renderer->render(world, &f.error); INFO(f.error); REQUIRE(gbuffer);
            // This independently authored fixture uses the production G-buffer's decoded material color as linear
            // HDR scene input. Actual world lighting/post orchestration has its separate presentation fixture.
            REQUIRE(f.renderer->resolveTemporalColor(*gbuffer, *gbuffer, *hdr, &f.error));
            ALMetalWorldTemporalMotionScene motion_scene;
            motion_scene.width = motion_scene.height = side; motion_scene.opaque_scene = &world;
            motion_scene.depth = gbuffer->depthStencilTexture(); motion_scene.reverse_z = false;
            motion_scene.reset = frame_index == 0; identity(motion_scene.clip_to_previous_clip);
            motion_scene.jitter_pixels[0] = jitter_x; motion_scene.jitter_pixels[1] = jitter_y;
            motion_scene.previous_opaque = &previous; motion_scene.previous_opaque_count = 1;
            auto motion = f.renderer->renderTemporalMotion(motion_scene, &f.error); INFO(f.error); REQUIRE(motion);
            ALMetalTemporalScalerFrame frame; frame.depth_reversed = false;
            frame.jitter_x = jitter_x; frame.jitter_y = jitter_y; frame.reset = frame_index == 0;
            ALMetalTemporalScaler::Ticket ticket = 0;
            REQUIRE(f.renderer->encodeTemporalUpscale(*scaler, descriptor, *hdr->colorTexture(0), *motion,
                *reconstructed, frame, ticket, &f.error)); INFO(f.error);
            REQUIRE(f.renderer->resolveTemporalColor(*hdr, *reconstructed, *resolved, &f.error));
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); INFO(f.error);
            REQUIRE(scaler->submitted(ticket, &f.error));
            std::vector<__fp16> pixels(side * 2 * side * 2 * 4);
            REQUIRE(resolved->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 2 * 8, &f.error));
            float sum = 0.f; unsigned count = 0;
            for (unsigned y = 0; y < side * 2; ++y) for (unsigned x = 0; x < side * 2; ++x)
            {
                const auto* pixel = pixels.data() + (y * side * 2 + x) * 4;
                CHECK(std::isfinite(float(pixel[0]))); CHECK(std::isfinite(float(pixel[1]))); CHECK(std::isfinite(float(pixel[2])));
                if (float(pixel[0]) > .2f && float(pixel[1]) < .15f && float(pixel[2]) < .1f)
                { sum += float(x); ++count; }
            }
            REQUIRE(count > 300); centroids[frame_index] = sum / float(count);
            previous = previousDraw(batch);
        }
        // Known .125 clip-space translation is 2 input / 4 output pixels. Reconstruction may soften the boundary;
        // the centroid must follow the independently moving skin rather than leave its prior silhouette behind.
        CHECK(std::abs((centroids[1] - centroids[0]) - 4.f) < 1.25f);
        CHECK(scaler->stats().encoded_calls == 2);
        CHECK(scaler->stats().submitted_calls == 2);
    }
}

TEST_CASE("Temporal rejects behind-camera and half-float-overflow previous projections", "[almetal][temporal][motion][disocclusion]")
{
    for (unsigned invalid_kind : {0u, 1u})
    {
        CAPTURE(invalid_kind);
        Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
        if (invalid_kind == 0) previous.modelview_projection[15] = -1.f;
        else previous.modelview_projection[12] = 1.e9f;
        auto pixels = motionPixels(f, batch, previous);
        CHECK(pixels.reactive[16 * side + 16] == 255);
        CHECK(std::isfinite(pixels.x(16, 16)));
        CHECK(std::isfinite(pixels.y(16, 16)));
        CHECK(pixels.x(16, 16) == 0.f); CHECK(pixels.y(16, 16) == 0.f);
    }
}
