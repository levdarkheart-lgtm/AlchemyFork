/**
 * @file almetal_world_source_commands_test.mm
 * @brief Real native source-bank G-buffer parity, invalidation and lifetime.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
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
    std::unique_ptr<ALMetalRenderTarget> encodeBank(ALMetalWorldSourceCommands& helper,
        const ALMetalWorldSourceCommands::Batch& bank,ALMetalCommandBuffer& command)
    {
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;pd.library=library.get();pd.support_indirect_commands=true;
        pd.vertex_function="almetal_world_material_source_rows_vertex";
        pd.fragment_function="almetal_world_material_source_rows_fragment";pd.color_attachment_count=4;
        for(unsigned i=0;i<4;++i)pd.colors[i].format=ALMetalPixelFormat::rgba8_unorm;
        pd.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error);REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor td;td.width=td.height=side;td.color_attachment_count=4;
        for(unsigned i=0;i<4;++i)td.colors[i].format=pd.colors[i].format;
        td.depth_stencil_format=pd.depth_stencil_format;
        auto target=ALMetalRenderTarget::create(*device,td,&error);REQUIRE(target);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        auto pass=target->makeRenderPass(config,&error);REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,"Snapshot source command bank",&error);REQUIRE(encoder);
        ALMetalDepthStencilDescriptor dd;
        auto depth=ALMetalDepthStencilState::create(*device,dd,&error);REQUIRE(depth);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));REQUIRE(encoder->setScissor({0,0,side,side},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        REQUIRE(helper.execute(*encoder,command,bank,&error));REQUIRE(encoder->end(&error));return target;
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
