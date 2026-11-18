/**
 * @file almetal_source_applicability_test.mm
 * @brief Prevalidated direct ranges, applicability and explicit native prefixes.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalworldsourcecommands.h"
#include "../almetalworldvisibility.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <unordered_set>
#include <stdexcept>

namespace
{
struct E3Cpu { std::vector<ALMetalWorldSimpleBatch> rows; };
struct E3Fixture : Fixture
{
    std::shared_ptr<E3Cpu> cpu;
    std::shared_ptr<Resources> native;
    std::vector<ALMetalWorldSourceCohortView> views;
    std::shared_ptr<ALMetalWorldSourcePages> pages;
    std::shared_ptr<ALMetalWorldSourceCommands> commands;
    const ALMetalShaderLibrary* library=nullptr;
    unsigned family=1;
    std::size_t count=6;
    unsigned extent=side;
    bool pipeline_indirect=true;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    ALMetalPixelFormat format=ALMetalPixelFormat::rgba16_float;
    void enableGraph()
    {
        // The shared fixture deliberately defaults off. Replace all resources
        // used by this narrower family fixture before authoring its sources.
        pipeline.reset();renderer.reset();data.reset();resources=std::make_shared<Resources>();
        device=ALMetalDevice::create(&error,ALMetalResourcePlanMode::enabled);REQUIRE(device);
        renderer=ALMetalWorldGBufferRenderer::create(*device,&error);REQUIRE(renderer);
        data=ALMetalWorldFrameData::create(*device,&error);REQUIRE(data);data->beginFrame(1);renderer->setFrameData(data);
        vertices=buffer(&geometry,sizeof(geometry));const std::uint16_t indices_value[]{0,1,2};
        indices=buffer(indices_value,sizeof(indices_value));
        const std::uint8_t rgba[]{180,80,40,255},normal[]{128,128,255,255},orm_pixel[]{255,128,64,255};
        color=texture(ALMetalPixelFormat::rgba8_unorm_srgb,1,rgba,4);
        flat=texture(ALMetalPixelFormat::rgba8_unorm,1,normal,4);orm=texture(ALMetalPixelFormat::rgba8_unorm,1,orm_pixel,4);
    }
    void make(unsigned selected_family,bool mask,bool synthetic_outside_bounds=false,
        std::size_t rows=6,unsigned grid=1,unsigned layout=0,bool alias_geometry=false)
    {
        family=selected_family;count=rows;pipeline.reset();cpu=std::make_shared<E3Cpu>();cpu->rows.resize(count);
        native=std::make_shared<Resources>(*resources);views.clear();
        const std::uint8_t pixels[]{240,80,32,255, 32,180,240,0, 64,240,80,0, 180,80,240,255};
        const auto* checker=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pixels,8);
        native->textures.push_back(resources->textures.back());
        ALMetalWorldMaterialDataCache materials(*device);
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;
        const ALMetalBuffer *first_vertices=nullptr,*first_indices=nullptr;
        for(std::size_t index=0;index<count;++index)
        {
            auto geometry=this->geometry;
            const float shift=synthetic_outside_bounds&&index<2?0.f:
                (layout==1?0.f:(layout==2?5.f:(count==6&&grid==1?
                    (index==1||index==4?5.f:(index==5?.8f:0.f)):(index%3==1?5.f:(index%3==2?.8f:0.f)))));
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
            if(grid>1)
            {
                // Real unique vertices/triangles, not repeated indices into a
                // three-vertex shader-cache hit. The bounds contain every point.
                const std::size_t n=(grid+1)*(grid+1);
                std::vector<float> positions(n*4),normals(n*4),uv(n*2),tangents(n*4);
                std::vector<std::uint8_t> colors(n*4,255);
                for(unsigned y=0;y<=grid;++y)for(unsigned x=0;x<=grid;++x)
                {
                    const auto v=std::size_t(y)*(grid+1)+x;
                    positions[v*4]=-.36f+.72f*float(x)/grid+shift;
                    positions[v*4+1]=-.36f+.72f*float(y)/grid;positions[v*4+2]=.25f;positions[v*4+3]=1;
                    normals[v*4+2]=1;uv[v*2]=float(x)/grid;uv[v*2+1]=float(y)/grid;
                    tangents[v*4]=tangents[v*4+3]=1;
                }
                std::vector<std::byte> bytes;
                const auto append=[&](const auto& values)
                {
                    const auto offset=bytes.size();const auto size=values.size()*sizeof(values[0]);
                    bytes.resize(offset+size);std::memcpy(bytes.data()+offset,values.data(),size);return offset;
                };
                source.position_offset=append(positions);source.normal_offset=append(normals);
                source.texcoord_offset=append(uv);source.color_offset=append(colors);source.tangent_offset=append(tangents);
                source.texcoord1_offset=source.texcoord2_offset=source.texcoord_offset;
                source.vertices=buffer(bytes.data(),bytes.size());native->buffers.push_back(resources->buffers.back());
                std::vector<std::uint32_t> topology{0};topology.reserve(1+6*grid*grid);
                for(unsigned y=0;y<grid;++y)for(unsigned x=0;x<grid;++x)
                {
                    const auto a=y*(grid+1)+x,b=a+1,c=a+grid+1,d=c+1;
                    topology.insert(topology.end(),{a,b,c,b,d,c});
                }
                source.indices=buffer(topology.data(),topology.size()*sizeof(topology[0]));
                native->buffers.push_back(resources->buffers.back());source.index_type=ALMetalIndexType::uint32;
                source.index_offset=sizeof(std::uint32_t);source.index_count=6*grid*grid;source.vertex_count=n;
            }
            if(alias_geometry)
            {
                if(!index){first_vertices=source.vertices;first_indices=source.indices;}
                else{source.vertices=first_vertices;source.indices=first_indices;source.index_type=grid>1?ALMetalIndexType::uint32:ALMetalIndexType::uint16;
                    source.index_offset=grid>1?sizeof(std::uint32_t):sizeof(std::uint16_t);}
            }
            auto material=ALMetalWorldMaterialSource::publish(*device,source,materials,&error);INFO(error);REQUIRE(material);
            auto textures=ALMetalWorldResourceSource::publish(*device,source,native->textures,&error);INFO(error);REQUIRE(textures);
            for(const auto& buffer:{material->materialLease(),material->pbrLease(false),material->pbrLease(true)})
                if(buffer)native->buffers.push_back(buffer);
            ALMetalWorldConservativeBounds bounds;
            const float bounds_shift=synthetic_outside_bounds&&index<2?5.f:shift;
            bounds.minimum[0]=-.36f+bounds_shift;bounds.maximum[0]=.36f+bounds_shift;
            bounds.minimum[1]=-.36f;bounds.maximum[1]=.36f;bounds.minimum[2]=bounds.maximum[2]=.25f;
            bounds.qualified=layout==3?false:(layout==1||layout==2||(count==6?index!=2&&index!=4:index%7!=2)); // Unknown keeps its original full call.
            auto proof=ALMetalWorldCohortSource::seal({{300+index,family,1},&source},&error,&bounds);
            INFO(error);REQUIRE(proof);proofs.push_back(std::move(proof));
        }
        for(std::size_t group=0;group<2;++group)
        {
            ALMetalWorldSourceCohortView view;view.logical_begin=group*(count/2);
            view.cohort=ALMetalWorldSourceCohort::seal(std::span(proofs).subspan(group*(count/2),group?count-count/2:count/2),cpu,native,70+group,&error);
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
        ALMetalWorldSourceGlobals globals;globals.flags[0]=reverse;globals.flags[2]=1;
        auto frame=pages->freezeCohortsForCommand(command,views,globals,&error,nullptr,true);
        INFO(error);REQUIRE(frame);return frame;
    }
    struct Encoded
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::shared_ptr<const ALMetalBuffer> queries;
    };
    Encoded encode(ALMetalCommandBuffer& command,const ALMetalWorldSourceFrameView& frame,
        const ALMetalWorldSourceCommands::Batch* bank=nullptr,bool queries=true,bool fail_direct=false)
    {
        ALMetalRenderPipelineDescriptor descriptor;descriptor.library=library;descriptor.support_indirect_commands=pipeline_indirect;
        descriptor.vertex_function=family==1?"almetal_world_material_direct_bound_vertex":"almetal_world_pbr_direct_bound_vertex";
        descriptor.fragment_function=family==1?"almetal_world_material_resources_fragment":"almetal_world_pbr_resources_fragment";
        descriptor.color_attachment_count=4;for(auto& color:descriptor.colors)color.format=format;
        descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        if(!pipeline)pipeline=ALMetalRenderPipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor target_descriptor;target_descriptor.width=target_descriptor.height=extent;
        target_descriptor.color_attachment_count=4;
        for(auto& color:target_descriptor.colors)color.format=format;
        target_descriptor.depth_stencil_format=descriptor.depth_stencil_format;
        Encoded result;result.target=ALMetalRenderTarget::create(*device,target_descriptor,&error);REQUIRE(result.target);
        ALMetalBufferDescriptor buffer_descriptor;buffer_descriptor.length=count*sizeof(std::uint64_t);
        result.queries=ALMetalBuffer::create(*device,buffer_descriptor,&error);REQUIRE(result.queries);
        std::memset(result.queries->mappedBytes(),0,buffer_descriptor.length);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        auto pass=result.target->makeRenderPass(config,&error);REQUIRE(pass);
        if(queries)((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer=(__bridge id<MTLBuffer>)result.queries->nativeHandle();
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,"E3 ordered real indexed workload",&error);REQUIRE(encoder);
        if(queries)REQUIRE(encoder->declareResourceUse(result.queries->resourceView(),ALMetalResourcePlanning::Access::write,&error));
        ALMetalDepthStencilDescriptor depth_descriptor;depth_descriptor.depth_compare=ALMetalCompareFunction::always;
        depth_descriptor.depth_write_enabled=true;
        depth_descriptor.stencil_enabled=true;
        depth_descriptor.front_stencil.depth_stencil_pass=depth_descriptor.back_stencil.depth_stencil_pass=ALMetalStencilOperation::replace;
        auto depth=ALMetalDepthStencilState::create(*device,depth_descriptor,&error);REQUIRE(depth);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setStencilReferenceValue(0x5a,&error));
        REQUIRE(encoder->setViewport({0,0,double(extent),double(extent),0,1},&error));REQUIRE(encoder->setScissor({0,0,extent,extent},&error));
        for(std::size_t row=0;row<count;++row)
        {
            REQUIRE(encoder->setCullMode(row>=3?ALMetalCullMode::none:ALMetalCullMode::back,&error));
            if(queries)REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,row*sizeof(std::uint64_t),&error));
            if(bank)REQUIRE(commands->executeRange(*encoder,command,*bank,row,1,&error));
            else
            {
                const auto direct=frame.drawDirectRange(*encoder,command,row,1,&error);
                if(fail_direct)
                {
                    CHECK_FALSE(direct);CHECK(direct.native_prefix);CHECK_FALSE(direct.mayFallback());
                    CHECK(direct.draws_encoded==0);CHECK_FALSE(command.isCommitted());
                    REQUIRE(encoder->end(&error));return result;
                }
                INFO(error);REQUIRE(direct);CHECK(direct.native_prefix);CHECK(direct.draws_encoded==1);
            }
        }
        REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&error));
        REQUIRE(encoder->end(&error));return result;
    }
    Pixels read(ALMetalRenderTarget& target)
    {
        Pixels p;
        for(unsigned slot=0;slot<4;++slot)
        {
            const auto format=target.colorTexture(slot)->descriptor().format;
            const std::size_t bpp=target.colorTexture(slot)->bytesPerBlock();
            (void)format;
            p.colors[slot].resize(extent*extent*bpp);
            REQUIRE(target.colorTexture(slot)->readback(*device,0,0,{},p.colors[slot].data(),extent*bpp,&error));
        }
        p.depth.resize(extent*extent);p.stencil.resize(extent*extent);
        REQUIRE(target.depthStencilTexture()->readback(*device,0,0,{},p.depth.data(),extent*4,&error));
        const std::size_t row=(extent+255)&~std::size_t(255);
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> plane=[native newBufferWithLength:row*extent options:MTLResourceStorageModeShared];REQUIRE(plane);
        auto command=[(__bridge id<MTLCommandQueue>)device->nativeCommandQueue() commandBuffer];
        auto blit=[command blitCommandEncoder];REQUIRE(blit);
        [blit copyFromTexture:(__bridge id<MTLTexture>)target.depthStencilTexture()->nativeHandle()
            sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(extent,extent,1)
            toBuffer:plane destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:row*extent
            options:MTLBlitOptionStencilFromDepthStencil];
        [blit endEncoding];[command commit];[command waitUntilCompleted];
        REQUIRE(command.status==MTLCommandBufferStatusCompleted);
        for(unsigned y=0;y<extent;++y)std::memcpy(p.stencil.data()+y*extent,static_cast<const std::uint8_t*>(plane.contents)+y*row,extent);
        return p;
    }

    static void requireFinite(const Pixels& pixels)
    {
        for(const auto& plane:pixels.colors)for(std::size_t offset=0;offset<plane.size();offset+=2)
        {std::uint16_t bits;std::memcpy(&bits,plane.data()+offset,2);REQUIRE((bits&0x7c00)!=0x7c00);}
    }
    struct Output {Pixels pixels;std::vector<std::uint64_t> queries;bool operator==(const Output&)const=default;};
    Output output(const Encoded& encoded)
    {
        Output result{read(*encoded.target),std::vector<std::uint64_t>(count)};
        std::memcpy(result.queries.data(),encoded.queries->mappedBytes(),count*sizeof(std::uint64_t));return result;
    }
};
struct E3Gate
{
    id<MTLSharedEvent> event;
    E3Gate(const ALMetalDevice& device,ALMetalCommandBuffer& command)
    {
        event=[(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent];REQUIRE(event);
        [(__bridge id<MTLCommandBuffer>)command.nativeHandle() encodeWaitForEvent:event value:1];
    }
    void release()noexcept{event.signaledValue=1;}
    ~E3Gate(){release();}
};
}

TEST_CASE("almetal: source applicability leaves small and unqualified work on direct encoding",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        using Route=ALMetalWorldSourceCommandRoute;using Reason=ALMetalWorldSourceCommandReason;
        E3Fixture f;f.make(1,false);
        ALMetalWorldSourceCommandContext context;context.pipeline_supports_indirect=true;
        const auto before=f.commands->stats();
        for(const auto& view:f.views)
        {
            const auto choice=f.commands->applicability(view,context);
            CHECK(choice.route==Route::direct);CHECK(choice.indexed_elements==9);
        }
        CHECK(f.commands->stats().requests==before.requests);CHECK(f.pages->stats().cohort_builds==0);
        f.make(1,false,false,32,64);
        for(const auto& view:f.views)
        {
            const auto work=alMetalInspectWorldCohortVisibilityWork(view);
            REQUIRE(work.valid);CHECK(work.draws==16);CHECK(work.indexed_elements==16*24576);
            CHECK(work.qualified_indexed_elements>196608);CHECK(work.outside_center_indexed_elements>49152);
            const auto choice=f.commands->applicability(view,context);
            CHECK(choice.route==(f.commands->supported()?Route::gpu_visibility:Route::direct));
            context.ordered_ranges=129;CHECK(f.commands->applicability(view,context).reason==Reason::fragmented);
            context.ordered_ranges=1;context.wireframe=true;
            CHECK(f.commands->applicability(view,context).reason==Reason::raster_policy);context.wireframe=false;
        }
        for(unsigned layout:{1u,2u,3u})
        {
            f.make(2,false,false,32,64,layout);
            for(const auto& view:f.views)
            {
                const auto choice=f.commands->applicability(view,context);
                CHECK(choice.route==(layout==2?Route::cpu_visibility:Route::direct));
                CHECK(choice.reason==(layout==1?Reason::inside:(layout==2?Reason::outside:Reason::unknown_bounds)));
            }
        }
    }
}

TEST_CASE("almetal: published direct and GPU-visible indexed ranges preserve exact observable output",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        E3Fixture f;
        for(unsigned family:{1u,2u})for(bool mask:{false,true})for(bool reverse:{false,true})
        {
            CAPTURE(family,mask,reverse);f.make(family,mask);
            E3Fixture::Output original;
            for(unsigned route=0;route<3;++route)
            {
                auto command=f.device->makeCommandBuffer("E3 exact direct/bank/visible parity",&f.error);REQUIRE(command);
                auto frame=f.freeze(*command,reverse);
                std::shared_ptr<const ALMetalWorldSourceCommands::Batch> bank;
                if(route==1)bank=f.commands->prepareCohortForCommand(*command,frame,&f.error);
                if(route==2)
                {
                    const auto prepared=f.commands->prepareVisibleCohortChecked(*command,frame,*f.device,*f.library,&f.error);
                    INFO(f.error);REQUIRE(prepared);CHECK(prepared.native_prefix);bank=prepared.batch;
                }
                if(route)REQUIRE(bank);
                auto encoded=f.encode(*command,*frame,bank.get());REQUIRE(command->commitAndWait(&f.error));
                const auto output=f.output(encoded);E3Fixture::requireFinite(output.pixels);
                REQUIRE(std::any_of(output.queries.begin(),output.queries.end(),[](auto q){return q>0;}));
                CHECK(output.queries[1]==0);CHECK(output.queries[2]>0); // Unknown extents keep the real call.
                CHECK(std::any_of(output.pixels.stencil.begin(),output.pixels.stencil.end(),[](auto s){return s==0x5a;}));
                if(!route)original=output;else CHECK(output==original);
            }
        }
    }
}

TEST_CASE("almetal: source prefix outcomes distinguish optional decline from an abandoned partial command",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        E3Fixture f;f.make(1,false);
        SECTION("denial before compute permits the required direct path")
        {
            f.pipeline_indirect=false;
            auto command=f.device->makeCommandBuffer("E3 denied visibility direct fallback",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);
            f.commands->setPublicationAdmission([](const auto&)->std::shared_ptr<const void>{return {};});
            const auto choice=f.commands->prepareVisibleCohortChecked(*command,frame,*f.device,*f.library,&f.error);
            CHECK_FALSE(choice);CHECK_FALSE(choice.native_prefix);CHECK(choice.mayFallback());
            auto encoded=f.encode(*command,*frame);REQUIRE(command->commitAndWait(&f.error));
            CHECK(f.output(encoded).queries[0]>0);CHECK(f.commands->stats().static_visibility_dispatches==0);
        }
        SECTION("failure after compute begin prohibits direct fallback")
        {
            auto command=f.device->makeCommandBuffer("E3 abandoned visibility prefix",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);auto completion=command->completionToken();
            f.commands->failVisibilityAfterBeginForTesting();
            const auto choice=f.commands->prepareVisibleCohortChecked(*command,frame,*f.device,*f.library,&f.error);
            CHECK_FALSE(choice);CHECK(choice.native_prefix);CHECK_FALSE(choice.mayFallback());
            CHECK_FALSE(command->isCommitted());CHECK(f.commands->stats().static_visibility_dispatches==0);
            command.reset();CHECK(completion->status()==ALMetalCommandStatus::unknown);
        }
        SECTION("direct prefix failure is explicit and never committed")
        {
            auto command=f.device->makeCommandBuffer("E3 abandoned direct prefix",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);frame->failDirectRangeAfterForTesting(0);
            auto completion=command->completionToken();(void)f.encode(*command,*frame,nullptr,true,true);
            CHECK_FALSE(command->isCommitted());command.reset();CHECK(completion->status()==ALMetalCommandStatus::unknown);
        }
    }
}

TEST_CASE("almetal: source native closures preserve complete enabled resource-use plans",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        E3Fixture f;f.enableGraph();f.make(2,false);
        for(unsigned route=0;route<3;++route)
        {
            auto command=f.device->makeCommandBuffer("E3 complete typed resource closure",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);std::shared_ptr<const ALMetalWorldSourceCommands::Batch> bank;
            if(route==1)bank=f.commands->prepareCohortForCommand(*command,frame,&f.error);
            if(route==2)
            {
                auto prepared=f.commands->prepareVisibleCohortChecked(*command,frame,*f.device,*f.library,&f.error);
                INFO(f.error);REQUIRE(prepared);bank=prepared.batch;
            }
            if(route)REQUIRE(bank);
            auto encoded=f.encode(*command,*frame,bank.get(),false);auto plan=command->resourceUsePlan();REQUIRE(plan);
            CHECK(plan->stats().valid);CHECK_FALSE(plan->stats().opaque);CHECK(plan->stats().resources>10);
            CHECK(plan->stats().active_pass==0);CHECK(plan->stats().passes==(route==2?2:1));
            REQUIRE(command->commitAndWait(&f.error));
            CHECK(command->status()==ALMetalCommandStatus::completed);
            const auto pixels=f.read(*encoded.target);
            CHECK(std::any_of(pixels.stencil.begin(),pixels.stencil.end(),[](auto s){return s==0x5a;}));
        }
    }
}

TEST_CASE("almetal: captured source backing leases obey existing cache asset bounds",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        E3Fixture f;f.enableGraph();f.make(1,false,false,6,1,1,true);
        std::unordered_set<void*> unique;std::uint64_t expected=0;
        const auto add=[&](id<MTLResource> value)
        {REQUIRE(value);if(unique.insert((__bridge void*)value).second)expected+=value.allocatedSize;};
        for(const auto& view:f.views)for(std::size_t row=0;row<view.cohort->size();++row)
        {
            const auto& source=view.cohort->source(row);add((__bridge id<MTLBuffer>)source.vertices->nativeHandle());
            add((__bridge id<MTLBuffer>)source.indices->nativeHandle());
            const auto& material=*source.native_material_source;
            for(const auto& buffer:{material.materialLease(),material.pbrLease(false),material.pbrLease(true)})
                if(buffer)add((__bridge id<MTLBuffer>)buffer->nativeHandle());
            for(const auto& texture:source.native_resource_source->generationLeases())
            {
                id<MTLTexture> root=(__bridge id<MTLTexture>)texture->nativeHandle();
                while(root.parentTexture)root=root.parentTexture;add(root);
            }
        }
        auto command=f.device->makeCommandBuffer("E3 exact cached backing asset sum",&f.error);REQUIRE(command);
        auto frame=f.freeze(*command);CHECK(f.pages->stats().cached_cohort_native_asset_bytes==expected);
        CHECK(expected>0);CHECK(frame->commandMembership()->directAssets().size()==unique.size());
        const auto first_index=frame->commandMembership()->indexResourceViews().front();
        // Aliases share the exact plan identity and generation; the asset sum
        // above does not multiply one backing by the number of logical rows.
        CHECK(frame->commandMembership()->indexResourceViews().size()==1);
        f.pages->clear();CHECK(f.pages->stats().cached_cohort_native_asset_bytes==0);
        CHECK(first_index); // Cache eviction cannot revoke an actual reader.
        auto encoded=f.encode(*command,*frame,nullptr,false);REQUIRE(command->commitAndWait(&f.error));
        E3Fixture::requireFinite(f.read(*encoded.target));
        // A new physical source generation must issue a new identity. The
        // old native-only lease stays valid through its completed consumer.
        f.make(1,false,false,6,1,1,true);
        auto next=f.device->makeCommandBuffer("E3 replacement generation",&f.error);REQUIRE(next);
        auto replacement=f.freeze(*next);CHECK(replacement->commandMembership()->indexResourceViews().front().identity()!=first_index.identity());
        replacement.reset();next.reset();f.pages->clear();
        ALMetalWorldSourcePageLimits limits;limits.maximum_cached_cohort_native_asset_bytes=1;
        f.pages=ALMetalWorldSourcePages::create(*f.device,*f.library,1,limits,&f.error);REQUIRE(f.pages);
        auto declined=f.device->makeCommandBuffer("E3 cache-pressure uncached required consumer",&f.error);REQUIRE(declined);
        auto uncached=f.freeze(*declined);CHECK(f.pages->stats().cached_cohort_native_asset_bytes==0);
        CHECK(f.pages->stats().cached_compositions==0);
        auto fallback=f.encode(*declined,*uncached,nullptr,false);REQUIRE(declined->commitAndWait(&f.error));
        E3Fixture::requireFinite(f.read(*fallback.target));
    }
}

TEST_CASE("almetal: source direct publications retain distinct pending view and geometry generations",
          "[almetal][source-applicability]")
{
    @autoreleasepool
    {
        E3Fixture f;f.make(2,true);
        auto a=f.device->makeCommandBuffer("E3 direct pending view A",&f.error);REQUIRE(a);E3Gate gate(*f.device,*a);
        auto first=f.freeze(*a);auto slot=first->directBindingSlot();
        auto target_a=f.encode(*a,*first);auto complete=a->completionToken();REQUIRE(a->commit(&f.error));
        f.views[0].view.modelview_projection[12]=.125f;
        auto b=f.device->makeCommandBuffer("E3 direct pending view B",&f.error);REQUIRE(b);
        auto second=f.freeze(*b);CHECK(second->directBindingSlot()!=slot);
        auto target_b=f.encode(*b,*second);REQUIRE(b->commit(&f.error));
        std::weak_ptr<const void> cpu=f.cpu,native=f.native;
        f.views.clear();f.cpu.reset();f.native.reset();CHECK(cpu.expired());CHECK_FALSE(native.expired());
        first.reset();second.reset();slot.reset();a.reset();b.reset();CHECK_FALSE(native.expired());
        gate.release();REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(complete->isComplete());CHECK(native.expired());
        CHECK(f.output(target_a).queries[0]>0);CHECK(f.output(target_b).queries[0]>0);
        // The view change must affect actual pixels, so equal-output checks
        // above cannot pass only because the fixture is unlit or all clipped.
        CHECK_FALSE(f.output(target_a).pixels==f.output(target_b).pixels);
    }
}

TEST_CASE("almetal: source commands complete-path component costs retain direct adverse controls",
          "[.][almetal][source-applicability-cost]")
{
    const char* selected=std::getenv("ALCHEMY_E3_COST_CELL");
    const char* output_path=std::getenv("ALCHEMY_E3_COST_CSV");
    REQUIRE(selected);REQUIRE(output_path);
    const std::string cell_text=selected;REQUIRE(cell_text.size()==1);REQUIRE(cell_text[0]>='0');REQUIRE(cell_text[0]<='5');
    const unsigned cell=cell_text[0]-'0';
    std::ofstream csv(output_path);REQUIRE(csv);
    csv<<"cell,block,warm,position,arm,route,draws,indices,qualified_indices,begin,end,gpu_ns,wall_ns,validation\n";
    @autoreleasepool
    {
        E3Fixture f;const auto count=cell==0?6u:(cell==3?64u:32u),grid=cell==0?1u:64u;
        f.make(cell==3?2:1,false,false,count,grid,cell==2?1:0);f.extent=128;
        ALMetalWorldSourceCommandContext context;context.pipeline_supports_indirect=true;
        ALMetalComputePipelineDescriptor compute_descriptor;compute_descriptor.library=f.library;
        compute_descriptor.function="almetal_world_static_visibility_commands";
        auto compute_sentinel=ALMetalComputePipeline::create(*f.device,compute_descriptor,&f.error);
        REQUIRE(compute_sentinel);
        REQUIRE([(__bridge id<MTLComputePipelineState>)compute_sentinel->nativeHandle() shaderValidation]==2);
        auto render=[&](bool candidate,unsigned iteration,bool inspect)
        {
            struct Result{ALMetalCommandGPUTime gpu;std::uint64_t wall=0;unsigned validation=0,route=0;E3Fixture::Encoded encoded;};
            Result result;
            const auto start=std::chrono::steady_clock::now();
            if(cell==5){f.commands->clear();f.pages->clear();}
            if(cell==4)for(auto& view:f.views)view.view.modelview_projection[12]=(iteration%2)?.0625f:-.0625f;
            bool use_gpu=candidate&&cell!=0;
            if(candidate)for(const auto& view:f.views)
            {
                const auto decision=f.commands->applicability(view,context);
                use_gpu&=decision.route==ALMetalWorldSourceCommandRoute::gpu_visibility;
            }
            auto command=f.device->makeCommandBuffer("E3 complete producer-view-to-completion component",&f.error);REQUIRE(command);
            auto frame=f.freeze(*command);
            std::shared_ptr<const ALMetalWorldSourceCommands::Batch> bank;
            if(candidate&&cell==0)bank=f.commands->prepareCohortForCommand(*command,frame,&f.error); // retained adverse control
            if(use_gpu)
            {
                const auto prepared=f.commands->prepareVisibleCohortChecked(*command,frame,*f.device,*f.library,&f.error);
                INFO(f.error);REQUIRE(prepared);bank=prepared.batch;result.route=2;
            }
            else if(bank)result.route=1;
            result.encoded=f.encode(*command,*frame,bank.get(),false);
            auto token=command->completionToken();REQUIRE(command->commitAndWait(&f.error));
            result.wall=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
            REQUIRE(token->gpuTime(result.gpu)==ALMetalCommandTimingStatus::ready);
            result.validation=[(__bridge id<MTLRenderPipelineState>)f.pipeline->nativeHandle() shaderValidation];
            REQUIRE(result.validation==2); // Actual cost PSO, not environment intent.
            (void)inspect;return result;
        };
        // Full four-MRT half bits and independent D32/S8 precheck before any
        // sample. Query checks live in correctness, production has no query.
        const auto a=render(false,0,true),b=render(true,0,true);
        CHECK(f.read(*a.encoded.target)==f.read(*b.encoded.target));REQUIRE(f.read(*a.encoded.target)==f.read(*b.encoded.target));
        const auto pixel=f.read(*a.encoded.target);E3Fixture::requireFinite(pixel);
        REQUIRE(std::any_of(pixel.stencil.begin(),pixel.stencil.end(),[](auto value){return value==0x5a;}));
        std::uint64_t total_indices=0,total_qualified=0;
        for(const auto& view:f.views)
        {
            const auto work=alMetalInspectWorldCohortVisibilityWork(view);
            total_indices+=work.indexed_elements;total_qualified+=work.qualified_indexed_elements;
        }
        double previous_end=0;
        for(unsigned block=0;block<160;++block)
        {
            const bool warm=block<10,abba=(block%2)==0;
            const bool order[4]{!abba,abba,abba,!abba};
            for(unsigned position=0;position<4;++position)
            {
                const bool candidate=order[position];const auto result=render(candidate,block,false);
                REQUIRE(result.gpu.start_seconds>=previous_end);previous_end=result.gpu.end_seconds;
                csv<<cell<<','<<block<<','<<warm<<','<<position<<','<<(candidate?'B':'A')<<','<<result.route<<','<<count<<','
                   <<total_indices<<','<<total_qualified<<','
                   <<std::setprecision(17)<<result.gpu.start_seconds<<','<<result.gpu.end_seconds<<','
                   <<result.gpu.elapsed_nanoseconds<<','<<result.wall<<','<<result.validation<<'\n';
            }
        }
        REQUIRE(csv.good());
    }
}

TEST_CASE("almetal: actual routed world cohorts preserve scalar gaps queries and all output planes",
          "[almetal][source-routing-owner]")
{
    @autoreleasepool
    {
        struct Cell{unsigned family,rows,grid,layout;};
        const Cell cells[]{{1,6,1,0},{1,32,64,0},{2,32,64,0},{2,32,64,1},{1,32,64,2},{2,6,1,3}};
        for(const auto cell:cells)
        {
            CAPTURE(cell.family,cell.rows,cell.grid,cell.layout);
            E3Fixture f;f.make(cell.family,false,false,cell.rows,cell.grid,cell.layout);
            // One actual scalar draw between the two native candidate ranges.
            auto raw=f.cpu->rows.front();f.views[1].base_before=1;++f.views[1].logical_begin;
            std::array<ALMetalWorldVisibilityQuery,3> queries{};
            for(unsigned i=0;i<queries.size();++i)
            {
                auto& q=queries[i];identity(q.modelview_projection);q.center[2]=.75f;
                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.1f;q.fan=7;q.count_samples=1;
                q.batch_offset=i==0?0:(i==1?cell.rows/2:cell.rows+1);
            }
            E3Fixture::Output expected;
            for(bool routed:{false,true})
            {
                auto scene=f.scene();scene.hdr=true;scene.normal_hdr=true;
                scene.cohorts=f.views.data();scene.cohort_count=f.views.size();scene.batches=&raw;scene.batch_count=1;
                scene.source_owner=f.native;scene.cpu_source_owner=f.cpu;
                scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
                scene.source_routing=routed;scene.source_native_visibility=routed;
                const auto before=f.renderer->worldSourceCommandStats();const auto pages=f.renderer->worldSourceStats().pages;
                const auto cpu=f.renderer->worldCpuVisibilityStats();
                REQUIRE(f.renderer->beginCommandBatch("E3 actual scalar/routed ordered owner",&f.error));
                std::shared_ptr<ALMetalVisibilityResultBatch> results;
                auto target=f.renderer->render(scene,&f.error,&results);INFO(f.error);REQUIRE(target);REQUIRE(results);
                auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);
                std::string pending_error;CHECK_FALSE(completion->waitUntilCompleted(&pending_error));
                CHECK_FALSE(pending_error.empty());const auto committed=ALMetalDevice::committedCommandBufferCount();
                REQUIRE(f.renderer->commitCommandBatch(&f.error));REQUIRE(completion->waitUntilCompleted(&f.error));
                CHECK(ALMetalDevice::committedCommandBufferCount()==committed+1);
                CHECK(completion->status()==ALMetalCommandStatus::completed);
                E3Fixture::Output actual{f.read(*target),std::vector<std::uint64_t>(queries.size())};
                for(unsigned i=0;i<queries.size();++i)REQUIRE(results->wait(i,actual.queries[i],&f.error));
                REQUIRE(actual.queries[0]>0); // Original query before any draw remains observable.
                const auto covered=std::count_if(actual.pixels.depth.begin(),actual.pixels.depth.end(),
                    [](auto value){return value!=0x3f800000u;});
                if(cell.layout==2)CHECK(covered==0);else REQUIRE(covered>0);
                if(!routed)expected=actual;else CHECK(actual==expected);
                const auto after=f.renderer->worldSourceCommandStats();
                const bool native=routed&&cell.grid==64&&cell.layout==0;
                CHECK(after.bank_draws-before.bank_draws==(native?cell.rows:0));
                CHECK(after.cache.static_visibility_dispatches-before.cache.static_visibility_dispatches==(native?1:0));
                if(native)
                {
                    CHECK(after.last_pipeline_validation!=UINT32_MAX);CHECK(after.cache.last_static_validation!=UINT32_MAX);
                    if(const char* validation=std::getenv("MTL_SHADER_VALIDATION"))
                    {
                        if(std::string(validation)=="1"){CHECK(after.last_pipeline_validation==1);CHECK(after.cache.last_static_validation==1);}
                        if(std::string(validation)=="0"){CHECK(after.last_pipeline_validation==2);CHECK(after.cache.last_static_validation==2);}
                    }
                    CHECK(after.routing_native-before.routing_native==2);
                }
                else
                {
                    CHECK(after.cache.requests==before.cache.requests);
                    CHECK(f.renderer->worldSourceStats().pages.cohort_requests==pages.cohort_requests);
                }
                if(routed&&cell.layout==2)
                {
                    const auto after_cpu=f.renderer->worldCpuVisibilityStats();
                    CHECK(after_cpu.culled_draws-cpu.culled_draws==cell.rows);
                    CHECK(after_cpu.visible_draws-cpu.visible_draws==1); // Original raw gap stays direct.
                }
            }
        }
    }
}

TEST_CASE("almetal: actual routed world failures preserve direct fallback before prefix and cancel after prefix",
          "[almetal][source-routing-owner]")
{
    @autoreleasepool
    {
        E3Fixture f;f.make(1,false,false,32,64);
        auto scene=f.scene();scene.hdr=true;scene.normal_hdr=true;scene.cohorts=f.views.data();scene.cohort_count=f.views.size();
        scene.source_owner=f.native;scene.cpu_source_owner=f.cpu;scene.source_routing=true;scene.source_native_visibility=true;
        REQUIRE(f.renderer->beginCommandBatch("E3 actual routed initial consumer",&f.error));
        auto first=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(first);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));
        const auto expected=f.read(*first);const auto initial=f.renderer->worldSourceCommandStats();REQUIRE(initial.bank_draws==32);
        auto commands=f.data->worldSourceCommands(&f.error);REQUIRE(commands);
        SECTION("admission before native prefix retains scalar output")
        {
            commands->clear();commands->setPublicationAdmission([](const auto&)->std::shared_ptr<const void>{return {};});
            REQUIRE(f.renderer->beginCommandBatch("E3 actual denied optional bank",&f.error));
            auto fallback=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(fallback);
            REQUIRE(f.renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));
            CHECK(f.read(*fallback)==expected);CHECK(f.renderer->worldSourceCommandStats().bank_draws==initial.bank_draws);
        }
        SECTION("compute-begin failure invalidates the enclosing borrowed owner")
        {
            const auto commits=ALMetalDevice::committedCommandBufferCount();
            commands->failVisibilityAfterBeginForTesting();
            REQUIRE(f.renderer->beginCommandBatch("E3 actual failed compute prefix",&f.error));
            auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);
            auto failed=f.renderer->render(scene,&f.error);CHECK_FALSE(failed);CHECK_FALSE(f.error.empty());
            CHECK_FALSE(f.renderer->commitCommandBatch(&f.error));CHECK_FALSE(f.renderer->commandBatchActive());
            CHECK(ALMetalDevice::committedCommandBufferCount()==commits);
            CHECK(completion->status()==ALMetalCommandStatus::unknown);
            std::string abandoned_error;CHECK_FALSE(completion->waitUntilCompleted(&abandoned_error));CHECK_FALSE(abandoned_error.empty());
            CHECK(f.renderer->worldSourceCommandStats().bank_draws==initial.bank_draws);
        }
    }
}

#include "almetal_source_routing_owner_cost.inl"
