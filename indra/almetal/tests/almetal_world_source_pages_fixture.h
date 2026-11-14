/**
 * @file almetal_world_source_pages_fixture.h
 * @brief Native source-page resources and real scalar encode fixture.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldsourcepages.h"
#include "../almetalworldmaterialdata.h"

namespace
{
[[maybe_unused]] void waitPageResources(const std::weak_ptr<Resources>& weak)
{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!weak.expired()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
    CHECK(weak.expired());
}
[[maybe_unused]] void waitPageTable(const std::weak_ptr<const ALMetalWorldResourceTable>& weak)
{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!weak.expired()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
    REQUIRE(weak.expired());
}
struct PageDescription {ALMetalWorldSimpleBatch source;};
struct PageFixture : Fixture
{
    std::unique_ptr<ALMetalShaderLibrary> library;
    ALMetalWorldMaterialDataCache cache;
    std::shared_ptr<PageDescription> cpu;
    ALMetalWorldSourceRegion region;
    ALMetalWorldSourceGlobals globals;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalDepthStencilState> depth;
    PageFixture():cache(*device)
    {
        library=ALMetalShaderLibrary::loadBuiltin(*device,&error);INFO(error);REQUIRE(library);
        cpu=std::make_shared<PageDescription>();cpu->source=batch(1);
        cpu->source.texcoord1_offset=cpu->source.texcoord2_offset=offsetof(Geometry,uv);
        publish(cpu->source);
        identity(region.modelview_projection);identity(region.modelview);
        for(unsigned axis=0;axis<3;++axis)region.normal_matrix[axis][axis]=1;
    }
    void publish(ALMetalWorldSimpleBatch& source)
    {
        REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
        REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
    }
    std::shared_ptr<ALMetalWorldSourcePages> pages(ALMetalWorldSourcePageLimits limits={})
    {
        auto value=ALMetalWorldSourcePages::create(*device,*library,1,limits,&error);
        INFO(error);REQUIRE(value);REQUIRE(value->supported());return value;
    }
    ALMetalWorldSourceRequest request(std::uint64_t id=1,std::uint64_t revision=1)
    {
        ALMetalWorldSourceRequest value;value.key={id,1,revision};value.source=&cpu->source;
        value.cpu_owner=cpu;value.native.owner=resources;
        for(const auto& buffer:resources->buffers)
        {
            if(buffer.get()==vertices)value.native.vertices=buffer;
            if(buffer.get()==indices)value.native.indices=buffer;
        }
        return value;
    }
    std::shared_ptr<const ALMetalWorldSourceFrameView> freeze(
        const std::shared_ptr<ALMetalWorldSourcePages>& pages,std::span<const ALMetalWorldSourceRequest> requests)
    {
        auto value=pages->freeze(requests,std::span(&region,1),globals,&error);
        INFO(error);REQUIRE(value);return value;
    }
    struct Encoded
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::unique_ptr<ALMetalCommandBuffer> command;
    };
    Encoded encode(const ALMetalWorldSourceFrameView& view,const ALMetalWorldSimpleBatch& source,
                   std::unique_ptr<ALMetalCommandBuffer> supplied_command={})
    {
        if(!pipeline)
        {
            ALMetalRenderPipelineDescriptor descriptor;descriptor.library=library.get();
            descriptor.vertex_function="almetal_world_material_source_rows_vertex";
            descriptor.fragment_function="almetal_world_material_source_rows_fragment";
            descriptor.color_attachment_count=4;
            for(unsigned slot=0;slot<4;++slot)descriptor.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
            descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
            pipeline=ALMetalRenderPipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(pipeline);
            ALMetalDepthStencilDescriptor dd;depth=ALMetalDepthStencilState::create(*device,dd,&error);REQUIRE(depth);
        }
        ALMetalRenderTargetDescriptor descriptor;descriptor.width=descriptor.height=side;descriptor.color_attachment_count=4;
        for(unsigned slot=0;slot<4;++slot)descriptor.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        Encoded value;value.target=ALMetalRenderTarget::create(*device,descriptor,&error);REQUIRE(value.target);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        auto pass=value.target->makeRenderPass(config,&error);REQUIRE(pass);
        value.command=std::move(supplied_command);
        if(!value.command)value.command=device->makeCommandBuffer("Source page reader test",&error);REQUIRE(value.command);
        auto encoder=ALMetalRenderEncoder::begin(*value.command,*pass,"Source page reader encoder",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));REQUIRE(encoder->setScissor({0,0,side,side},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));REQUIRE(view.bind(*encoder,*value.command,&error));
        // Rebinding the same immutable view must not repeat the native union
        // or grow completion pins, but still restores overwritten bindings.
        REQUIRE(view.bind(*encoder,*value.command,&error));
        REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,source.index_count,
            source.index_type,*source.indices,source.index_offset,view.rowId(0),&error));
        REQUIRE(encoder->end(&error));return value;
    }
};
}

