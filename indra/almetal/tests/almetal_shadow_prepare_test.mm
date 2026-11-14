/**
 * @file almetal_shadow_prepare_test.mm
 * @brief Immutable source ownership, ordered preparation and complete CPU cost.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowprepare.h"
#include "llmath.h"
#include "llmatrix4a.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

namespace
{
using Batch = ALMetalWorldSpotShadowBatch;
using Record = ALMetalShadowSourceRecord;
using Input = ALMetalShadowPrepareInput;
using Clock = std::chrono::steady_clock;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::shared_ptr<const ALMetalBuffer> vertices, indices16, indices32;
    std::shared_ptr<const ALMetalTexture> texture;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
    static constexpr std::size_t weight_offset = 48, uv_offset = 96, color_offset = 120;

    Fixture()
    {
        device = ALMetalDevice::create(&error); REQUIRE(device);
        const std::array<std::array<float, 4>, 3> positions{{
            {{-.75f, -.75f, 0.f, 1.f}}, {{.75f, -.75f, 0.f, 1.f}}, {{0.f, .75f, 0.f, 1.f}}}};
        const std::array<std::array<float, 4>, 3> weights{{
            {{.5f, 0.f, 0.f, 0.f}}, {{.5f, 0.f, 0.f, 0.f}}, {{.5f, 0.f, 0.f, 0.f}}}};
        const std::array<std::array<float, 2>, 3> uv{{{{.25f, .5f}}, {{.25f, .5f}}, {{.25f, .5f}}}};
        const std::array<std::uint8_t, 12> white{{255,255,255,255,255,255,255,255,255,255,255,255}};
        std::array<std::uint8_t, 132> bytes{};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data() + weight_offset, weights.data(), sizeof(weights));
        std::memcpy(bytes.data() + uv_offset, uv.data(), sizeof(uv));
        std::memcpy(bytes.data() + color_offset, white.data(), sizeof(white));
        ALMetalBufferDescriptor descriptor; descriptor.length = bytes.size();
        auto vb = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(vb);
        REQUIRE(vb->write(0, bytes.data(), bytes.size(), &error));
        vertices = std::move(vb);
        const std::array<std::uint16_t, 3> i16{{0,1,2}};
        descriptor.length = sizeof(i16);
        auto ib16 = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(ib16);
        REQUIRE(ib16->write(0, i16.data(), sizeof(i16), &error));
        indices16 = std::move(ib16);
        const std::array<std::uint32_t, 3> i32{{0,1,2}};
        descriptor.length = sizeof(i32);
        auto ib32 = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(ib32);
        REQUIRE(ib32->write(0, i32.data(), sizeof(i32), &error));
        indices32 = std::move(ib32);
        ALMetalTextureDescriptor td; td.width = 1; td.height = 1;
        td.format = ALMetalPixelFormat::rgba8_unorm;
        td.usage = ALMetalTextureUsage::shader_read;
        auto image = ALMetalTexture::create(*device, td, &error); REQUIRE(image);
        auto command = device->makeCommandBuffer("Shadow preparation fixture", &error); REQUIRE(command);
        REQUIRE(image->upload(*command, 0, 0, {}, white.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error));
        texture = std::move(image);
        std::array<float, 16> values{};
        values[4] = values[9] = values[14] = 1.f;
        pose = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &error);
        REQUIRE(pose);
    }

    Batch prototype(ALMetalWorldSpotShadowCoverage coverage, bool rigged = false,
                    bool index32 = false) const
    {
        Batch batch;
        batch.vertices = vertices.get(); batch.indices = (index32 ? indices32 : indices16).get();
        batch.vertex_count = 3; batch.index_count = 3;
        batch.index_type = index32 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.texcoord_offset = uv_offset; batch.color_offset = color_offset;
        batch.coverage = coverage; batch.double_sided = true;
        std::memcpy(batch.modelview_projection, alMetalShadowIdentity.data(), sizeof(batch.modelview_projection));
        if (coverage != ALMetalWorldSpotShadowCoverage::opaque)
        {
            batch.texture_count = 1;
            batch.coverageData().textures[0] = texture.get();
            batch.coverageData().alpha_cutoffs[0] = .5f;
            batch.coverageData().pbr_base_color_transforms[0][0][0] = 1.f;
            batch.coverageData().pbr_base_color_transforms[0][0][1] = 1.f;
        }
        if (rigged)
        {
            batch.weight_offset = weight_offset;
            batch.skin_palette_record = pose;
            batch.skin_palette = pose->data();
            batch.skin_palette_float_count = pose->size(); batch.skin_joint_count = pose->jointCount();
        }
        return batch;
    }

    std::shared_ptr<const Record> source(std::uint64_t generation, const Batch& batch)
    {
        std::vector<std::shared_ptr<const ALMetalTexture>> textures;
        if (batch.texture_count) textures.push_back(texture);
        return Record::create(generation, batch, vertices,
            batch.index_type == ALMetalIndexType::uint32 ? indices32 : indices16,
            std::move(textures), &error);
    }
};

Input inputFor(const std::shared_ptr<const Record>& source, std::uint64_t epoch = 7)
{
    Input input;
    input.source = source; input.source_generation = source->generation(); input.epoch = epoch;
    input.has_model = true; input.model[12] = .125f;
    input.view[0] = .75f; input.projection[5] = .5f;
    return input;
}
}

TEST_CASE("almetal: shadow sources seal coverage and retain exact native generations",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto prototype = fixture.prototype(ALMetalWorldSpotShadowCoverage::pbr_mask, true);
        auto& producer_reference = prototype.coverageData();
        auto source = fixture.source(1, prototype); INFO(fixture.error); REQUIRE(source);
        CHECK(source->requiresPalette());
        CHECK_FALSE(source->prototype().skin_palette);
        CHECK_FALSE(source->prototype().skinPaletteRecord());
        CHECK_FALSE(source->prototype().sourceRecord());
        REQUIRE(source->prototype().sharedCoverageData());
        producer_reference.alpha_cutoffs[0] = .875f;
        CHECK(source->prototype().coverageData().alpha_cutoffs[0] == .5f);
        auto input = inputFor(source); input.palette = fixture.pose;
        auto packet = std::make_shared<std::vector<Input>>(1, input);
        std::array<Batch, 1> output;
        const auto source_owners = source.use_count();
        const auto pose_owners = fixture.pose.use_count();
        const auto coverage_owners = source->prototype().sharedCoverageData().use_count();
        REQUIRE(alMetalPrepareShadowBatches(*packet, output, 7));
        CHECK(source.use_count() == source_owners);
        CHECK(fixture.pose.use_count() == pose_owners);
        CHECK(source->prototype().sharedCoverageData().use_count() == coverage_owners);
        CHECK(output[0].hasBorrowedInputs());
        CHECK_FALSE(output[0].source_record);
        CHECK_FALSE(output[0].skin_palette_record);
        CHECK_FALSE(output[0].sharedCoverageData());
        CHECK(output[0].sourceRecord() == source.get());
        CHECK(output[0].skinPaletteRecord() == fixture.pose.get());
        CHECK(output[0].coverageDataIdentity() == source->prototype().coverageDataIdentity());
        auto republished = fixture.source(2, output[0]); REQUIRE(republished);
        CHECK_FALSE(republished->prototype().hasBorrowedInputs());
        CHECK_FALSE(republished->prototype().skinPaletteRecord());
        CHECK_FALSE(republished->prototype().sourceRecord());
        CHECK(republished->prototype().coverageData().alpha_cutoffs[0] == .5f);
        republished.reset();
        auto copied = output[0];
        copied.coverageData().alpha_cutoffs[0] = .25f;
        CHECK(output[0].coverageData().alpha_cutoffs[0] == .5f);
        CHECK(source->prototype().coverageData().alpha_cutoffs[0] == .5f);
        CHECK(copied.coverageDataIdentity() != output[0].coverageDataIdentity());
        std::weak_ptr<const ALMetalBuffer> vertex_lifetime = fixture.vertices;
        std::weak_ptr<const ALMetalTexture> texture_lifetime = fixture.texture;
        fixture.vertices.reset(); fixture.indices16.reset(); fixture.texture.reset();
        source.reset(); input = {}; copied = {}; prototype = {};
        CHECK_FALSE(vertex_lifetime.expired());
        CHECK_FALSE(texture_lifetime.expired());
        output[0] = {};
        CHECK_FALSE(vertex_lifetime.expired());
        CHECK_FALSE(texture_lifetime.expired());
        packet.reset();
        CHECK(vertex_lifetime.expired());
        CHECK(texture_lifetime.expired());
    }
}

TEST_CASE("almetal: shadow publication rejects mismatched leases and unsupported raw state",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        const auto valid = fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask, true);
        REQUIRE(fixture.source(1, valid));
        CHECK_FALSE(fixture.source(0, valid));
        auto invalid = valid; invalid.vertices = nullptr;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.indices = fixture.indices32.get();
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.position_offset = 128;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.index_offset = 2;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.skin_palette_record.reset();
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.avatar = true;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.terrain = true;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.coverageData().textures[0] = nullptr;
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.coverageData().alpha_cutoffs[0] = std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(fixture.source(1, invalid));
        invalid = valid; invalid.coverageData().pbr_base_color_transforms[0][0][0] =
            std::numeric_limits<float>::infinity();
        // Legacy coverage does not consume PBR transforms.
        CHECK(fixture.source(1, invalid));
        invalid.coverage = ALMetalWorldSpotShadowCoverage::pbr_mask;
        CHECK_FALSE(fixture.source(1, invalid));
    }
}

TEST_CASE("almetal: borrowed palette publication retains identity once on a cache miss",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto arena = ALMetalWorldFrameData::create(*fixture.device, &fixture.error);
        REQUIRE(arena);
        arena->beginFrame(1);
        auto first = fixture.device->makeCommandBuffer("Borrowed palette first reader", &fixture.error);
        auto second = fixture.device->makeCommandBuffer("Borrowed palette second reader", &fixture.error);
        REQUIRE(first); REQUIRE(second);
        const auto before = fixture.pose.use_count();
        ALMetalWorldDataSlice initial, repeated;
        REQUIRE(arena->publishPaletteRecord(*first, fixture.pose.get(), initial, &fixture.error));
        CHECK(fixture.pose.use_count() == before + 1);
        for (unsigned at = 0; at < 32; ++at)
            REQUIRE(arena->publishPaletteRecord(*first, fixture.pose.get(), repeated, &fixture.error));
        REQUIRE(arena->publishPaletteRecord(*second, fixture.pose.get(), repeated, &fixture.error));
        CHECK(fixture.pose.use_count() == before + 1);
        CHECK(initial.buffer == repeated.buffer);
        CHECK(initial.offset == repeated.offset);
        CHECK(arena->stats().record_publications == 1);
        CHECK(arena->stats().record_reuses == 33);
        CHECK_FALSE(arena->publishPaletteRecord(*first, nullptr, repeated, &fixture.error));
        first.reset(); second.reset();
        arena->beginFrame(2);
        CHECK(fixture.pose.use_count() == before);
    }
}

TEST_CASE("almetal: serial and bounded Dispatch prepare identical ordered shadow packets",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        std::vector<std::shared_ptr<const Record>> sources;
        for (const auto coverage : {ALMetalWorldSpotShadowCoverage::opaque,
                ALMetalWorldSpotShadowCoverage::legacy_mask, ALMetalWorldSpotShadowCoverage::pbr_mask})
            for (bool rigged : {false, true})
                sources.push_back(fixture.source(sources.size() + 1, fixture.prototype(coverage, rigged)));
        for (const auto& source : sources) REQUIRE(source);
        for (const std::size_t count : {0u, 1u, 255u, 256u, 1025u})
        {
            std::vector<Input> inputs;
            for (std::size_t at = 0; at < count; ++at)
            {
                auto input = inputFor(sources[at % sources.size()]);
                input.model[12] += float(at) / 1024.f;
                input.model[1] = float(at % 13) / 97.f;
                input.view[6] = float(at % 7) / 31.f;
                input.projection[11] = -float(at % 5) / 113.f;
                input.has_model = at % 3 != 0;
                if (input.source->requiresPalette()) input.palette = fixture.pose;
                inputs.push_back(std::move(input));
            }
            std::vector<Batch> serial(count), parallel(count);
            ALMetalShadowPrepareStats serial_stats, parallel_stats;
            REQUIRE(alMetalPrepareShadowBatches(inputs, serial, 7, nullptr, false, &serial_stats));
            REQUIRE(alMetalPrepareShadowBatches(inputs, parallel, 7, nullptr, true, &parallel_stats));
            CHECK(parallel_stats.jobs == (count ? count >= 256 ? 2u : 1u : 0u));
            CHECK(parallel_stats.dispatched == (count >= 256));
            for (std::size_t at = 0; at < count; ++at)
            {
                CHECK(std::memcmp(serial[at].modelview_projection, parallel[at].modelview_projection, 64) == 0);
                CHECK(serial[at].sourceRecord() == inputs[at].source.get());
                CHECK(parallel[at].sourceRecord() == inputs[at].source.get());
                CHECK(serial[at].coverageDataIdentity() == parallel[at].coverageDataIdentity());
                CHECK(serial[at].skinPaletteRecord() == parallel[at].skinPaletteRecord());
                CHECK_FALSE(serial[at].source_record);
                CHECK_FALSE(parallel[at].skin_palette_record);
                LLMatrix4a model(inputs[at].model.data()), view(inputs[at].view.data());
                LLMatrix4a projection(inputs[at].projection.data()), modelview = view, expected;
                if (inputs[at].has_model) modelview.setMul(model, view);
                expected.setMul(modelview, projection);
                CHECK(std::memcmp(parallel[at].modelview_projection, expected.getF32ptr(), 64) == 0);
            }
        }
    }
}

TEST_CASE("almetal: compact shadow instances share one immutable view without per-index ownership",
          "[almetal][shadow-prepare]")
{
    static_assert(std::is_trivially_copyable_v<ALMetalShadowPrepareInstance>);
    @autoreleasepool
    {
        Fixture fixture;
        std::vector<std::shared_ptr<const Record>> sources;
        for (const auto coverage : {ALMetalWorldSpotShadowCoverage::opaque,
                ALMetalWorldSpotShadowCoverage::legacy_mask, ALMetalWorldSpotShadowCoverage::pbr_mask})
            for (bool rigged : {false, true})
                sources.push_back(fixture.source(sources.size() + 1, fixture.prototype(coverage, rigged)));
        for (const auto& source : sources) REQUIRE(source);
        for (unsigned view_number = 0; view_number < 6; ++view_number)
        for (std::size_t count : {0u, 1u, 255u, 256u, 1025u})
        {
            ALMetalShadowPrepareContext context;
            context.view[0] = .75f; context.view[6] = float(view_number + 1) / 31.f;
            context.view[14] = -float(view_number) / 17.f;
            context.projection[5] = .5f; context.projection[11] = -.03125f;
            std::vector<Input> legacy;
            std::vector<ALMetalShadowPrepareInstance> instances;
            for (std::size_t at = 0; at < count; ++at)
            {
                auto owned = inputFor(sources[at % sources.size()]);
                owned.view = context.view; owned.projection = context.projection;
                owned.has_model = at % 3 != 0;
                owned.model[1] = float(at % 13) / 97.f;
                owned.model[12] += float(at) / 1024.f;
                if (owned.source->requiresPalette()) owned.palette = fixture.pose;
                ALMetalShadowPrepareInstance instance;
                instance.source = owned.source.get(); instance.palette = owned.palette.get();
                instance.source_generation = owned.source_generation; instance.epoch = owned.epoch;
                instance.model = owned.model; instance.has_model = owned.has_model;
                instances.push_back(instance); legacy.push_back(std::move(owned));
            }
            std::vector<Batch> reference(count), serial(count), parallel(count);
            REQUIRE(alMetalPrepareShadowBatches(legacy, reference, 7));
            const auto source_owners = sources[0].use_count(), pose_owners = fixture.pose.use_count();
            ALMetalShadowPrepareStats stats;
            REQUIRE(alMetalPrepareShadowBatches(instances, context, serial, 7));
            REQUIRE(alMetalPrepareShadowBatches(instances, context, parallel, 7, nullptr, true, &stats));
            CHECK(stats.jobs == (count ? count >= 256 ? 2u : 1u : 0u));
            CHECK(sources[0].use_count() == source_owners);
            CHECK(fixture.pose.use_count() == pose_owners);
            for (std::size_t at = 0; at < count; ++at)
            {
                REQUIRE(std::memcmp(reference[at].modelview_projection, serial[at].modelview_projection, 64) == 0);
                REQUIRE(std::memcmp(reference[at].modelview_projection, parallel[at].modelview_projection, 64) == 0);
                REQUIRE(serial[at].sourceRecord() == legacy[at].source.get());
                REQUIRE(serial[at].skinPaletteRecord() == legacy[at].palette.get());
                REQUIRE(serial[at].coverageDataIdentity() == reference[at].coverageDataIdentity());
                REQUIRE(sources[at % sources.size()]->matchesStableInputs(serial[at]));
            }
            // Preparation publishes values, never pointers into instance or
            // context scratch; that memory may immediately be recycled.
            instances.clear();
            context.view.fill(std::numeric_limits<float>::quiet_NaN());
            if (count) CHECK(std::memcmp(reference.back().modelview_projection,
                                        parallel.back().modelview_projection, 64) == 0);
        }
    }
}

TEST_CASE("almetal: compact shadow context and instance failures reset the whole transaction",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto source = fixture.source(11, fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask, true));
        REQUIRE(source);
        ALMetalShadowPrepareContext context;
        ALMetalShadowPrepareInstance valid;
        valid.source = source.get(); valid.palette = fixture.pose.get();
        valid.source_generation = 11; valid.epoch = 7; valid.has_model = true;
        std::vector<ALMetalShadowPrepareInstance> inputs(257, valid);
        std::vector<Batch> output(inputs.size());
        REQUIRE(alMetalPrepareShadowBatches(inputs, context, output, 7));
        const auto rejected = [&]
        {
            CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, output, 7, nullptr, true));
            CHECK(std::all_of(output.begin(), output.end(), [](const Batch& b) {
                return !b.vertices && !b.sourceRecord() && !b.skinPaletteRecord() && !b.hasBorrowedInputs();
            }));
        };
        context.view[1] = std::numeric_limits<float>::quiet_NaN(); rejected();
        context = {}; context.projection[7] = std::numeric_limits<float>::infinity(); rejected();
        context = {}; inputs.back().source_generation = 12; rejected();
        inputs.back() = valid; inputs.back().epoch = 8; rejected();
        inputs.back() = valid; inputs.back().source = nullptr; rejected();
        inputs.back() = valid; inputs.back().palette = nullptr; rejected();
        inputs.back() = valid; inputs.back().model[4] = std::numeric_limits<float>::quiet_NaN(); rejected();
        inputs.back() = valid;
        std::atomic<std::uint64_t> cancelled{8}; ALMetalShadowPrepareStats stats;
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, output, 7, &cancelled, true, &stats));
        CHECK(stats.cancelled); CHECK(stats.jobs == 0);
        REQUIRE(alMetalPrepareShadowBatches(inputs, context, output, 7));
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, std::span<Batch>(output).first(1), 7));
        CHECK_FALSE(output[0].sourceRecord());
    }
}

TEST_CASE("almetal: repeated indexed texture slots preserve ordered source identity PPD096",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (auto coverage : {ALMetalWorldSpotShadowCoverage::legacy_mask,
                              ALMetalWorldSpotShadowCoverage::pbr_mask})
        for (std::size_t slots : {2u, 8u})
        {
            CAPTURE(coverage, slots);
            auto prototype = fixture.prototype(coverage, true);
            prototype.texture_count = slots;
            for (std::size_t slot = 0; slot < slots; ++slot)
            {
                prototype.coverageData().textures[slot] = fixture.texture.get();
                prototype.coverageData().alpha_cutoffs[slot] = .125f + float(slot) / 32.f;
                prototype.coverageData().pbr_base_color_transforms[slot][0][0] = 1.f;
                prototype.coverageData().pbr_base_color_transforms[slot][0][1] = 1.f;
            }
            std::vector<std::shared_ptr<const ALMetalTexture>> leases(slots, fixture.texture);
            auto source = Record::create(1, prototype, fixture.vertices, fixture.indices16,
                                         leases, &fixture.error);
            INFO(fixture.error); REQUIRE(source);
            auto input = inputFor(source); input.palette = fixture.pose;
            std::array<Batch, 1> output;
            REQUIRE(alMetalPrepareShadowBatches({&input, 1}, output, 7));
            CHECK(source->matchesStableInputs(output[0]));
            CHECK(output[0].texture_count == slots);
            const auto& prepared = static_cast<const Batch&>(output[0]).coverageData();
            for (std::size_t slot = 0; slot < slots; ++slot)
            {
                CHECK(prepared.textures[slot] == fixture.texture.get());
                CHECK(prepared.alpha_cutoffs[slot] == .125f + float(slot) / 32.f);
            }
            // The old producer bug retained one unique handle for several
            // shader-visible slots. That is not the ordered lease contract.
            leases.resize(1);
            CHECK_FALSE(Record::create(2, prototype, fixture.vertices, fixture.indices16,
                                      leases, &fixture.error));
        }
    }
}

TEST_CASE("almetal: immutable source proof rejects public stable field mutations",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto source = fixture.source(1, fixture.prototype(ALMetalWorldSpotShadowCoverage::pbr_mask, true));
        REQUIRE(source);
        auto input = inputFor(source); input.palette = fixture.pose;
        std::array<Batch, 1> output;
        REQUIRE(alMetalPrepareShadowBatches({&input, 1}, output, 7));
        REQUIRE(source->matchesStableInputs(output[0]));
        const auto rejects = [&](auto mutate)
        {
            auto changed = output[0]; mutate(changed);
            CHECK_FALSE(source->matchesStableInputs(changed));
        };
        rejects([](auto& b) { b.clearBorrowedInputs(); });
        rejects([](auto& b) { b.vertices = nullptr; });
        rejects([](auto& b) { b.indices = nullptr; });
        rejects([](auto& b) { ++b.position_offset; });
        rejects([](auto& b) { ++b.normal_offset; });
        rejects([](auto& b) { ++b.texcoord_offset; });
        rejects([](auto& b) { ++b.color_offset; });
        rejects([](auto& b) { ++b.weight_offset; });
        rejects([](auto& b) { ++b.cloth_offset; });
        rejects([](auto& b) { ++b.vertex_count; });
        rejects([](auto& b) { ++b.index_offset; });
        rejects([](auto& b) { b.index_type = ALMetalIndexType::uint32; });
        rejects([](auto& b) { ++b.index_count; });
        rejects([](auto& b) { ++b.texture_count; });
        rejects([](auto& b) { b.coverage = ALMetalWorldSpotShadowCoverage::legacy_mask; });
        rejects([](auto& b) { b.double_sided = !b.double_sided; });
        rejects([](auto& b) { b.terrain = true; });
        rejects([](auto& b) { b.avatar = true; });
        rejects([](auto& b) { b.avatar_cloth = true; });
        rejects([](auto& b) { b.terrain_smoothing = true; });
        rejects([](auto& b) { b.terrain_tessellation_pinned = true; });
        rejects([&](auto& b) { b.terrain_height_texture = fixture.texture.get(); });
        rejects([](auto& b) { b.terrain_grid_scale = 1.f; });
        rejects([](auto& b) { b.avatar_draw_alpha = .5f; });
        rejects([](auto& b) { b.texture_matrix[0] = 2.f; });
        rejects([](auto& b) { b.avatar_wind[0] = 1.f; });
        rejects([](auto& b) { b.avatar_sin_wave[0] = 1.f; });
        rejects([](auto& b) { b.avatar_gravity[0] = 1.f; });
        rejects([](auto& b) { b.avatar_cloth_depth[0] = 1.f; });
        rejects([](auto& b) { b.terrain_tessellation_factors[0] = 1; });
        rejects([](auto& b) { b.coverageData().textures[0] = nullptr; });
        rejects([](auto& b) { b.coverageData().alpha_cutoffs[0] = .25f; });
        rejects([](auto& b) { b.coverageData().pbr_base_color_transforms[0][0][0] = 2.f; });
        // View and pose validation is intentionally outside this certificate.
        auto changed = output[0]; changed.modelview_projection[0] = std::numeric_limits<float>::quiet_NaN();
        CHECK(source->matchesStableInputs(changed));
        changed = output[0]; ++changed.skin_joint_count;
        CHECK(source->matchesStableInputs(changed));
        changed = output[0]; changed.skin_palette = nullptr;
        CHECK_FALSE(source->matchesStableInputs(changed));

        auto static_prototype = fixture.prototype(ALMetalWorldSpotShadowCoverage::opaque);
        static_prototype.weight_offset = Fixture::weight_offset;
        CHECK_FALSE(fixture.source(3, static_prototype));
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 1;
        descriptor.usage = ALMetalTextureUsage::render_target;
        auto nonsampleable = ALMetalTexture::create(*fixture.device, descriptor, &fixture.error);
        REQUIRE(nonsampleable);
        auto prototype = fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask);
        prototype.coverageData().textures[0] = nonsampleable.get();
        std::shared_ptr<const ALMetalTexture> lease = std::move(nonsampleable);
        CHECK_FALSE(Record::create(4, prototype, fixture.vertices, fixture.indices16,
                                  {lease}, &fixture.error));
    }
}

TEST_CASE("almetal: stale cancellation generation and bad inputs never publish a prefix",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto source = fixture.source(9, fixture.prototype(ALMetalWorldSpotShadowCoverage::opaque));
        REQUIRE(source);
        std::vector<Input> inputs(512, inputFor(source));
        std::vector<Batch> output(inputs.size());
        std::atomic<std::uint64_t> epoch{7};
        const auto rejects = [&](auto change)
        {
            auto changed = inputs; change(changed.back());
            REQUIRE(alMetalPrepareShadowBatches(inputs, output, 7, &epoch, true));
            CHECK_FALSE(alMetalPrepareShadowBatches(changed, output, 7, &epoch, true));
            for (const auto& batch : output)
            {
                CHECK_FALSE(batch.vertices); CHECK_FALSE(batch.indices); CHECK_FALSE(batch.sourceRecord());
            }
        };
        rejects([](auto& input) { input.source_generation = 8; });
        rejects([](auto& input) { input.epoch = 6; });
        rejects([](auto& input) { input.source.reset(); });
        rejects([](auto& input) { input.model[0] = std::numeric_limits<float>::infinity(); });
        rejects([](auto& input) { input.projection[15] = std::numeric_limits<float>::quiet_NaN(); });
        rejects([&](auto& input) { input.palette = fixture.pose; });
        ALMetalShadowPrepareStats stats;
        epoch = 8;
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, output, 7, &epoch, true, &stats));
        CHECK(stats.cancelled);
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, {output.data(), output.size() - 1}, 7));
        for (std::size_t at = 0; at + 1 < output.size(); ++at) CHECK_FALSE(output[at].sourceRecord());
    }
}

TEST_CASE("almetal: prepared native shadows preserve direct depth coverage and changing poses",
          "[almetal][shadow-prepare]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto renderer = ALMetalWorldGBufferRenderer::create(*fixture.device, &fixture.error);
        INFO(fixture.error); REQUIRE(renderer);
        ALMetalTextureDescriptor td; td.type = ALMetalTextureType::texture_2d_array;
        td.width = td.height = 32; td.array_length = 2; td.format = ALMetalPixelFormat::r32_float;
        td.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        auto target = ALMetalTexture::create(*fixture.device, td, &fixture.error); REQUIRE(target);
        for (const auto coverage : {ALMetalWorldSpotShadowCoverage::opaque,
                ALMetalWorldSpotShadowCoverage::legacy_mask, ALMetalWorldSpotShadowCoverage::texture_mask,
                ALMetalWorldSpotShadowCoverage::pbr_mask})
        for (bool rigged : {false, true})
        for (bool reverse_z : {false, true})
        {
            CAPTURE(coverage, rigged, reverse_z);
            auto prototype = fixture.prototype(coverage, rigged, reverse_z);
            auto source = fixture.source(1, prototype); REQUIRE(source);
            std::vector<Input> inputs(256, inputFor(source));
            std::vector<Batch> direct(256, prototype), prepared(256);
            for (std::size_t at = 0; at < inputs.size(); ++at)
            {
                inputs[at].model[12] += float(at) / 1024.f;
                inputs[at].projection[14] = reverse_z ? .5f : 0.f;
                if (rigged) inputs[at].palette = fixture.pose;
                direct[at].modelview_projection[0] = .75f;
                direct[at].modelview_projection[5] = .5f;
                direct[at].modelview_projection[12] = inputs[at].model[12] * .75f;
                direct[at].modelview_projection[14] = inputs[at].projection[14];
            }
            auto packet = std::make_shared<std::vector<Input>>(inputs);
            REQUIRE(alMetalPrepareShadowBatches(*packet, prepared, 7, nullptr, true));
            ALMetalWorldSpotShadowScene scene;
            scene.source_owner = packet;
            scene.width = scene.height = 32; scene.reverse_z = reverse_z;
            scene.batches = direct.data(); scene.batch_count = direct.size();
            REQUIRE(renderer->renderSpotShadow(*target, 0, scene, &fixture.error));
            scene.batches = prepared.data();
            scene.source_owner.reset();
            CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            scene.source_owner = packet;
            REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            std::array<float, 1024> before{}, after{};
            REQUIRE(target->readback(*fixture.device, 0, 0, {}, before.data(), 32 * sizeof(float), &fixture.error));
            REQUIRE(target->readback(*fixture.device, 0, 1, {}, after.data(), 32 * sizeof(float), &fixture.error));
            CHECK(before == after);
            const float clear = reverse_z ? 0.f : 1.f;
            CHECK(std::any_of(after.begin(), after.end(), [clear](float value) { return value != clear; }));
            // The certificate does not make public output fields trusted.
            // A changed stable tuple takes the ordinary validation path;
            // invalid per-view MVP/current-pose metadata must always fail.
            auto wrong = prepared[0];
            scene.batches = &wrong; scene.batch_count = 1;
            wrong.position_offset = fixture.vertices->length();
            CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            wrong = prepared[0];
            wrong.modelview_projection[0] = std::numeric_limits<float>::quiet_NaN();
            CHECK(source->matchesStableInputs(wrong));
            CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            if (rigged)
            {
                wrong = prepared[0]; ++wrong.skin_joint_count;
                CHECK(source->matchesStableInputs(wrong));
                CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            }
            if (coverage != ALMetalWorldSpotShadowCoverage::opaque)
            {
                wrong = prepared[0];
                wrong.coverageData().alpha_cutoffs[0] = std::numeric_limits<float>::quiet_NaN();
                CHECK_FALSE(source->matchesStableInputs(wrong));
                CHECK_FALSE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
            }
            scene.batches = prepared.data(); scene.batch_count = prepared.size();
            if (rigged)
            {
                std::array<float, 16> values{};
                values[4] = values[9] = values[14] = 1.f; values[15] = .25f;
                auto changed = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &fixture.error);
                REQUIRE(changed);
                for (auto& input : inputs) input.palette = changed;
                *packet = inputs;
                REQUIRE(alMetalPrepareShadowBatches(*packet, prepared, 7, nullptr, true));
                REQUIRE(renderer->renderSpotShadow(*target, 1, scene, &fixture.error));
                REQUIRE(target->readback(*fixture.device, 0, 1, {}, after.data(), 32 * sizeof(float), &fixture.error));
                CHECK(before != after);
            }
        }
    }
}

TEST_CASE("almetal: opt-in complete owned shadow preparation CPU cost",
          "[.][almetal][shadow-prepare-cost]")
{
#ifndef NDEBUG
    SKIP("Shadow preparation timing requires an optimized build");
#endif
    @autoreleasepool
    {
        Fixture fixture;
        std::array<Batch, 3> prototypes = {
            fixture.prototype(ALMetalWorldSpotShadowCoverage::opaque, true),
            fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask, true),
            fixture.prototype(ALMetalWorldSpotShadowCoverage::pbr_mask, true)};
        std::cout << std::setprecision(12)
            << "SHADOW_PREPARE_SCOPE source_sealing=3_records_per_arm inputs=owned_per_draw"
               " includes=source_seal_input_copy_output_allocation_prepare_ownership_release_pool_drain"
               " excludes=unchanged_scene_traversal_pose_solve_native_encoding_GPU FPS_claim=0\n";
        for (const std::size_t count : {32u, 256u, 2048u, 8192u})
        {
            std::array<std::vector<double>, 2> samples;
            for (unsigned block = 0; block < 10; ++block)
            for (unsigned repetition = 0; repetition < 10; ++repetition)
            for (unsigned arm = 0; arm < 4; ++arm)
            {
                const bool parallel = (arm == 1 || arm == 2) != bool(block % 2);
                const auto started = Clock::now();
                ALMetalShadowPrepareStats stats;
                @autoreleasepool
                {
                    std::array<std::shared_ptr<const Record>, 3> sources;
                    for (std::size_t at = 0; at < sources.size(); ++at)
                    {
                        sources[at] = fixture.source(block * 10 + repetition + 1, prototypes[at]);
                        if (!sources[at]) throw std::runtime_error(fixture.error);
                    }
                    std::vector<Input> inputs;
                    inputs.reserve(count);
                    for (std::size_t at = 0; at < count; ++at)
                    {
                        auto input = inputFor(sources[at % sources.size()]);
                        input.model[12] += float(at % 256) / 1024.f;
                        input.palette = fixture.pose;
                        inputs.push_back(std::move(input));
                    }
                    std::vector<Batch> output(count);
                    if (!alMetalPrepareShadowBatches(inputs, output, 7, nullptr, parallel, &stats, &fixture.error))
                        throw std::runtime_error(fixture.error);
                    // All local input/output/source ownership dies in this arm.
                }
                const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
                samples[parallel].push_back(elapsed);
                std::cout << "SHADOW_PREPARE_SAMPLE draws=" << count << " block=" << block
                    << " repetition=" << repetition << " arm=" << arm
                    << " variant=" << (parallel ? "dispatch" : "serial")
                    << " jobs=" << stats.jobs << " cpu_total_ms=" << elapsed << '\n';
            }
            for (unsigned arm = 0; arm < 2; ++arm)
            {
                auto& values = samples[arm]; std::sort(values.begin(), values.end());
                REQUIRE(values.size() == 200);
                std::cout << "SHADOW_PREPARE_SUMMARY draws=" << count
                    << " variant=" << (arm ? "dispatch" : "serial")
                    << " samples=" << values.size() << " p50_ms=" << values[99]
                    << " p95_ms=" << values[189] << '\n';
            }
        }
    }
}

TEST_CASE("almetal: opt-in complete compact shadow preparation CPU cost",
          "[.][almetal][shadow-prepare-compact-cost]")
{
    const auto enabled = [](const char* name) {
        const char* value = std::getenv(name); return value && value[0] && value[0] != '0';
    };
    if (!enabled("ALCHEMY_SHADOW_PREPARE_COMPACT_BENCHMARK"))
        SKIP("Set ALCHEMY_SHADOW_PREPARE_COMPACT_BENCHMARK=1 for the explicit timing run");
#ifndef NDEBUG
    SKIP("Shadow preparation timing requires an optimized build");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    @autoreleasepool
    {
        Fixture fixture;
        std::array<Batch, 3> prototypes = {
            fixture.prototype(ALMetalWorldSpotShadowCoverage::opaque, true),
            fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask, true),
            fixture.prototype(ALMetalWorldSpotShadowCoverage::pbr_mask, true)};
        std::cout << std::setprecision(12)
            << "SHADOW_PREPARE_SCOPE source_sealing=3_records_per_arm inputs=compact_raw_per_draw"
               " context=one_per_view ownership=command_aggregate_source_pose_pointer_dedup"
               " includes=source_seal_aggregate_acquire_input_copy_output_allocation_prepare_ownership_release_pool_drain"
               " excludes=unchanged_scene_traversal_pose_solve_native_encoding_GPU FPS_claim=0\n";
        for (const std::size_t count : {32u, 256u, 2048u, 8192u})
        {
            std::array<std::vector<double>, 2> samples;
            for (unsigned block = 0; block < 10; ++block)
            for (unsigned repetition = 0; repetition < 10; ++repetition)
            for (unsigned arm = 0; arm < 4; ++arm)
            {
                const bool parallel = (arm == 1 || arm == 2) != bool(block % 2);
                const auto started = Clock::now();
                ALMetalShadowPrepareStats stats;
                @autoreleasepool
                {
                    std::array<std::shared_ptr<const Record>, 3> sources;
                    for (std::size_t at = 0; at < sources.size(); ++at)
                    {
                        sources[at] = fixture.source(block * 10 + repetition + 1, prototypes[at]);
                        if (!sources[at]) throw std::runtime_error(fixture.error);
                    }
                    std::vector<std::shared_ptr<const Record>> source_pins;
                    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> pose_pins;
                    std::unordered_set<const Record*> source_ids;
                    std::unordered_set<const ALMetalAttachmentPaletteRecord*> pose_ids;
                    ALMetalShadowPrepareContext context;
                    context.view[0] = .75f; context.projection[5] = .5f;
                    std::vector<ALMetalShadowPrepareInstance> inputs;
                    inputs.reserve(count);
                    for (std::size_t at = 0; at < count; ++at)
                    {
                        const auto& source = sources[at % sources.size()];
                        if (source_ids.insert(source.get()).second) source_pins.push_back(source);
                        if (pose_ids.insert(fixture.pose.get()).second) pose_pins.push_back(fixture.pose);
                        ALMetalShadowPrepareInstance input;
                        input.source = source.get(); input.palette = fixture.pose.get();
                        input.source_generation = source->generation(); input.epoch = 7;
                        input.has_model = true; input.model[12] = .125f + float(at % 256) / 1024.f;
                        inputs.push_back(input);
                    }
                    std::vector<Batch> output(count);
                    if (!alMetalPrepareShadowBatches(inputs, context, output, 7, nullptr, parallel, &stats, &fixture.error))
                        throw std::runtime_error(fixture.error);
                    // Aggregate pins, scratch and output all die in this arm.
                }
                const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
                samples[parallel].push_back(elapsed);
                std::cout << "SHADOW_PREPARE_SAMPLE draws=" << count << " block=" << block
                    << " repetition=" << repetition << " arm=" << arm
                    << " variant=" << (parallel ? "dispatch" : "serial")
                    << " jobs=" << stats.jobs << " cpu_total_ms=" << elapsed << '\n';
            }
            for (unsigned arm = 0; arm < 2; ++arm)
            {
                auto& values = samples[arm]; std::sort(values.begin(), values.end());
                REQUIRE(values.size() == 200);
                std::cout << "SHADOW_PREPARE_SUMMARY draws=" << count
                    << " variant=" << (arm ? "dispatch" : "serial")
                    << " samples=" << values.size() << " p50_ms=" << values[99]
                    << " p95_ms=" << values[189] << '\n';
            }
        }
    }
}
