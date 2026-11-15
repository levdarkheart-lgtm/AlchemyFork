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
#include "almetal_world_prepare_fixture.h"

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

namespace
{
struct PaletteQueueGate
{
    id<MTLSharedEvent> event;
    std::unique_ptr<ALMetalCommandBuffer> blocker;
    explicit PaletteQueueGate(Fixture& f)
    {
        event=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(event);
        blocker=f.device->makeCommandBuffer("Attachment pending GPU gate",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(blocker->commit(&f.error));
    }
    ~PaletteQueueGate(){event.signaledValue=1;}
    void release(Fixture& f){event.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));}
};
std::unique_ptr<ALMetalRenderTarget> paletteLighting(Fixture& f)
{
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width=descriptor.height=side;
    descriptor.colors[0].format=ALMetalPixelFormat::rgba16_float;
    descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
    auto target=ALMetalRenderTarget::create(*f.device,descriptor,&f.error);REQUIRE(target);
    ALMetalRenderPassConfig clear;clear.colors[0].clear={.03125,.0625,.125,.5};
    clear.depth_store=clear.stencil_store=ALMetalStoreAction::store;
    auto pass=target->makeRenderPass(clear,&f.error);REQUIRE(pass);
    auto command=f.device->makeCommandBuffer("Attachment forward source clear",&f.error);REQUIRE(command);
    auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Attachment forward source",&f.error);REQUIRE(encoder);
    REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));
    return target;
}
std::vector<std::uint8_t> paletteColor(Fixture& f,const ALMetalRenderTarget& target)
{
    const auto* texture=target.colorTexture(0);REQUIRE(texture);
    const auto bpp=texture->descriptor().format==ALMetalPixelFormat::rgba16_float?8u:4u;
    std::vector<std::uint8_t> pixels(side*side*bpp);
    REQUIRE(texture->readback(*f.device,0,0,{},pixels.data(),side*bpp,&f.error));
    return pixels;
}
}

TEST_CASE("almetal: attachment identity is shared by main probe prepared and shadow consumers",
          "[almetal][attachment-palette][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;
        auto library=f.data->builtinLibrary(&f.error);REQUIRE(library);
        auto probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);INFO(f.error);REQUIRE(probe);
        probe->setFrameData(f.data);
        std::uint64_t serial=2;
        for(auto joints:{1u,110u}) for(bool reverse:{false,true})
        {
            CAPTURE(joints,reverse);
            auto values=attachmentValues(joints,.125f);
            auto record=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),joints,&f.error);REQUIRE(record);
            auto batch=f.batch(4);batch.skin_palette=values.data();batch.skin_palette_float_count=values.size();batch.skin_joint_count=joints;
            auto scene=f.scene();scene.reverse_z=reverse;scene.batches=&batch;scene.batch_count=1;
            auto raw=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(raw);
            const auto expected=f.read(*raw);
            CHECK(std::any_of(expected.depth.begin(),expected.depth.end(),[&](auto x){return x!=(reverse?0u:0x3f800000u);}));
            f.data->beginFrame(serial++);
            const auto before=f.data->stats();
            batch.skin_palette_record=record.get();batch.skin_palette=record->data();
            auto main=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(main);CHECK(f.read(*main)==expected);
            auto ordinary_probe=probe->render(scene,&f.error);INFO(f.error);REQUIRE(ordinary_probe);CHECK(f.read(*ordinary_probe)==expected);
            auto ready=f.prepare(scene,{batch});
            auto prepared=f.renderer->renderPrepared(*ready,&f.error);INFO(f.error);REQUIRE(prepared);CHECK(f.read(*prepared)==expected);
            ALMetalTextureDescriptor descriptor;descriptor.type=ALMetalTextureType::texture_2d_array;
            descriptor.format=ALMetalPixelFormat::r32_float;descriptor.width=descriptor.height=side;descriptor.array_length=2;
            descriptor.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
            auto depth=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(depth);
            ALMetalWorldSpotShadowBatch shadow;shadow.vertices=batch.vertices;shadow.indices=batch.indices;
            shadow.vertex_count=batch.vertex_count;shadow.index_count=batch.index_count;shadow.weight_offset=batch.weight_offset;
            shadow.skin_palette_record=record;shadow.skin_palette=record->data();shadow.skin_palette_float_count=record->size();shadow.skin_joint_count=joints;
            identity(shadow.modelview_projection);
            ALMetalWorldSpotShadowScene shadow_scene;shadow_scene.width=shadow_scene.height=side;
            shadow_scene.reverse_z=reverse;shadow_scene.batches=&shadow;shadow_scene.batch_count=1;
            REQUIRE(f.renderer->renderSpotShadow(*depth,0,shadow_scene,&f.error));
            CHECK(f.data->stats().record_publications-before.record_publications==1);
            CHECK(f.data->stats().record_reuses-before.record_reuses>=3);
        }
    }
}

