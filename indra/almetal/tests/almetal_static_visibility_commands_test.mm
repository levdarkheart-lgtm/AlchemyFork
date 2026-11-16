/**
 * @file almetal_static_visibility_commands_test.mm
 * @brief Static GPU ICB visibility parity, command ranges and reader ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalworldsourcecommands.h"

namespace
{
struct StaticVisibilityCpu { std::vector<ALMetalWorldSimpleBatch> rows; };
struct StaticVisibilityFixture : Fixture
{
    std::shared_ptr<StaticVisibilityCpu> cpu;
    std::shared_ptr<Resources> native;
    std::vector<ALMetalWorldSourceCohortView> views;
    std::shared_ptr<ALMetalWorldSourcePages> pages;
    std::shared_ptr<ALMetalWorldSourceCommands> commands;
    const ALMetalShaderLibrary* library=nullptr;
    unsigned family=1;
    static constexpr std::size_t count=6;
    void make(unsigned selected_family,bool mask,bool synthetic_outside_bounds=false)
    {
        family=selected_family;cpu=std::make_shared<StaticVisibilityCpu>();cpu->rows.resize(count);
        native=std::make_shared<Resources>(*resources);views.clear();
        const std::uint8_t pixels[]{240,80,32,255, 32,180,240,0, 64,240,80,0, 180,80,240,255};
        const auto* checker=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pixels,8);
        native->textures.push_back(resources->textures.back());
        ALMetalWorldMaterialDataCache materials(*device);
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;
        for(std::size_t index=0;index<count;++index)
        {
            auto geometry=this->geometry;
            const float shift=synthetic_outside_bounds&&index<2?0.f:(index==1||index==4?5.f:(index==5?.8f:0.f));
            for(auto& point:geometry.positions){point[0]=point[0]*.45f+shift;point[1]*=.45f;}
            geometry.uv[0][0]=0;geometry.uv[0][1]=0;
            geometry.uv[1][0]=1;geometry.uv[1][1]=0;
            geometry.uv[2][0]=.5f;geometry.uv[2][1]=1;
            ALMetalBufferDescriptor descriptor;descriptor.length=sizeof(geometry);
            std::shared_ptr<ALMetalBuffer> vertices=ALMetalBuffer::create(*device,descriptor,&error);REQUIRE(vertices);
            REQUIRE(vertices->write(0,&geometry,sizeof(geometry),&error));native->buffers.push_back(vertices);
            // Nonzero offsets and both element types are part of the actual
            // encoded call. A wrong GPU address reads the leading degenerate.
            const std::uint16_t indices16[]{0,0,1,2};const std::uint32_t indices32[]{0,0,1,2};
            descriptor.length=index%2?sizeof(indices32):sizeof(indices16);
            std::shared_ptr<ALMetalBuffer> indices=ALMetalBuffer::create(*device,descriptor,&error);REQUIRE(indices);
            REQUIRE(indices->write(0,index%2?static_cast<const void*>(indices32):indices16,descriptor.length,&error));
            native->buffers.push_back(indices);
            auto& source=cpu->rows[index];source=batch(family);source.vertices=vertices.get();source.indices=indices.get();
            source.index_type=index%2?ALMetalIndexType::uint32:ALMetalIndexType::uint16;
            source.index_offset=index%2?sizeof(std::uint32_t):sizeof(std::uint16_t);source.index_count=3;
            source.texcoord1_offset=source.texcoord2_offset=source.texcoord_offset;
            source.diffuse_textures[0]=checker;source.texture_matrix[12]=.13f*index;
            source.double_sided=index>=3;
            if(family==1)
            {
                source.material_alpha_mode=mask?2:0;source.materialSlots()[0].alpha_cutoff=mask?.5f:-1.f;
                source.materialSlots()[0].specular_color[0]=.04f+.07f*index;
            }
            else
            {
                source.alpha_mask=mask;source.pbrSlots()[0].alpha_cutoff=mask?.5f:-1.f;
                source.pbrSlots()[0].base_color_factor[0]=.3f+.1f*index;
            }
            auto material=ALMetalWorldMaterialSource::publish(*device,source,materials,&error);INFO(error);REQUIRE(material);
            auto textures=ALMetalWorldResourceSource::publish(*device,source,native->textures,&error);INFO(error);REQUIRE(textures);
            for(const auto& buffer:{material->materialLease(),material->pbrLease(false),material->pbrLease(true)})
                if(buffer)native->buffers.push_back(buffer);
            ALMetalWorldConservativeBounds bounds;
            const float bounds_shift=synthetic_outside_bounds&&index<2?5.f:shift;
            bounds.minimum[0]=-.36f+bounds_shift;bounds.maximum[0]=.36f+bounds_shift;
            bounds.minimum[1]=-.36f;bounds.maximum[1]=.36f;bounds.minimum[2]=bounds.maximum[2]=.25f;
            bounds.qualified=index!=2&&index!=4; // Unknown keeps its original full call.
            auto proof=ALMetalWorldCohortSource::seal({{300+index,family,1},&source},&error,&bounds);
            INFO(error);REQUIRE(proof);proofs.push_back(std::move(proof));
        }
        for(std::size_t group=0;group<2;++group)
        {
            ALMetalWorldSourceCohortView view;view.logical_begin=group*3;
            view.cohort=ALMetalWorldSourceCohort::seal(std::span(proofs).subspan(group*3,3),cpu,native,70+group,&error);
            INFO(error);REQUIRE(view.cohort);view.cpu_owner=cpu;view.native_owner=native;
            identity(view.view.modelview_projection);identity(view.view.modelview);
            view.view.modelview_projection[13]=group?.2f:-.2f;
            for(unsigned axis=0;axis<3;++axis)view.view.normal_matrix[axis][axis]=view.view.rigged_normal_matrix[axis][axis]=1;
            views.push_back(std::move(view));
        }
        library=data->builtinLibrary(&error).get();REQUIRE(library);
        pages=ALMetalWorldSourcePages::create(*device,*library,1,{},&error);INFO(error);REQUIRE(pages);
        commands=ALMetalWorldSourceCommands::create(*device,{},&error);REQUIRE(commands);
    }
    std::shared_ptr<const ALMetalWorldSourceFrameView> freeze(ALMetalCommandBuffer& command,bool reverse=false)
    {
        ALMetalWorldSourceGlobals globals;globals.flags[0]=reverse;
        auto frame=pages->freezeCohortsForCommand(command,views,globals,&error,nullptr,true);
        INFO(error);REQUIRE(frame);return frame;
    }
    struct Encoded
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::shared_ptr<const ALMetalBuffer> queries;
    };
    Encoded encode(ALMetalCommandBuffer& command,const ALMetalWorldSourceCommands::Batch& bank)
    {
        ALMetalRenderPipelineDescriptor descriptor;descriptor.library=library;descriptor.support_indirect_commands=true;
        descriptor.vertex_function=family==1?"almetal_world_material_direct_bound_vertex":"almetal_world_pbr_direct_bound_vertex";
        descriptor.fragment_function=family==1?"almetal_world_material_resources_fragment":"almetal_world_pbr_resources_fragment";
        descriptor.color_attachment_count=4;for(auto& color:descriptor.colors)color.format=ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        auto pipeline=ALMetalRenderPipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor target_descriptor;target_descriptor.width=target_descriptor.height=side;
        target_descriptor.color_attachment_count=4;
        for(auto& color:target_descriptor.colors)color.format=ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.depth_stencil_format=descriptor.depth_stencil_format;
        Encoded result;result.target=ALMetalRenderTarget::create(*device,target_descriptor,&error);REQUIRE(result.target);
        ALMetalBufferDescriptor buffer_descriptor;buffer_descriptor.length=count*sizeof(std::uint64_t);
        result.queries=ALMetalBuffer::create(*device,buffer_descriptor,&error);REQUIRE(result.queries);
        std::memset(result.queries->mappedBytes(),0,buffer_descriptor.length);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        auto pass=result.target->makeRenderPass(config,&error);REQUIRE(pass);
        ((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer=(__bridge id<MTLBuffer>)result.queries->nativeHandle();
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,"Static visibility ranged bank",&error);REQUIRE(encoder);
        ALMetalDepthStencilDescriptor depth_descriptor;depth_descriptor.depth_compare=ALMetalCompareFunction::always;
        depth_descriptor.depth_write_enabled=false;
        auto depth=ALMetalDepthStencilState::create(*device,depth_descriptor,&error);REQUIRE(depth);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));REQUIRE(encoder->setScissor({0,0,side,side},&error));
        for(std::size_t row=0;row<count;++row)
        {
            REQUIRE(encoder->setCullMode(row>=3?ALMetalCullMode::none:ALMetalCullMode::back,&error));
            REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,row*sizeof(std::uint64_t),&error));
            REQUIRE(commands->executeRange(*encoder,command,bank,row,1,&error));
        }
        REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&error));
        REQUIRE(encoder->end(&error));return result;
    }
    struct Output {Pixels pixels;std::array<std::uint64_t,count> queries{};bool operator==(const Output&)const=default;};
    Output output(const Encoded& encoded)
    {
        Output result{read(*encoded.target),{}};std::memcpy(result.queries.data(),encoded.queries->mappedBytes(),sizeof(result.queries));return result;
    }
};
struct StaticVisibilityGate
{
    id<MTLSharedEvent> event;
    StaticVisibilityGate(const ALMetalDevice& device,ALMetalCommandBuffer& command)
    {
        event=[(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent];REQUIRE(event);
        [(__bridge id<MTLCommandBuffer>)command.nativeHandle() encodeWaitForEvent:event value:1];
    }
    void release()noexcept{event.signaledValue=1;}
    ~StaticVisibilityGate(){release();}
};
}

TEST_CASE("almetal: static GPU command counts preserve indexed offsets, material base instances and query splits",
          "[almetal][static-visibility-commands]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})for(bool mask:{false,true})for(bool reverse:{false,true})
        {
            CAPTURE(family,mask,reverse);StaticVisibilityFixture f;f.make(family,mask);
            StaticVisibilityFixture::Output expected;
            for(unsigned mode:{0u,1u,0u,1u})
            {
                auto command=f.device->makeCommandBuffer("Static full/visible bank parity",&f.error);REQUIRE(command);
                auto frame=f.freeze(*command,reverse);const auto before=f.commands->stats();
                auto bank=mode?f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error):
                    f.commands->prepareCohortForCommand(*command,frame,&f.error);
                INFO(f.error);REQUIRE(bank);auto encoded=f.encode(*command,*bank);
                REQUIRE(command->commitAndWait(&f.error));auto actual=f.output(encoded);
                if(!mode)expected=actual;else CHECK(actual==expected);
                CHECK(actual.queries[0]>0);CHECK(actual.queries[1]==0);CHECK(actual.queries[2]>0);
                CHECK(actual.queries[4]==0);CHECK(actual.queries[5]>0);
                const auto after=f.commands->stats();
                CHECK(after.static_visibility_dispatches-before.static_visibility_dispatches==mode);
                CHECK(after.static_visibility_rows-before.static_visibility_rows==mode*f.count);
                CHECK(after.executions-before.executions==f.count);
            }
            // The third call restored full-count kind after GPU mutation.
            CHECK(f.commands->stats().builds==2);CHECK(f.commands->stats().hits==2);
        }
    }
}

TEST_CASE("almetal: synthetic static bounds prove GPU zero-count writes for both native index types",
          "[almetal][static-visibility-commands]")
{
    @autoreleasepool
    {
        // Explicit mechanism probe, not a production geometry/parity contract:
        // rows0/1 have visible geometry and deliberately synthetic outside
        // qualified bounds. A no-op compute kernel would leave both queries >0.
        StaticVisibilityFixture f;f.make(1,false,true);
        for(bool visible:{false,true})
        {
            auto command=f.device->makeCommandBuffer("Synthetic static GPU count proof",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);
            auto bank=visible?f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error):
                f.commands->prepareCohortForCommand(*command,frame,&f.error);
            INFO(f.error);REQUIRE(bank);auto encoded=f.encode(*command,*bank);REQUIRE(command->commitAndWait(&f.error));
            const auto result=f.output(encoded);
            if(visible){CHECK(result.queries[0]==0);CHECK(result.queries[1]==0);}
            else{CHECK(result.queries[0]>0);CHECK(result.queries[1]>0);}
            CHECK(result.queries[2]>0); // Unknown bounds retain the uint16 call.
        }
    }
}

TEST_CASE("almetal: static visibility owns pending slots and native generations without CPU source graphs",
          "[almetal][static-visibility-commands]")
{
    @autoreleasepool
    {
        StaticVisibilityFixture f;f.make(2,false);
        auto a=f.device->makeCommandBuffer("Static visible pending A",&f.error);REQUIRE(a);StaticVisibilityGate gate(*f.device,*a);
        auto frame_a=f.freeze(*a);const auto slot_a=frame_a->directBindingSlot();
        auto bank_a=f.commands->prepareVisibleCohortForCommand(*a,frame_a,*f.device,*f.library,&f.error);INFO(f.error);REQUIRE(bank_a);
        auto target_a=f.encode(*a,*bank_a);auto completion=a->completionToken();REQUIRE(a->commit(&f.error));
        CHECK_FALSE(completion->isComplete());
        auto b=f.device->makeCommandBuffer("Static visible pending B",&f.error);REQUIRE(b);
        f.views[0].view.modelview_projection[12]=.1f;
        auto frame_b=f.freeze(*b);CHECK(frame_b->directBindingSlot()!=slot_a);
        auto bank_b=f.commands->prepareVisibleCohortForCommand(*b,frame_b,*f.device,*f.library,&f.error);INFO(f.error);REQUIRE(bank_b);
        auto target_b=f.encode(*b,*bank_b);REQUIRE(b->commit(&f.error));
        std::weak_ptr<const void> cpu=f.cpu,native=f.native;
        std::weak_ptr<const ALMetalBuffer> geometry=f.native->buffers[f.resources->buffers.size()];
        f.views.clear();f.cpu.reset();f.native.reset();CHECK(cpu.expired());CHECK_FALSE(native.expired());
        frame_a.reset();frame_b.reset();bank_a.reset();bank_b.reset();a.reset();b.reset();
        CHECK_FALSE(native.expired());gate.release();REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(completion->isComplete());CHECK(native.expired());
        CHECK(geometry.expired()); // Idle banks retain native handles, not C++ geometry wrappers.
        CHECK_FALSE(f.output(target_a).pixels.depth.empty());CHECK_FALSE(f.output(target_b).pixels.depth.empty());
        f.commands->clear();CHECK(f.commands->stats().live_banks==0);
    }
}

TEST_CASE("almetal: static visibility declines missing kernels and preserves warm banks after failed publication",
          "[almetal][static-visibility-commands]")
{
    @autoreleasepool
    {
        StaticVisibilityFixture f;f.make(1,false);
        auto command=f.device->makeCommandBuffer("Static visibility rejection",&f.error);REQUIRE(command);
        auto frame=f.freeze(*command);
        auto old_library=ALMetalShaderLibrary::compileSource(*f.device,"#include <metal_stdlib>\nusing namespace metal;\nkernel void old_entry(uint i [[thread_position_in_grid]]) {}",
            "Static visibility absent entry",&f.error);INFO(f.error);REQUIRE(old_library);
        ALMetalWorldSourceCommandFailure reason=ALMetalWorldSourceCommandFailure::none;
        CHECK_FALSE(f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*old_library,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::unsupported);CHECK(f.commands->stats().static_visibility_dispatches==0);
        auto full=f.commands->prepareCohortForCommand(*command,frame,&f.error);INFO(f.error);REQUIRE(full);
        f.commands->failNextPublicationForTesting();
        CHECK_FALSE(f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::native_allocation);
        f.commands->setPublicationAdmission([](const ALMetalWorldSourcePublicationWork& work)->std::shared_ptr<const void>
            {CHECK(work.encoded_row_bytes>=StaticVisibilityFixture::count*64);return {};});
        CHECK_FALSE(f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::admission);CHECK(f.commands->stats().static_visibility_dispatches==0);
        f.commands->setPublicationAdmission({});
        auto full_again=f.commands->prepareCohortForCommand(*command,frame,&f.error);REQUIRE(full_again);
        CHECK(f.commands->stats().builds==1);CHECK(f.commands->stats().hits==1);
        ALMetalWorldSourceCommandLimits limits;limits.maximum_live_native_bytes=1;
        auto bounded=ALMetalWorldSourceCommands::create(*f.device,limits,&f.error);REQUIRE(bounded);
        CHECK_FALSE(bounded->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error,&reason));
        CHECK(reason==ALMetalWorldSourceCommandFailure::capacity);
        CHECK(bounded->stats().live_banks==0);CHECK(bounded->stats().unique_retained_binding_bytes==0);
        CHECK(bounded->stats().static_visibility_dispatches==0);
        const auto before=f.commands->stats();
        auto visible=f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error);INFO(f.error);REQUIRE(visible);
        auto same_view=f.commands->prepareVisibleCohortForCommand(*command,frame,*f.device,*f.library,&f.error);INFO(f.error);REQUIRE(same_view);
        CHECK(f.commands->stats().static_visibility_dispatches-before.static_visibility_dispatches==2);
        CHECK(f.commands->stats().builds-before.builds==1);CHECK(f.commands->stats().hits-before.hits==1);
        auto encoded=f.encode(*command,*full_again);REQUIRE(command->commitAndWait(&f.error));CHECK(f.output(encoded).queries[0]>0);
    }
}
