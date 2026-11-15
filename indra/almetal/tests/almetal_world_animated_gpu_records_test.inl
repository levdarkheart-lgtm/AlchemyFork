/**
 * @file almetal_world_animated_gpu_records_test.inl
 * @brief Retained attachment source, late pose and native retirement fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included by the existing native world preparation fixture translation unit.
 */
#include "../almetalworldmaterialdata.h"
#include "../almetalworldresources.h"
#include "../almetalworldanimated.h"
#include <bit>

namespace AnimatedGpuRecordTest
{
std::shared_ptr<const ALMetalAttachmentPaletteRecord> palette(float translation,std::string& error,bool nonuniform=false)
{
    std::array<float,28> values{};
    for(unsigned joint=0;joint<2;++joint)
    {
        values[4+12*joint]=values[9+12*joint]=values[14+12*joint]=1;
        values[7+12*joint]=translation*(joint?1.f:.5f);
    }
    if(nonuniform)
    {
        values[4]=0;values[5]=1;values[8]=-1;values[9]=0;values[7]=.125f;
        values[16]=1.25f;values[17]=.25f;values[21]=.75f;values[23]=-.125f;
    }
    auto record=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),2,&error);
    INFO(error);REQUIRE(record);return record;
}
std::vector<ALMetalWorldVisibilityQuery> queries(unsigned count)
{
    std::vector<ALMetalWorldVisibilityQuery> result(3);
    for(unsigned at=0;at<result.size();++at)
    {
        auto& query=result[at];identity(query.modelview_projection);query.center[0]=.5f;query.center[1]=-.5f;query.center[2]=-.75f;
        query.half_extent[0]=query.half_extent[1]=.08f;query.half_extent[2]=.0625f;
        query.batch_offset=at==0?0:at==1?count/2:count;query.fan=7;query.count_samples=1;
    }
    return result;
}
struct Output
{
    Pixels pixels;std::vector<std::uint64_t> queries;
    bool operator==(const Output&) const=default;
};
Output read(Fixture& f,ALMetalRenderTarget& target,const std::shared_ptr<ALMetalVisibilityResultBatch>& queries,unsigned count)
{
    REQUIRE(queries);Output result{f.read(target),std::vector<std::uint64_t>(count)};
    for(unsigned at=0;at<count;++at)REQUIRE(queries->wait(at,result.queries[at],&f.error));
    return result;
}
struct RiggedFixture : Fixture
{
    ALMetalWorldMaterialDataCache materials{*device};
    const ALMetalBuffer* rigged_vertices=nullptr;
    RiggedFixture()
    {
        auto mesh=geometry;
        for(unsigned vertex=0;vertex<4;++vertex)
        {
            mesh.positions[vertex][3]=0;
            mesh.weights[vertex][0]=vertex&1?.75f:.25f;
            mesh.weights[vertex][1]=vertex&1?1.25f:1.75f;
            mesh.uv[vertex][0]=vertex==1?.95f:.05f;
            mesh.uv[vertex][1]=vertex==2?.95f:.05f;
        }
        rigged_vertices=buffer(&mesh,sizeof(mesh));
        const std::array<std::uint8_t,16> pixels{255,80,32,0, 32,220,80,255, 80,32,255,255, 255,220,80,0};
        color=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pixels.data(),8);
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        renderer=ALMetalWorldGBufferRenderer::create(*device,1,library,&error);REQUIRE(renderer);renderer->setFrameData(data);
    }
    ALMetalWorldSimpleBatch source(unsigned family,bool masked,const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& pose)
    {
        auto result=batch(family);result.vertices=rigged_vertices;
        result.weight_offset=offsetof(Geometry,weights);result.skin_palette_record=pose.get();
        result.skin_palette=pose->data();result.skin_palette_float_count=pose->size();result.skin_joint_count=pose->jointCount();
        if(family==1)result.material_alpha_mode=masked?2:0;
        else {result.alpha_mask=masked;result.pbrSlots()[0].alpha_cutoff=masked?.5f:-1.f;}
        REQUIRE(ALMetalWorldMaterialSource::publish(*device,result,materials,&error));
        const auto keep=[&](const std::shared_ptr<const ALMetalBuffer>& buffer)
        {if(buffer && std::none_of(resources->buffers.begin(),resources->buffers.end(),[&](const auto& old){return old==buffer;}))resources->buffers.push_back(buffer);};
        keep(result.native_material_source->materialLease());keep(result.native_material_source->pbrLease(false));keep(result.native_material_source->pbrLease(true));
        REQUIRE(ALMetalWorldResourceSource::publish(*device,result,resources->textures,&error));
        return result;
    }
    ALMetalWorldSourceNativeScope scope(const ALMetalWorldSimpleBatch& batch)
    {
        ALMetalWorldSourceNativeScope result;result.owner=resources;
        for(const auto& buffer:resources->buffers)
        {if(buffer.get()==batch.vertices)result.vertices=buffer;if(buffer.get()==batch.indices)result.indices=buffer;}
        REQUIRE(result.vertices);REQUIRE(result.indices);return result;
    }
    std::shared_ptr<const ALMetalWorldAnimatedSource> seal(const ALMetalWorldSimpleBatch& batch,unsigned family,std::uint64_t identity_value=1,std::uint64_t revision=1)
    {
        auto prototype=batch;prototype.skin_palette_record=nullptr;prototype.skin_palette=nullptr;
        prototype.skin_palette_float_count=prototype.skin_joint_count=0;prototype.body_palette_record=nullptr;
        const auto& values=static_cast<const ALMetalWorldSimpleBatch&>(prototype);
        const std::uint64_t flavor=family|(1ull<<7)|(1ull<<13)|(std::uint64_t(prototype.alpha_mask)<<6)|
            (std::uint64_t(prototype.texture_count>1)<<3)|
            (std::uint64_t(prototype.material_alpha_mode)<<16)|
            (std::uint64_t(family==1 && values.materialSlots()[0].normal_texture)<<4)|
            (std::uint64_t(family==1 && values.materialSlots()[0].specular_texture)<<5);
        auto value=ALMetalWorldAnimatedSource::seal({identity_value,flavor,revision},prototype,scope(prototype),&error);
        INFO(error);REQUIRE(value);return value;
    }
};
ALMetalWorldViewMatrices matrices(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldViewMatrices result;
    std::memcpy(result.modelview_projection,batch.modelview_projection,64);std::memcpy(result.modelview,batch.modelview,64);
    std::memcpy(result.normal_matrix,batch.normal_matrix,48);std::memcpy(result.rigged_normal_matrix,batch.rigged_normal_matrix,48);
    std::memcpy(result.texture_matrix,batch.texture_matrix,64);return result;
}
struct CpuOwners
{
    std::vector<std::shared_ptr<const ALMetalWorldAnimatedSource>> sources;
    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> palettes;
};
ALMetalWorldSourceReference reference(const std::shared_ptr<const ALMetalWorldAnimatedSource>& source,
    const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& pose,const ALMetalWorldViewMatrices& view)
{
    ALMetalWorldSourceReference result;result.source=&source->prototype();result.view=view;
    result.animated_source=source.get();result.animated_palette=pose.get();return result;
}
Output render(RiggedFixture& f,ALMetalWorldGBufferScene scene,std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldSourceReference> references={},std::shared_ptr<const CpuOwners> owners={},
    std::vector<ALMetalWorldDrawIndex> order={})
{
    const auto count=batches.size()+references.size();auto q=queries(count);
    if(!references.empty() && order.empty())for(unsigned at=0;at<references.size();++at)order.push_back(alMetalWorldDrawReferenceBit|at);
    auto input=ALMetalWorldViewInputs::adopt(scene,std::move(batches),std::move(references),std::move(order),
        std::move(q),std::move(owners),f.resources,1,&f.error);INFO(f.error);REQUIRE(input);
    std::shared_ptr<ALMetalVisibilityResultBatch> results;auto target=f.renderer->renderOwnedInputs(*input,&f.error,&results);
    INFO(f.error);REQUIRE(target);return read(f,*target,results,3);
}
}