TEST_CASE("almetal: attachment records preserve forward alpha glow and impostor output",
          "[almetal][attachment-palette]")
{
    @autoreleasepool
    {
        Fixture f;
        auto record=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);REQUIRE(record);
        ALMetalWorldAlphaEnvironment environment;
        environment.water_plane[2]=1;environment.water_plane[3]=-1;
        for(unsigned i=0;i<3;++i)environment.eye_to_sky[i][i]=1;
        environment.light_direction[2]=1;environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1;
        environment.glow_max_y[0]=1;environment.glow_max_y[2]=-1;environment.glow_max_y[3]=1;
        environment.water_fog[0]=1;environment.water_fog[2]=-1;
        for(unsigned mode=0;mode<4;++mode)
        {
            CAPTURE(mode);
            auto batch=f.batch(mode==2?2:4);
            batch.weight_offset=offsetof(Fixture::Geometry,weights);
            batch.skin_palette=f.attachment_pose.data();batch.skin_palette_float_count=f.attachment_pose.size();batch.skin_joint_count=1;
            batch.emissive_offset=offsetof(Fixture::Geometry,colors);
            if(mode==0)batch.impostor_fullbright=true;
            if(mode==1)batch.legacy_glow=true;
            if(mode==2){batch.pbr_glow=true;batch.pbrSlots()[0].emissive_color[1]=.5f;}
            if(mode==3){batch.pre_water_alpha=true;batch.pre_water_fullbright=true;}
            auto base_batch=f.batch(4);auto base_scene=f.scene();base_scene.batches=&base_batch;base_scene.batch_count=1;
            auto gbuffer=f.renderer->render(base_scene,&f.error);INFO(f.error);REQUIRE(gbuffer);
            auto composed=paletteLighting(f);
            ALMetalWorldOpaquePostDeferredScene scene;scene.width=scene.height=side;
            scene.gbuffer=gbuffer.get();scene.composed_lighting=composed.get();scene.batches=&batch;scene.batch_count=1;
            scene.alpha_environment=&environment;
            auto raw=f.renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(raw);
            const auto expected=paletteColor(f,*raw);
            CHECK(expected!=paletteColor(f,*composed));
            batch.skin_palette_record=record.get();batch.skin_palette=record->data();
            const auto before=f.data->stats();
            auto certified=f.renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(certified);
            CHECK(paletteColor(f,*certified)==expected);
            CHECK(f.data->stats().record_publications+f.data->stats().record_reuses==before.record_publications+before.record_reuses+1);
            auto wrong=batch;wrong.skin_palette=f.attachment_pose.data();scene.batches=&wrong;
            CHECK_FALSE(f.renderer->renderOpaquePostDeferred(scene,&f.error));
            // Water/haze marker early-outs cannot accept an unrelated record.
            wrong=ALMetalWorldSimpleBatch{};wrong.water_haze=true;wrong.skin_palette_record=record.get();
            CHECK_FALSE(f.renderer->renderOpaquePostDeferred(scene,&f.error));
        }
        auto batch=f.batch(4);batch.impostor_fullbright=true;
        auto scene=f.scene();scene.batches=&batch;scene.batch_count=1;
        auto raw=f.renderer->render(f.scene(),&f.error);auto certified=f.renderer->render(f.scene(),&f.error);REQUIRE(raw);REQUIRE(certified);
        const auto clear=paletteColor(f,*raw);
        REQUIRE(f.renderer->compositeImpostorPostDeferred(*raw,scene,&f.error));
        const auto expected=paletteColor(f,*raw);CHECK(expected!=clear);
        batch.skin_palette_record=record.get();batch.skin_palette=record->data();
        const auto before=f.data->stats();
        REQUIRE(f.renderer->compositeImpostorPostDeferred(*certified,scene,&f.error));
        CHECK(paletteColor(f,*certified)==expected);
        CHECK(f.data->stats().record_reuses==before.record_reuses+1);
        batch.skin_joint_count=2;
        CHECK_FALSE(f.renderer->compositeImpostorPostDeferred(*certified,scene,&f.error));
    }
}

