/**
 * @file almetal_shadow_capture_test.mm
 * @brief Early shadow coalescing keeps source validation and raster order.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalshadowcapture.h"
#include "../almetalshadowindirect.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct Fixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device;
        std::unique_ptr<ALMetalBuffer> vertices;
        std::unique_ptr<ALMetalBuffer> indices;
        std::array<float, 16> matrix{};

        explicit Fixture(bool wide = false)
        {
            device = ALMetalDevice::create(&error);
            INFO(error);
            REQUIRE(device);
            const std::array<std::array<float, 4>, 6> positions{{
                {{-0.75f, -0.75f, 0.f, 1.f}},
                {{ 0.75f, -0.75f, 0.f, 1.f}},
                {{ 0.f,    0.75f, 0.f, 1.f}},
                {{-0.5f, -0.5f, 0.f, 1.f}},
                {{ 0.5f, -0.5f, 0.f, 1.f}},
                {{ 0.f,   0.5f, 0.f, 1.f}},
            }};
            ALMetalBufferDescriptor descriptor;
            descriptor.length = sizeof(positions);
            vertices = ALMetalBuffer::create(*device, descriptor, &error);
            REQUIRE(vertices);
            REQUIRE(vertices->write(0, positions.data(), sizeof(positions), &error));
            std::array<std::uint32_t, 128 * 3 + 6> values{};
            for (std::size_t index = 0; index < values.size(); ++index)
                values[index] = index % 3;
            descriptor.length = values.size() * (wide ? 4 : 2);
            indices = ALMetalBuffer::create(*device, descriptor, &error);
            REQUIRE(indices);
            if (wide)
                REQUIRE(indices->write(0, values.data(), descriptor.length, &error));
            else
            {
                std::array<std::uint16_t, values.size()> narrow{};
                std::copy(values.begin(), values.end(), narrow.begin());
                REQUIRE(indices->write(0, narrow.data(), sizeof(narrow), &error));
            }
            for (std::size_t axis = 0; axis < 4; ++axis)
                matrix[axis * 4 + axis] = 1.f;
        }

        ALMetalOpaqueShadowSource source(std::size_t draw = 0, bool wide = false) const
        {
            ALMetalOpaqueShadowSource value;
            value.vertices = vertices.get();
            value.indices = indices.get();
            value.modelview_projection = matrix.data();
            value.vertex_count = 3;
            value.index_count = 3;
            value.index_offset = draw * 3 * (wide ? 4 : 2);
            value.index_type = wide ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
            return value;
        }
    };
}

TEST_CASE("almetal: early shadow admission reconciles sources independently of records",
          "[almetal][shadow-capture]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowCaptureAdmission admission;
        std::vector<ALMetalWorldSpotShadowBatch> records;
        for (std::size_t draw = 0; draw < 128; ++draw)
            REQUIRE(admission.appendOpaque(records, fixture.source(draw)));
        CHECK(admission.sourceCount() == 128);
        CHECK(admission.mergedSourceCount() == 127);
        REQUIRE(records.size() == 1);
        CHECK(records[0].index_count == 128 * 3);
        CHECK(admission.complete(128));
        CHECK_FALSE(admission.complete(127));
        CHECK_FALSE(admission.complete(129));

        SECTION("late invalid range cannot hide behind a compatible packet")
        {
            auto invalid = fixture.source(128);
            invalid.index_count = 99;
            CHECK_FALSE(admission.appendOpaque(records, invalid));
        }
        SECTION("late nonfinite matrix poisons even an otherwise complete transaction")
        {
            fixture.matrix[12] = std::numeric_limits<float>::quiet_NaN();
            CHECK_FALSE(admission.appendOpaque(records, fixture.source(128)));
        }
        SECTION("invalid geometry and index types are rejected before allocation")
        {
            auto invalid = fixture.source(128);
            invalid.index_type = static_cast<ALMetalIndexType>(99);
            CHECK_FALSE(admission.appendOpaque(records, invalid));
        }
        CHECK(admission.sourceCount() == 128);
        CHECK(records.size() == 1);
        CHECK(records[0].index_count == 128 * 3);
        CHECK_FALSE(admission.complete(128));
        admission.reset();
        CHECK(admission.complete(0));
    }
}

TEST_CASE("almetal: early shadow admission preserves heterogeneous boundaries",
          "[almetal][shadow-capture]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (unsigned kind = 0; kind < 9; ++kind)
        {
            CAPTURE(kind);
            ALMetalShadowCaptureAdmission admission;
            std::vector<ALMetalWorldSpotShadowBatch> records;
            REQUIRE(admission.appendOpaque(records, fixture.source()));
            auto next = fixture.source(1);
            auto matrix = fixture.matrix;
            switch (kind)
            {
                case 0: admission.barrier(); break; // Render-family boundary.
                case 1: next.double_sided = true; break;
                case 2: next.reverse_z = true; break;
                case 3: matrix[12] = 0.125f; next.modelview_projection = matrix.data(); break;
                case 4: next.position_offset = 16; break;
                case 5: next.vertex_count = 4; break;
                case 6: next.index_offset += 2; break;
                case 7: next.index_count = 2; break;
                case 8:
                    REQUIRE(admission.admitSeparate());
                    records.emplace_back().terrain = true;
                    break;
            }
            REQUIRE(admission.appendOpaque(records, next));
            CHECK(records.size() == (kind == 8 ? 3 : 2));
            CHECK(admission.sourceCount() == (kind == 8 ? 3 : 2));
            CHECK(admission.mergedSourceCount() == 0);
            CHECK(admission.complete(admission.sourceCount()));
        }
        // Covered, rigged and avatar producers have the same separate-source
        // admission contract as terrain; none may become an opaque tail.
        for (unsigned family = 0; family < 3; ++family)
        {
            ALMetalShadowCaptureAdmission admission;
            std::vector<ALMetalWorldSpotShadowBatch> records;
            REQUIRE(admission.appendOpaque(records, fixture.source()));
            REQUIRE(admission.admitSeparate());
            auto& separate = records.emplace_back();
            if (family == 0) separate.coverage = ALMetalWorldSpotShadowCoverage::texture_mask;
            if (family == 1) separate.skin_palette = fixture.matrix.data();
            if (family == 2) separate.avatar = true;
            REQUIRE(admission.appendOpaque(records, fixture.source(1)));
            CHECK(records.size() == 3);
            CHECK(admission.complete(3));
        }
    }
}

TEST_CASE("almetal: early shadow coalescing preserves real GPU raw depth",
          "[almetal][shadow-capture]")
{
    @autoreleasepool
    {
        for (const bool wide : {false, true})
        {
            Fixture fixture(wide);
            auto renderer = ALMetalWorldGBufferRenderer::create(*fixture.device, &fixture.error);
            REQUIRE(renderer);
            ALMetalTextureDescriptor descriptor;
            descriptor.type = ALMetalTextureType::texture_2d_array;
            descriptor.format = ALMetalPixelFormat::r32_float;
            descriptor.width = 32;
            descriptor.height = 32;
            descriptor.array_length = 2;
            descriptor.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
            auto target = ALMetalTexture::create(*fixture.device, descriptor, &fixture.error);
            REQUIRE(target);
            for (const bool reverse_z : {false, true})
            {
                for (const bool clamp : {false, true})
                {
                    for (unsigned pattern = 0; pattern < 4; ++pattern)
                    {
                        CAPTURE(wide, reverse_z, clamp, pattern);
                        fixture.matrix[14] = clamp ? (reverse_z ? 2.f : -2.f)
                                                   : (reverse_z ? 0.5f : 0.f);
                        ALMetalShadowCaptureAdmission scalar_admission, merged_admission;
                        std::vector<ALMetalWorldSpotShadowBatch> scalar, merged;
                        for (std::size_t draw = 0; draw < 128; ++draw)
                        {
                            auto candidate = fixture.source(pattern == 1 ? 0 : draw, wide);
                            auto matrix = fixture.matrix;
                            // Repeated offsets, cull changes, and changing
                            // placements are real noncoalescible workloads.
                            if (pattern == 2) candidate.double_sided = (draw & 1u) != 0;
                            if (pattern == 3)
                            {
                                matrix[12] = (draw & 1u) != 0 ? 0.125f : 0.f;
                                candidate.modelview_projection = matrix.data();
                            }
                            candidate.reverse_z = reverse_z;
                            scalar_admission.barrier();
                            REQUIRE(scalar_admission.appendOpaque(scalar, candidate));
                            REQUIRE(merged_admission.appendOpaque(merged, candidate));
                        }
                        REQUIRE(scalar.size() == 128);
                        CHECK(merged.size() == (pattern == 0 ? 1 : 128));
                        REQUIRE(merged_admission.complete(128));
                        ALMetalWorldSpotShadowScene scene;
                        scene.width = 32;
                        scene.height = 32;
                        scene.reverse_z = reverse_z;
                        scene.depth_clamp = clamp;
                        scene.bulk_draws = false;
                        scene.indirect_draws = false;
                        scene.batches = scalar.data();
                        scene.batch_count = scalar.size();
                        scene.source_batch_count = scalar_admission.sourceCount();
                        const auto before_scalar = renderer->shadowIndirectStats();
                        REQUIRE(renderer->renderSpotShadow(*target, 0, scene, &fixture.error));
                        const auto after_scalar = renderer->shadowIndirectStats();
                        CHECK(after_scalar.source_records - before_scalar.source_records == 128);
                        CHECK(after_scalar.encoded_records - before_scalar.encoded_records == 128);
                        CHECK(after_scalar.early_coalesced_records == before_scalar.early_coalesced_records);
                        scene.batches = merged.data();
                        scene.batch_count = merged.size();
                        scene.source_batch_count = merged_admission.sourceCount();
                        scene.early_merged_source_count = merged_admission.mergedSourceCount();
                        REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
                        const auto after_merged = renderer->shadowIndirectStats();
                        CHECK(after_merged.source_records - after_scalar.source_records == 128);
                        CHECK(after_merged.encoded_records - after_scalar.encoded_records == merged.size());
                        CHECK(after_merged.early_coalesced_records - after_scalar.early_coalesced_records ==
                              merged_admission.mergedSourceCount());
                        INFO(fixture.error);
                        std::array<float, 32 * 32> original{}, coalesced{};
                        REQUIRE(target->readback(*fixture.device, 0, 0, {}, original.data(),
                                                 32 * sizeof(float), &fixture.error));
                        REQUIRE(target->readback(*fixture.device, 0, 1, {}, coalesced.data(),
                                                 32 * sizeof(float), &fixture.error));
                        CHECK(original == coalesced);
                        CHECK(std::abs(original[16 * 32 + 16] -
                            (clamp ? (reverse_z ? 1.f : 0.f) : 0.5f)) < 1e-6f);
                        ++scene.early_merged_source_count;
                        const auto before_invalid_census = ALMetalDevice::committedCommandBufferCount();
                        CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
                        CHECK(ALMetalDevice::committedCommandBufferCount() == before_invalid_census);
                        CHECK(renderer->shadowIndirectStats().source_records == after_merged.source_records);
                    }
                }
            }

            // Separate-family records keep their ordinary backend validation.
            // A logically reconciled list with an invalid later source must
            // fail before any native command is committed, even after the
            // entire valid prefix collapsed to one physical record.
            ALMetalShadowCaptureAdmission prefix_admission;
            std::vector<ALMetalWorldSpotShadowBatch> prefix;
            for (std::size_t draw = 0; draw < 128; ++draw)
                REQUIRE(prefix_admission.appendOpaque(prefix, fixture.source(draw, wide)));
            REQUIRE(prefix.size() == 1);
            for (unsigned family = 0; family < 5; ++family)
            {
                CAPTURE(wide, family);
                auto admission = prefix_admission;
                auto records = prefix;
                REQUIRE(admission.admitSeparate());
                auto& invalid = records.emplace_back(prefix.front());
                if (family == 0) invalid.vertices = nullptr;
                if (family == 1) invalid.coverage = ALMetalWorldSpotShadowCoverage::texture_mask;
                if (family == 2)
                {
                    invalid.skin_palette = fixture.matrix.data();
                    invalid.skin_palette_float_count = 15;
                    invalid.skin_joint_count = 1;
                }
                if (family == 3)
                {
                    invalid.avatar = true;
                    invalid.avatar_draw_alpha = 2.f;
                }
                if (family == 4) invalid.terrain = true;
                REQUIRE(admission.complete(129));
                ALMetalWorldSpotShadowScene scene;
                scene.width = scene.height = 32;
                scene.batches = records.data();
                scene.batch_count = records.size();
                scene.source_batch_count = admission.sourceCount();
                scene.early_merged_source_count = admission.mergedSourceCount();
                const auto before = ALMetalDevice::committedCommandBufferCount();
                CHECK_FALSE(renderer->renderSpotShadow(*target, 0, scene, &fixture.error));
                CHECK(ALMetalDevice::committedCommandBufferCount() == before);
            }
        }
    }
}
