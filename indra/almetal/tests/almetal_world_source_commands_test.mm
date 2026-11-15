/**
 * @file almetal_world_source_commands_test.mm
 * @brief Real native source-bank G-buffer parity, invalidation and lifetime.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include <cstdlib>
#include <filesystem>

namespace
{
struct BankFixture : Fixture
{
    static constexpr unsigned slots=8,source_count=8;
    struct Mesh
    {
        float positions[slots*3][4]{},normals[slots*3][4]{},uv[slots*3][2]{};
        std::uint8_t colors[slots*3][4]{};
        float tangents[slots*3][4]{};
    };
    ALMetalWorldMaterialDataCache cache;
    std::vector<ALMetalWorldSimpleBatch> draws;
    std::shared_ptr<ALMetalWorldSourceView> metadata=std::make_shared<ALMetalWorldSourceView>();
    const ALMetalBuffer* mesh=nullptr;
    const ALMetalBuffer* index_buffer=nullptr;
    const ALMetalTexture* checker=nullptr;
    struct CohortCpu
    {
        std::vector<ALMetalWorldSimpleBatch> values;
        std::vector<ALMetalWorldKeyedSource> keys;
    };
    std::shared_ptr<CohortCpu> cohort_cpu;
    ALMetalWorldSourceCohortView cohort_view;
    std::shared_ptr<ALMetalWorldSourcePages> cohort_pages;
    BankFixture():cache(*device)
    {
        Mesh data;std::array<std::uint16_t,slots*6> index{};
        for(unsigned slot=0;slot<slots;++slot)
        for(unsigned corner=0;corner<3;++corner)
        {
            const auto vertex=slot*3+corner;
            data.positions[vertex][0]=geometry.positions[corner][0]*.24f-.75f+.5f*(slot%4);
            data.positions[vertex][1]=geometry.positions[corner][1]*.42f+(slot<4?-.5f:.5f);
            data.positions[vertex][2]=.25f;const std::uint32_t packed=slot;
            std::memcpy(&data.positions[vertex][3],&packed,4);
            data.normals[vertex][2]=1;data.tangents[vertex][0]=data.tangents[vertex][3]=1;
            data.uv[vertex][0]=corner==1?1.f:0;data.uv[vertex][1]=corner==2?1.f:0;
            std::fill_n(data.colors[vertex],4,std::uint8_t{255});
            data.colors[vertex][1]=120+slot*16;
            index[vertex]=index[slots*3+vertex]=vertex;
        }
        mesh=buffer(&data,sizeof(data));index_buffer=buffer(index.data(),sizeof(index));
        constexpr std::uint8_t pixels[]{220,80,40,255, 40,190,110,0, 100,120,240,0, 180,140,60,255};
        checker=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pixels,8);
    }
    void publish(ALMetalWorldSimpleBatch& source)
    {
        REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
        REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
    }
    void make(unsigned family,bool mask=false)
    {
        draws.clear();metadata=std::make_shared<ALMetalWorldSourceView>();
        for(unsigned i=0;i<source_count;++i)
        {
            const unsigned actual=family==3?(i<4?1:2):family;
            auto source=batch(actual);source.vertices=mesh;source.indices=index_buffer;
            source.vertex_count=source.index_count=slots*3;
            source.normal_offset=offsetof(Mesh,normals);source.texcoord_offset=offsetof(Mesh,uv);
            source.texcoord1_offset=source.texcoord2_offset=source.texcoord_offset;
            source.color_offset=offsetof(Mesh,colors);source.tangent_offset=offsetof(Mesh,tangents);
            source.texture_count=slots;source.material_slot_count=actual==1?slots:0;
            source.pbr_slot_count=actual==2?slots:0;
            source.material_alpha_mode=actual==1&&mask?2:0;source.alpha_mask=actual==2&&mask;
            source.double_sided=actual==2&&i>=6;
            source.modelview_projection[0]=.45f;source.modelview_projection[5]=.22f;
            source.modelview_projection[12]=i%2?.5f:-.5f;
            source.modelview_projection[13]=-.75f+.5f*(i/2);
            source.modelview[12]=.01f*i;
            for(unsigned slot=0;slot<slots;++slot)
            {
                source.diffuse_textures[slot]=checker;
                if(actual==1)
                {
                    auto& material=source.materialSlots()[slot];material.normal_texture=flat;material.specular_texture=orm;
                    material.specular_color[0]=.04f*(slot+1)+.01f*i;material.env_intensity=.05f*slot;
                    material.alpha_cutoff=.5f;
                }
                else
                {
                    auto& pbr=source.pbrSlots()[slot];pbr.normal_texture=flat;pbr.orm_texture=orm;pbr.emissive_texture=color;
                    pbr.base_color_factor[0]=.4f+.05f*i;pbr.metallic_factor=.08f*slot;
                    pbr.roughness_factor=.2f+.07f*slot;pbr.alpha_cutoff=mask?.5f:-1.f;
                }
            }
            publish(source);draws.push_back(std::move(source));
        }
        rebuildMetadata();
    }
    void rebuildMetadata()
    {
        metadata=std::make_shared<ALMetalWorldSourceView>();
        for(std::size_t i=0;i<draws.size();++i)
        {
            const auto& source=draws[i];if(!source.material_slot_count&&!source.pbr_slot_count)continue;
            ALMetalWorldSourceRegion region;
            std::memcpy(region.modelview_projection,source.modelview_projection,64);
            std::memcpy(region.modelview,source.modelview,64);std::memcpy(region.normal_matrix,source.normal_matrix,48);
            metadata->candidates.push_back({{100+i,source.pbr_slot_count?2u:1u,1},i,
                static_cast<std::uint32_t>(metadata->regions.size())});
            metadata->regions.push_back(region);
        }
    }
    ALMetalWorldGBufferScene view(bool banks,unsigned format=0)
    {
        auto s=scene();s.batches=draws.data();s.batch_count=draws.size();s.source_owner=resources;
        s.normal_hdr=format&2;s.hdr=format&1;s.reverse_z=format&4;
        s.source_command_banks=banks;if(banks)s.source_rows=metadata;
        s.mirror_clip=format==7;s.mirror_clip_plane[0]=1;s.mirror_clip_plane[3]=.1f;
        return s;
    }
    struct Output{Pixels pixels;std::vector<std::uint64_t> queries;bool operator==(const Output&)const=default;};
    Output render(ALMetalWorldGBufferScene scene,std::span<const ALMetalWorldVisibilityQuery> queries={})
    {
        scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        auto target=renderer->render(scene,&error,&results);INFO(error);REQUIRE(target);
        Output output{read(*target),{}};
        if(!queries.empty())
        {
            REQUIRE(results);output.queries.resize(queries.size());
            for(std::size_t i=0;i<queries.size();++i)REQUIRE(results->wait(i,output.queries[i],&error));
        }
        return output;
    }
    std::vector<ALMetalWorldSourceCommandDraw> commandDraws()const
    {
        std::vector<ALMetalWorldSourceCommandDraw> result;
        for(std::size_t i=0;i<metadata->candidates.size();++i)
        {
            const auto& candidate=metadata->candidates[i];const auto& source=draws[candidate.draw_index];
            result.push_back({candidate.key,static_cast<std::uint32_t>(i),source.indices,source.index_offset,source.index_count,source.index_type});
        }
        return result;
    }
    std::shared_ptr<const ALMetalWorldSourceFrameView> freeze(ALMetalCommandBuffer& command)
    {
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        auto pages=data->worldSourcePages(*library,1,&error);REQUIRE(pages);
        std::vector<ALMetalWorldSourceRequest> requests;
        for(const auto& candidate:metadata->candidates)
            requests.push_back({candidate.key,&draws[candidate.draw_index],{}, {resources,{},{}},candidate.region});
        ALMetalWorldSourceGlobals globals;
        auto result=pages->freezeForCommand(command,requests,metadata->regions,globals,&error);
        INFO(error);REQUIRE(result);return result;
    }
    static std::shared_ptr<const ALMetalTexture> cohortTexture(
        const std::shared_ptr<const void>& native,const ALMetalTexture* texture) noexcept
    {
        const auto owner=std::static_pointer_cast<const Resources>(native);
        for(const auto& lease:owner->textures)if(lease.get()==texture)return lease;
        return {};
    }
    void makeCohort(unsigned family,bool descriptor=false)
    {
        make(family,true);
        const auto& common=draws.front();
        for(auto& source:draws)
        {
            if(&source!=&common)
            {
                std::memcpy(source.modelview_projection,common.modelview_projection,64);
                std::memcpy(source.modelview,common.modelview,64);
                std::memcpy(source.normal_matrix,common.normal_matrix,48);
                std::memcpy(source.rigged_normal_matrix,common.rigged_normal_matrix,48);
            }
            if(!descriptor)source.native_resource_source.reset();
            for(const auto& buffer:{source.native_material_source->materialLease(),
                    source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                if(buffer&&std::none_of(resources->buffers.begin(),resources->buffers.end(),
                    [&](const auto& prior){return prior==buffer;}))resources->buffers.push_back(buffer);
        }
        rebuildMetadata();cohort_cpu=std::make_shared<CohortCpu>();cohort_cpu->values=draws;
        for(std::size_t index=0;index<draws.size();++index)
            cohort_cpu->keys.push_back({{100+index,family,1},&cohort_cpu->values[index]});
        auto certificate=ALMetalWorldSourceCohort::seal(cohort_cpu->keys,cohort_cpu,resources,7,&error,&cohortTexture);
        INFO(error);REQUIRE(certificate);
        cohort_view={};cohort_view.cohort=certificate;cohort_view.cpu_owner=cohort_cpu;cohort_view.native_owner=resources;
        std::memcpy(cohort_view.view.modelview_projection,common.modelview_projection,64);
        std::memcpy(cohort_view.view.modelview,common.modelview,64);
        std::memcpy(cohort_view.view.normal_matrix,common.normal_matrix,48);
        std::memcpy(cohort_view.view.rigged_normal_matrix,common.rigged_normal_matrix,48);
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        cohort_pages=data->worldSourcePages(*library,1,&error);INFO(error);REQUIRE(cohort_pages);
    }
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeCohort(ALMetalCommandBuffer& command,bool direct=false)
    {
        ALMetalWorldSourceGlobals globals;
        auto result=direct?cohort_pages->freezeCohortsForCommand(command,std::span(&cohort_view,1),globals,&error,nullptr,true):
            cohort_pages->freezeCohortForCommand(command,cohort_view,globals,&error);
        INFO(error);REQUIRE(result);REQUIRE(result->commandMembership());return result;
    }
    std::unique_ptr<ALMetalRenderTarget> encodeBank(ALMetalWorldSourceCommands& helper,
        const ALMetalWorldSourceCommands::Batch& bank,ALMetalCommandBuffer& command,
        std::span<const std::array<std::size_t,2>> ranges={},
        std::shared_ptr<const ALMetalBuffer>* queries=nullptr,bool check_rejections=false,bool direct=false)
    {
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;pd.library=library.get();pd.support_indirect_commands=true;
        const bool pbr=!draws.empty()&&draws.front().pbr_slot_count;
        pd.vertex_function=pbr?"almetal_world_pbr_source_rows_vertex":"almetal_world_material_source_rows_vertex";
        pd.fragment_function=pbr?"almetal_world_pbr_source_rows_fragment":"almetal_world_material_source_rows_fragment";pd.color_attachment_count=4;
        if(direct)
        {
            pd.vertex_function=pbr?"almetal_world_pbr_direct_bound_vertex":"almetal_world_material_direct_bound_vertex";
            pd.fragment_function=pbr?"almetal_world_pbr_resources_fragment":"almetal_world_material_resources_fragment";
        }
        for(unsigned i=0;i<4;++i)pd.colors[i].format=ALMetalPixelFormat::rgba8_unorm;
        pd.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error);REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor td;td.width=td.height=side;td.color_attachment_count=4;
        for(unsigned i=0;i<4;++i)td.colors[i].format=pd.colors[i].format;
        td.depth_stencil_format=pd.depth_stencil_format;
        auto target=ALMetalRenderTarget::create(*device,td,&error);REQUIRE(target);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        std::shared_ptr<const ALMetalBuffer> results;
        if(queries)
        {
            REQUIRE_FALSE(ranges.empty());ALMetalBufferDescriptor query;
            query.length=ranges.size()*sizeof(std::uint64_t);query.storage=ALMetalStorageMode::shared;
            query.label="Cohort range counting query results";
            results=ALMetalBuffer::create(*device,query,&error);INFO(error);REQUIRE(results);
            REQUIRE(results->mappedBytes());std::memset(results->mappedBytes(),0,query.length);
        }
        auto pass=target->makeRenderPass(config,&error);REQUIRE(pass);
        if(results)((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer=
            (__bridge id<MTLBuffer>)results->nativeHandle();
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,"Snapshot source command bank",&error);REQUIRE(encoder);
        ALMetalDepthStencilDescriptor dd;
        auto depth=ALMetalDepthStencilState::create(*device,dd,&error);REQUIRE(depth);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));REQUIRE(encoder->setScissor({0,0,side,side},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        if(check_rejections)
        {
            const auto before=helper.stats().executions;
            CHECK_FALSE(helper.executeRange(*encoder,command,bank,0,0,&error));
            CHECK_FALSE(helper.executeRange(*encoder,command,bank,bank.size(),1,&error));
            CHECK_FALSE(helper.executeRange(*encoder,command,bank,0,std::numeric_limits<std::size_t>::max(),&error));
            auto wrong=device->makeCommandBuffer("Wrong cohort range command",&error);REQUIRE(wrong);
            CHECK_FALSE(helper.executeRange(*encoder,*wrong,bank,0,1,&error));
            CHECK(helper.stats().executions==before);error.clear();
            helper.failExecutionAfterForTesting(1);
        }
        if(ranges.empty())REQUIRE(helper.execute(*encoder,command,bank,&error));
        else for(std::size_t index=0;index<ranges.size();++index)
        {
            if(results)REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,index*sizeof(std::uint64_t),&error));
            if(check_rejections&&index==1)
            {
                const auto before=helper.stats().executions;
                CHECK_FALSE(helper.executeRange(*encoder,command,bank,ranges[index][0],ranges[index][1],&error));
                CHECK(helper.stats().executions==before);error.clear();
            }
            REQUIRE(helper.executeRange(*encoder,command,bank,ranges[index][0],ranges[index][1],&error));
        }
        if(results)REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&error));
        REQUIRE(encoder->end(&error));if(queries)*queries=std::move(results);return target;
    }
};
struct BankQueueGate
{
    id<MTLSharedEvent> event;
    std::unique_ptr<ALMetalCommandBuffer> command;
    explicit BankQueueGate(BankFixture& f)
    {
        event=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(event);
        command=f.device->makeCommandBuffer("Pending source-bank gate",&f.error);REQUIRE(command);
        [(__bridge id<MTLCommandBuffer>)command->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(command->commit(&f.error));
    }
    ~BankQueueGate(){event.signaledValue=1;}
    void release(BankFixture& f){event.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));}
};
}

TEST_CASE("almetal: persistent source banks preserve native material slots, MRTs and state barriers",
          "[almetal][world-source-commands]")
{
    @autoreleasepool
    {
        BankFixture f;
        for(unsigned family:{1u,2u})for(bool mask:{false,true})
        {
            f.make(family,mask);
            for(unsigned format=0;format<8;++format)
            {
                CAPTURE(family,mask,format);
                const auto expected=f.render(f.view(false,format));
                bool visible=false;
                for(std::size_t offset=4;offset<expected.pixels.colors[0].size();offset+=4)
                    if(std::memcmp(expected.pixels.colors[0].data(),expected.pixels.colors[0].data()+offset,4))
                    {visible=true;break;}
                REQUIRE(visible);
                const auto before=f.renderer->worldSourceCommandStats();
                const auto actual=f.render(f.view(true,format));
                CHECK(actual==expected);
                const auto after=f.renderer->worldSourceCommandStats();
                CHECK(after.requested_draws-before.requested_draws==BankFixture::source_count);
                CHECK(after.bank_draws-before.bank_draws==BankFixture::source_count);
                CHECK(after.fallback_draws==before.fallback_draws);
                // PBR double-sided rows form a second exact culling run.
                CHECK(after.execute_calls-before.execute_calls==(family==2?2:1));
                const auto warm=f.render(f.view(true,format));CHECK(warm==expected);
                const auto reused=f.renderer->worldSourceCommandStats();
                CHECK(reused.cache.builds==after.cache.builds);
                CHECK(reused.cache.hits>after.cache.hits);
            }
        }
        for(unsigned raw_family:{0u,7u})
        {
            CAPTURE(raw_family);
            f.make(3,true);f.draws.insert(f.draws.begin()+4,f.batch(raw_family));f.rebuildMetadata();
            std::vector<ALMetalWorldVisibilityQuery> queries(3);
            for(unsigned i=0;i<queries.size();++i)
            {
                auto& q=queries[i];identity(q.modelview_projection);q.center[2]=.6f;
                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.1f;
                q.fan=7;q.count_samples=1;q.batch_offset=i==0?2:i==1?4:9;
            }
            const auto expected=f.render(f.view(false),queries);
            const auto before=f.renderer->worldSourceCommandStats();
            CHECK(f.render(f.view(true),queries)==expected);
            const auto after=f.renderer->worldSourceCommandStats();
            CHECK(after.bank_draws-before.bank_draws==8);CHECK(after.execute_calls-before.execute_calls==4);
        }
    }
}

TEST_CASE("almetal: source bank misses and optional failures keep ordinary native draws",
          "[almetal][world-source-commands]")
{
    @autoreleasepool
    {
        BankFixture f;f.make(1,true);auto expected=f.render(f.view(false));
        REQUIRE(f.render(f.view(true))==expected);
        auto previous=f.renderer->worldSourceCommandStats();
        f.draws[0].index_offset=BankFixture::slots*3*sizeof(std::uint16_t);
        CHECK(f.render(f.view(true))==f.render(f.view(false)));
        auto changed=f.renderer->worldSourceCommandStats();CHECK(changed.cache.builds>previous.cache.builds);
        f.draws[0].materialSlots()[0].env_intensity=.8f;f.publish(f.draws[0]);
        ++f.metadata->candidates[0].key.revision;
        CHECK(f.render(f.view(true))==f.render(f.view(false)));
        auto revised=f.renderer->worldSourceCommandStats();CHECK(revised.cache.builds>changed.cache.builds);
        std::array<std::uint32_t,BankFixture::slots*6> wide_indices{};
        for(unsigned i=0;i<wide_indices.size();++i)wide_indices[i]=i%(BankFixture::slots*3);
        f.index_buffer=f.buffer(wide_indices.data(),sizeof(wide_indices));
        for(auto& source:f.draws)
        {source.indices=f.index_buffer;source.index_type=ALMetalIndexType::uint32;source.index_offset=0;}
        for(auto& candidate:f.metadata->candidates)++candidate.key.revision;
        CHECK(f.render(f.view(true))==f.render(f.view(false)));
        CHECK(f.renderer->worldSourceCommandStats().cache.builds>revised.cache.builds);
        f.renderer->clearWorldSourceCommands();f.renderer->failNextWorldSourceCommandPublicationForTesting();
        const auto before=f.renderer->worldSourceCommandStats();
        CHECK(f.render(f.view(true))==f.render(f.view(false)));
        auto failed=f.renderer->worldSourceCommandStats();
        CHECK(failed.bank_draws==before.bank_draws);CHECK(failed.fallback_draws-before.fallback_draws==8);
        CHECK(failed.last_fallback==ALMetalWorldSourceCommandFailure::native_allocation);
        auto short_scene=f.view(true);short_scene.source_command_minimum_run=9;
        CHECK(f.render(short_scene)==f.render(f.view(false)));
        CHECK(f.renderer->worldSourceCommandStats().last_fallback==ALMetalWorldSourceCommandFailure::short_run);

        auto command=f.device->makeCommandBuffer("Source bank exact row proof",&f.error);REQUIRE(command);
        auto frame=f.freeze(*command);auto draws=f.commandDraws();
        auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);REQUIRE(helper->supported());
        auto wrong=draws;++wrong[0].key.revision;
        ALMetalWorldSourceCommandFailure reason{};
        CHECK_FALSE(helper->prepareForCommand(*command,frame,wrong,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
        ALMetalWorldSourceCommandLimits tiny;tiny.maximum_live_native_bytes=1;
        auto bounded=ALMetalWorldSourceCommands::create(*f.device,tiny,&f.error);REQUIRE(bounded);
        CHECK_FALSE(bounded->prepareForCommand(*command,frame,draws,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::capacity);
        CHECK(bounded->stats().live_icb_allocated_bytes==0);CHECK(bounded->stats().unique_retained_index_bytes==0);
    }
}

TEST_CASE("almetal: dirty source bank credits persist per command and warm hits need none",
          "[almetal][world-source-commands]")
{
    @autoreleasepool
    {
        BankFixture f;f.make(1);
        auto command=f.device->makeCommandBuffer("Cold source bank publication credit",&f.error);REQUIRE(command);
        auto frame=f.freeze(*command);auto draws=f.commandDraws();
        auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
        std::vector<unsigned> attempts;std::weak_ptr<int> ticket;
        helper->setPublicationAdmission([attempt=0,&attempts,&ticket](const auto& work)mutable->std::shared_ptr<const void>
        {
            attempts.push_back(++attempt);CHECK(work.chargedBytes()>0);
            if(attempt<3)return {};
            auto credit=std::make_shared<int>(7);ticket=credit;return credit;
        });
        ALMetalWorldSourceCommandFailure reason{};
        CHECK_FALSE(helper->prepareForCommand(*command,frame,draws,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::admission);
        CHECK_FALSE(helper->prepareForCommand(*command,frame,draws,&f.error,&reason));
        auto admitted=helper->prepareForCommand(*command,frame,draws,&f.error,&reason);INFO(f.error);REQUIRE(admitted);
        CHECK((attempts==std::vector<unsigned>{1,2,3}));REQUIRE_FALSE(ticket.expired());
        auto warm_command=f.device->makeCommandBuffer("Warm bank has no publication work",&f.error);REQUIRE(warm_command);
        auto warm=helper->prepareForCommand(*warm_command,frame,draws,&f.error,&reason);REQUIRE(warm);
        CHECK(attempts.size()==3);CHECK(helper->stats().builds==1);CHECK(helper->stats().hits==1);
        const auto unique_bytes=helper->stats().unique_retained_index_bytes;
        REQUIRE(unique_bytes>0);
        // Different order publishes another bank, sharing the one native index
        // allocation rather than counting/pinning it as a second allocation.
        auto reversed=draws;std::reverse(reversed.begin(),reversed.end());
        auto alternate=helper->prepareForCommand(*command,frame,reversed,&f.error,&reason);REQUIRE(alternate);
        CHECK(helper->stats().unique_retained_index_bytes==unique_bytes);
        helper->clear();CHECK(helper->stats().cached_banks==0);
        CHECK(helper->stats().live_banks==2);REQUIRE_FALSE(ticket.expired());
        admitted.reset();alternate.reset();warm.reset();frame.reset();
        warm_command.reset();command.reset();
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!ticket.expired()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(ticket.expired());CHECK(helper->stats().live_banks==0);
        CHECK(helper->stats().unique_retained_index_bytes==0);
    }
}

TEST_CASE("almetal: pending banks retain old native generations while changed views publish safely",
          "[almetal][world-source-commands]")
{
    @autoreleasepool
    {
        BankFixture f;f.make(1,true);
        const auto expected_old=f.render(f.view(false));
        CHECK(f.render(f.view(true))==expected_old); // Warm PSOs/tables before queue gate.
        std::weak_ptr<const ALMetalBuffer> old_index;
        for(const auto& buffer:f.resources->buffers)if(buffer.get()==f.index_buffer)old_index=buffer;
        REQUIRE_FALSE(old_index.expired());
        BankQueueGate gate(f);
        REQUIRE(f.renderer->beginCommandBatch("Pending old source bank",&f.error));
        auto old_scene=f.view(true);
        auto old_target=f.renderer->render(old_scene,&f.error);INFO(f.error);REQUIRE(old_target);
        auto old_completion=f.renderer->commandBatchCompletion();REQUIRE(old_completion);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));CHECK_FALSE(old_completion->isComplete());
        old_scene.source_owner.reset();old_scene.source_rows.reset();
        f.renderer->clearWorldSourceCommands();
        REQUIRE(f.renderer->worldSourceCommandStats().cache.live_banks>0);
        // Producer COW publishes a new native aggregate. The old command's
        // exact FrameView still owns the previous C++ index generation; the
        // idle command cache owns native indices only.
        auto next_resources=std::make_shared<Resources>(*f.resources);
        std::erase_if(next_resources->buffers,[&](const auto& buffer){return buffer.get()==f.index_buffer;});
        f.resources=std::move(next_resources);
        std::array<std::uint16_t,BankFixture::slots*6> indices{};
        for(unsigned i=0;i<indices.size();++i)indices[i]=i%(BankFixture::slots*3);
        f.index_buffer=f.buffer(indices.data(),sizeof(indices));
        for(auto& source:f.draws){source.indices=f.index_buffer;source.materialSlots()[0].env_intensity=.9f;f.publish(source);}
        for(auto& candidate:f.metadata->candidates)++candidate.key.revision;
        REQUIRE(f.renderer->beginCommandBatch("Pending new source bank",&f.error));
        auto new_target=f.renderer->render(f.view(true),&f.error);INFO(f.error);REQUIRE(new_target);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(old_index.expired());
        const auto pending=f.renderer->worldSourceCommandStats().cache;
        CHECK(pending.live_banks>=2);CHECK(pending.unique_retained_index_bytes>0);
        gate.release(f);
        CHECK(f.read(*old_target)==expected_old.pixels);
        const auto expected_new=f.render(f.view(false));
        CHECK(f.read(*new_target)==expected_new.pixels);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!old_index.expired()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(old_index.expired());
        // A cached idle bank did not become a C++ producer-generation pin.
        CHECK(f.renderer->worldSourceCommandStats().cache.cached_banks==1);
    }
}

TEST_CASE("almetal: old native libraries keep source-bank requests on ordinary rendering",
          "[almetal][world-source-commands][.world-source-commands-previous-shader]")
{
    @autoreleasepool
    {
        const char* path=std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY");
        REQUIRE(path);REQUIRE(std::filesystem::is_regular_file(path));
        BankFixture f;f.make(2,true);const auto expected=f.render(f.view(false));
        REQUIRE(f.renderer->reloadShaders(std::string(path),&f.error));
        REQUIRE_FALSE(f.renderer->shaderLibrary()->hasFunction("almetal_world_pbr_source_rows_vertex"));
        const auto before=f.renderer->worldSourceCommandStats();
        CHECK(f.render(f.view(true))==expected);
        const auto after=f.renderer->worldSourceCommandStats();
        CHECK(after.bank_draws==before.bank_draws);CHECK(after.execute_calls==before.execute_calls);
        CHECK(after.fallback_draws-before.fallback_draws==8);
        CHECK(after.last_fallback==ALMetalWorldSourceCommandFailure::source_setup);
    }
}

TEST_CASE("almetal: source bank admission cannot mutate its validated command snapshot",
          "[almetal][world-source-commands]")
{
    @autoreleasepool
    {
        BankFixture f;f.make(1);
        auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
        const auto original=f.commandDraws();
        auto reference_command=f.device->makeCommandBuffer("Source bank snapshot reference",&f.error);REQUIRE(reference_command);
        auto reference_frame=f.freeze(*reference_command);
        auto reference_bank=helper->prepareForCommand(*reference_command,reference_frame,original,&f.error);REQUIRE(reference_bank);
        auto reference=f.encodeBank(*helper,*reference_bank,*reference_command);
        REQUIRE(reference_command->commitAndWait(&f.error));
        const auto expected=f.read(*reference);
        reference_bank.reset();reference_frame.reset();helper->clear();

        ALMetalBufferDescriptor bd;bd.length=3*sizeof(std::uint16_t);bd.storage=ALMetalStorageMode::shared;
        auto unrelated=ALMetalBuffer::create(*f.device,bd,&f.error);REQUIRE(unrelated);
        constexpr std::uint16_t triangle[]{0,1,2};
        REQUIRE(unrelated->write(0,triangle,sizeof(triangle),&f.error));
        auto borrowed=original;
        auto command=f.device->makeCommandBuffer("Source bank callback mutation",&f.error);REQUIRE(command);
        auto frame=f.freeze(*command);
        helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
        {
            borrowed[0].indices=unrelated.get();borrowed[0].index_count=3;borrowed[0].index_offset=0;
            return std::make_shared<int>(1);
        });
        auto bank=helper->prepareForCommand(*command,frame,borrowed,&f.error);INFO(f.error);REQUIRE(bank);
        auto target=f.encodeBank(*helper,*bank,*command);
        REQUIRE(command->commitAndWait(&f.error));CHECK(f.read(*target)==expected);
        const auto builds=helper->stats().builds;
        auto warm_command=f.device->makeCommandBuffer("Original snapshot warm match",&f.error);REQUIRE(warm_command);
        auto warm=helper->prepareForCommand(*warm_command,frame,original,&f.error);REQUIRE(warm);
        CHECK(helper->stats().builds==builds);CHECK(helper->stats().hits>0);
        auto warm_target=f.encodeBank(*helper,*warm,*warm_command);
        REQUIRE(warm_command->commitAndWait(&f.error));CHECK(f.read(*warm_target)==expected);
        // The callback's genuinely different caller storage was never silently
        // substituted for the immutable native bank or admitted as this row.
        ALMetalWorldSourceCommandFailure reason{};
        auto wrong_command=f.device->makeCommandBuffer("Mutated row must fail exact proof",&f.error);REQUIRE(wrong_command);
        CHECK_FALSE(helper->prepareForCommand(*wrong_command,frame,borrowed,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
        auto orphan_helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(orphan_helper);
        auto orphan_command=f.device->makeCommandBuffer("Bank outlives command publisher",&f.error);REQUIRE(orphan_command);
        auto orphan_frame=f.freeze(*orphan_command);
        auto orphan_bank=orphan_helper->prepareForCommand(*orphan_command,orphan_frame,original,&f.error);REQUIRE(orphan_bank);
        auto orphan_target=f.encodeBank(*orphan_helper,*orphan_bank,*orphan_command);
        orphan_frame.reset();orphan_bank.reset();orphan_helper.reset();
        REQUIRE(orphan_command->commitAndWait(&f.error));CHECK(f.read(*orphan_target)==expected);
    }
}

TEST_CASE("almetal: retained cohort banks reuse immutable membership and execute ordered query ranges",
          "[almetal][world-source-commands][world-cohort-commands]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);BankFixture f;f.makeCohort(family);
            auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);REQUIRE(helper->supported());
            auto command=f.device->makeCommandBuffer("Whole immutable cohort reference",&f.error);REQUIRE(command);
            auto frame=f.freezeCohort(*command);const auto membership=frame->commandMembership();
            REQUIRE(membership->calls().size()==BankFixture::source_count);
            auto bank=helper->prepareCohortForCommand(*command,frame,&f.error);INFO(f.error);REQUIRE(bank);
            CHECK(bank->size()==BankFixture::source_count);
            auto whole=f.encodeBank(*helper,*bank,*command);REQUIRE(command->commitAndWait(&f.error));
            const auto expected=f.read(*whole);const auto before=helper->stats();
            auto ranged_command=f.device->makeCommandBuffer("Cohort query split ranges",&f.error);REQUIRE(ranged_command);
            auto ranged_frame=f.freezeCohort(*ranged_command);
            CHECK(ranged_frame->commandMembership()==membership);
            auto ranged=helper->prepareCohortForCommand(*ranged_command,ranged_frame,&f.error);INFO(f.error);REQUIRE(ranged);
            CHECK(helper->stats().builds==before.builds);CHECK(helper->stats().hits==before.hits+1);
            const std::array<std::array<std::size_t,2>,3> ranges{{{0,1},{1,3},{4,4}}};
            std::shared_ptr<const ALMetalBuffer> queries;
            auto split=f.encodeBank(*helper,*ranged,*ranged_command,ranges,&queries,true);
            REQUIRE(ranged_command->commitAndWait(&f.error));CHECK(f.read(*split)==expected);
            REQUIRE(queries);
            // The exact producing command completed above; unlike the world
            // renderer this low-level fixture owns its native query buffer.
            const auto* samples=static_cast<const std::uint64_t*>(queries->mappedBytes());REQUIRE(samples);
            REQUIRE(queries->length()==ranges.size()*sizeof(std::uint64_t));
            for(std::size_t index=0;index<ranges.size();++index)CHECK(samples[index]>0);
            CHECK(helper->stats().executions==before.executions+3);
            CHECK(helper->stats().logical_draws==before.logical_draws+BankFixture::source_count);
        }
    }
}

TEST_CASE("almetal: cohort bank publication preserves denial injection reentrancy and exact frame qualification",
          "[almetal][world-source-commands][world-cohort-commands]")
{
    @autoreleasepool
    {
        BankFixture f;f.make(1);
        auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
        auto generic_command=f.device->makeCommandBuffer("Generic frame lacks cohort certificate",&f.error);REQUIRE(generic_command);
        auto generic=f.freeze(*generic_command);ALMetalWorldSourceCommandFailure reason{};
        CHECK_FALSE(helper->prepareCohortForCommand(*generic_command,generic,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
        CHECK_FALSE(helper->prepareCohortForCommand(*generic_command,{},&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
        f.makeCohort(1);
        auto command=f.device->makeCommandBuffer("Cohort admission and recovery",&f.error);REQUIRE(command);
        auto frame=f.freezeCohort(*command);
        unsigned callbacks=0;
        helper->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {++callbacks;CHECK(work.encoded_bank_range_bytes==BankFixture::source_count*40);return {};});
        CHECK_FALSE(helper->prepareCohortForCommand(*command,frame,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::admission);CHECK(callbacks==1);
        CHECK(helper->stats().builds==0);CHECK(helper->stats().live_icb_allocated_bytes==0);
        CHECK(helper->stats().unique_retained_index_bytes==0);
        helper->failNextPublicationForTesting();
        CHECK_FALSE(helper->prepareCohortForCommand(*command,frame,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::native_allocation);CHECK(callbacks==1);
        std::weak_ptr<int> ticket;
        helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
        {
            ++callbacks;ALMetalWorldSourceCommandFailure inner{};
            CHECK_FALSE(helper->prepareCohortForCommand(*command,frame,&f.error,&inner));
            CHECK(inner==ALMetalWorldSourceCommandFailure::validation);
            auto credit=std::make_shared<int>(9);ticket=credit;return credit;
        });
        auto bank=helper->prepareCohortForCommand(*command,frame,&f.error,&reason);INFO(f.error);REQUIRE(bank);
        CHECK(reason==ALMetalWorldSourceCommandFailure::none);CHECK(callbacks==2);REQUIRE_FALSE(ticket.expired());
        auto warm_command=f.device->makeCommandBuffer("Warm cohort avoids publication callback",&f.error);REQUIRE(warm_command);
        auto warm_frame=f.freezeCohort(*warm_command);
        auto warm=helper->prepareCohortForCommand(*warm_command,warm_frame,&f.error,&reason);REQUIRE(warm);
        CHECK(callbacks==2);CHECK(helper->stats().builds==1);CHECK(helper->stats().hits==1);
        helper->clear();
        helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
        {helper->clear();return std::make_shared<int>(1);});
        CHECK_FALSE(helper->prepareCohortForCommand(*command,frame,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::admission);CHECK(helper->stats().cached_banks==0);
        helper->setPublicationAdmission({});
        auto recovered=helper->prepareCohortForCommand(*command,frame,&f.error,&reason);REQUIRE(recovered);
        CHECK(helper->stats().builds==2);
        ALMetalWorldSourceCommandLimits tiny;tiny.maximum_live_native_bytes=1;
        auto bounded=ALMetalWorldSourceCommands::create(*f.device,tiny,&f.error);REQUIRE(bounded);
        CHECK_FALSE(bounded->prepareCohortForCommand(*command,frame,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::capacity);CHECK(bounded->stats().live_icb_allocated_bytes==0);
        CHECK(bounded->stats().unique_retained_index_bytes==0);
        auto committed=f.device->makeCommandBuffer("Committed cohort cannot prepare",&f.error);REQUIRE(committed);
        REQUIRE(committed->commitAndWait(&f.error));
        CHECK_FALSE(helper->prepareCohortForCommand(*committed,frame,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
        helper->clear();recovered.reset();bank.reset();warm.reset();frame.reset();warm_frame.reset();
        command.reset();warm_command.reset();CHECK(ticket.expired());
    }
}

TEST_CASE("almetal: cohort banks retain only native command generations and do not pin idle CPU readers",
          "[almetal][world-source-commands][world-cohort-commands]")
{
    @autoreleasepool
    {
        for(bool submit:{false,true})
        {
            CAPTURE(submit);BankFixture f;f.makeCohort(1);
            auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
            auto reference_command=f.device->makeCommandBuffer("Cohort generation reference",&f.error);REQUIRE(reference_command);
            auto reference_frame=f.freezeCohort(*reference_command);
            auto reference_bank=helper->prepareCohortForCommand(*reference_command,reference_frame,&f.error);REQUIRE(reference_bank);
            auto reference=f.encodeBank(*helper,*reference_bank,*reference_command);
            REQUIRE(reference_command->commitAndWait(&f.error));const auto expected=f.read(*reference);
            reference_bank.reset();reference_frame.reset();reference_command.reset();
            BankQueueGate gate(f);
            auto command=f.device->makeCommandBuffer("Pending native-only cohort bank",&f.error);REQUIRE(command);
            auto frame=f.freezeCohort(*command);std::weak_ptr<const ALMetalWorldSourceCommandMembership> membership=frame->commandMembership();
            auto bank=helper->prepareCohortForCommand(*command,frame,&f.error);REQUIRE(bank);
            auto target=f.encodeBank(*helper,*bank,*command);
            auto completion=command->completionToken();REQUIRE(completion);
            if(submit){REQUIRE(command->commit(&f.error));CHECK_FALSE(completion->isComplete());}
            std::weak_ptr<BankFixture::CohortCpu> cpu=f.cohort_cpu;
            std::weak_ptr<const ALMetalWorldSourceCohort> certificate=f.cohort_view.cohort;
            std::weak_ptr<const ALMetalWorldMaterialSource> material=f.draws.front().native_material_source;
            std::weak_ptr<Resources> native=f.resources;
            std::weak_ptr<const ALMetalBuffer> index;
            for(const auto& value:f.resources->buffers)if(value.get()==f.index_buffer)index=value;
            REQUIRE_FALSE(index.expired());
            f.cohort_view={};f.cohort_cpu.reset();f.draws.clear();f.metadata.reset();f.resources.reset();
            bank.reset();frame.reset();
            CHECK(cpu.expired());CHECK(certificate.expired());CHECK(material.expired());
            CHECK_FALSE(native.expired());CHECK_FALSE(index.expired());
            if(submit)
            {
                gate.release(f);CHECK(f.read(*target)==expected);CHECK(completion->isComplete());
                command.reset();
            }
            else{command.reset();gate.release(f);}
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!native.expired()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
            CHECK(native.expired());CHECK(index.expired());
            // Cache ownership of ICB/native index storage is not a C++
            // producer reader and must not keep the retired CPU graph alive.
            REQUIRE(helper->stats().cached_banks>0);
            f.cohort_pages->clear();CHECK(membership.expired());
            const auto prior=helper->stats();
            BankFixture next;next.makeCohort(1);
            auto next_command=next.device->makeCommandBuffer("Expired cohort membership is not a hot proof",&next.error);REQUIRE(next_command);
            auto next_frame=next.freezeCohort(*next_command);
            auto replacement=helper->prepareCohortForCommand(*next_command,next_frame,&next.error);INFO(next.error);REQUIRE(replacement);
            CHECK(helper->stats().builds==prior.builds+1);CHECK(helper->stats().evictions==prior.evictions+1);
            CHECK(helper->stats().cached_banks==1);
            replacement.reset();next_frame.reset();next_command.reset();
            helper->clear();CHECK(helper->stats().live_banks==0);
            CHECK(helper->stats().unique_retained_index_bytes==0);
        }
    }
}

TEST_CASE("almetal: denied or capacity-rejected bank publication preserves an existing warm cache PPD207",
          "[almetal][world-source-commands][world-cohort-commands]")
{
    @autoreleasepool
    {
        for(bool cohort_path:{false,true})for(bool index_limit:{false,true})
        {
            CAPTURE(cohort_path,index_limit);BankFixture a,b;
            REQUIRE(a.device->nativeHandle()==b.device->nativeHandle());
            if(cohort_path){a.makeCohort(1);b.makeCohort(1);}else{a.make(1);b.make(1);}
            ALMetalWorldSourceCommandLimits limits;limits.maximum_cached_banks=1;
            if(index_limit)limits.maximum_unique_index_buffers=1;
            auto helper=ALMetalWorldSourceCommands::create(*a.device,limits,&a.error);REQUIRE(helper);
            bool admit=true;unsigned callbacks=0;std::weak_ptr<int> latest_credit;
            helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
            {
                ++callbacks;if(!admit)return {};
                auto credit=std::make_shared<int>(callbacks);latest_credit=credit;return credit;
            });
            auto command_a=a.device->makeCommandBuffer("Warm bank A",&a.error);REQUIRE(command_a);
            auto command_b=b.device->makeCommandBuffer("Optional bank B",&b.error);REQUIRE(command_b);
            auto frame_a=cohort_path?a.freezeCohort(*command_a):a.freeze(*command_a);
            auto frame_b=cohort_path?b.freezeCohort(*command_b):b.freeze(*command_b);
            const auto draws_a=a.commandDraws(),draws_b=b.commandDraws();ALMetalWorldSourceCommandFailure reason{};
            const auto prepare=[&](ALMetalCommandBuffer& command,const auto& frame,const auto& draws)
            {return cohort_path?helper->prepareCohortForCommand(command,frame,&a.error,&reason):
                helper->prepareForCommand(command,frame,draws,&a.error,&reason);};
            auto bank_a=prepare(*command_a,frame_a,draws_a);INFO(a.error);REQUIRE(bank_a);
            CHECK(callbacks==1);const auto before=helper->stats();REQUIRE(before.cached_banks==1);
            admit=false;
            CHECK_FALSE(prepare(*command_b,frame_b,draws_b));CHECK(reason==ALMetalWorldSourceCommandFailure::admission);
            CHECK(callbacks==2);CHECK(helper->stats().cached_banks==1);
            CHECK(helper->stats().evictions==before.evictions);
            CHECK(helper->stats().unique_retained_index_bytes==before.unique_retained_index_bytes);
            auto warm_command=a.device->makeCommandBuffer("A remains warm after denied B",&a.error);REQUIRE(warm_command);
            auto warm_a=prepare(*warm_command,frame_a,draws_a);REQUIRE(warm_a);
            CHECK(callbacks==2);CHECK(helper->stats().builds==before.builds);CHECK(helper->stats().hits==before.hits+1);
            std::shared_ptr<const ALMetalWorldSourceCommands::Batch> second_warm;
            std::unique_ptr<ALMetalCommandBuffer> second_command;
            if(index_limit)
            {
                // A's queued/uncommitted readers prevent its native index
                // footprint retiring. Reject B without evicting usable A.
                admit=true;CHECK_FALSE(prepare(*command_b,frame_b,draws_b));
                CHECK(reason==ALMetalWorldSourceCommandFailure::capacity);CHECK(latest_credit.expired());
                CHECK(helper->stats().cached_banks==1);CHECK(helper->stats().evictions==before.evictions);
                second_command=a.device->makeCommandBuffer("A survives B index-cap failure",&a.error);REQUIRE(second_command);
                const auto attempts=callbacks;second_warm=prepare(*second_command,frame_a,draws_a);REQUIRE(second_warm);
                CHECK(callbacks==attempts);CHECK(helper->stats().builds==before.builds);
            }
            second_warm.reset();second_command.reset();warm_a.reset();warm_command.reset();
            bank_a.reset();frame_a.reset();command_a.reset();
            // Once A is cache-only, successful B can evict it and remain
            // within the same unique-index/live caps. No idle-cache wedge.
            admit=true;auto bank_b=prepare(*command_b,frame_b,draws_b);INFO(a.error);REQUIRE(bank_b);
            CHECK(helper->stats().cached_banks==1);CHECK(helper->stats().builds==before.builds+1);
            CHECK(helper->stats().evictions==before.evictions+1);
            bank_b.reset();frame_b.reset();command_b.reset();helper->clear();
            CHECK(helper->stats().unique_retained_index_bytes==0);CHECK(helper->stats().live_banks==0);
        }
    }
}

TEST_CASE("almetal: direct-bound cohort banks preserve original pixels queries and warm immutable commands",
          "[almetal][world-source-commands][world-cohort-commands][world-direct-bindings]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);BankFixture f;f.makeCohort(family);
            auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
            constexpr std::array<std::array<std::size_t,2>,2> ranges{{{0,4},{4,4}}};
            auto reference=f.device->makeCommandBuffer("Inherited source-row command control",&f.error);REQUIRE(reference);
            auto reference_frame=f.freezeCohort(*reference);auto reference_bank=helper->prepareCohortForCommand(*reference,reference_frame,&f.error);REQUIRE(reference_bank);
            std::shared_ptr<const ALMetalBuffer> reference_queries;
            auto reference_target=f.encodeBank(*helper,*reference_bank,*reference,ranges,&reference_queries);
            REQUIRE(reference->commitAndWait(&f.error));const auto expected=f.read(*reference_target);
            std::array<std::uint64_t,2> expected_queries{};REQUIRE(reference_queries->read(0,expected_queries.data(),sizeof(expected_queries),&f.error));
            CHECK(expected_queries[0]>0);CHECK(expected_queries[1]>0);
            unsigned callbacks=0;std::weak_ptr<unsigned> credit;
            helper->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
            {
                ++callbacks;CHECK(work.encoded_bank_range_bytes==BankFixture::source_count*(40+(family==1?192:208)));
                auto value=std::make_shared<unsigned>(1);credit=value;return value;
            });
            auto command=f.device->makeCommandBuffer("Direct bound native cohort commands",&f.error);REQUIRE(command);
            auto frame=f.freezeCohort(*command,true);const auto slot=frame->directBindingSlot();REQUIRE(slot);
            // PPD237: these are otherwise valid keys/index ranges for this
            // privately frozen frame. The generic inherited-buffer API must
            // reject its incompatible direct ABI before any staging/credit.
            const auto wrong_draws=f.commandDraws();const auto before_wrong=helper->stats();
            REQUIRE_FALSE(wrong_draws.empty());REQUIRE(wrong_draws.size()==frame->size());
            for(const auto& draw:wrong_draws)
            {
                REQUIRE(draw.indices);REQUIRE(frame->matchesSourceCommand(draw.view_index,draw.key,*draw.indices));
                const auto& certified=frame->commandMembership()->calls()[draw.view_index];
                REQUIRE(draw.index_count==certified.index_count);REQUIRE(draw.index_offset==certified.index_offset);
                REQUIRE(draw.index_type==certified.index_type);
            }
            const auto before_allocations=alMetalResourceAllocationStats();ALMetalWorldSourceCommandFailure wrong_reason{};
            CHECK_FALSE(helper->prepareForCommand(*command,frame,wrong_draws,&f.error,&wrong_reason));
            CHECK(wrong_reason==ALMetalWorldSourceCommandFailure::validation);CHECK(callbacks==0);
            const auto rejected=helper->stats();CHECK(rejected.builds==before_wrong.builds);CHECK(rejected.direct_encoded_calls==before_wrong.direct_encoded_calls);
            CHECK(rejected.cached_banks==before_wrong.cached_banks);CHECK(rejected.evictions==before_wrong.evictions);
            CHECK(rejected.live_icb_allocated_bytes==before_wrong.live_icb_allocated_bytes);
            CHECK(rejected.unique_retained_index_bytes==before_wrong.unique_retained_index_bytes);
            CHECK(rejected.unique_retained_binding_bytes==before_wrong.unique_retained_binding_bytes);
            CHECK(rejected.charged_logical_argument_bytes==before_wrong.charged_logical_argument_bytes);
            CHECK(rejected.charged_logical_binding_bytes==before_wrong.charged_logical_binding_bytes);
            CHECK(alMetalResourceAllocationStats().buffers_created==before_allocations.buffers_created);
            auto bank=helper->prepareCohortForCommand(*command,frame,&f.error);INFO(f.error);REQUIRE(bank);CHECK(callbacks==1);CHECK_FALSE(credit.expired());
            std::shared_ptr<const ALMetalBuffer> query_buffer;
            auto target=f.encodeBank(*helper,*bank,*command,ranges,&query_buffer,false,true);
            REQUIRE(command->commitAndWait(&f.error));CHECK(f.read(*target)==expected);
            std::array<std::uint64_t,2> actual_queries{};REQUIRE(query_buffer->read(0,actual_queries.data(),sizeof(actual_queries),&f.error));
            CHECK(actual_queries==expected_queries);
            const auto cold=helper->stats();CHECK(cold.direct_builds==1);CHECK(cold.direct_encoded_calls==BankFixture::source_count);
            CHECK(cold.charged_logical_argument_bytes==40*BankFixture::source_count);
            CHECK(cold.charged_logical_binding_bytes==BankFixture::source_count*(family==1?192:208));
            CHECK(cold.unique_retained_binding_bytes>0);CHECK(cold.retained_binding_limit_bytes==frame->directBindingAssetLimit());
            const auto policy=f.cohort_pages->stats().direct_policy_bytes;const auto allocations=f.cohort_pages->stats().direct_slot_allocations;
            std::weak_ptr<const ALMetalWorldSourceFrameView> weak_frame=frame;bank.reset();frame.reset();command.reset();
            const auto retire=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!weak_frame.expired()&&std::chrono::steady_clock::now()<retire)std::this_thread::yield();
            REQUIRE(weak_frame.expired());CHECK(credit.expired());
            auto warm_command=f.device->makeCommandBuffer("Reuse fixed direct binding slot and bank",&f.error);REQUIRE(warm_command);
            auto warm_frame=f.freezeCohort(*warm_command,true);CHECK(warm_frame->directBindingSlot()==slot);
            auto warm=helper->prepareCohortForCommand(*warm_command,warm_frame,&f.error);INFO(f.error);REQUIRE(warm);
            CHECK(callbacks==1);CHECK(helper->stats().direct_hits==1);CHECK(helper->stats().direct_encoded_calls==cold.direct_encoded_calls);
            CHECK(f.cohort_pages->stats().direct_policy_bytes==policy);CHECK(f.cohort_pages->stats().direct_slot_allocations==allocations);
            auto wrong=f.device->makeCommandBuffer("Direct bank may not adopt another command's slot",&f.error);REQUIRE(wrong);
            ALMetalWorldSourceCommandFailure reason{};CHECK_FALSE(helper->prepareCohortForCommand(*wrong,warm_frame,&f.error,&reason));
            CHECK(reason==ALMetalWorldSourceCommandFailure::validation);
            auto warm_target=f.encodeBank(*helper,*warm,*warm_command,{},nullptr,false,true);
            REQUIRE(warm_command->commitAndWait(&f.error));CHECK(f.read(*warm_target)==expected);
        }
    }
}

TEST_CASE("almetal: direct bank denial preserves old commands and extra binding assets use a separate cap",
          "[almetal][world-source-commands][world-cohort-commands][world-direct-bindings]")
{
    @autoreleasepool
    {
        BankFixture f;f.makeCohort(1);
        // Legal small draw in a larger native geometry allocation. Its gross
        // binding footprint must not be folded into the ICB/index-only cap.
        ALMetalBufferDescriptor descriptor;descriptor.length=4*1024*1024;descriptor.storage=ALMetalStorageMode::shared;
        auto authored=ALMetalBuffer::create(*f.device,descriptor,&f.error);REQUIRE(authored);
        REQUIRE(authored->write(0,f.mesh->mappedBytes(),f.mesh->length(),&f.error));
        std::shared_ptr<const ALMetalBuffer> large=std::move(authored);
        f.resources->buffers.push_back(large);
        for(auto& source:f.cohort_cpu->values)source.vertices=large.get();
        for(auto& key:f.cohort_cpu->keys)key.key.revision=2;
        f.cohort_view.cohort=ALMetalWorldSourceCohort::seal(f.cohort_cpu->keys,f.cohort_cpu,f.resources,7,&f.error,&BankFixture::cohortTexture);REQUIRE(f.cohort_view.cohort);
        auto command=f.device->makeCommandBuffer("Direct binding asset-scope control",&f.error);REQUIRE(command);
        auto frame=f.freezeCohort(*command,true);
        auto sizing=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(sizing);
        auto first=sizing->prepareCohortForCommand(*command,frame,&f.error);REQUIRE(first);const auto measured=sizing->stats();
        ALMetalWorldSourceCommandLimits limits;limits.maximum_live_native_bytes=measured.live_icb_allocated_bytes+measured.unique_retained_index_bytes;
        REQUIRE(measured.unique_retained_binding_bytes>limits.maximum_live_native_bytes);
        auto helper=ALMetalWorldSourceCommands::create(*f.device,limits,&f.error);REQUIRE(helper);
        ALMetalWorldSourceCommandFailure reason{};bool allow=false;std::weak_ptr<unsigned> ticket;
        helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
        {if(!allow)return {};auto value=std::make_shared<unsigned>(1);ticket=value;return value;});
        CHECK_FALSE(helper->prepareCohortForCommand(*command,frame,&f.error,&reason));CHECK(reason==ALMetalWorldSourceCommandFailure::admission);
        CHECK(helper->stats().direct_builds==0);CHECK(helper->stats().unique_retained_binding_bytes==0);
        allow=true;auto accepted=helper->prepareCohortForCommand(*command,frame,&f.error,&reason);INFO(f.error);REQUIRE(accepted);
        CHECK(helper->stats().live_icb_allocated_bytes+helper->stats().unique_retained_index_bytes<=limits.maximum_live_native_bytes);
        CHECK(helper->stats().unique_retained_binding_bytes>limits.maximum_live_native_bytes);
        CHECK(helper->stats().unique_retained_binding_bytes<=helper->stats().retained_binding_limit_bytes);CHECK_FALSE(ticket.expired());
        // The same completed geometry exceeds an explicit SMALL asset budget,
        // even though its indexed-call storage still fits the old cap.
        auto library=f.data->builtinLibrary(&f.error);REQUIRE(library);ALMetalWorldSourcePageLimits small;
        small.maximum_cached_cohort_native_asset_bytes=2*1024*1024;
        auto small_pages=ALMetalWorldSourcePages::create(*f.device,*library,1,small,&f.error);REQUIRE(small_pages);
        auto small_command=f.device->makeCommandBuffer("Independent direct binding asset limit",&f.error);REQUIRE(small_command);
        ALMetalWorldSourceGlobals globals;
        auto small_frame=small_pages->freezeCohortsForCommand(*small_command,std::span(&f.cohort_view,1),globals,&f.error,nullptr,true);REQUIRE(small_frame);
        auto bounded=ALMetalWorldSourceCommands::create(*f.device,limits,&f.error);REQUIRE(bounded);
        CHECK_FALSE(bounded->prepareCohortForCommand(*small_command,small_frame,&f.error,&reason));CHECK(reason==ALMetalWorldSourceCommandFailure::capacity);
        CHECK(bounded->stats().live_icb_allocated_bytes==0);CHECK(bounded->stats().unique_retained_binding_bytes==0);
        accepted.reset();auto completion=command->completionToken();command.reset();CHECK(completion->isComplete());CHECK(ticket.expired());
        first.reset();frame.reset();f.cohort_pages->clear();helper->trimUnusedBanks();sizing->trimUnusedBanks();
        CHECK(helper->stats().cached_banks==0);CHECK(helper->stats().unique_retained_binding_bytes==0);
    }
}

TEST_CASE("almetal: combined native index and geometry roles retain independent denial and retirement debt",
          "[almetal][world-source-commands][world-cohort-commands][world-direct-bindings]")
{
    @autoreleasepool
    {
        BankFixture f;f.makeCohort(1);const auto index_offset=f.mesh->length();
        ALMetalBufferDescriptor descriptor;descriptor.length=index_offset+f.index_buffer->length();descriptor.storage=ALMetalStorageMode::shared;
        auto authored=ALMetalBuffer::create(*f.device,descriptor,&f.error);REQUIRE(authored);
        REQUIRE(authored->write(0,f.mesh->mappedBytes(),f.mesh->length(),&f.error));
        REQUIRE(authored->write(index_offset,f.index_buffer->mappedBytes(),f.index_buffer->length(),&f.error));
        std::shared_ptr<const ALMetalBuffer> combined=std::move(authored);f.resources->buffers.push_back(combined);
        for(auto& source:f.cohort_cpu->values)source.vertices=combined.get();
        f.cohort_view.cohort=ALMetalWorldSourceCohort::seal(f.cohort_cpu->keys,f.cohort_cpu,f.resources,7,&f.error,&BankFixture::cohortTexture);REQUIRE(f.cohort_view.cohort);
        auto helper=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(helper);
        auto command_a=f.device->makeCommandBuffer("A binds combined storage as geometry only",&f.error);REQUIRE(command_a);
        auto frame_a=f.freezeCohort(*command_a,true);auto bank_a=helper->prepareCohortForCommand(*command_a,frame_a,&f.error);REQUIRE(bank_a);
        const auto before=helper->stats();REQUIRE(before.unique_retained_binding_bytes>0);
        auto target_a=f.encodeBank(*helper,*bank_a,*command_a,{},nullptr,false,true);
        auto cpu_b=std::make_shared<BankFixture::CohortCpu>();cpu_b->values=f.cohort_cpu->values;
        for(std::size_t index=0;index<cpu_b->values.size();++index)
        {
            cpu_b->values[index].indices=combined.get();cpu_b->values[index].index_offset=index_offset;
            cpu_b->keys.push_back({{100+index,1,2},&cpu_b->values[index]});f.cohort_pages->invalidate(100+index,2);
        }
        auto cohort_b=f.cohort_view;cohort_b.cpu_owner=cpu_b;
        cohort_b.cohort=ALMetalWorldSourceCohort::seal(cpu_b->keys,cpu_b,f.resources,7,&f.error,&BankFixture::cohortTexture);REQUIRE(cohort_b.cohort);
        auto command_b=f.device->makeCommandBuffer("B also addresses combined storage as indices",&f.error);REQUIRE(command_b);
        ALMetalWorldSourceGlobals globals;
        auto frame_b=f.cohort_pages->freezeCohortsForCommand(*command_b,std::span(&cohort_b,1),globals,&f.error,nullptr,true);REQUIRE(frame_b);
        bool allow=false;unsigned callbacks=0;
        const auto combined_bytes=[(__bridge id<MTLBuffer>)combined->nativeHandle() allocatedSize];
        helper->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>
        {
            ++callbacks;CHECK(helper->stats().unique_retained_index_bytes==before.unique_retained_index_bytes+combined_bytes);
            return allow?std::make_shared<unsigned>(1):nullptr;
        });
        ALMetalWorldSourceCommandFailure reason{};
        CHECK_FALSE(helper->prepareCohortForCommand(*command_b,frame_b,&f.error,&reason));CHECK(reason==ALMetalWorldSourceCommandFailure::admission);
        CHECK(callbacks==1);CHECK(helper->stats().unique_retained_index_bytes==before.unique_retained_index_bytes);
        CHECK(helper->stats().unique_retained_binding_bytes==before.unique_retained_binding_bytes);
        CHECK(helper->stats().cached_banks==before.cached_banks);CHECK(helper->stats().evictions==before.evictions);
        auto warm_a=helper->prepareCohortForCommand(*command_a,frame_a,&f.error);REQUIRE(warm_a);CHECK(callbacks==1);
        allow=true;auto bank_b=helper->prepareCohortForCommand(*command_b,frame_b,&f.error);INFO(f.error);REQUIRE(bank_b);
        CHECK(helper->stats().unique_retained_index_bytes==before.unique_retained_index_bytes+combined_bytes);
        BankQueueGate gate(f);REQUIRE(command_a->commit(&f.error));CHECK_FALSE(command_a->completionToken()->isComplete());
        // B's uncommitted command and its cache entry may retire while A's
        // real GPU reader still needs X as geometry. No category transfer or
        // permanent index promotion may remain after the last index user.
        bank_b.reset();frame_b.reset();command_b.reset();f.cohort_pages->clear();helper->trimUnusedBanks();
        CHECK(helper->stats().unique_retained_index_bytes==before.unique_retained_index_bytes);
        CHECK(helper->stats().unique_retained_binding_bytes==before.unique_retained_binding_bytes);
        CHECK(helper->stats().cached_banks==1);
        auto retry=f.device->makeCommandBuffer("Combined-role publication recovers",&f.error);REQUIRE(retry);
        auto recovered_frame=f.cohort_pages->freezeCohortsForCommand(*retry,std::span(&cohort_b,1),globals,&f.error,nullptr,true);REQUIRE(recovered_frame);
        auto recovered=helper->prepareCohortForCommand(*retry,recovered_frame,&f.error);REQUIRE(recovered);
        auto target_b=f.encodeBank(*helper,*recovered,*retry,{},nullptr,false,true);REQUIRE(retry->commit(&f.error));
        gate.release(f);CHECK(f.read(*target_b)==f.read(*target_a));
    }
}
