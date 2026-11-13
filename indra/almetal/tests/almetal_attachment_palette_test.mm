/**
 * @file almetal_attachment_palette_test.mm
 * @brief Immutable attachment-pose identity, command lifetime and shadow parity.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalworld.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
std::vector<float> attachmentValues(std::uint32_t joints, float origin = 0.f)
{
    std::vector<float> values(4 + joints * 12, 0.f);
    values[0] = origin;
    for (std::uint32_t joint = 0; joint < joints; ++joint)
    {
        values[4 + joint * 12] = 1.f;
        values[4 + joint * 12 + 5] = 1.f;
        values[4 + joint * 12 + 10] = 1.f;
    }
    return values;
}
}

TEST_CASE("almetal: attachment records validate and own one immutable pose",
          "[almetal][attachment-palette]")
{
    std::string error;
    auto values = attachmentValues(2, 0.25f);
    auto record = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
    INFO(error);
    REQUIRE(record);
    CHECK(record->data() != values.data());
    CHECK(record->size() == values.size());
    CHECK(record->jointCount() == 2);
    CHECK(std::equal(values.begin(), values.end(), record->data()));
    values[0] = -0.5f;
    CHECK(record->data()[0] == 0.25f);
    auto following = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
    REQUIRE(following);
    CHECK(following != record);
    CHECK(following->data()[0] == -0.5f);
    CHECK_FALSE(ALMetalAttachmentPaletteRecord::create(nullptr, values.size(), 2, &error));
    CHECK_FALSE(ALMetalAttachmentPaletteRecord::create(values.data(), values.size() - 1, 2, &error));
    CHECK_FALSE(ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 0, &error));
    auto largest = attachmentValues(110);
    CHECK(ALMetalAttachmentPaletteRecord::create(largest.data(), largest.size(), 110, &error));
    largest = attachmentValues(111);
    CHECK_FALSE(ALMetalAttachmentPaletteRecord::create(largest.data(), largest.size(), 111, &error));
    for (const auto index : {std::size_t{0}, values.size() - 1})
    {
        const float old = values[index];
        for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                   std::numeric_limits<float>::infinity(),
                                   -std::numeric_limits<float>::infinity()})
        {
            values[index] = invalid;
            CHECK_FALSE(ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error));
        }
        values[index] = old;
    }

    ALMetalWorldSpotShadowBatch batch;
    batch.skin_palette_record = record;
    batch.skin_palette = record->data();
    batch.skin_palette_float_count = record->size();
    batch.skin_joint_count = record->jointCount();
    ALMetalWorldSpotShadowBatch copied(batch);
    ALMetalWorldSpotShadowBatch moved(std::move(copied));
    ALMetalWorldSpotShadowBatch assigned;
    assigned = moved;
    batch = ALMetalWorldSpotShadowBatch{};
    CHECK_FALSE(batch.skin_palette_record);
    record.reset();
    CHECK(assigned.skin_palette == assigned.skin_palette_record->data());
    CHECK(assigned.skin_palette[0] == 0.25f);
}

TEST_CASE("almetal: palette identity binds every command and retires across frames",
          "[almetal][attachment-palette]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        auto values = attachmentValues(2, 0.125f);
        auto record = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
        REQUIRE(record);
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> identity = record;
        arena->beginFrame(7);
        auto first = device->makeCommandBuffer("Attachment palette first reader", &error); REQUIRE(first);
        auto second = device->makeCommandBuffer("Attachment palette second reader", &error); REQUIRE(second);
        ALMetalWorldDataSlice original, repeated;
        REQUIRE(arena->publishPalette(*first, record, original, &error));
        REQUIRE(arena->publishPalette(*second, record, repeated, &error));
        CHECK(repeated.buffer == original.buffer);
        CHECK(repeated.offset == original.offset);
        CHECK(arena->stats().record_publications == 1);
        CHECK(arena->stats().record_reuses == 1);
        CHECK(arena->stats().published_bytes == values.size() * sizeof(float));
        CHECK(arena->stats().reused_bytes == values.size() * sizeof(float));
        ALMetalBufferDescriptor descriptor;
        descriptor.length = values.size() * sizeof(float);
        auto output = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(output);
        REQUIRE(output->copyFrom(*second, *repeated.buffer, repeated.offset, 0, repeated.length, &error));
        REQUIRE(first->commitAndWait(&error));
        ALMetalWorldDataSlice rejected = original;
        CHECK_FALSE(arena->publishPalette(*first, record, rejected, &error));
        CHECK(rejected.buffer == nullptr);
        CHECK_FALSE(arena->publishPalette(*second, {}, rejected, &error));
        CHECK(rejected.buffer == nullptr);
        CHECK(arena->stats().record_reuses == 1);
        // The second reader is uncommitted. A hit must have registered it;
        // completing only the first reader cannot release the shared page.
        arena->beginFrame(8);
        auto third = device->makeCommandBuffer("Attachment palette next frame", &error); REQUIRE(third);
        ALMetalWorldDataSlice next;
        REQUIRE(arena->publishPalette(*third, record, next, &error));
        CHECK(next.buffer != original.buffer);
        CHECK(arena->stats().retired_pages == 1);
        CHECK(arena->stats().record_publications == 2);
        REQUIRE(second->commitAndWait(&error));
        std::vector<float> read(values.size());
        REQUIRE(output->read(0, read.data(), descriptor.length, &error));
        CHECK(read == values);

        record.reset();
        CHECK_FALSE(identity.expired()); // Current frame cache retains identity.
        third.reset(); // Cancellation completes the pending page lease.
        arena->beginFrame(9);
        CHECK(identity.expired());
        CHECK(arena->stats().retired_pages == 0);
        auto fourth = device->makeCommandBuffer("Attachment palette recycled page", &error); REQUIRE(fourth);
        values[0] = -0.25f;
        record = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
        REQUIRE(arena->publishPalette(*fourth, record, next, &error));
        CHECK(arena->stats().pages_created == 2);
        CHECK(arena->stats().pages_reused == 1);
        REQUIRE(output->copyFrom(*fourth, *next.buffer, next.offset, 0, next.length, &error));
        // Encoded native buffers outlive the arena and CPU record/cache owner.
        arena.reset(); record.reset();
        REQUIRE(fourth->commitAndWait(&error));
        REQUIRE(output->read(0, read.data(), descriptor.length, &error));
        CHECK(read == values);

        // Equal bytes do not merge distinct certified identities. Abandoning
        // the first reader must not free a page still named by a cache hit.
        arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        arena->beginFrame(1);
        auto a = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
        auto b = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 2, &error);
        REQUIRE(a); REQUIRE(b);
        first = device->makeCommandBuffer("Attachment palette abandoned first reader", &error); REQUIRE(first);
        second = device->makeCommandBuffer("Attachment palette retained second reader", &error); REQUIRE(second);
        REQUIRE(arena->publishPalette(*first, a, original, &error));
        REQUIRE(arena->publishPalette(*second, a, repeated, &error));
        REQUIRE(arena->publishPalette(*second, b, next, &error));
        CHECK(next.buffer == original.buffer);
        CHECK(next.offset != original.offset);
        CHECK(arena->stats().record_publications == 2);
        CHECK(arena->stats().record_reuses == 1);
        first.reset();
        arena->beginFrame(2);
        third = device->makeCommandBuffer("Attachment palette after one abandonment", &error); REQUIRE(third);
        REQUIRE(arena->publishPalette(*third, a, next, &error));
        CHECK(next.buffer != original.buffer);
        CHECK(arena->stats().retired_pages == 1);
        second.reset(); third.reset();
        arena->beginFrame(3);
        CHECK(arena->stats().retired_pages == 0);
    }
}

TEST_CASE("almetal: immutable attachment records preserve native shadow output and admission",
          "[almetal][attachment-palette]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error); INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        constexpr std::size_t width = 32, height = 32;
        const std::array<std::array<float, 4>, 3> positions{{
            {{-0.75f, -0.75f, 0.f, 1.f}}, {{0.75f, -0.75f, 0.f, 1.f}}, {{0.f, 0.75f, 0.f, 1.f}}}};
        const std::array<std::array<float, 4>, 3> weights{{
            {{0.5f, 0.f, 0.f, 0.f}}, {{0.5f, 0.f, 0.f, 0.f}}, {{0.5f, 0.f, 0.f, 0.f}}}};
        const std::array<std::array<float, 2>, 3> uv{{{{0.25f, 0.5f}}, {{0.25f, 0.5f}}, {{0.25f, 0.5f}}}};
        const std::array<std::uint8_t, 12> white{{255,255,255,255,255,255,255,255,255,255,255,255}};
        constexpr std::size_t weight_offset = sizeof(positions);
        constexpr std::size_t uv_offset = weight_offset + sizeof(weights);
        constexpr std::size_t color_offset = uv_offset + sizeof(uv);
        std::array<std::uint8_t, color_offset + sizeof(white)> bytes{};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data() + weight_offset, weights.data(), sizeof(weights));
        std::memcpy(bytes.data() + uv_offset, uv.data(), sizeof(uv));
        std::memcpy(bytes.data() + color_offset, white.data(), sizeof(white));
        ALMetalBufferDescriptor buffer_descriptor; buffer_descriptor.length = bytes.size();
        auto vertices = ALMetalBuffer::create(*device, buffer_descriptor, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, bytes.data(), bytes.size(), &error));
        const std::array<std::uint16_t, 3> indices{{0,1,2}};
        buffer_descriptor.length = sizeof(indices);
        auto elements = ALMetalBuffer::create(*device, buffer_descriptor, &error); REQUIRE(elements);
        REQUIRE(elements->write(0, indices.data(), sizeof(indices), &error));
        ALMetalTextureDescriptor target_descriptor;
        target_descriptor.type = ALMetalTextureType::texture_2d_array;
        target_descriptor.format = ALMetalPixelFormat::r32_float;
        target_descriptor.width = width; target_descriptor.height = height; target_descriptor.array_length = 2;
        target_descriptor.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        auto target = ALMetalTexture::create(*device, target_descriptor, &error); REQUIRE(target);
        ALMetalTextureDescriptor tex_descriptor; tex_descriptor.width = 1; tex_descriptor.height = 1;
        tex_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        tex_descriptor.usage = ALMetalTextureUsage::shader_read;
        auto texture = ALMetalTexture::create(*device, tex_descriptor, &error); REQUIRE(texture);
        auto upload = device->makeCommandBuffer("Attachment palette coverage texture", &error); REQUIRE(upload);
        REQUIRE(texture->upload(*upload, 0, 0, {}, white.data(), 4, &error));
        auto hidden = ALMetalTexture::create(*device, tex_descriptor, &error); REQUIRE(hidden);
        const std::array<std::uint8_t, 4> transparent{{255,255,255,0}};
        REQUIRE(hidden->upload(*upload, 0, 0, {}, transparent.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        std::uint64_t frame = 1;
        for (const bool with_arena : {false, true})
        for (const std::uint32_t joints : {1u, 110u})
        for (const auto coverage : {ALMetalWorldSpotShadowCoverage::opaque,
                ALMetalWorldSpotShadowCoverage::legacy_mask,
                ALMetalWorldSpotShadowCoverage::texture_mask,
                ALMetalWorldSpotShadowCoverage::pbr_mask})
        for (const bool reverse_z : {false, true})
        {
            CAPTURE(with_arena, joints, coverage, reverse_z);
            renderer->setFrameData(with_arena ? arena : nullptr);
            arena->beginFrame(frame++);
            auto values = attachmentValues(joints, 0.125f);
            auto record = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), joints, &error);
            REQUIRE(record);
            ALMetalWorldSpotShadowBatch batch;
            batch.vertices = vertices.get(); batch.indices = elements.get();
            batch.vertex_count = 3; batch.index_count = 3; batch.weight_offset = weight_offset;
            batch.texcoord_offset = uv_offset; batch.color_offset = color_offset;
            batch.skin_palette = values.data(); batch.skin_palette_float_count = values.size();
            batch.skin_joint_count = joints; batch.coverage = coverage;
            for (std::size_t i = 0; i < 4; ++i) batch.modelview_projection[i*4+i] = 1.f;
            batch.modelview_projection[14] = reverse_z ? 0.5f : 0.f;
            if (coverage != ALMetalWorldSpotShadowCoverage::opaque)
            {
                batch.texture_count = 1; batch.coverageData().textures[0] = texture.get();
                batch.coverageData().alpha_cutoffs[0] = 0.5f;
            }
            ALMetalWorldSpotShadowScene scene;
            scene.batches = &batch; scene.batch_count = 1; scene.width = width; scene.height = height;
            scene.reverse_z = reverse_z;
            REQUIRE(renderer->renderSpotShadow(*target, 0, scene, &error));
            batch.skin_palette_record = record;
            batch.skin_palette = record->data();
            REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &error));
            std::array<float, width*height> scalar{}, certified{};
            REQUIRE(target->readback(*device, 0, 0, {}, scalar.data(), width*sizeof(float), &error));
            REQUIRE(target->readback(*device, 0, 1, {}, certified.data(), width*sizeof(float), &error));
            CHECK(scalar == certified);
            CHECK(std::abs(certified[height/2*width + width/2] - 0.5f) < 1e-6f);

            // Same producer address, new immutable pose: the shader must read
            // its changed Z translation instead of a previous identity's slice.
            values[4 + 11] = 0.25f;
            auto changed = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), joints, &error);
            REQUIRE(changed);
            batch.skin_palette_record = changed; batch.skin_palette = changed->data();
            REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &error));
            std::array<float, width*height> changed_pixels{};
            REQUIRE(target->readback(*device, 0, 1, {}, changed_pixels.data(), width*sizeof(float), &error));
            CHECK(changed_pixels != certified);
            CHECK(std::abs(changed_pixels[height/2*width + width/2] - 0.5f) > 0.05f);
            batch.skin_palette_record = record; batch.skin_palette = record->data();
            if (coverage != ALMetalWorldSpotShadowCoverage::opaque)
            {
                batch.coverageData().textures[0] = hidden.get();
                REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &error));
                REQUIRE(target->readback(*device, 0, 1, {}, changed_pixels.data(), width*sizeof(float), &error));
                const float clear = reverse_z ? 0.f : 1.f;
                CHECK(std::all_of(changed_pixels.begin(), changed_pixels.end(),
                                  [clear](float value) { return value == clear; }));
                batch.coverageData().textures[0] = texture.get();
            }

            const auto invalid = [&](auto change)
            {
                auto wrong = batch; change(wrong);
                scene.batches = &wrong;
                CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &error));
                CHECK(error.find("record metadata") != std::string::npos);
                scene.batches = &batch;
            };
            invalid([](auto& b) { b.skin_palette = nullptr; });
            invalid([&](auto& b) { b.skin_palette = values.data(); });
            invalid([](auto& b) { --b.skin_palette_float_count; });
            invalid([](auto& b) { ++b.skin_joint_count; });
            invalid([](auto& b) { b.avatar = true; });
            invalid([](auto& b) { b.terrain = true; });
            // Certification never admits invalid geometry or raw body inputs.
            auto wrong = batch; wrong.weight_offset = bytes.size(); scene.batches = &wrong;
            CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &error));
            wrong = batch; wrong.skin_palette_record.reset(); wrong.skin_palette = values.data();
            values.back() = std::numeric_limits<float>::quiet_NaN();
            CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &error));
        }
    }
}
