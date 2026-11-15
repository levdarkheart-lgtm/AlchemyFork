/**
 * @file almetal_shadow_prepare_test.mm
 * @brief Immutable source ownership, ordered preparation and complete CPU cost.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowprepare.h"
#include "../almetalshadowcapture.h"
#include "../../llcommon/alnativeframebudget.h"
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
#include <utility>

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

TEST_CASE("almetal: budgeted compact shadows preserve immutable ordered packets and retire both CPU credits",
          "[almetal][shadow-prepare][shadow-prepare-budget]")
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
        const std::array<std::array<float, 16>, 2> uv = []
        {
            std::array<std::array<float, 16>, 2> result{alMetalShadowIdentity, alMetalShadowIdentity};
            result[0][12] = .25f;result[1][0] = .5f;return result;
        }();
        ALMetalShadowPrepareContext context;
        context.view[12] = .125f;context.projection[5] = .75f;context.texture_matrices = uv;
        ALNativeFrameBudget budget;
        ALNativeFrameBudget::FrameInputs plan;plan.frame = 1;budget.beginFrame(plan);
        for (std::size_t count : {0u, 1u, 255u, 256u, 257u})
        {
            CAPTURE(count);
            std::vector<ALMetalShadowPrepareInstance> inputs(count);
            for (std::size_t index = 0; index < count; ++index)
            {
                auto& instance = inputs[index];instance.source = sources[index % sources.size()].get();
                instance.source_generation = instance.source->generation();instance.epoch = 7;
                instance.palette = instance.source->requiresPalette() ? fixture.pose.get() : nullptr;
                instance.has_model = index % 3 != 0;instance.model[12] = float(index) / 1024.f;
                if (index % 3) instance.texture_matrix_slot = static_cast<std::uint32_t>(index % 2);
            }
            std::vector<Batch> serial(count), budgeted(count);
            REQUIRE(alMetalPrepareShadowBatches(inputs, context, serial, 7));
            const auto before = budget.snapshot();
            const auto source_owners = sources[0].use_count(), pose_owners = fixture.pose.use_count();
            ALMetalShadowPrepareStats stats;
            REQUIRE(alMetalPrepareShadowBatches(inputs, context, budgeted, budget, 7, nullptr, true, &stats));
            const bool dispatched = count >= 256;
            CHECK(stats.jobs == (count ? dispatched ? 2u : 1u : 0u));
            CHECK(stats.dispatched == dispatched);CHECK_FALSE(stats.cancelled);
            const auto after = budget.snapshot();
            CHECK(after.admitted - before.admitted == (dispatched ? 2u : 0u));
            CHECK(after.retired - before.retired == (dispatched ? 2u : 0u));
            CHECK(after.preparation_in_flight == 0);CHECK(after.optional_views_in_flight == 0);
            CHECK(after.scene_publications_in_flight == 0);CHECK(after.uploads_in_flight == 0);
            CHECK(sources[0].use_count() == source_owners);CHECK(fixture.pose.use_count() == pose_owners);
            for (std::size_t index = 0; index < count; ++index)
            {
                CHECK(std::memcmp(serial[index].modelview_projection, budgeted[index].modelview_projection, 64) == 0);
                CHECK(std::memcmp(serial[index].texture_matrix, budgeted[index].texture_matrix, 64) == 0);
                CHECK(serial[index].sourceRecord() == budgeted[index].sourceRecord());
                CHECK(serial[index].skinPaletteRecord() == budgeted[index].skinPaletteRecord());
                CHECK(serial[index].coverageDataIdentity() == budgeted[index].coverageDataIdentity());
                CHECK(inputs[index].source->matchesStableInputs(budgeted[index]));
            }
        }
    }
}

TEST_CASE("almetal: shadow dispatch shares occupied slots and follows budget pressure startup and drain policy",
          "[almetal][shadow-prepare][shadow-prepare-budget]")
{
    @autoreleasepool
    {
        using Budget = ALNativeFrameBudget;
        Fixture fixture;
        auto source = fixture.source(11, fixture.prototype(ALMetalWorldSpotShadowCoverage::pbr_mask, true));
        REQUIRE(source);
        ALMetalShadowPrepareInstance instance;
        instance.source = source.get();instance.palette = fixture.pose.get();instance.source_generation = 11;instance.epoch = 7;
        std::vector<ALMetalShadowPrepareInstance> inputs(257, instance);
        std::vector<Batch> serial(inputs.size()), output(inputs.size());
        ALMetalShadowPrepareContext context;
        REQUIRE(alMetalPrepareShadowBatches(inputs, context, serial, 7));
        Budget::Limits limits;limits.preparation_jobs = 2;
        Budget budget(limits);Budget::FrameInputs plan;plan.frame = 1;budget.beginFrame(plan);
        ALMetalShadowPrepareStats stats;
        const auto require_serial = [&](Budget& policy, bool allow = true)
        {
            REQUIRE(alMetalPrepareShadowBatches(inputs, context, output, policy, 7, nullptr, allow, &stats));
            CHECK(stats.jobs == 1);CHECK_FALSE(stats.dispatched);
            for (std::size_t index = 0; index < inputs.size(); ++index)
            {
                REQUIRE(std::memcmp(serial[index].modelview_projection, output[index].modelview_projection, 64) == 0);
                REQUIRE(output[index].sourceRecord() == source.get());
                REQUIRE(output[index].skinPaletteRecord() == fixture.pose.get());
            }
        };
        const auto require_dispatched = [&](Budget& policy)
        {
            const auto initial = policy.snapshot();
            REQUIRE(alMetalPrepareShadowBatches(inputs, context, output, policy, 7, nullptr, true, &stats));
            CHECK(stats.jobs == 2);CHECK(stats.dispatched);
            const auto completed = policy.snapshot();
            CHECK(completed.admitted == initial.admitted + 2);
            CHECK(completed.retired == initial.retired + 2);CHECK(completed.preparation_in_flight == 0);
            for (std::size_t index = 0; index < inputs.size(); ++index)
            {
                REQUIRE(std::memcmp(serial[index].modelview_projection, output[index].modelview_projection, 64) == 0);
                REQUIRE(output[index].sourceRecord() == source.get());
                REQUIRE(output[index].skinPaletteRecord() == fixture.pose.get());
            }
        };
        auto occupied = budget.tryAcquire(Budget::Work::preparation);REQUIRE(occupied);
        auto before = budget.snapshot();
        require_serial(budget);
        auto after = budget.snapshot();
        CHECK(after.preparation_in_flight == 1);
        CHECK(after.admitted == before.admitted + 1); // first reserve succeeded, second was denied
        CHECK(after.retired == before.retired + 1);CHECK(after.deferred == before.deferred + 1);
        auto full = budget.tryAcquire(Budget::Work::preparation);REQUIRE(full);
        before = budget.snapshot();require_serial(budget);after = budget.snapshot();
        CHECK(after.preparation_in_flight == 2);CHECK(after.admitted == before.admitted);
        CHECK(after.deferred == before.deferred + 1);
        full.credit.reset();occupied.credit.reset();CHECK(budget.snapshot().preparation_in_flight == 0);

        before = budget.snapshot();require_serial(budget, false);
        CHECK(budget.snapshot().admitted == before.admitted);
        plan.frame = 2;plan.frame_interval_ns = 16'000'000;plan.required_main_cpu_ns = 30'000'000;
        budget.beginFrame(plan);REQUIRE(budget.snapshot().main_busy);require_dispatched(budget);
        plan.frame = 3;plan.required_main_cpu_ns = 0;plan.os_available_bytes = 1;
        budget.beginFrame(plan);REQUIRE(budget.snapshot().memory_pressure);require_dispatched(budget);
        // Preserve the budget's existing permissive preparation startup,
        // rather than inventing a second initialization/pressure policy.
        Budget uninitialized;REQUIRE_FALSE(uninitialized.snapshot().initialized);require_dispatched(uninitialized);
        Budget draining(limits);plan = {};plan.frame = 4;draining.beginFrame(plan);draining.beginDrain();
        before = draining.snapshot();REQUIRE_FALSE(before.main_busy);REQUIRE_FALSE(before.memory_pressure);
        require_serial(draining);CHECK(draining.snapshot().admitted == before.admitted);
        CHECK(draining.snapshot().deferred == before.deferred + 1);

        // Invalid input discovered after both ranges were admitted must clear
        // the entire packet and release both synchronous credits.
        Budget failures(limits);plan = {};plan.frame = 4;failures.beginFrame(plan);
        inputs.back().source_generation = 12;before = failures.snapshot();
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, output, failures, 7, nullptr, true, &stats));
        CHECK(stats.jobs == 2);CHECK(stats.dispatched);CHECK_FALSE(stats.cancelled);
        after = failures.snapshot();CHECK(after.admitted == before.admitted + 2);
        CHECK(after.retired == before.retired + 2);CHECK(after.preparation_in_flight == 0);
        const auto cleared = [&]
        { CHECK(std::all_of(output.begin(), output.end(), [](const Batch& batch)
            { return !batch.vertices && !batch.sourceRecord() && !batch.skinPaletteRecord() && !batch.hasBorrowedInputs(); })); };
        cleared();inputs.back() = instance;
        // A stale epoch is rejected before admission, not dispatched merely
        // to discover cancellation; no new credits or valid prefix survive.
        std::atomic<std::uint64_t> epoch{8};before = failures.snapshot();
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, output, failures, 7, &epoch, true, &stats));
        CHECK(stats.cancelled);CHECK(stats.jobs == 0);cleared();
        CHECK(failures.snapshot().admitted == before.admitted);CHECK(failures.snapshot().preparation_in_flight == 0);
        epoch.store(7);
        REQUIRE(alMetalPrepareShadowBatches(inputs, context, output, failures, 7, &epoch, true, &stats));
        CHECK(stats.jobs == 2);CHECK(failures.snapshot().preparation_in_flight == 0);
        context.view[1] = std::numeric_limits<float>::quiet_NaN();before = failures.snapshot();
        CHECK_FALSE(alMetalPrepareShadowBatches(inputs, context, output, failures, 7, &epoch, true, &stats));
        CHECK(stats.jobs == 0);cleared();CHECK(failures.snapshot().admitted == before.admitted);
        CHECK(failures.snapshot().preparation_in_flight == 0);
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
        rejects([](auto& b) { b.avatar_wind[0] = 1.f; });
        rejects([](auto& b) { b.avatar_sin_wave[0] = 1.f; });
        rejects([](auto& b) { b.avatar_gravity[0] = 1.f; });
        rejects([](auto& b) { b.avatar_cloth_depth[0] = 1.f; });
        rejects([](auto& b) { b.terrain_tessellation_factors[0] = 1; });
        rejects([](auto& b) { b.coverageData().textures[0] = nullptr; });
        rejects([](auto& b) { b.coverageData().alpha_cutoffs[0] = .25f; });
        rejects([](auto& b) { b.coverageData().pbr_base_color_transforms[0][0][0] = 2.f; });
        // View, pose and animated UV validation are outside this certificate.
        auto changed = output[0]; changed.texture_matrix[0] = 2.f;
        CHECK(source->matchesStableInputs(changed));
        changed = output[0]; changed.texture_matrix[0] = std::numeric_limits<float>::quiet_NaN();
        CHECK(source->matchesStableInputs(changed));
        changed = output[0]; changed.modelview_projection[0] = std::numeric_limits<float>::quiet_NaN();
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

TEST_CASE("almetal: compact shadow UV snapshots preserve transforms and reject whole invalid packets",
          "[almetal][shadow-prepare][shadow-prepare-uv]")
{
    static_assert(std::is_trivially_copyable_v<ALMetalShadowPrepareInstance>);
    @autoreleasepool
    {
        Fixture fixture;
        auto prototype=fixture.prototype(ALMetalWorldSpotShadowCoverage::legacy_mask);
        prototype.texture_matrix[0]=.25f;prototype.texture_matrix[12]=.375f;
        auto source=fixture.source(1,prototype);INFO(fixture.error);REQUIRE(source);
        CHECK(std::memcmp(source->prototype().texture_matrix,prototype.texture_matrix,64)==0);
        auto fixed_input=inputFor(source);std::array<Batch,1> fixed_output;
        REQUIRE(alMetalPrepareShadowBatches({&fixed_input,1},fixed_output,7));
        CHECK(std::memcmp(fixed_output[0].texture_matrix,prototype.texture_matrix,64)==0);
        std::array<std::array<float,16>,3> matrices{alMetalShadowIdentity,alMetalShadowIdentity,alMetalShadowIdentity};
        matrices[0][0]=.5f;matrices[0][5]=2.f;matrices[0][12]=.25f;
        matrices[1][0]=0;matrices[1][1]=1;matrices[1][4]=-1;matrices[1][5]=0;matrices[1][13]=.75f;
        matrices[2][12]=-.125f;matrices[2][13]=.375f;
        ALMetalShadowPrepareContext context;context.texture_matrices=matrices;
        std::vector<ALMetalShadowPrepareInstance> instances(513);
        for(std::size_t at=0;at<instances.size();++at)
        {
            auto& input=instances[at];input.source=source.get();input.source_generation=1;input.epoch=7;
            if(at%4!=3)input.texture_matrix_slot=static_cast<std::uint32_t>(at%3);
        }
        std::vector<Batch> serial(instances.size()),parallel(instances.size());
        REQUIRE(alMetalPrepareShadowBatches(instances,context,serial,7));
        ALMetalShadowPrepareStats stats;
        REQUIRE(alMetalPrepareShadowBatches(instances,context,parallel,7,nullptr,true,&stats));CHECK(stats.jobs==2);
        for(std::size_t at=0;at<instances.size();++at)
        {
            const float* expected=instances[at].texture_matrix_slot==ALMetalShadowPrepareInstance::no_texture_matrix
                ?prototype.texture_matrix:matrices[instances[at].texture_matrix_slot].data();
            CHECK(std::memcmp(serial[at].texture_matrix,expected,64)==0);
            CHECK(std::memcmp(serial[at].texture_matrix,parallel[at].texture_matrix,64)==0);
            CHECK(source->matchesStableInputs(parallel[at]));
        }
        const auto rejects=[&](auto change)
        {
            for(bool dispatch:{false,true})
            {
                auto changed=instances;auto values=matrices;change(changed,values);
                ALMetalShadowPrepareContext invalid_context=context;invalid_context.texture_matrices=values;
                REQUIRE(alMetalPrepareShadowBatches(instances,context,parallel,7));
                CHECK_FALSE(alMetalPrepareShadowBatches(changed,invalid_context,parallel,7,nullptr,dispatch));
                for(const auto& batch:parallel){CHECK_FALSE(batch.vertices);CHECK_FALSE(batch.sourceRecord());}
            }
        };
        rejects([](auto& input,auto&){input.back().texture_matrix_slot=3;});
        rejects([](auto& input,auto&){input.back().texture_matrix_slot=ALMetalShadowPrepareInstance::no_texture_matrix-1;});
        rejects([](auto& input,auto& values){input.back().texture_matrix_slot=2;values[2][12]=std::numeric_limits<float>::infinity();});
        rejects([](auto& input,auto& values){input.back().texture_matrix_slot=2;values[2][0]=std::numeric_limits<float>::quiet_NaN();});
        // Unselected storage does not poison a fixed-source-UV packet.
        for(auto& input:instances)input.texture_matrix_slot=ALMetalShadowPrepareInstance::no_texture_matrix;
        for(auto& matrix:matrices)matrix.fill(std::numeric_limits<float>::quiet_NaN());
        REQUIRE(alMetalPrepareShadowBatches(instances,context,parallel,7,nullptr,true));
        CHECK(std::memcmp(parallel.back().texture_matrix,prototype.texture_matrix,64)==0);
        // Outputs own the selected values and do not borrow the UV span.
        CHECK(serial[0].texture_matrix[0]==.5f);CHECK(serial[0].texture_matrix[12]==.25f);
    }
}

TEST_CASE("almetal: animated compact UV shadows match original masked pixels without resealing sources",
          "[almetal][shadow-prepare][shadow-prepare-uv]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor image_descriptor;image_descriptor.width=2;image_descriptor.height=1;
        image_descriptor.format=ALMetalPixelFormat::rgba8_unorm;image_descriptor.usage=ALMetalTextureUsage::shader_read;
        auto image=ALMetalTexture::create(*fixture.device,image_descriptor,&fixture.error);REQUIRE(image);
        const std::array<std::uint8_t,8> alpha_lanes{255,255,255,0,255,255,255,255};
        auto upload=fixture.device->makeCommandBuffer("Animated UV alpha lanes",&fixture.error);REQUIRE(upload);
        REQUIRE(image->upload(*upload,0,0,{},alpha_lanes.data(),8,&fixture.error));REQUIRE(upload->commitAndWait(&fixture.error));
        fixture.texture=std::move(image);
        auto renderer=ALMetalWorldGBufferRenderer::create(*fixture.device,&fixture.error);INFO(fixture.error);REQUIRE(renderer);
        ALMetalTextureDescriptor td;td.type=ALMetalTextureType::texture_2d_array;td.width=td.height=32;td.array_length=2;
        td.format=ALMetalPixelFormat::r32_float;td.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
        auto target=ALMetalTexture::create(*fixture.device,td,&fixture.error);REQUIRE(target);
        for(const auto coverage:{ALMetalWorldSpotShadowCoverage::legacy_mask,ALMetalWorldSpotShadowCoverage::texture_mask})
        for(bool rigged:{false,true})
        for(bool reverse_z:{false,true})
        {
            CAPTURE(coverage,rigged,reverse_z);
            auto prototype=fixture.prototype(coverage,rigged);auto source=fixture.source(1,prototype);REQUIRE(source);
            auto owner=std::make_shared<std::pair<std::shared_ptr<const Record>,std::shared_ptr<const ALMetalAttachmentPaletteRecord>>>(source,fixture.pose);
            ALMetalShadowPrepareInstance instance;instance.source=source.get();instance.source_generation=1;instance.epoch=7;
            instance.palette=rigged?fixture.pose.get():nullptr;instance.texture_matrix_slot=0;
            ALMetalShadowPrepareContext context;
            // GL clip Z=0 maps to forward depth0.5 but is the reverse-Z far
            // clear value. Put both reference and prepared geometry at0.5 in
            // reverse-Z so accepted alpha produces distinguishable depth.
            context.projection[14]=reverse_z?.5f:0.f;
            std::array<std::array<float,16>,1> uv{alMetalShadowIdentity};context.texture_matrices=uv;
            std::array<Batch,1> prepared;
            std::array<std::array<float,1024>,2> states;
            for(unsigned animation=0;animation<2;++animation)
            {
                uv[0][12]=animation?.5f:0.f;
                REQUIRE(alMetalPrepareShadowBatches({&instance,1},context,prepared,7));
                CHECK(source->matchesStableInputs(prepared[0]));
                auto direct=prototype;std::memcpy(direct.texture_matrix,uv[0].data(),64);
                direct.modelview_projection[14]=context.projection[14];
                ALMetalWorldSpotShadowScene scene;scene.width=scene.height=32;scene.reverse_z=reverse_z;scene.source_owner=owner;
                scene.batches=&direct;scene.batch_count=1;scene.bulk_draws=false;
                const bool direct_ok=renderer->renderSpotShadow(*target,0,scene,&fixture.error);INFO(fixture.error);REQUIRE(direct_ok);
                scene.batches=prepared.data();
                const bool prepared_ok=renderer->renderSpotShadow(*target,1,scene,&fixture.error);INFO(fixture.error);REQUIRE(prepared_ok);
                std::array<float,1024> reference{},actual{};
                REQUIRE(target->readback(*fixture.device,0,0,{},reference.data(),32*sizeof(float),&fixture.error));
                REQUIRE(target->readback(*fixture.device,0,1,{},actual.data(),32*sizeof(float),&fixture.error));
                CHECK(actual==reference);states[animation]=actual;
                for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
                {
                    auto wrong=prepared[0];wrong.texture_matrix[12]=invalid;
                    CHECK(source->matchesStableInputs(wrong));scene.batches=&wrong;
                    CHECK_FALSE(renderer->renderSpotShadow(*target,1,scene,&fixture.error));
                }
            }
            CHECK(states[0]!=states[1]);
            const float clear=reverse_z?0.f:1.f;
            CHECK(std::all_of(states[0].begin(),states[0].end(),[&](float value){return value==clear;}));
            CHECK(std::any_of(states[1].begin(),states[1].end(),[&](float value){return value!=clear;}));
            CHECK(std::memcmp(source->prototype().texture_matrix,alMetalShadowIdentity.data(),64)==0);
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


namespace
{
// A frozen-record boundary model of captureMetalShadowScene, not an LL scene
// traversal or a viewer timing. The raw arm mirrors its small-family scalar
// construction and LLMatrix4a multiplication, rather than compact serial prep.
struct SegmentPlan
{
    struct Family { std::size_t begin, end; };
    Fixture& fixture;
    std::vector<Family> families;
    std::vector<LLMatrix4a> models;
    std::vector<std::array<float, 16>> uv;
    std::vector<unsigned> source_ids;
    std::size_t opaque_families;
    bool query_split;
    LLMatrix4a projection;
    std::array<LLMatrix4a, 3> views;
    std::vector<std::shared_ptr<const Record>> retained;

    SegmentPlan(Fixture& f, std::size_t count, unsigned family_count, bool query = true) : fixture(f),
        opaque_families((family_count + 1) / 2), query_split(query),
        projection(alMetalShadowIdentity.data())
    {
        // Two adjacent triangles permit a positive opaque-merge control.
        const std::array<std::uint16_t, 6> indices{{0, 1, 2, 0, 1, 2}};
        ALMetalBufferDescriptor descriptor; descriptor.length = sizeof(indices);
        auto buffer = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(buffer);
        REQUIRE(buffer->write(0, indices.data(), sizeof(indices), &f.error));
        f.indices16 = std::move(buffer);
        auto v = alMetalShadowIdentity, p = alMetalShadowIdentity;
        v[0] = .75f; v[6] = .03125f; v[12] = -.0625f; p[5] = .5f; p[11] = -.015625f;
        for (unsigned at = 0; at < views.size(); ++at)
        { v[12] = -.0625f * float(at + 1); views[at] = LLMatrix4a(v.data()); }
        projection = LLMatrix4a(p.data());
        for (unsigned family = 0; family < family_count; ++family)
        {
            const Family range{count * family / family_count, count * (family + 1) / family_count};
            families.push_back(range);
            for (std::size_t at = range.begin; at < range.end; ++at)
            {
                auto model = alMetalShadowIdentity, texture = alMetalShadowIdentity;
                model[1] = float((at / 2) % 13) / 128.f; model[12] = float((at / 2) % 31) / 64.f;
                texture[12] = float(at % 11) / 128.f;
                models.emplace_back(model.data()); uv.push_back(texture);
                source_ids.push_back(family < opaque_families ? (family % 3 == 2 ? 2u : unsigned(at % 2))
                    : 3u + (family - unsigned(opaque_families)) % 4);
            }
        }
        retained.reserve(count);
        for (std::size_t at = 0; at < count; ++at) retained.push_back(seal(at, 1));
    }
    std::shared_ptr<const Record> seal(std::size_t at, unsigned revision)
    {
        const unsigned id = source_ids[at];
        const auto coverage = id < 3 ? ALMetalWorldSpotShadowCoverage::opaque :
            id < 5 ? ALMetalWorldSpotShadowCoverage::legacy_mask : ALMetalWorldSpotShadowCoverage::pbr_mask;
        auto batch = fixture.prototype(coverage, id == 2 || id == 4 || id == 6);
        // Production unrigged opaque capture leaves these unused fields at
        // their defaults; appendOpaque reconstructs exactly that same tuple.
        if (coverage == ALMetalWorldSpotShadowCoverage::opaque && !batch.skin_palette)
        { batch.texcoord_offset = 0; batch.color_offset = 0; }
        batch.index_offset = id == 1 ? 6 : 0;
        auto source = fixture.source(std::uint64_t(revision) * (models.size() + 1) + at + 1, batch);
        if (!source) throw std::runtime_error(fixture.error);
        return source;
    }
};
struct SegmentResult
{
    // Command-wide pins and vector capacities survive successive view clears.
    std::vector<std::shared_ptr<const Record>> sources;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
    std::vector<std::shared_ptr<const Record>> publications, source_pins;
    std::unordered_set<const Record*> source_ids;
    std::unordered_set<const ALMetalAttachmentPaletteRecord*> pose_ids;
    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> pose_pins;
    std::vector<Batch> batches, prepared;
    std::vector<ALMetalShadowPrepareInstance> inputs;
    std::vector<std::array<float, 16>> texture_matrices;
    ALMetalShadowCaptureAdmission admission;
    std::size_t query_offset = 0, query_sources = 0, jobs = 0, dispatches = 0;
    bool complete = false;
};
bool segmentAppend(SegmentResult& result, Batch& batch)
{
    if (batch.coverage == ALMetalWorldSpotShadowCoverage::opaque && !batch.skin_palette)
    {
        ALMetalOpaqueShadowSource source;
        source.vertices = batch.vertices; source.indices = batch.indices;
        source.position_offset = batch.position_offset; source.vertex_count = batch.vertex_count;
        source.index_offset = batch.index_offset; source.index_count = batch.index_count;
        source.index_type = batch.index_type; source.modelview_projection = batch.modelview_projection;
        source.double_sided = batch.double_sided; source.reverse_z = false;
        const auto old_size = result.batches.size();
        if (!result.admission.appendOpaque(result.batches, source)) return false;
        if (result.batches.size() == old_size) result.batches.back().clearBorrowedInputs();
        else if (const auto* record = batch.sourceRecord())
        {
            auto& appended = result.batches.back(); appended.borrowPreparedInputs(record, nullptr, nullptr);
            if (!record->matchesStableInputs(appended)) appended.clearBorrowedInputs();
        }
    }
    else
    {
        if (!result.admission.admitSeparate()) return false;
        result.batches.push_back(std::move(batch));
    }
    return true;
}
// route: 0=current raw scalar, 1=aggregate compact serial, 2=aggregate Dispatch.
// mode: 0=retained sources, 1=cold source seals, 2=new late pose record.
// fault: 1=cancelled epoch, 2=malformed last compact input after opaque output.
bool segmentPrepare(SegmentPlan& plan, unsigned route, unsigned mode,
    unsigned revision, ALNativeFrameBudget& budget, SegmentResult& result,
    unsigned view_number = 0, unsigned fault = 0)
{
    result.batches.clear(); result.admission.reset(); result.complete = false;
    result.query_offset = result.query_sources = result.jobs = result.dispatches = 0;
    const auto& view = plan.views[view_number];
    if (!result.pose && mode == 2)
    {
        std::array<float, 16> values{}; values[4] = values[9] = values[14] = 1.f;
        values[7] = float(revision % 17) / 256.f;
        result.pose = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &plan.fixture.error);
        if (!result.pose) throw std::runtime_error(plan.fixture.error);
    }
    if (!result.pose) result.pose = plan.fixture.pose;
    if (mode == 1 && result.sources.empty()) result.sources.resize(plan.models.size());
    const auto acquire = [&](std::size_t at)
    {
        const auto& source = [&]() -> const std::shared_ptr<const Record>& {
            if (mode != 1) return plan.retained[at];
            if (!result.sources[at]) result.sources[at] = plan.seal(at, revision);
            return result.sources[at];
        }();
        result.publications.push_back(source);
        if (result.source_ids.insert(source.get()).second) result.source_pins.push_back(source);
        const auto* palette = source->requiresPalette() ? result.pose.get() : nullptr;
        if (palette && result.pose_ids.insert(palette).second) result.pose_pins.push_back(result.pose);
        return std::pair{source.get(), palette};
    };
    const auto fail = [&] { result.batches.clear(); result.admission.invalidate();
        result.query_offset = result.query_sources = 0; };
    std::atomic<std::uint64_t> current_epoch{fault == 1 ? 8u : 7u};
    for (unsigned segment = 0; segment < (plan.query_split ? 2u : 1u); ++segment)
    {
        const auto first = segment ? plan.opaque_families : 0;
        const auto last = (!plan.query_split || segment) ? plan.families.size() : plan.opaque_families;
        auto& inputs = result.inputs; auto& texture_matrices = result.texture_matrices;
        inputs.clear(); texture_matrices.clear();
        if (route && first < last)
        {
            const auto count = plan.families[last - 1].end - plan.families[first].begin;
            inputs.reserve(count); texture_matrices.reserve(count);
        }
        for (std::size_t family = first; family < last; ++family)
        {
            if (!route) result.admission.barrier();
            for (std::size_t at = plan.families[family].begin; at < plan.families[family].end; ++at)
            {
                const auto [source, palette] = acquire(at); const auto& prototype = source->prototype();
                const bool model = (at / 2) % 3 != 0;
                const bool texture = prototype.coverage != ALMetalWorldSpotShadowCoverage::opaque && at % 3 != 0;
                if (!route)
                {
                    Batch batch; static_cast<ALMetalWorldSpotShadowValues&>(batch) = prototype;
                    batch.borrowPreparedInputs(nullptr, palette, prototype.coverageDataIdentity());
                    LLMatrix4a modelview = view, mvp;
                    if (model) modelview.setMul(plan.models[at], view);
                    mvp.setMul(modelview, plan.projection);
                    std::memcpy(batch.modelview_projection, mvp.getF32ptr(), 64);
                    if (texture) std::memcpy(batch.texture_matrix, plan.uv[at].data(), 64);
                    if (palette) { batch.skin_palette = palette->data();
                        batch.skin_palette_float_count = palette->size(); batch.skin_joint_count = palette->jointCount(); }
                    if (!segmentAppend(result, batch)) { fail(); return false; }
                }
                else
                {
                    ALMetalShadowPrepareInstance input;
                    input.source = source; input.palette = palette; input.source_generation = source->generation();
                    input.epoch = 7; input.has_model = model;
                    if (model) std::memcpy(input.model.data(), plan.models[at].getF32ptr(), 64);
                    if (texture) { input.texture_matrix_slot = texture_matrices.size(); texture_matrices.push_back(plan.uv[at]); }
                    inputs.push_back(input);
                }
            }
        }
        if (route && !inputs.empty())
        {
            if (fault == 2 && last == plan.families.size()) ++inputs.back().source_generation;
            ALMetalShadowPrepareContext context; context.texture_matrices = texture_matrices;
            std::memcpy(context.view.data(), view.getF32ptr(), 64);
            std::memcpy(context.projection.data(), plan.projection.getF32ptr(), 64);
            auto& prepared = result.prepared; prepared.clear(); prepared.resize(inputs.size());
            ALMetalShadowPrepareStats stats;
            if (!alMetalPrepareShadowBatches(inputs, context, prepared, budget, 7, &current_epoch,
                    route == 2, &stats, &plan.fixture.error)) { fail(); return false; }
            result.jobs += stats.jobs; result.dispatches += stats.dispatched;
            std::size_t output = 0;
            for (std::size_t family = first; family < last; ++family)
            {
                result.admission.barrier();
                for (auto at = plan.families[family].begin; at < plan.families[family].end; ++at)
                    if (!segmentAppend(result, prepared[output++])) { fail(); return false; }
            }
            prepared.clear();
        }
        // Flush all opaque records before the original directional query point.
        if (plan.query_split && !segment) { result.admission.barrier(); result.query_offset = result.batches.size();
            result.query_sources = result.admission.sourceCount(); }
    }
    result.complete = result.admission.complete(plan.models.size());
    return result.complete;
}
void sameSegmentValues(const SegmentResult& expected, const SegmentResult& actual)
{
    REQUIRE(expected.complete); REQUIRE(actual.complete);
    REQUIRE(expected.batches.size() == actual.batches.size());
    CHECK(expected.query_offset == actual.query_offset); CHECK(expected.query_sources == actual.query_sources);
    CHECK(expected.admission.mergedSourceCount() == actual.admission.mergedSourceCount());
    for (std::size_t at = 0; at < expected.batches.size(); ++at)
    {
        const auto& a = expected.batches[at]; const auto& b = actual.batches[at];
        CHECK(a.vertices == b.vertices); CHECK(a.indices == b.indices); CHECK(a.index_count == b.index_count);
        CHECK(a.index_offset == b.index_offset); CHECK(a.coverage == b.coverage); CHECK(a.weight_offset == b.weight_offset);
        CHECK(std::memcmp(a.modelview_projection, b.modelview_projection, 64) == 0);
        CHECK(std::memcmp(a.texture_matrix, b.texture_matrix, 64) == 0);
        CHECK(a.skin_palette_float_count == b.skin_palette_float_count); CHECK(a.skin_joint_count == b.skin_joint_count);
        REQUIRE(bool(a.skin_palette) == bool(b.skin_palette));
        if (a.skin_palette) REQUIRE(std::memcmp(a.skin_palette, b.skin_palette, a.skin_palette_float_count * sizeof(float)) == 0);
        CHECK(a.texture_count == b.texture_count);
        if (a.texture_count) { CHECK(a.coverageData().textures[0] == b.coverageData().textures[0]);
            CHECK(a.coverageData().alpha_cutoffs[0] == b.coverageData().alpha_cutoffs[0]); }
    }
}
}

TEST_CASE("almetal: mixed small shadow families preserve scalar order query splits and budget fallback",
          "[almetal][shadow-prepare][shadow-prepare-segments]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (const auto& [count, families] : {std::pair{3u, 3u}, std::pair{3u, 8u}, std::pair{867u, 8u}, std::pair{1158u, 24u}})
        for (bool query_split : {false, true})
        {
            SegmentPlan plan(fixture, count, families, query_split);
            ALNativeFrameBudget budget; ALNativeFrameBudget::FrameInputs frame; frame.frame = 1; budget.beginFrame(frame);
            SegmentResult reference; REQUIRE(segmentPrepare(plan, 0, 0, 1, budget, reference));
            if (count == 3 && families == 3) { CHECK(reference.query_offset == (query_split ? 2u : 0u)); CHECK(reference.batches.size() == 3); }
            if (count > 3) CHECK(reference.admission.mergedSourceCount() > 0);
            for (unsigned mode = 0; mode < 3; ++mode)
            {
                std::array<SegmentResult, 3> result;
                for (unsigned view = 0; view < 3; ++view)
                {
                    for (unsigned route = 0; route < 3; ++route)
                        REQUIRE(segmentPrepare(plan, route, mode, 2, budget, result[route], view));
                    sameSegmentValues(result[0], result[1]); sameSegmentValues(result[0], result[2]);
                    if (count == 3 && families == 3)
                        for (unsigned route : {1u, 2u}) for (std::size_t at : {0u, 1u})
                        {
                            const auto& batch = result[route].batches[at];
                            CHECK(batch.coverage == ALMetalWorldSpotShadowCoverage::opaque); CHECK_FALSE(batch.skin_palette);
                            REQUIRE(batch.sourceRecord()); // The family barrier kept these opaque records unmerged.
                            CHECK(batch.sourceRecord()->matchesStableInputs(batch));
                        }
                    CHECK(result[0].jobs == 0); CHECK(result[1].dispatches == 0);
                    CHECK(result[2].dispatches == (count >= 512 ? query_split ? 2u : 1u : 0u));
                    CHECK(budget.snapshot().preparation_in_flight == 0);
                }
            }
            auto occupied = budget.tryAcquire(ALNativeFrameBudget::Work::preparation); REQUIRE(occupied);
            SegmentResult denied; REQUIRE(segmentPrepare(plan, 2, 0, 1, budget, denied));
            sameSegmentValues(reference, denied);
            CHECK(denied.dispatches == 0); CHECK(budget.snapshot().preparation_in_flight == 1);
            if (count >= 512) CHECK(budget.snapshot().deferred > 0);
            occupied.credit.reset(); CHECK(budget.snapshot().preparation_in_flight == 0);
            for (unsigned fault : {1u, 2u})
            {
                SegmentResult failed; CHECK_FALSE(segmentPrepare(plan, 2, 0, 1, budget, failed, 0, fault));
                CHECK_FALSE(failed.complete); CHECK(failed.batches.empty()); CHECK(failed.query_offset == 0);
                CHECK(budget.snapshot().preparation_in_flight == 0);
            }
        }
    }
}

TEST_CASE("almetal: opt-in mixed-family scalar and aggregate preparation CPU cost",
          "[.][almetal][shadow-prepare-segment-cost]")
{
    const auto enabled = [](const char* name) { const char* value = std::getenv(name); return value && value[0] && value[0] != '0'; };
    if (!enabled("ALCHEMY_SHADOW_SEGMENT_BENCHMARK")) SKIP("Set ALCHEMY_SHADOW_SEGMENT_BENCHMARK=1 for explicit timing");
#ifndef NDEBUG
    SKIP("Shadow segment timing requires an optimized build");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    @autoreleasepool
    {
        Fixture fixture;
        std::cout << std::setprecision(12) << "SHADOW_SEGMENT_SCOPE includes=record_acquire_pose_pins_cold_seals_snapshots_outputs_budget_prepare_family_admission_scratch_owner_release_pool_drain"
            " excludes=scene_traversal_native_publication_lookup_asset_allocation_pose_solve_native_encoding_GPU views=3 source_records_per_draw=1 pose_records=1 pose_joints=1 shared_native_geometry=1 modes=0_retained_1_cold_records_2_late_pose raw_reference=current_small_family_branch selection_not_emulated=1 reusable_command_aggregate=1 reusable_scratch=1 route2=budgeted_allow_dispatch_actual_jobs_logged worker_prototype=1 FPS_claim=0\n";
        for (const std::size_t count : {256u, 512u, 867u, 1158u, 2048u, 8192u})
        for (const unsigned families : {1u, 8u, 24u})
        for (unsigned mode = 0; mode < 3; ++mode)
        for (bool query_split : {false, true})
        {
            SegmentPlan plan(fixture, count, families, query_split); std::array<std::vector<double>, 3> samples;
            for (unsigned block = 0; block < 9; ++block)
            for (unsigned repetition = 0; repetition < 3; ++repetition)
            {
                std::array<std::array<std::size_t, 3>, 3> results{};
                for (unsigned arm = 0; arm < 3; ++arm)
                {
                    const unsigned route = (arm + block + repetition) % 3;
                    ALNativeFrameBudget budget; ALNativeFrameBudget::FrameInputs frame; frame.frame = 1; budget.beginFrame(frame);
                    const auto started = Clock::now(); std::size_t jobs = 0, dispatches = 0; double first_view_ms = 0;
                    @autoreleasepool
                    {
                        SegmentResult result;
                        for (unsigned view = 0; view < 3; ++view)
                        {
                            if (!segmentPrepare(plan, route, mode, block * 3 + repetition + 2, budget, result, view))
                                throw std::runtime_error("Incomplete shadow segment timing arm");
                            results[route] = {result.admission.sourceCount(), result.batches.size(), result.query_offset};
                            jobs += result.jobs; dispatches += result.dispatches;
                            if (!view) first_view_ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
                        }
                    }
                    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
                    samples[route].push_back(elapsed);
                    std::cout << "SHADOW_SEGMENT_SAMPLE draws=" << count << " families=" << families << " mode=" << mode << " query_split=" << query_split
                        << " block=" << block << " repetition=" << repetition << " route=" << route
                        << " jobs=" << jobs << " dispatches=" << dispatches
                        << " logical_sources=" << results[route][0] << " physical_records=" << results[route][1]
                        << " query_record_offset=" << results[route][2] << " first_view_ms=" << first_view_ms << " cpu_total_ms=" << elapsed << '\n';
                }
                REQUIRE(results[0] == results[1]); REQUIRE(results[0] == results[2]);
            }
            for (unsigned route = 0; route < 3; ++route)
            {
                auto values = samples[route]; std::sort(values.begin(), values.end()); REQUIRE(values.size() == 27);
                std::cout << "SHADOW_SEGMENT_SUMMARY draws=" << count << " families=" << families << " mode=" << mode << " query_split=" << query_split
                    << " route=" << route << " samples=" << values.size() << " p50_ms=" << values[13]
                    << " p95_ms=" << values[25] << '\n';
            }
        }
    }
}