TEST_CASE("almetal: retained rigged material sources preserve late poses masks and all output formats",
          "[almetal][world-animated-gpu-records][PPD-287]")
{
    using namespace AnimatedGpuRecordTest;
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})for(bool masked:{false,true})
        {
            CAPTURE(family,masked);RiggedFixture f;auto first=palette(0,f.error),moved=palette(.375f,f.error);
            auto prototype=f.source(family,masked,first);auto proof=f.seal(prototype,family);
            auto owners=std::make_shared<CpuOwners>();owners->sources={proof};owners->palettes={first,moved};
            std::vector<ALMetalWorldSimpleBatch> raw(4,prototype);std::vector<ALMetalWorldSourceReference> refs;
            for(unsigned draw=0;draw<raw.size();++draw)
            {
                auto& batch=raw[draw];const auto& pose=draw&1?moved:first;
                batch.skin_palette_record=pose.get();batch.skin_palette=pose->data();
                batch.modelview_projection[0]=batch.modelview_projection[5]=.4f;
                batch.modelview_projection[12]=(draw&1)?.45f:-.45f;
                batch.modelview_projection[13]=draw<2?-.45f:.45f;
                refs.push_back(reference(proof,pose,matrices(batch)));
            }
            for(unsigned format=0;format<8;++format)
            {
                CAPTURE(format);auto scene=f.scene();scene.hdr=format&1;scene.normal_hdr=format&2;scene.reverse_z=format&4;
                const auto expected=render(f,scene,raw);REQUIRE(expected.queries.front()>0);
                if(masked)
                {
                    const auto clear=render(f,scene,{});auto opaque=raw;
                    for(auto& batch:opaque){if(family==1)batch.material_alpha_mode=0;else batch.alpha_mask=false;}
                    const auto solid=render(f,scene,opaque);
                    unsigned covered=0,solid_covered=0;
                    for(unsigned pixel=0;pixel<expected.pixels.depth.size();++pixel)
                    {covered+=expected.pixels.depth[pixel]!=clear.pixels.depth[pixel];solid_covered+=solid.pixels.depth[pixel]!=clear.pixels.depth[pixel];}
                    CHECK(covered>0);CHECK(covered<solid_covered);
                }
                CHECK(render(f,scene,{},refs,owners)==expected); // Ordinary direct fallback uses the SAME late pose.
                scene.animated_gpu_records=true;const auto before=f.renderer->worldAnimatedStats();
                CHECK(render(f,scene,{},refs,owners)==expected);const auto after=f.renderer->worldAnimatedStats();
                CHECK(after.requested_draws==before.requested_draws+raw.size());
                CHECK(after.gpu_draws==before.gpu_draws+raw.size());CHECK(after.fallback_draws==before.fallback_draws);
                CHECK(after.execute_calls>before.execute_calls);
            }
            if(masked)
            {
                // One bounded maximum-slot/index-offset row, rather than
                // multiplying the complete format matrix. Packed slot7 must
                // address the last numeric/resource entry, not slot0.
                auto indexed=prototype;auto indexed_mesh=f.geometry;
                for(unsigned vertex=0;vertex<4;++vertex)
                {
                    indexed_mesh.positions[vertex][3]=std::bit_cast<float>(7u);
                    indexed_mesh.weights[vertex][0]=.25f;indexed_mesh.weights[vertex][1]=1.75f;
                    indexed_mesh.uv[vertex][0]=vertex==1?.95f:.05f;indexed_mesh.uv[vertex][1]=vertex==2?.95f:.05f;
                }
                indexed.vertices=f.buffer(&indexed_mesh,sizeof(indexed_mesh));
                const std::array<std::uint16_t,4> indices{3,0,1,2};indexed.indices=f.buffer(indices.data(),sizeof(indices));indexed.index_offset=2;
                indexed.texture_count=8;std::fill_n(indexed.diffuse_textures.begin(),8,f.color);indexed.diffuse_textures[7]=f.orm;
                if(family==1)
                {
                    const auto slot=static_cast<const ALMetalWorldSimpleBatch&>(indexed).materialSlots()[0];indexed.material_slot_count=8;
                    auto& slots=indexed.materialSlots();std::fill(slots.begin(),slots.end(),slot);slots[7].env_intensity=.75f;
                }
                else
                {
                    const auto slot=static_cast<const ALMetalWorldSimpleBatch&>(indexed).pbrSlots()[0];indexed.pbr_slot_count=8;
                    auto& slots=indexed.pbrSlots();std::fill(slots.begin(),slots.end(),slot);slots[7].base_color_factor[1]=.25f;
                }
                REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,indexed,f.materials,&f.error));
                for(const auto& buffer:{indexed.native_material_source->materialLease(),indexed.native_material_source->pbrLease(false),indexed.native_material_source->pbrLease(true)})
                    if(buffer)f.resources->buffers.push_back(buffer);
                REQUIRE(ALMetalWorldResourceSource::publish(*f.device,indexed,f.resources->textures,&f.error));
                auto indexed_source=f.seal(indexed,family,8);auto indexed_owners=std::make_shared<CpuOwners>();indexed_owners->sources={indexed_source};indexed_owners->palettes={first};
                const auto expected=render(f,f.scene(),{indexed});auto zero_slot=indexed;
                for(auto& position:indexed_mesh.positions)position[3]=0;
                zero_slot.vertices=f.buffer(&indexed_mesh,sizeof(indexed_mesh));CHECK_FALSE(render(f,f.scene(),{zero_slot}).pixels==expected.pixels);
                auto scene=f.scene();scene.animated_gpu_records=true;const auto before=f.renderer->worldAnimatedStats();
                CHECK(render(f,scene,{}, {reference(indexed_source,first,matrices(indexed))},indexed_owners)==expected);
                CHECK(f.renderer->worldAnimatedStats().gpu_draws==before.gpu_draws+1);
            }
        }
    }
}