TEST_CASE("almetal: queued world draws keep native palette bytes without retaining CPU records",
          "[almetal][attachment-palette][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;
        auto batch=f.batch(4);auto scene=f.scene();scene.batches=&batch;scene.batch_count=1;
        auto raw=f.renderer->render(scene,&f.error);REQUIRE(raw);const auto expected=f.read(*raw);
        auto record=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);REQUIRE(record);
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak=record;
        batch.skin_palette_record=record.get();batch.skin_palette=record->data();
        auto inputs=ALMetalWorldViewInputs::adopt(scene,{batch},{},f.resources,1,&f.error);REQUIRE(inputs);
        auto ticket=ALMetalWorldPreparation::create(inputs,1,{},&f.error);REQUIRE(ticket);REQUIRE(ticket->runJob(0));
        auto ready=ticket->finish(&f.error);REQUIRE(ready);
        // Renderer construction synchronously uploads fallback textures on
        // this queue; complete it before installing the actual GPU wait.
        auto library=f.data->builtinLibrary(&f.error);REQUIRE(library);
        auto probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);REQUIRE(probe);probe->setFrameData(f.data);
        PaletteQueueGate gate(f);
        REQUIRE(f.renderer->beginCommandBatch("Attachment native-only pending reader",&f.error));
        auto old=f.renderer->renderPrepared(*ready,&f.error);INFO(f.error);REQUIRE(old);
        auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(completion->isComplete());
        // Clear all CPU packet/cache owners while this real submitted command
        // is GPU-gated. Only its encoded native arena bytes are needed.
        ready.reset();ticket.reset();inputs.reset();record.reset();f.data->beginFrame(2);
        CHECK(weak.expired());
        f.attachment_pose[0]=.5f;
        auto changed=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);REQUIRE(changed);
        batch.skin_palette_record=changed.get();batch.skin_palette=changed->data();
        auto current=probe->render(scene,&f.error);INFO(f.error);REQUIRE(current);
        CHECK_FALSE(completion->isComplete());
        gate.release(f);
        CHECK_FALSE(f.read(*current)==expected);
        CHECK(completion->isComplete());CHECK(f.read(*old)==expected);
        // A canceled encoder likewise leaves no live CPU palette or pending
        // arena credit once the frame owner drops its identity cache.
        REQUIRE(f.renderer->beginCommandBatch("Attachment canceled palette reader",&f.error));
        auto canceled=f.renderer->render(scene,&f.error);REQUIRE(canceled);
        completion=f.renderer->commandBatchCompletion();REQUIRE(completion);
        f.renderer->cancelCommandBatch();CHECK(completion->isComplete());
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> changed_weak=changed;
        changed.reset();f.data->beginFrame(3);CHECK(changed_weak.expired());
    }
}

