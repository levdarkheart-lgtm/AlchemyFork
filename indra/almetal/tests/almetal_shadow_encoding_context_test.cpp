/**
 * @file almetal_shadow_encoding_context_test.cpp
 * @brief Independent native shadow encoding ownership and transaction parity.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#include "../almetalshadowindirect.h"
#include "../almetalshadowprepare.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <barrier>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> main_data;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    static constexpr std::size_t weight_offset = 48;

    Fixture()
    {
        device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        main_data = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(main_data);
        renderer->setFrameData(main_data);
        main_data->beginFrame(1);
        const std::array<std::array<float, 4>, 6> data{{
            {{-.75f, -.75f, 0.f, 1.f}}, {{.75f, -.75f, 0.f, 1.f}}, {{0.f, .75f, 0.f, 1.f}},
            {{.5f, 0.f, 0.f, 0.f}}, {{.5f, 0.f, 0.f, 0.f}}, {{.5f, 0.f, 0.f, 0.f}}}};
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(data);
        auto vb = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(vb); REQUIRE(vb->write(0, data.data(), sizeof(data), &error));
        vertices = std::move(vb);
        const std::array<std::uint16_t, 3> elements{{0, 1, 2}};
        descriptor.length = sizeof(elements);
        auto ib = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(ib); REQUIRE(ib->write(0, elements.data(), sizeof(elements), &error));
        indices = std::move(ib);
    }

    ~Fixture()
    {
        // Tests may intentionally abandon commands; submitted native work
        // must finish before the device and test resource publishers die.
        if (device) device->waitUntilIdle();
    }

    ALMetalWorldSpotShadowBatch batch(float z = 0.f) const
    {
        ALMetalWorldSpotShadowBatch result;
        result.vertices = vertices.get(); result.indices = indices.get();
        result.vertex_count = 3; result.index_count = 3;
        result.double_sided = true;
        std::copy(alMetalShadowIdentity.begin(), alMetalShadowIdentity.end(), result.modelview_projection);
        result.modelview_projection[14] = z;
        return result;
    }

    std::unique_ptr<ALMetalTexture> target(std::uint32_t width, std::uint32_t height)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_2d_array;
        descriptor.format = ALMetalPixelFormat::r32_float;
        descriptor.width = width; descriptor.height = height; descriptor.array_length = 2;
        descriptor.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        auto result = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        return result;
    }

    ALMetalWorldSpotShadowScene scene(const ALMetalWorldSpotShadowBatch& batch,
                                     std::uint32_t width, std::uint32_t height) const
    {
        // The aggregate is a real producer lease for both native buffers.
        struct Lease { std::shared_ptr<const ALMetalBuffer> vertices, indices; };
        ALMetalWorldSpotShadowScene result;
        result.batches = &batch; result.batch_count = 1;
        result.width = width; result.height = height;
        result.source_owner = std::make_shared<const Lease>(Lease{vertices, indices});
        return result;
    }

    std::vector<float> read(ALMetalTexture& target, std::uint32_t slice)
    {
        const auto& descriptor = target.descriptor();
        std::vector<float> result(descriptor.width * descriptor.height);
        REQUIRE(target.readback(*device, 0, slice, {}, result.data(),
                               descriptor.width * sizeof(float), &error));
        return result;
    }
};
}

TEST_CASE("almetal: shadow contexts encode independently across owner handoff and resize",
          "[almetal][shadow-encoding-context]")
{
    Fixture fixture;
    auto first = fixture.renderer->createShadowEncodingContext(&fixture.error);
    auto second = fixture.renderer->createShadowEncodingContext(&fixture.error);
    INFO(fixture.error); REQUIRE(first); REQUIRE(second);
    CHECK(first->frameDataStats().published_bytes == 0);
    CHECK(second->frameDataStats().published_bytes == 0);
    std::uint64_t frame = 1;
    for (const auto dimensions : {std::array<std::uint32_t, 2>{32, 32}, {48, 24}, {16, 40}})
    for (const bool reverse : {false, true})
    {
        CAPTURE(dimensions[0], dimensions[1], reverse);
        auto target = fixture.target(dimensions[0], dimensions[1]);
        auto other_target = fixture.target(dimensions[1], dimensions[0]);
        auto batch = fixture.batch(reverse ? .5f : 0.f);
        auto scene = fixture.scene(batch, dimensions[0], dimensions[1]);
        scene.reverse_z = reverse;
        auto other_scene = fixture.scene(batch, dimensions[1], dimensions[0]);
        other_scene.reverse_z = reverse;
        REQUIRE(fixture.renderer->renderSpotShadow(*target, 0, scene, &fixture.error));
        const auto expected = fixture.read(*target, 0);
        REQUIRE(std::any_of(expected.begin(), expected.end(), [reverse](float v)
            { return v != (reverse ? 0.f : 1.f); }));
        auto outer_owner = std::make_shared<const std::uint64_t>(frame);
        REQUIRE(first->slotAvailable()); REQUIRE(second->slotAvailable());
        REQUIRE(first->begin(frame, outer_owner, "Private shadow worker", &fixture.error));
        REQUIRE(second->begin(frame++, outer_owner, "Private shadow caller", &fixture.error));
        std::barrier rendezvous(2);
        bool worker_ok = false;
        std::string worker_error;
        std::jthread worker([&]
        {
            rendezvous.arrive_and_wait();
            worker_ok = first->encode(*target, 1, scene, &worker_error);
        });
        rendezvous.arrive_and_wait();
        // Mutate both the main renderer and another context while the first
        // owns its encoder. No main frame data or target scratch is borrowed.
        const bool main_ok = fixture.renderer->renderSpotShadow(
            *other_target, 0, other_scene, &fixture.error);
        const bool second_ok = second->encode(*other_target, 1, other_scene, &fixture.error);
        worker.join();
        INFO(worker_error); REQUIRE(worker_ok); REQUIRE(main_ok); REQUIRE(second_ok);
        CHECK(first->active()); CHECK(second->active());
        CHECK_FALSE(first->slotAvailable());
        auto completion = first->completion(); REQUIRE(completion);
        CHECK_FALSE(completion->isComplete()); // Encoded, still unsubmitted.
        REQUIRE(first->commit(&fixture.error)); REQUIRE(second->commit(&fixture.error));
        CHECK(fixture.read(*target, 1) == expected);
        CHECK(fixture.read(*other_target, 1) == fixture.read(*other_target, 0));
        CHECK(completion->isComplete());
        CHECK(first->frameDataStats().published_bytes > 0);
        CHECK(second->frameDataStats().published_bytes > 0);
        CHECK(first->shadowIndirectStats().source_records == frame - 1);
    }
}

TEST_CASE("almetal: shadow context failure and cancellation cannot submit a prefix",
          "[almetal][shadow-encoding-context]")
{
    Fixture fixture;
    auto context = fixture.renderer->createShadowEncodingContext(&fixture.error);
    REQUIRE(context);
    auto target = fixture.target(32, 32);
    auto baseline_batch = fixture.batch();
    auto baseline = fixture.scene(baseline_batch, 32, 32);
    REQUIRE(fixture.renderer->renderSpotShadow(*target, 0, baseline, &fixture.error));
    const auto expected = fixture.read(*target, 0);
    auto changed_batch = fixture.batch(.5f);
    auto changed = fixture.scene(changed_batch, 32, 32);
    CHECK_FALSE(context->begin(1, {}, "Missing outer owner", &fixture.error));
    std::uint64_t frame = 2;
    for (unsigned failure = 0; failure < 8; ++failure)
    {
        CAPTURE(failure);
        auto owner = std::make_shared<const std::uint64_t>(frame);
        std::weak_ptr<const std::uint64_t> weak_owner = owner;
        REQUIRE(context->begin(frame++, owner, "Abandoned shadow prefix", &fixture.error));
        owner.reset(); CHECK_FALSE(weak_owner.expired());
        auto receipt = context->completion(); REQUIRE(receipt);
        const auto committed = ALMetalDevice::committedCommandBufferCount();
        if (failure == 0)
        {
            REQUIRE(context->encode(*target, 0, changed, &fixture.error));
            context->cancel();
        }
        else if (failure == 1)
        {
            REQUIRE(context->encode(*target, 0, changed, &fixture.error));
            // A second encode is rejected and poisons the successful prefix;
            // this context cannot accumulate multiple views in one command.
            auto invalid = changed; invalid.width = 0;
            CHECK_FALSE(context->encode(*target, 0, invalid, &fixture.error));
            CHECK_FALSE(context->commit(&fixture.error));
        }
        else
        {
            auto invalid = changed;
            ALMetalShadowBounds bounds;
            switch (failure)
            {
                case 2: invalid.width = 0; break; // Actual encoder preflight.
                case 3: invalid.visibility_query_count = 1; break;
                case 4: invalid.visibility_bounds = &bounds; invalid.visibility_bounds_count = 1; break;
                case 5: invalid.gpu_visibility = true; break;
                case 6: invalid.metal4_visibility = true; break;
                case 7: invalid.source_owner.reset(); break;
            }
            CHECK_FALSE(context->encode(*target, 0, invalid, &fixture.error));
            // A failed first attempt cannot be repaired into a submitted
            // transaction by a subsequent valid call.
            CHECK_FALSE(context->encode(*target, 0, changed, &fixture.error));
            CHECK_FALSE(context->commit(&fixture.error));
        }
        CHECK_FALSE(context->active()); CHECK(receipt->isComplete());
        CHECK(weak_owner.expired());
        CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        CHECK(fixture.read(*target, 0) == expected);
    }
    REQUIRE(context->begin(frame, std::make_shared<const int>(1), "Recovered context", &fixture.error));
    REQUIRE(context->encode(*target, 0, changed, &fixture.error));
    REQUIRE(context->commit(&fixture.error));
    CHECK(fixture.read(*target, 0) != expected);
    // A genuine empty view is still a successful clear, distinct from begin
    // followed by commit without an encode. Cleared producer vectors may keep
    // non-null data() pointers; zero-count optional inputs are never consumed.
    ALMetalWorldVisibilityQuery unused_query;
    ALMetalShadowBounds unused_bounds;
    ALMetalWorldSpotShadowScene empty;
    empty.width = empty.height = 32;
    empty.visibility_queries = &unused_query;
    empty.visibility_bounds = &unused_bounds;
    REQUIRE(context->begin(++frame, std::make_shared<const int>(1), "Empty captured view", &fixture.error));
    REQUIRE(context->encode(*target, 1, empty, &fixture.error));
    REQUIRE(context->commit(&fixture.error));
    const auto clear = fixture.read(*target, 1);
    CHECK(std::all_of(clear.begin(), clear.end(), [](float value) { return value == 1.f; }));
}

TEST_CASE("almetal: shadow context retains borrowed source and palette owners until cancellation",
          "[almetal][shadow-encoding-context]")
{
    Fixture fixture;
    auto context = fixture.renderer->createShadowEncodingContext(&fixture.error);
    REQUIRE(context);
    auto target = fixture.target(32, 32);
    std::array<float, 16> values{};
    values[4] = values[9] = values[14] = 1.f;
    values[15] = .25f;
    auto palette = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &fixture.error);
    REQUIRE(palette);
    auto batch = fixture.batch(); batch.weight_offset = Fixture::weight_offset;
    batch.skin_palette = palette->data(); batch.skin_palette_float_count = palette->size();
    batch.skin_joint_count = palette->jointCount(); batch.skin_palette_record = palette;
    auto source = ALMetalShadowSourceRecord::create(1, batch, fixture.vertices,
                                                   fixture.indices, {}, &fixture.error);
    INFO(fixture.error); REQUIRE(source);
    struct Owner
    {
        std::shared_ptr<const ALMetalShadowSourceRecord> source;
        std::shared_ptr<const ALMetalAttachmentPaletteRecord> palette;
    };
    auto owner = std::make_shared<const Owner>(Owner{source, palette});
    std::weak_ptr<const Owner> weak_owner = owner;
    std::weak_ptr<const ALMetalShadowSourceRecord> weak_source = source;
    std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak_palette = palette;
    batch.skin_palette_record.reset();
    batch.borrowPreparedInputs(source.get(), palette.get(), nullptr);
    auto scene = fixture.scene(batch, 32, 32); scene.source_owner = owner;
    REQUIRE(fixture.renderer->renderSpotShadow(*target, 0, scene, &fixture.error));
    const auto expected = fixture.read(*target, 0);
    // Clear the main arena's retained pose identity before the lifetime check.
    fixture.main_data->beginFrame(2);
    REQUIRE(context->begin(1, std::make_shared<const int>(1), "Borrowed pose context", &fixture.error));
    REQUIRE(context->encode(*target, 1, scene, &fixture.error));
    CHECK(context->frameDataStats().record_publications == 1);
    CHECK(fixture.main_data->stats().record_publications == 1);
    source.reset(); palette.reset(); owner.reset(); scene.source_owner.reset();
    CHECK_FALSE(weak_owner.expired()); CHECK_FALSE(weak_source.expired()); CHECK_FALSE(weak_palette.expired());
    context->cancel();
    CHECK(weak_owner.expired()); CHECK(weak_source.expired());
    // The private arena still owns its frame-local palette identity. Advancing
    // this context's own generation retires that identity without touching main.
    REQUIRE(context->begin(2, std::make_shared<const int>(2), "Retire pose identity", &fixture.error));
    context->cancel();
    CHECK(weak_palette.expired());
    CHECK(fixture.read(*target, 0) == expected);
    // Canceled slice 1 is deliberately never read: it was never initialized.
}

TEST_CASE("almetal: shadow contexts retain their executable generation after renderer reload and destruction",
          "[almetal][shadow-encoding-context]")
{
    Fixture fixture;
    auto old_context = fixture.renderer->createShadowEncodingContext(&fixture.error);
    REQUIRE(old_context);
    const auto old_generation = old_context->shaderGeneration();
    const auto old_anisotropy = old_context->anisotropy();
    REQUIRE(fixture.renderer->setAnisotropy(4, &fixture.error));
    CHECK(old_context->anisotropy() == old_anisotropy);
    REQUIRE(fixture.renderer->reloadShaders(&fixture.error));
    CHECK(fixture.renderer->shaderGeneration() == old_generation + 1);
    auto new_context = fixture.renderer->createShadowEncodingContext(&fixture.error);
    REQUIRE(new_context);
    CHECK(new_context->anisotropy() == 4);
    CHECK(new_context->shaderGeneration() == old_generation + 1);
    CHECK(old_context->shaderGeneration() == old_generation);
    auto target = fixture.target(32, 32);
    auto batch = fixture.batch(); auto scene = fixture.scene(batch, 32, 32);
    fixture.renderer.reset(); fixture.main_data.reset();
    REQUIRE(old_context->begin(1, std::make_shared<const int>(1), "Old executable context", &fixture.error));
    REQUIRE(new_context->begin(1, std::make_shared<const int>(1), "New executable context", &fixture.error));
    REQUIRE(old_context->encode(*target, 0, scene, &fixture.error));
    REQUIRE(new_context->encode(*target, 1, scene, &fixture.error));
    REQUIRE(old_context->commit(&fixture.error)); REQUIRE(new_context->commit(&fixture.error));
    const auto old_pixels = fixture.read(*target, 0);
    CHECK(old_pixels == fixture.read(*target, 1));
    CHECK(std::abs(old_pixels[16 * 32 + 16] - .5f) < 1e-6f);
}