TEST_CASE("almetal: current attachment poses drive retained visibility without republishing source generations",
          "[almetal][world-animated-gpu-records][PPD-287]")
{
    using namespace AnimatedGpuRecordTest;
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);RiggedFixture f;auto first=palette(0,f.error);auto prototype=f.source(family,true,first);
            auto proof=f.seal(prototype,family);const auto native_vertices=proof->prototype().vertices;
            const auto numeric=proof->prototype().native_material_source;const auto resource=proof->prototype().native_resource_source;
            Output original;
            for(unsigned step=0;step<6;++step)
            {
                CAPTURE(step);auto pose=palette(step==1?4.f:0.f,f.error,step==2);auto owners=std::make_shared<CpuOwners>();owners->sources={proof};owners->palettes={pose};
                auto batch=prototype;batch.skin_palette_record=pose.get();batch.skin_palette=pose->data();
                if(step==4)
                {batch.modelview_projection[0]=batch.modelview_projection[5]=.25f;batch.modelview_projection[10]=0;
                 batch.modelview_projection[3]=.5f;batch.modelview_projection[15]=.25f;} // Eye-crossing, nondegenerate visible part.
                if(step==5)batch.modelview_projection[14]=-1.25f; // Exact GL near plane.
                const auto expected=render(f,f.scene(),{batch});auto scene=f.scene();scene.animated_gpu_records=true;
                if(step>=4)
                {
                    auto opaque=batch;if(family==1)opaque.material_alpha_mode=0;else opaque.alpha_mask=false;
                    const auto control=render(f,f.scene(),{opaque}),clear=render(f,f.scene(),{});
                    CHECK_FALSE(control.pixels.depth==clear.pixels.depth);
                }
                const auto before=f.renderer->worldAnimatedStats();const auto actual=render(f,scene,{},
                    {reference(proof,pose,matrices(batch))},owners);const auto after=f.renderer->worldAnimatedStats();
                CHECK(actual==expected);CHECK(after.gpu_draws==before.gpu_draws+1);CHECK(after.bounds_tests>before.bounds_tests);
                CHECK(proof->prototype().vertices==native_vertices);CHECK(proof->prototype().native_material_source==numeric);
                CHECK(proof->prototype().native_resource_source==resource);
                if(step==0)original=actual;if(step==1)CHECK_FALSE(actual.pixels==original.pixels);if(step==3)CHECK(actual==original);
            }
        }
    }
}

