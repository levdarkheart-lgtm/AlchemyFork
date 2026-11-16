/**
 * @file almetal_shadow_recipe_test.mm
 * @brief CPU-only shadow recipes restore exact weak native generations.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowprepare.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

namespace
{
using Batch = ALMetalWorldSpotShadowBatch;
using Record = ALMetalShadowSourceRecord;
using Recipe = ALMetalShadowSourceRecipe;

struct RecipeFixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    std::shared_ptr<const ALMetalTexture> texture;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
    RecipeFixture()
    {
        device = ALMetalDevice::create(&error); REQUIRE(device);
        ALMetalBufferDescriptor descriptor; descriptor.length = 144;
        vertices = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(vertices);
        descriptor.length = 6;
        indices = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(indices);
        ALMetalTextureDescriptor image; image.width = image.height = 2;
        image.format = ALMetalPixelFormat::rgba8_unorm;
        texture = ALMetalTexture::create(*device, image, &error); REQUIRE(texture);
        std::array<float, 16> values{};
        values[4] = values[9] = values[14] = 1.f;
        pose = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &error);
        REQUIRE(pose);
    }
    Batch prototype(bool covered = true, bool rigged = false) const
    {
        Batch value;
        value.vertices = vertices.get(); value.indices = indices.get();
        value.vertex_count = value.index_count = 3;
        value.texcoord_offset = 96; value.color_offset = 120;
        if (covered)
        {
            value.coverage = ALMetalWorldSpotShadowCoverage::pbr_mask;
            value.texture_count = 1;
            value.coverageData().textures[0] = texture.get();
            value.coverageData().alpha_cutoffs[0] = .375f;
        }
        if (rigged)
        {
            value.weight_offset = 48;
            value.skin_palette_record = pose;
            value.skin_palette = pose->data();
            value.skin_palette_float_count = pose->size();
            value.skin_joint_count = pose->jointCount();
        }
        return value;
    }
    std::shared_ptr<const Record> source(const Batch& prototype)
    {
        std::vector<std::shared_ptr<const ALMetalTexture>> images;
        if (prototype.texture_count) images.push_back(texture);
        return Record::create(7, prototype, vertices, indices, std::move(images), &error);
    }
};
}

TEST_CASE("almetal: shadow recipe survives record retirement without native ownership",
          "[almetal][shadow-recipe]")
{
    @autoreleasepool
    {
        RecipeFixture f;
        auto prototype = f.prototype();
        auto& mutable_coverage = prototype.coverageData();
        auto source = f.source(prototype); INFO(f.error); REQUIRE(source);
        const auto vertices_count = f.vertices.use_count(), texture_count = f.texture.use_count();
        auto recipe = Recipe::create(*source, &f.error); REQUIRE(recipe);
        CHECK(f.vertices.use_count() == vertices_count);
        CHECK(f.texture.use_count() == texture_count);
        CHECK(recipe->vertexBytes() == 144);
        CHECK(recipe->indexBytes() == 6);
        CHECK(recipe->textureBytes(0) == 16);
        CHECK(recipe->textureBytes(1) == 0);
        CHECK(recipe->ownedBytes() >= sizeof(Recipe) + sizeof(Batch::CoverageData));
        std::weak_ptr<const Record> old_source = source;
        std::weak_ptr<const ALMetalBuffer> old_vertices = f.vertices;
        std::weak_ptr<const ALMetalTexture> old_texture = f.texture;
        source.reset(); CHECK(old_source.expired());
        mutable_coverage.alpha_cutoffs[0] = .875f;
        auto restored = recipe->restore(8, &f.error); INFO(f.error); REQUIRE(restored);
        CHECK(restored->generation() == 8);
        CHECK(restored->vertexLease() == f.vertices);
        CHECK(restored->indexLease() == f.indices);
        REQUIRE(restored->textureLeases().size() == 1);
        CHECK(restored->textureLeases()[0] == f.texture);
        CHECK(restored->prototype().coverageData().alpha_cutoffs[0] == .375f);
        CHECK_FALSE(restored->prototype().sourceRecord());
        CHECK_FALSE(restored->prototype().skinPaletteRecord());
        auto detached = restored->prototype();
        detached.coverageData().alpha_cutoffs[0] = .25f;
        CHECK(restored->prototype().coverageData().alpha_cutoffs[0] == .375f);
        f.vertices.reset(); f.indices.reset(); f.texture.reset(); prototype = {}; detached = {};
        CHECK_FALSE(old_vertices.expired()); CHECK_FALSE(old_texture.expired());
        restored.reset();
        CHECK(old_vertices.expired()); CHECK(old_texture.expired());
        CHECK_FALSE(recipe->restore(9, &f.error));
    }
}

TEST_CASE("almetal: shadow recipes cannot substitute expired native generations",
          "[almetal][shadow-recipe]")
{
    @autoreleasepool
    {
        RecipeFixture f;
        auto source = f.source(f.prototype()); REQUIRE(source);
        auto recipe = Recipe::create(*source); REQUIRE(recipe);
        source.reset();
        SECTION("expired vertices") { f.vertices.reset(); }
        SECTION("expired indices") { f.indices.reset(); }
        SECTION("expired texture") { f.texture.reset(); }
        SECTION("same descriptors are different backing generations")
        {
            f.vertices.reset(); f.indices.reset(); f.texture.reset();
            RecipeFixture replacement;
            CHECK_FALSE(recipe->restore(8, &f.error));
            CHECK(replacement.vertices);
        }
        CHECK_FALSE(recipe->restore(9, &f.error));
        CHECK_FALSE(f.error.empty());
    }
}

TEST_CASE("almetal: shadow recipe rebind keeps current rigged pose validation",
          "[almetal][shadow-recipe]")
{
    @autoreleasepool
    {
        RecipeFixture f;
        auto source = f.source(f.prototype(true, true)); REQUIRE(source);
        auto recipe = Recipe::create(*source); REQUIRE(recipe);
        const std::weak_ptr<const ALMetalAttachmentPaletteRecord> old_pose = f.pose;
        source.reset(); f.pose.reset(); CHECK(old_pose.expired());
        CHECK(recipe->requiresPalette());
        CHECK_FALSE(recipe->restore(0)); CHECK_FALSE(recipe->restore(7));
        auto restored = recipe->restore(8); REQUIRE(restored);
        CHECK(restored->requiresPalette());
        CHECK_FALSE(restored->prototype().skin_palette);
        CHECK_FALSE(restored->prototype().skinPaletteRecord());
        ALMetalShadowPrepareInput input;
        input.source = restored; input.source_generation = 8; input.epoch = 5;
        std::array<Batch, 1> output;
        CHECK_FALSE(alMetalPrepareShadowBatches(std::span(&input, 1), output, 5));
        CHECK_FALSE(output[0].sourceRecord());
        std::array<float, 16> values{};
        values[4] = values[9] = values[14] = 1.f; values[0] = .25f;
        auto current_pose = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1);
        REQUIRE(current_pose); input.palette = current_pose;
        REQUIRE(alMetalPrepareShadowBatches(std::span(&input, 1), output, 5));
        CHECK(output[0].skinPaletteRecord() == current_pose.get());
        CHECK(output[0].skin_palette == current_pose->data());
        CHECK(restored->matchesStableInputs(output[0]));
        input.source_generation = 7;
        CHECK_FALSE(alMetalPrepareShadowBatches(std::span(&input, 1), output, 5));
        CHECK_FALSE(output[0].sourceRecord());
    }
}

TEST_CASE("almetal: shadow recipe keeps the certified backing identity across COW",
          "[almetal][shadow-recipe]")
{
    @autoreleasepool
    {
        RecipeFixture f;
        const auto old_generation = f.vertices;
        auto source = f.source(f.prototype(false)); REQUIRE(source);
        auto recipe = Recipe::create(*source); REQUIRE(recipe); source.reset();
        ALMetalBufferDescriptor descriptor; descriptor.length = old_generation->length();
        f.vertices = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(f.vertices);
        REQUIRE(f.vertices.get() != old_generation.get());
        auto rebound = recipe->restore(8); REQUIRE(rebound);
        CHECK(rebound->vertexLease() == old_generation);
        CHECK(rebound->vertexLease() != f.vertices);
        // A caller must reject an old producer revision before this rebind;
        // the native certificate never guesses a replacement generation.
        CHECK(rebound->prototype().vertices == old_generation.get());
    }
}

TEST_CASE("almetal: shadow recipe does not weaken initial producer certification",
          "[almetal][shadow-recipe]")
{
    @autoreleasepool
    {
        RecipeFixture f;
        auto prototype = f.prototype(false, true);
        prototype.skin_palette_record.reset();
        CHECK_FALSE(f.source(prototype));
        prototype = f.prototype(false);
        prototype.vertices = nullptr;
        CHECK_FALSE(f.source(prototype));
        auto source = f.source(f.prototype(false)); REQUIRE(source);
        auto recipe = Recipe::create(*source); REQUIRE(recipe);
        CHECK(recipe->textureBytes(0) == 0);
        auto restored = recipe->restore(8); REQUIRE(restored);
        CHECK(restored->textureLeases().empty());
        CHECK_FALSE(restored->requiresPalette());
    }
}