TEST_CASE("almetal: world record interning pins the earlier deduplicated arena page",
          "[almetal][attachment-palette][PPD-251]")
{
    @autoreleasepool
    {
        Fixture f;auto arena=f.data;arena->beginFrame(10);
        auto values=attachmentValues(110,.125f);
        const auto original_values=values;
        auto a=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);
        auto b=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);
        REQUIRE(a);REQUIRE(b);REQUIRE(a!=b);
        const auto bytes=values.size()*sizeof(float);
        ALMetalBufferDescriptor descriptor;descriptor.length=bytes*2;
        auto output=ALMetalBuffer::create(*f.device,descriptor,&f.error);REQUIRE(output);
        std::vector<std::uint8_t> pressure(1024*1024,0xa5);
        // Complete all synchronous fixture/resource initialization before the
        // gate. Only queued publications/copies occur until release below.
        PaletteQueueGate gate(f);
        auto first=f.device->makeCommandBuffer("World palette first page and pressure",&f.error);REQUIRE(first);
        ALMetalWorldDataSlice original,second_page,alias,identity_hit;
        REQUIRE(arena->publishPaletteRecord(*first,a.get(),original,&f.error,true));
        REQUIRE(arena->publish(*first,pressure.data(),pressure.size(),second_page,&f.error,false));
        REQUIRE(second_page.buffer!=original.buffer);CHECK(arena->stats().pages_created==2);
        auto abandoned=f.device->makeCommandBuffer("World palette equal second identity",&f.error);REQUIRE(abandoned);
        const auto published=arena->stats().published_bytes;
        REQUIRE(arena->publishPaletteRecord(*abandoned,b.get(),alias,&f.error,true));
        CHECK(alias.buffer==original.buffer);CHECK(alias.offset==original.offset);CHECK(alias.length==bytes);
        CHECK(arena->stats().published_bytes==published);
        CHECK(arena->stats().record_publications==2);
        auto old_reader=f.device->makeCommandBuffer("World palette cached earlier-page reader",&f.error);REQUIRE(old_reader);
        // Default false on an EXISTING identity still takes the O(1) record
        // hit. It must not replace the first-page range with active.back().
        REQUIRE(arena->publishPaletteRecord(*old_reader,b.get(),identity_hit,&f.error));
        CHECK(identity_hit.buffer==original.buffer);CHECK(identity_hit.offset==original.offset);
        CHECK(arena->stats().record_reuses==1);CHECK(arena->stats().published_bytes==published);
        REQUIRE(output->copyFrom(*old_reader,*identity_hit.buffer,identity_hit.offset,0,bytes,&f.error));
        auto old_completion=old_reader->completionToken();REQUIRE(old_completion);
        REQUIRE(old_reader->commit(&f.error));CHECK_FALSE(old_completion->isComplete());
        // Both earlier commands are abandoned. Only the cached identity hit
        // now protects page one, while page two becomes reusable next frame.
        first.reset();abandoned.reset();
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak_a=a,weak_b=b;
        a.reset();b.reset();arena->beginFrame(11);
        CHECK(weak_a.expired());CHECK(weak_b.expired());CHECK(arena->stats().retired_pages==1);
        values[0]=-.25f;
        auto changed=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);REQUIRE(changed);
        auto next=f.device->makeCommandBuffer("World palette new frame changed pose",&f.error);REQUIRE(next);
        ALMetalWorldDataSlice current;
        REQUIRE(arena->publishPaletteRecord(*next,changed.get(),current,&f.error,true));
        CHECK(current.buffer==second_page.buffer);CHECK(current.buffer!=original.buffer);
        CHECK(arena->stats().pages_created==2);
        REQUIRE(output->copyFrom(*next,*current.buffer,current.offset,bytes,bytes,&f.error));
        auto next_completion=next->completionToken();REQUIRE(next_completion);REQUIRE(next->commit(&f.error));
        auto cancel=f.device->makeCommandBuffer("World palette abandoned new-frame identity hit",&f.error);REQUIRE(cancel);
        ALMetalWorldDataSlice canceled;
        REQUIRE(arena->publishPaletteRecord(*cancel,changed.get(),canceled,&f.error,true));
        auto canceled_completion=cancel->completionToken();REQUIRE(canceled_completion);cancel.reset();CHECK(canceled_completion->isComplete());
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak_changed=changed;
        changed.reset();arena->beginFrame(12);
        CHECK(weak_changed.expired());CHECK(arena->stats().retired_pages==2);
        CHECK_FALSE(old_completion->isComplete());CHECK_FALSE(next_completion->isComplete());
        gate.release(f);
        CHECK(old_completion->isComplete());CHECK(next_completion->isComplete());
        std::vector<float> actual(values.size()*2);
        REQUIRE(output->read(0,actual.data(),bytes*2,&f.error));
        CHECK(std::equal(original_values.begin(),original_values.end(),actual.begin()));
        CHECK(std::equal(values.begin(),values.end(),actual.begin()+values.size()));
        arena->beginFrame(13);CHECK(arena->stats().retired_pages==0);
        auto recycled=f.device->makeCommandBuffer("World palette pages recover after completion",&f.error);REQUIRE(recycled);
        auto following=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);REQUIRE(following);
        REQUIRE(arena->publishPaletteRecord(*recycled,following.get(),current,&f.error,true));
        CHECK(arena->stats().pages_created==2);CHECK(arena->stats().pages_reused==2);
    }
}

TEST_CASE("almetal: world draws intern distinct equal palettes without changing native output",
          "[almetal][attachment-palette][PPD-251]")
{
    @autoreleasepool
    {
        Fixture f;auto values=attachmentValues(110,.125f);
        auto a=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);
        auto b=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),110,&f.error);REQUIRE(a);REQUIRE(b);
        std::array<Pixels,2> pixels;std::array<std::uint64_t,2> published{};
        for(unsigned distinct=0;distinct<2;++distinct)
        {
            f.data->beginFrame(distinct+2);const auto before=f.data->stats();
            std::array<ALMetalWorldSimpleBatch,2> batches{f.batch(4),f.batch(4)};
            for(unsigned draw=0;draw<2;++draw)
            {
                const auto& record=distinct&&draw?b:a;
                batches[draw].skin_palette_record=record.get();batches[draw].skin_palette=record->data();
                batches[draw].skin_palette_float_count=record->size();batches[draw].skin_joint_count=record->jointCount();
            }
            auto scene=f.scene();scene.batches=batches.data();scene.batch_count=batches.size();
            auto target=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);
            pixels[distinct]=f.read(*target);published[distinct]=f.data->stats().published_bytes-before.published_bytes;
            CHECK(f.data->stats().record_publications-before.record_publications==(distinct?2u:1u));
        }
        CHECK(pixels[0]==pixels[1]);CHECK(published[0]==published[1]);
        CHECK(std::any_of(pixels[1].depth.begin(),pixels[1].depth.end(),[](auto value){return value!=0x3f800000u;}));
        // The existing default shadow policy remains covered by the earlier
        // test requiring equal-byte new identities to occupy distinct ranges.
    }
}

#include "almetal_world_body_palette_native.inl"