TEST_CASE("almetal: animated records preserve dynamic UV unknown bounds cloth and publication denial fallbacks",
          "[almetal][world-animated-gpu-records][PPD-287]")
{
    using namespace AnimatedGpuRecordTest;
    @autoreleasepool
    {
        RiggedFixture f;auto pose=palette(0,f.error);auto prototype=f.source(2,true,pose);auto proof=f.seal(prototype,2);
        auto owners=std::make_shared<CpuOwners>();owners->sources={proof};owners->palettes={pose};
        auto changed=prototype;changed.texture_matrix[12]=.375f;const auto expected_uv=render(f,f.scene(),{changed});
        auto scene=f.scene();scene.animated_gpu_records=true;auto before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{}, {reference(proof,pose,matrices(changed))},owners)==expected_uv);
        auto after=f.renderer->worldAnimatedStats();CHECK(after.gpu_draws==before.gpu_draws);CHECK(after.fallback_draws==before.fallback_draws+1);

        // An otherwise legal native private geometry allocation cannot be
        // scanned by the CPU publisher. Its bounds stay unknown/fail-visible.
        ALMetalBufferDescriptor descriptor;descriptor.length=f.rigged_vertices->length();descriptor.storage=ALMetalStorageMode::private_gpu;
        std::shared_ptr<ALMetalBuffer> opaque_geometry=ALMetalBuffer::create(*f.device,descriptor,&f.error);REQUIRE(opaque_geometry);
        auto upload=f.device->makeCommandBuffer("Private rigged source geometry",&f.error);REQUIRE(upload);
        REQUIRE(opaque_geometry->copyFrom(*upload,*f.rigged_vertices,0,0,descriptor.length,&f.error));REQUIRE(upload->commitAndWait(&f.error));
        f.resources->buffers.push_back(opaque_geometry);auto unknown=prototype;unknown.vertices=opaque_geometry.get();
        auto unknown_proof=f.seal(unknown,2,2);CHECK_FALSE(unknown_proof->boundsQualified());
        auto unknown_owners=std::make_shared<CpuOwners>();unknown_owners->sources={unknown_proof};unknown_owners->palettes={pose};
        const auto expected_unknown=render(f,f.scene(),{unknown});before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{}, {reference(unknown_proof,pose,matrices(unknown))},unknown_owners)==expected_unknown);
        after=f.renderer->worldAnimatedStats();CHECK(after.gpu_draws==before.gpu_draws+1);CHECK(after.bounds_unknown==before.bounds_unknown+1);

        // PPD-291: GPU input FTZ can erase the only fractional lane and
        // select the FIRST encoded joint7 fallback. A CPU nonzero-fraction
        // scan must not certify joint0-only bounds. Keep the draw unqualified.
        auto ftz_geometry=f.geometry;
        for(unsigned vertex=0;vertex<4;++vertex)
        {
            ftz_geometry.positions[vertex][3]=0;std::fill_n(ftz_geometry.weights[vertex],4,0.f);
            ftz_geometry.weights[vertex][0]=7.f;ftz_geometry.weights[vertex][1]=std::numeric_limits<float>::denorm_min();
            ftz_geometry.uv[vertex][0]=vertex==1?.95f:.05f;ftz_geometry.uv[vertex][1]=vertex==2?.95f:.05f;
        }
        std::array<float,100> ftz_values{};
        for(unsigned joint=0;joint<8;++joint)ftz_values[4+12*joint]=ftz_values[9+12*joint]=ftz_values[14+12*joint]=1;
        ftz_values[7]=4; // Joint0 is outside, fallback joint7 stays visible.
        auto ftz_pose=ALMetalAttachmentPaletteRecord::create(ftz_values.data(),ftz_values.size(),8,&f.error);REQUIRE(ftz_pose);
        auto ftz=prototype;ftz.vertices=f.buffer(&ftz_geometry,sizeof(ftz_geometry));ftz.skin_palette_record=ftz_pose.get();
        ftz.skin_palette=ftz_pose->data();ftz.skin_palette_float_count=ftz_pose->size();ftz.skin_joint_count=ftz_pose->jointCount();
        auto ftz_source=f.seal(ftz,2,3);CHECK_FALSE(ftz_source->boundsQualified());
        auto ftz_owners=std::make_shared<CpuOwners>();ftz_owners->sources={ftz_source};ftz_owners->palettes={ftz_pose};
        const auto expected_ftz=render(f,f.scene(),{ftz});before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{}, {reference(ftz_source,ftz_pose,matrices(ftz))},ftz_owners)==expected_ftz);
        after=f.renderer->worldAnimatedStats();CHECK(after.bounds_unknown==before.bounds_unknown+1);
        for(auto& weight:ftz_geometry.weights)weight[1]=0;
        auto opaque_ftz=ftz;opaque_ftz.vertices=f.buffer(&ftz_geometry,sizeof(ftz_geometry));opaque_ftz.alpha_mask=false;
        CHECK_FALSE(render(f,f.scene(),{opaque_ftz}).pixels.depth==render(f,f.scene(),{}).pixels.depth);

        auto cloth=f.batch(6);cloth.vertices=f.rigged_vertices;cloth.avatar_cloth=true;cloth.cloth_offset=offsetof(Fixture::Geometry,cloth);
        const auto expected_cloth=render(f,f.scene(),{cloth});before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{cloth})==expected_cloth);after=f.renderer->worldAnimatedStats();CHECK(after.requested_draws==before.requested_draws);
        auto simple=f.batch(0),terrain=f.batch(7);
        const auto expected_mixed=render(f,f.scene(),{simple,prototype,terrain,prototype,cloth});before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{simple,terrain,cloth},
            {reference(proof,pose,matrices(prototype)),reference(proof,pose,matrices(prototype))},owners,
            {0,alMetalWorldDrawReferenceBit,1,alMetalWorldDrawReferenceBit|1u,2})==expected_mixed);
        after=f.renderer->worldAnimatedStats();CHECK(after.gpu_draws==before.gpu_draws+2);

        // Fresh certificate/context: reject a real cold publication, then
        // grant the retry. A declined optional path keeps the whole draw.
        auto fresh_data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(fresh_data);fresh_data->beginFrame(1);
        auto fresh_library=fresh_data->builtinLibrary(&f.error);REQUIRE(fresh_library);
        auto fresh_renderer=ALMetalWorldGBufferRenderer::create(*f.device,1,fresh_library,&f.error);REQUIRE(fresh_renderer);
        fresh_renderer->setFrameData(fresh_data);f.renderer=std::move(fresh_renderer);f.data=fresh_data;unsigned requests=0;bool allow=false;
        std::weak_ptr<const int> receipt;
        fresh_data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork& work)->std::shared_ptr<const void>
        {++requests;CHECK(work.chargedBytes()>0);if(!allow)return {};auto value=std::make_shared<const int>(7);receipt=value;return value;});
        const auto expected=render(f,f.scene(),{prototype});before=f.renderer->worldAnimatedStats();
        CHECK(render(f,scene,{}, {reference(proof,pose,matrices(prototype))},owners)==expected);
        after=f.renderer->worldAnimatedStats();CHECK(requests>0);CHECK(after.gpu_draws==before.gpu_draws);
        CHECK(after.fallback_draws==before.fallback_draws+1);CHECK(after.source_publications==before.source_publications);
        allow=true;before=f.renderer->worldAnimatedStats();CHECK(render(f,scene,{}, {reference(proof,pose,matrices(prototype))},owners)==expected);
        after=f.renderer->worldAnimatedStats();CHECK(after.gpu_draws==before.gpu_draws+1);REQUIRE(f.device->waitUntilIdle(&f.error));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!receipt.expired() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(receipt.expired());
    }
}

