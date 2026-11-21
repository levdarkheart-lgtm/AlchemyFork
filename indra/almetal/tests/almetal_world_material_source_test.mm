/**
 * @file almetal_world_material_source_test.mm
 * @brief Native pixels, publication cost and lifetime of sealed material data.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalworldmaterialdata.h"
#include <cmath>
#include <time.h>

namespace
{
ALMetalWorldSimpleBatch materialBatch(Fixture& f, unsigned family, bool indexed)
{
    auto batch = f.batch(family);
    batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
    if (!indexed) return batch;
    batch.texture_count = 2;
    batch.diffuse_textures[1] = f.color;
    if (family == 1)
    {
        batch.material_slot_count = 2;
        auto& slots = batch.materialSlots();
        slots[1] = slots[0];
        slots[1].env_intensity = .65f;
        slots[1].specular_color[1] = .2f;
    }
    else
    {
        batch.pbr_slot_count = 2;
        auto& slots = batch.pbrSlots();
        slots[1] = slots[0];
        slots[1].roughness_factor = .8f;
        slots[1].metallic_factor = .15f;
        slots[1].emissive_color[0] = .25f;
    }
    auto geometry = f.geometry;
    const std::int32_t texture_index = 1;
    for (auto& position : geometry.positions)
        std::memcpy(&position[3], &texture_index, sizeof(texture_index));
    batch.vertices = f.buffer(&geometry, sizeof(geometry));
    return batch;
}

std::shared_ptr<const ALMetalBuffer> tableLease(
    const ALMetalWorldMaterialSource& source, unsigned family, bool normal_hdr)
{
    return family == 1 ? source.materialLease() : source.pbrLease(normal_hdr);
}

struct MaterialRender
{
    Pixels pixels;
    std::uint64_t published_bytes = 0;
};

MaterialRender renderMaterial(Fixture& f, ALMetalWorldGBufferScene scene,
    const ALMetalWorldSimpleBatch& batch, bool owned_input, std::uint64_t frame)
{
    f.data->beginFrame(frame);
    scene.batches = &batch;
    scene.batch_count = 1;
    const auto before = f.data->stats().published_bytes;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (owned_input)
    {
        auto ready = f.adopt(scene, {batch});
        target = f.renderer->renderOwnedInputs(*ready, &f.error);
    }
    else target = f.renderer->render(scene, &f.error);
    INFO(f.error);
    REQUIRE(target);
    const auto published = f.data->stats().published_bytes - before;
    return {f.read(*target), published};
}

std::shared_ptr<const ALMetalWorldViewInputs> tryAdoptMaterial(
    Fixture& f, ALMetalWorldGBufferScene scene, const ALMetalWorldSimpleBatch& batch)
{
    auto inputs = ALMetalWorldViewInputs::adopt(scene, {batch}, {}, f.resources, 1, &f.error);
    if (!inputs) return {};

    if (!alMetalValidateWorldBatch(inputs->scene(), batch, &f.error)) return {};
    return inputs;
}

void waitForMaterialRelease(const std::weak_ptr<const ALMetalBuffer>& weak)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!weak.expired() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(weak.expired());
}
}

TEST_CASE("almetal: sealed material tables reuse native bytes across direct and owned_input views",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        for (bool indexed : {false, true})
        {
            CAPTURE(family, indexed);
            auto batch = materialBatch(f, family, indexed);
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            INFO(f.error);
            REQUIRE(source);
            REQUIRE(source->matches(batch));
            REQUIRE(batch.native_material_source == source);
            auto equivalent = batch;
            equivalent.native_material_source.reset();
            const auto before_reuse = cache.stats();
            auto reused = ALMetalWorldMaterialSource::publish(*f.device, equivalent, cache, &f.error);
            REQUIRE(reused);
            CHECK(cache.stats().publications == before_reuse.publications);
            CHECK(cache.stats().published_bytes == before_reuse.published_bytes);
            CHECK(cache.stats().hits > before_reuse.hits);
            for (bool normal_hdr : {false, true})
            {
                CAPTURE(normal_hdr);
                auto table = tableLease(*source, family, normal_hdr);
                REQUIRE(table);
                CHECK(table == tableLease(*reused, family, normal_hdr));
                REQUIRE(table->mappedBytes());
                const auto* first = static_cast<const std::byte*>(table->mappedBytes());
                const std::vector<std::byte> original_bytes(first, first + table->length());
                for (unsigned view = 0; view < 3; ++view)
                {
                    CAPTURE(view);
                    const bool owned_input = view != 0;
                    auto view_batch = batch;
                    view_batch.modelview_projection[12] = view == 2 ? .15f : 0.f;
                    auto scene = f.scene();
                    scene.hdr = true;
                    scene.normal_hdr = normal_hdr;
                    scene.reverse_z = view == 2;
                    REQUIRE(source->matches(view_batch));
                    auto manual = view_batch;
                    manual.native_material_source.reset();
                    const auto expected = renderMaterial(f, scene, manual, owned_input, ++frame);
                    const auto actual = renderMaterial(f, scene, view_batch, owned_input, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(std::any_of(actual.pixels.depth.begin(), actual.pixels.depth.end(),
                        [reverse = scene.reverse_z](auto value)
                        { return value != (reverse ? 0u : 0x3f800000u); }));
                    // This measures real FrameData publication, including the
                    // owned_input slab; a synthetic cache hit cannot satisfy it.
                    CHECK(expected.published_bytes == actual.published_bytes + table->length());
                    CHECK(cache.stats().publications == before_reuse.publications);
                    CHECK(cache.stats().published_bytes == before_reuse.published_bytes);
                    CHECK(std::memcmp(table->mappedBytes(), original_bytes.data(), table->length()) == 0);
                }
                REQUIRE(f.renderer->setAnisotropy(4, &f.error));
                CHECK(source->matches(batch));
                const auto sampler_before = cache.stats().published_bytes;
                auto scene = f.scene(); scene.normal_hdr = normal_hdr; scene.hdr = true;
                auto manual = batch; manual.native_material_source.reset();
                const auto sampled = renderMaterial(f, scene, batch, false, ++frame);
                const auto sampled_manual = renderMaterial(f, scene, manual, false, ++frame);
                CHECK(sampled.pixels == sampled_manual.pixels);
                CHECK(cache.stats().published_bytes == sampler_before);
            }
        }
    }
}

TEST_CASE("almetal: sealed material edits preserve pending native pixels and weak lease retirement",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        for (unsigned family : {1u, 2u})
        for (bool abandon : {false, true})
        {
            CAPTURE(family, abandon);
            Fixture f;
            ALMetalWorldMaterialDataCache cache(*f.device);
            auto batch = materialBatch(f, family, false);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderMaterial(f, scene, batch, false, 2).pixels;
            // These references remain in the producer's authored storage.
            // Successful sealing must not certify that mutable allocation.
            auto& old_material = batch.materialSlots();
            auto& old_pbr = batch.pbrSlots();
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            REQUIRE(source);
            auto old_buffer = tableLease(*source, family, scene.normal_hdr);
            REQUIRE(old_buffer);
            std::weak_ptr<const ALMetalBuffer> weak = old_buffer;
            const auto* bytes = static_cast<const std::byte*>(old_buffer->mappedBytes());
            REQUIRE(bytes);
            const std::vector<std::byte> original_bytes(bytes, bytes + old_buffer->length());
            old_material[0].env_intensity = .99f;
            old_pbr[0].roughness_factor = .05f;
            CHECK(source->matches(batch));
            CHECK(std::memcmp(old_buffer->mappedBytes(), original_bytes.data(), old_buffer->length()) == 0);

            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];
            REQUIRE(gate);
            struct ReleaseGate { id<MTLSharedEvent> value; ~ReleaseGate() { value.signaledValue = 1; } } release{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold old native material generation", &f.error);
                REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Material generation retirement", &f.error));
            scene.batches = &batch; scene.batch_count = 1;
            auto target = f.renderer->render(scene, &f.error);
            INFO(f.error); REQUIRE(target);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));

            auto changed = batch;
            if (family == 1) changed.materialSlots()[0].env_intensity = .8f;
            else changed.pbrSlots()[0].roughness_factor = .9f;
            CHECK_FALSE(source->matches(changed));
            CHECK_FALSE(changed.native_material_source);
            auto replacement = ALMetalWorldMaterialSource::publish(*f.device, changed, cache, &f.error);
            REQUIRE(replacement);
            CHECK(tableLease(*replacement, family, scene.normal_hdr) != old_buffer);
            CHECK(std::memcmp(old_buffer->mappedBytes(), original_bytes.data(), old_buffer->length()) == 0);
            batch.native_material_source.reset(); source.reset(); old_buffer.reset();
            if (!abandon)
            {
                cache.clear();
                CHECK(cache.stats().entries == 0);
            }
            else CHECK(cache.stats().entries > 0);
            CHECK_FALSE(weak.expired());
            if (abandon)
            {
                f.renderer->cancelCommandBatch();
                waitForMaterialRelease(weak);
            }
            else
            {
                CHECK_FALSE(completion->isComplete());
                gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error));
                CHECK(f.read(*target) == expected);
                waitForMaterialRelease(weak);
                auto edited_scene = f.scene(); edited_scene.hdr = true;
                const auto edited = renderMaterial(f, edited_scene, changed, true, 3).pixels;
                CHECK_FALSE(edited == expected);
                auto manual = changed; manual.native_material_source.reset();
                CHECK(renderMaterial(f, edited_scene, manual, true, 4).pixels == edited);
            }
        }
    }
}

TEST_CASE("almetal: stale material certificates preserve native fallback and malformed rejection",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame = 10;
        for (unsigned family : {1u, 2u})
        {
            CAPTURE(family);
            auto batch = materialBatch(f, family, false);
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            REQUIRE(source);
            if (family == 1)
            {
                // Forward/impostor BLEND prototypes share this factory. Their
                // valid stable table does not make BLEND a G-buffer family.
                auto blend = batch; blend.material_alpha_mode = 1;
                auto blend_source = ALMetalWorldMaterialSource::publish(*f.device, blend, cache, &f.error);
                REQUIRE(blend_source); REQUIRE(blend_source->matches(blend));
                auto scene = f.scene(); scene.batches = &blend; scene.batch_count = 1;
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryAdoptMaterial(f, scene, blend));
                blend.material_alpha_mode = 4;
                const auto before = cache.stats().published_bytes;
                CHECK_FALSE(ALMetalWorldMaterialSource::publish(*f.device, blend, cache, &f.error));
                CHECK(blend.native_material_source == blend_source);
                CHECK(cache.stats().published_bytes == before);
            }
            for (unsigned edit = 0; edit < 3; ++edit)
            {
                CAPTURE(edit);
                auto stale = batch;
                if (family == 1)
                {
                    if (edit == 0) stale.materialSlots()[0].specular_color[0] = .9f;
                    if (edit == 1)
                    {
                        stale.materialSlots()[0].normal_texture = nullptr;
                        stale.materialSlots()[0].specular_texture = nullptr;
                    }
                    if (edit == 2) stale.material_alpha_mode = 2;
                }
                else
                {
                    if (edit == 0) stale.pbrSlots()[0].emissive_color[0] = .7f;
                    if (edit == 1) stale.pbrSlots()[0].texture_transform[0][2] = .5f;
                    if (edit == 2) stale.alpha_mask = true;
                }
                stale.native_material_source = source;
                CHECK_FALSE(source->matches(stale));
                auto manual = stale; manual.native_material_source.reset();
                for (bool owned_input : {false, true})
                {
                    auto scene = f.scene(); scene.hdr = true;
                    const auto expected = renderMaterial(f, scene, manual, owned_input, ++frame);
                    const auto actual = renderMaterial(f, scene, stale, owned_input, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(actual.published_bytes == expected.published_bytes);
                }
            }
            for (unsigned defect = 0; defect < 6; ++defect)
            {
                CAPTURE(defect);
                auto invalid = batch;
                if (defect == 0) invalid.texture_count = 2;
                if (defect == 1)
                {
                    if (family == 1) invalid.material_slot_count = 9;
                    else invalid.pbr_slot_count = 9;
                }
                if (defect == 2)
                {
                    if (family == 1) invalid.material_alpha_mode = 1;
                    else invalid.pbrSlots()[0].roughness_factor = 1.1f;
                }
                if (defect == 3)
                {
                    if (family == 1)
                    {
                        invalid.texture_count = invalid.material_slot_count = 2;
                        invalid.diffuse_textures[1] = f.color;
                        invalid.materialSlots()[1] = invalid.materialSlots()[0];
                        invalid.materialSlots()[1].normal_texture = nullptr;
                    }
                    else invalid.pbrSlots()[0].normal_texture = nullptr;
                }
                if (defect == 4)
                {
                    if (family == 1) invalid.materialSlots()[0].env_intensity = std::numeric_limits<float>::infinity();
                    else invalid.pbrSlots()[0].alpha_cutoff = std::numeric_limits<float>::quiet_NaN();
                }
                if (defect == 5) invalid.vertices = nullptr;
                invalid.native_material_source = source;
                // A material certificate does not certify dynamic geometry.
                // Even a valid certificate must still run normal preflight.
                if (defect == 5) CHECK(source->matches(invalid));
                else CHECK_FALSE(source->matches(invalid));
                auto scene = f.scene(); scene.batches = &invalid; scene.batch_count = 1;
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryAdoptMaterial(f, scene, invalid));
                invalid.native_material_source.reset();
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryAdoptMaterial(f, scene, invalid));
            }
        }
    }
}

TEST_CASE("almetal: sealed table copies and moves preserve source aliases and native values",
          "[almetal][world-material-source][world-material-storage]")
{
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<ALMetalWorldSimpleBatch>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<ALMetalWorldSimpleBatch>);
    STATIC_REQUIRE_FALSE(std::is_nothrow_copy_constructible_v<ALMetalWorldSimpleBatch>);
    STATIC_REQUIRE_FALSE(std::is_nothrow_copy_assignable_v<ALMetalWorldSimpleBatch>);
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame=700;
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);
            auto original=materialBatch(f,family,true);
            original.modelview_projection[12]=.1f;
            original.texture_matrix[12]=.125f;
            ALMetalWorldMaterialSlot* material_alias=family==1?original.materialSlots().data():nullptr;
            ALMetalWorldPBRSlot* pbr_alias=family==2?original.pbrSlots().data():nullptr;
            auto source=ALMetalWorldMaterialSource::publish(*f.device,original,cache,&f.error);
            INFO(f.error);REQUIRE(source);REQUIRE(source->matches(original));
            const auto scene=f.scene();
            const auto expected=renderMaterial(f,scene,original,false,++frame).pixels;
            ALMetalWorldSimpleBatch copied(original);
            auto assigned=materialBatch(f,family,false);
            assigned=original;
            auto move_input=copied;
            ALMetalWorldSimpleBatch moved(std::move(move_input));
            auto move_assigned=materialBatch(f,family,false);
            move_assigned=std::move(assigned);
            copied.operator=(copied);
            copied.operator=(std::move(copied));

            // These aliases still refer to the ORIGINAL authored array. No
            // copied certificate may expose it, including after move/assign.
            if(material_alias)material_alias[0].env_intensity=material_alias[1].env_intensity=std::numeric_limits<float>::quiet_NaN();
            if(pbr_alias)pbr_alias[0].roughness_factor=pbr_alias[1].roughness_factor=std::numeric_limits<float>::quiet_NaN();
            for(const auto* batch:{&original,&copied,&moved,&move_assigned})
            {
                REQUIRE(source->matches(*batch));
                CHECK(batch->native_material_source==source);
                CHECK(batch->modelview_projection[12]==.1f);
                CHECK(batch->texture_matrix[12]==.125f);
                if(family==1)CHECK(&batch->materialSlots()==&std::as_const(original).materialSlots());
                else CHECK(&batch->pbrSlots()==&std::as_const(original).pbrSlots());
                CHECK(renderMaterial(f,scene,*batch,false,++frame).pixels==expected);
                CHECK(renderMaterial(f,scene,*batch,true,++frame).pixels==expected);
            }

            // First mutable access materializes a private finite snapshot;
            // editing the copied value detaches only its own certificate.
            if(family==1)copied.materialSlots()[1].env_intensity=.05f;
            else copied.pbrSlots()[1].roughness_factor=.05f;
            CHECK_FALSE(copied.native_material_source);
            CHECK_FALSE(source->matches(copied));
            CHECK(source->matches(original));CHECK(source->matches(moved));
            const auto edited=renderMaterial(f,scene,copied,false,++frame).pixels;
            CHECK_FALSE(edited==expected);
            CHECK(renderMaterial(f,scene,copied,true,++frame).pixels==edited);
            CHECK(renderMaterial(f,scene,original,true,++frame).pixels==expected);

            // Moving the original producer also leaves its old authored
            // aliases alive; only the destination retains the sealed proof.
            ALMetalWorldSimpleBatch moved_original(std::move(original));
            CHECK(source->matches(moved_original));
            if(material_alias)CHECK(original.materialSlots().data()==material_alias);
            if(pbr_alias)CHECK(original.pbrSlots().data()==pbr_alias);
            CHECK(renderMaterial(f,scene,moved_original,true,++frame).pixels==expected);

            // Detaching the original producer reuses its authored allocation:
            // old mutable references stay valid, but cannot reach the sealed
            // copy or its immutable native constant bytes.
            auto producer=materialBatch(f,family,true);
            producer.modelview_projection[12]=.1f;producer.texture_matrix[12]=.125f;
            auto* held_material=family==1?producer.materialSlots().data():nullptr;
            auto* held_pbr=family==2?producer.pbrSlots().data():nullptr;
            auto sealed=ALMetalWorldMaterialSource::publish(*f.device,producer,cache,&f.error);REQUIRE(sealed);
            const auto unchanged=producer;
            if(held_material)
            {
                held_material[0].env_intensity=std::numeric_limits<float>::quiet_NaN();
                auto& writable=producer.materialSlots();CHECK(writable.data()==held_material);
                CHECK(std::isfinite(writable[0].env_intensity));writable[0].env_intensity=.13f;
            }
            if(held_pbr)
            {
                held_pbr[0].roughness_factor=std::numeric_limits<float>::quiet_NaN();
                auto& writable=producer.pbrSlots();CHECK(writable.data()==held_pbr);
                CHECK(std::isfinite(writable[0].roughness_factor));writable[0].roughness_factor=.13f;
            }
            CHECK_FALSE(producer.native_material_source);CHECK_FALSE(sealed->matches(producer));
            CHECK(sealed->matches(unchanged));
            CHECK(renderMaterial(f,scene,unchanged,false,++frame).pixels==expected);
        }
    }
}

TEST_CASE("almetal: manual table copies and moves retain defaults and independent values",
          "[almetal][world-material-source][world-material-storage]")
{
    ALMetalWorldSimpleBatch defaults;
    CHECK(std::as_const(defaults).materialSlots()[7].specular_color[0]==1.f);
    CHECK(std::as_const(defaults).materialSlots()[7].alpha_cutoff==.5f);
    CHECK(std::as_const(defaults).pbrSlots()[7].roughness_factor==1.f);
    CHECK(std::as_const(defaults).pbrSlots()[7].texture_transform[3][1]==1.f);
    const auto* default_material=&std::as_const(defaults).materialSlots();
    const auto* default_pbr=&std::as_const(defaults).pbrSlots();
    auto empty_copy=defaults;
    CHECK(&std::as_const(empty_copy).materialSlots()==default_material);
    CHECK(&std::as_const(empty_copy).pbrSlots()==default_pbr);
    ALMetalWorldSimpleBatch original;
    auto& material_alias=original.materialSlots();
    auto& pbr_alias=original.pbrSlots();
    material_alias[7].env_intensity=.37f;pbr_alias[7].roughness_factor=.63f;
    original.modelview_projection[12]=.25f;
    ALMetalWorldSimpleBatch copied(original),assigned;
    CHECK(&std::as_const(copied).materialSlots()!=&material_alias);
    CHECK(&std::as_const(copied).pbrSlots()!=&pbr_alias);
    assigned.materialSlots()[7].env_intensity=.99f;
    assigned.pbrSlots()[7].roughness_factor=.01f;
    assigned=original;
    material_alias[7].env_intensity=.11f;pbr_alias[7].roughness_factor=.22f;
    ALMetalWorldSimpleBatch moved(std::move(copied)),move_assigned;
    move_assigned.materialSlots()[7].env_intensity=.88f;
    move_assigned=std::move(assigned);
    moved.operator=(moved);
    moved.operator=(std::move(moved));
    for(const auto* batch:{&moved,&move_assigned})
    {
        CHECK_FALSE(batch->native_material_source);
        CHECK(batch->materialSlots()[7].env_intensity==.37f);
        CHECK(batch->pbrSlots()[7].roughness_factor==.63f);
        CHECK(batch->modelview_projection[12]==.25f);
    }
    moved.materialSlots()[7].env_intensity=.77f;
    moved.pbrSlots()[7].roughness_factor=.44f;
    CHECK(material_alias[7].env_intensity==.11f);CHECK(pbr_alias[7].roughness_factor==.22f);
    CHECK(std::as_const(move_assigned).materialSlots()[7].env_intensity==.37f);
    CHECK(std::as_const(move_assigned).pbrSlots()[7].roughness_factor==.63f);
    moved=defaults;
    CHECK(std::as_const(moved).materialSlots()[7].env_intensity==0.f);
    CHECK(std::as_const(moved).pbrSlots()[7].roughness_factor==1.f);
    CHECK(&std::as_const(moved).materialSlots()==default_material);
    CHECK(&std::as_const(moved).pbrSlots()==default_pbr);
    ALMetalWorldSimpleBatch authored;
    auto* material_data=authored.materialSlots().data();auto* pbr_data=authored.pbrSlots().data();
    ALMetalWorldSimpleBatch moved_authored(std::move(authored));
    CHECK(std::as_const(moved_authored).materialSlots().data()==material_data);
    CHECK(std::as_const(moved_authored).pbrSlots().data()==pbr_data);
    CHECK(&std::as_const(authored).materialSlots()==default_material);
    CHECK(&std::as_const(authored).pbrSlots()==default_pbr);
    std::cout<<"WORLD_MATERIAL_STORAGE_LAYOUT batch_bytes="<<sizeof(ALMetalWorldSimpleBatch)
        <<" material_slots_bytes="<<sizeof(ALMetalWorldSimpleBatch::MaterialSlots)
        <<" pbr_slots_bytes="<<sizeof(ALMetalWorldSimpleBatch::PBRSlots)
        <<" const_defaults_shared=1 unsealed_copies_independent=1 sealed_moves_noexcept=1\n";
}

TEST_CASE("almetal: CPU slot publication shares immutable values without publishing native uniforms",
          "[almetal][world-material-source][world-material-storage]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalWorldMaterialDataCache cache(*f.device);std::uint64_t frame=900;
        for(unsigned family:{1u,2u})for(bool indexed:{false,true})
        {
            CAPTURE(family,indexed);auto batch=materialBatch(f,family,indexed);
            auto manual=batch;
            const auto expected=renderMaterial(f,f.scene(),manual,false,++frame).pixels;
            auto* material_alias=family==1?batch.materialSlots().data():nullptr;
            auto* pbr_alias=family==2?batch.pbrSlots().data():nullptr;
            const auto before_tables=cache.stats().publications;
            const auto before=alMetalResourceAllocationStats();const auto commands=ALMetalDevice::committedCommandBufferCount();
            REQUIRE(batch.freezeCpuSlots(&f.error));
            const auto after=alMetalResourceAllocationStats();
            CHECK(after.buffers_created==before.buffers_created);CHECK(after.textures_created==before.textures_created);
            CHECK(ALMetalDevice::committedCommandBufferCount()==commands);
            CHECK_FALSE(batch.native_material_source);CHECK_FALSE(batch.native_resource_source);
            CHECK(cache.stats().publications==before_tables);
            auto copied=batch;
            if(material_alias)
            {
                CHECK(std::as_const(batch).materialSlots().data()!=material_alias);
                CHECK(&std::as_const(copied).materialSlots()==&std::as_const(batch).materialSlots());
                material_alias[0].env_intensity=std::numeric_limits<float>::quiet_NaN();
            }
            if(pbr_alias)
            {
                CHECK(std::as_const(batch).pbrSlots().data()!=pbr_alias);
                CHECK(&std::as_const(copied).pbrSlots()==&std::as_const(batch).pbrSlots());
                pbr_alias[0].roughness_factor=std::numeric_limits<float>::quiet_NaN();
            }
            REQUIRE(batch.freezeCpuSlots(&f.error));
            CHECK(renderMaterial(f,f.scene(),batch,false,++frame).pixels==expected);
            CHECK(renderMaterial(f,f.scene(),copied,true,++frame).pixels==expected);
            auto equivalent=manual;
            auto native=ALMetalWorldMaterialSource::publish(*f.device,batch,cache,&f.error);INFO(f.error);REQUIRE(native);
            auto ordinary=ALMetalWorldMaterialSource::publish(*f.device,equivalent,cache,&f.error);REQUIRE(ordinary);
            REQUIRE(native->matches(batch));CHECK_FALSE(native->matches(copied));
            for(bool normal_hdr:{false,true})CHECK(tableLease(*native,family,normal_hdr)==tableLease(*ordinary,family,normal_hdr));
            REQUIRE(ALMetalWorldResourceSource::publish(*f.device,batch,f.resources->textures,&f.error));
            const auto resource=batch.native_resource_source;
            const auto* slots=family==1?static_cast<const void*>(&std::as_const(batch).materialSlots()):
                static_cast<const void*>(&std::as_const(batch).pbrSlots());
            REQUIRE(batch.freezeCpuSlots(&f.error));
            CHECK(batch.native_material_source==native);CHECK(batch.native_resource_source==resource);CHECK(native->matches(batch));
            CHECK(slots==(family==1?static_cast<const void*>(&std::as_const(batch).materialSlots()):
                static_cast<const void*>(&std::as_const(batch).pbrSlots())));
            if(family==1)copied.materialSlots()[0].env_intensity=.91f;
            else copied.pbrSlots()[0].roughness_factor=.91f;
            CHECK(renderMaterial(f,f.scene(),batch,true,++frame).pixels==expected);
            // The fallback remains authored and editable when a complete CPU
            // snapshot cannot be issued; no partial family is committed.
            ALMetalWorldSimpleBatch invalid;
            auto* authored=invalid.materialSlots().data();authored[0].env_intensity=.37f;
            invalid.material_slot_count=1;invalid.pbr_slot_count=ALMetalWorldSimpleBatch::max_material_slots+1;
            CHECK_FALSE(invalid.freezeCpuSlots(&f.error));
            CHECK(std::as_const(invalid).materialSlots().data()==authored);
            CHECK(std::as_const(invalid).materialSlots()[0].env_intensity==.37f);
        }
    }
}

// Complete owner-side work, including capture construction, adoption, real
// credited GCD preparation, native encoding, CPU storage retirement and submit.
// The final interval additionally waits for all three views on the native GPU.
// Hidden because these controlled costs are not a viewer FPS measurement.