TEST_CASE("almetal: queued animated commands retain native generations without retaining CPU source or pose graphs",
          "[almetal][world-animated-gpu-records][PPD-287]")
{
    using namespace AnimatedGpuRecordTest;
    @autoreleasepool
    {
        RiggedFixture f;auto old_pose=palette(0,f.error),new_pose=palette(.375f,f.error);
        auto old_batch=f.source(1,true,old_pose),new_batch=old_batch;new_batch.skin_palette_record=new_pose.get();new_batch.skin_palette=new_pose->data();
        auto new_mesh=f.geometry;for(unsigned vertex=0;vertex<4;++vertex)
        {
            new_mesh.positions[vertex][3]=0;new_mesh.weights[vertex][0]=.25f;new_mesh.weights[vertex][1]=1.75f;
            new_mesh.uv[vertex][0]=vertex==1?.95f:.05f;new_mesh.uv[vertex][1]=vertex==2?.95f:.05f;
            new_mesh.colors[vertex][0]=80;new_mesh.colors[vertex][1]=255;
        }
        new_batch.vertices=f.buffer(&new_mesh,sizeof(new_mesh));
        auto old_source=f.seal(old_batch,1,10,1),new_source=f.seal(new_batch,1,10,2);
        const auto expected_old=render(f,f.scene(),{old_batch}),expected_new=render(f,f.scene(),{new_batch});
        REQUIRE_FALSE(expected_old.pixels==expected_new.pixels);
        // Cancellation uses the same native publication owner but executes
        // no GPU work. Finish this independent transaction before the gate.
        {
            auto pose=palette(0,f.error);auto batch=f.source(1,false,pose);auto source=f.seal(batch,1,12);
            auto owners=std::make_shared<CpuOwners>();owners->sources={source};owners->palettes={pose};
            auto canceled_scene=f.scene();canceled_scene.animated_gpu_records=true;
            std::vector<ALMetalWorldSourceReference> refs{reference(source,pose,matrices(batch))};
            auto input=ALMetalWorldViewInputs::adopt(canceled_scene,{},std::move(refs),
                {alMetalWorldDrawReferenceBit},queries(1),owners,f.resources,1,&f.error);REQUIRE(input);
            REQUIRE(f.renderer->beginCommandBatch("Abandon animated publication",&f.error));
            auto unused=f.renderer->renderOwnedInputs(*input,&f.error);INFO(f.error);REQUIRE(unused);
            const auto abandoned=f.renderer->commandBatchCompletion();REQUIRE(abandoned);CHECK_FALSE(abandoned->isComplete());
            f.renderer->cancelCommandBatch();CHECK(abandoned->isComplete());
        }
        auto old_owners=std::make_shared<CpuOwners>();old_owners->sources={old_source};old_owners->palettes={old_pose};
        auto new_owners=std::make_shared<CpuOwners>();new_owners->sources={new_source};new_owners->palettes={new_pose};
        auto scene=f.scene();scene.animated_gpu_records=true;
        const auto packet=[&](const auto& source,const auto& pose,const auto& batch,const auto& owners)
        {
            std::vector<ALMetalWorldSourceReference> refs{reference(source,pose,matrices(batch))};std::vector<ALMetalWorldDrawIndex> order{alMetalWorldDrawReferenceBit};
            auto input=ALMetalWorldViewInputs::adopt(scene,{},std::move(refs),std::move(order),queries(1),owners,f.resources,1,&f.error);
            INFO(f.error);REQUIRE(input);return input;
        };
        auto old_input=packet(old_source,old_pose,old_batch,old_owners),new_input=packet(new_source,new_pose,new_batch,new_owners);
        std::weak_ptr<const ALMetalWorldAnimatedSource> weak_source=old_source;std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak_pose=old_pose;
        std::weak_ptr<const ALMetalBuffer> weak_vertices;
        {const auto native_scope=f.scope(old_batch);weak_vertices=native_scope.vertices;}
        const std::weak_ptr<Resources> weak_native=f.resources;
        // Every renderer/library/texture/buffer initialization is ABOVE this
        // same-queue gate. Only nonblocking encoding/commit occurs below it.
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold animated native generations",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(f.renderer->beginCommandBatch("Old animated native generation",&f.error));
        std::shared_ptr<ALMetalVisibilityResultBatch> old_queries,new_queries;
        auto old_target=f.renderer->renderOwnedInputs(*old_input,&f.error,&old_queries);INFO(f.error);REQUIRE(old_target);
        auto old_completion=f.renderer->commandBatchCompletion();REQUIRE(old_completion);REQUIRE(f.renderer->commitCommandBatch(&f.error));
        REQUIRE(f.renderer->beginCommandBatch("New animated native generation",&f.error));
        auto new_target=f.renderer->renderOwnedInputs(*new_input,&f.error,&new_queries);INFO(f.error);REQUIRE(new_target);
        auto new_completion=f.renderer->commandBatchCompletion();REQUIRE(new_completion);REQUIRE(f.renderer->commitCommandBatch(&f.error));
        old_input.reset();new_input.reset();old_owners.reset();new_owners.reset();old_source.reset();new_source.reset();old_pose.reset();new_pose.reset();
        old_batch={};new_batch={};f.data->beginFrame(2);f.resources.reset();CHECK(weak_source.expired());CHECK(weak_pose.expired());
        CHECK_FALSE(old_completion->isComplete());CHECK_FALSE(new_completion->isComplete());CHECK_FALSE(weak_vertices.expired());CHECK_FALSE(weak_native.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(old_completion->isComplete());CHECK(new_completion->isComplete());
        CHECK(read(f,*old_target,old_queries,3)==expected_old);CHECK(read(f,*new_target,new_queries,3)==expected_new);
        // Explicitly release intentional renderer/context storage before
        // asserting final retirement, rather than demanding idle caches vanish.
        old_target.reset();new_target.reset();old_queries.reset();new_queries.reset();f.renderer.reset();f.data.reset();blocker.reset();
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while((!weak_vertices.expired() || !weak_native.expired()) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(weak_vertices.expired());CHECK(weak_native.expired());
    }
}
