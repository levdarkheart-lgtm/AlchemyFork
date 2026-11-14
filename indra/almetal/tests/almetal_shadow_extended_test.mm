/**
 * @file almetal_shadow_extended_test.mm
 * @brief Actual native attachment/cutout indirect ABI and depth parity.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#include "../almetalshadowindirect.h"
#include "../almetalshadowprepare.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <memory>
#include <chrono>
#include <limits>
#include <thread>
#include <stdexcept>
#include <vector>

TEST_CASE("almetal: rigged and masked command tables preserve scalar shadow pixels",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error); INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        renderer->setFrameData(arena);
        constexpr std::size_t width = 32, height = 32;
        const std::array<std::array<float, 4>, 3> positions{{
            {{-0.7f,-0.7f,0.f,1.f}}, {{0.7f,-0.7f,0.f,1.f}}, {{0.f,0.7f,0.f,1.f}}}};
        const std::array<std::array<float, 4>, 3> weights{{
            {{0.5f,0.f,0.f,0.f}}, {{0.5f,0.f,0.f,0.f}}, {{0.5f,0.f,0.f,0.f}}}};
        const std::array<std::array<float, 2>, 3> uv{{{{0.25f,0.5f}},{{0.25f,0.5f}},{{0.25f,0.5f}}}};
        const std::array<std::uint8_t, 12> colors{{255,255,255,255,255,255,255,255,255,255,255,255}};
        constexpr auto weight_offset = sizeof(positions);
        constexpr auto uv_offset = weight_offset + sizeof(weights);
        constexpr auto color_offset = uv_offset + sizeof(uv);
        std::array<std::uint8_t, color_offset + sizeof(colors)> bytes{};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data()+weight_offset, weights.data(), sizeof(weights));
        std::memcpy(bytes.data()+uv_offset, uv.data(), sizeof(uv));
        std::memcpy(bytes.data()+color_offset, colors.data(), sizeof(colors));
        ALMetalBufferDescriptor bd; bd.length = bytes.size();
        auto vertices = ALMetalBuffer::create(*device, bd, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, bytes.data(), bytes.size(), &error));
        const std::array<std::uint16_t, 3> indices{{0,1,2}};
        bd.length = sizeof(indices);
        auto elements = ALMetalBuffer::create(*device, bd, &error); REQUIRE(elements);
        REQUIRE(elements->write(0, indices.data(), sizeof(indices), &error));
        ALMetalTextureDescriptor td;
        td.type = ALMetalTextureType::texture_2d_array;
        td.format = ALMetalPixelFormat::r32_float;
        td.width = width; td.height = height; td.array_length = 2;
        td.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        auto target = ALMetalTexture::create(*device, td, &error); REQUIRE(target);
        td = {}; td.width = 1; td.height = 1; td.format = ALMetalPixelFormat::rgba8_unorm;
        td.usage = ALMetalTextureUsage::shader_read;
        auto white = ALMetalTexture::create(*device, td, &error); REQUIRE(white);
        auto transparent = ALMetalTexture::create(*device, td, &error); REQUIRE(transparent);
        auto upload = device->makeCommandBuffer("Extended shadow mask fixtures", &error); REQUIRE(upload);
        const std::array<std::uint8_t, 4> zero_alpha{{255,255,255,0}};
        REQUIRE(white->upload(*upload, 0, 0, {}, colors.data(), 4, &error));
        REQUIRE(transparent->upload(*upload, 0, 0, {}, zero_alpha.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        std::uint64_t frame = 1;
        for (const bool rigged : {false,true})
        for (const auto coverage : {ALMetalWorldSpotShadowCoverage::opaque,
            ALMetalWorldSpotShadowCoverage::legacy_mask,
            ALMetalWorldSpotShadowCoverage::texture_mask,
            ALMetalWorldSpotShadowCoverage::pbr_mask})
        for (const bool reverse : {false,true})
        {
            if (!rigged && coverage == ALMetalWorldSpotShadowCoverage::opaque) continue;
            CAPTURE(rigged,coverage,reverse);
            arena->beginFrame(frame++);
            std::vector<float> pose(4 + 110*12, 0.f);
            for (std::size_t i=0;i<110;++i)
            { pose[4+i*12]=1.f; pose[4+i*12+5]=1.f; pose[4+i*12+10]=1.f; }
            pose[4+11] = 0.125f;
            auto record = ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),110,&error); REQUIRE(record);
            std::vector<ALMetalWorldSpotShadowBatch> batches(256);
            std::vector<ALMetalShadowBounds> bounds(batches.size());
            for (std::size_t i=0;i<batches.size();++i)
            {
                auto& batch = batches[i];
                batch.vertices=vertices.get(); batch.indices=elements.get();
                batch.vertex_count=3; batch.index_count=3; batch.coverage=coverage;
                for (std::size_t axis=0;axis<4;++axis) batch.modelview_projection[axis*4+axis]=1.f;
                batch.modelview_projection[12] = i%4==0 ? 4.f : float(i%5)*0.04f;
                batch.modelview_projection[14] = reverse ? 0.5f : 0.f;
                if (rigged)
                {
                    batch.skin_palette_record=record; batch.skin_palette=record->data();
                    batch.skin_palette_float_count=record->size(); batch.skin_joint_count=record->jointCount();
                    batch.weight_offset=weight_offset;
                }
                if (coverage!=ALMetalWorldSpotShadowCoverage::opaque)
                {
                    batch.texcoord_offset=uv_offset; batch.color_offset=color_offset;
                    batch.texture_count=1;
                    batch.coverageData().textures[0]=(i%3==0 ? transparent.get():white.get());
                    batch.coverageData().alpha_cutoffs[0]=0.5f;
                }
                bounds[i].conservative=true;
                bounds[i].min[0]=-0.7f; bounds[i].min[1]=-0.7f;
                bounds[i].max[0]=0.7f; bounds[i].max[1]=0.7f;
                bounds[i].min[2]=bounds[i].max[2]=rigged ? 0.125f : 0.f;
            }
            ALMetalWorldSpotShadowScene scene;
            scene.batches=batches.data(); scene.batch_count=batches.size();
            scene.width=width; scene.height=height; scene.reverse_z=reverse;
            scene.bulk_draws=false;
            std::array<ALMetalWorldVisibilityQuery,2> queries{};
            for(std::size_t q=0;q<queries.size();++q)
            {
                auto& query=queries[q];query.batch_offset=q==0?64:192;query.count_samples=1;
                for(std::size_t i=0;i<4;++i)query.modelview_projection[i*4+i]=1.f;
                query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=0.3f;
            }
            scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> scalar_queries,indirect_queries;
            REQUIRE(renderer->renderSpotShadow(*target,0,scene,&error,&scalar_queries));
            REQUIRE(scalar_queries);
            std::array<std::uint64_t,2> expected_queries{};
            for(std::size_t q=0;q<queries.size();++q)
                REQUIRE(scalar_queries->wait(q,expected_queries[q],&error));
            const auto before=renderer->shadowIndirectStats();
            scene.indirect_draws=true;
            REQUIRE(renderer->renderSpotShadow(*target,1,scene,&error,&indirect_queries));
            const auto after=renderer->shadowIndirectStats();
            CHECK(after.extended_prepared_draws-before.extended_prepared_draws==batches.size());
            std::array<float,width*height> scalar{},indirect{};
            REQUIRE(target->readback(*device,0,0,{},scalar.data(),width*sizeof(float),&error));
            REQUIRE(target->readback(*device,0,1,{},indirect.data(),width*sizeof(float),&error));
            CHECK(scalar==indirect);
            CHECK(scalar[height/2*width+width/2] != (reverse ? 0.f : 1.f));
            scene.gpu_visibility=true;
            scene.visibility_bounds=bounds.data();scene.visibility_bounds_count=bounds.size();
            for(const bool metal4:{false,true})
            {
                CAPTURE(metal4);
                scene.metal4_visibility=metal4;
                const auto visibility_before=renderer->shadowIndirectStats();
                REQUIRE(renderer->renderSpotShadow(*target,1,scene,&error,&indirect_queries));
                const auto visibility_after=renderer->shadowIndirectStats();
                CHECK(visibility_after.visibility_dispatches==visibility_before.visibility_dispatches+1);
                CHECK(visibility_after.gpu_generated_draws==visibility_before.gpu_generated_draws+batches.size());
                REQUIRE(target->readback(*device,0,1,{},indirect.data(),width*sizeof(float),&error));
                CHECK(scalar==indirect);
                CHECK(visibility_after.visibility_failures==0);
                REQUIRE(indirect_queries);
                for(std::size_t q=0;q<queries.size();++q)
                {
                    std::uint64_t value=0;
                    REQUIRE(indirect_queries->wait(q,value,&error));
                    CHECK(value==expected_queries[q]);
                }
            }
            scene.indirect_draws=false;scene.gpu_visibility=false;scene.stable_shadow_data=true;
            REQUIRE(renderer->renderSpotShadow(*target,1,scene,&error,&indirect_queries));
            REQUIRE(target->readback(*device,0,1,{},indirect.data(),width*sizeof(float),&error));
            CHECK(scalar==indirect);
            REQUIRE(indirect_queries);
            for(std::size_t q=0;q<queries.size();++q)
            {
                std::uint64_t value=0;REQUIRE(indirect_queries->wait(q,value,&error));
                CHECK(value==expected_queries[q]);
            }
            scene.indirect_draws=true;scene.gpu_visibility=true;scene.stable_shadow_data=false;
            if(rigged && coverage==ALMetalWorldSpotShadowCoverage::opaque && !reverse)
            {
                auto owner=std::make_shared<int>(7);
                std::weak_ptr<int> lease=owner;
                scene.source_owner=owner;
                REQUIRE(renderer->beginCommandBatch("Uncommitted source lease",&error));
                REQUIRE(renderer->renderSpotShadow(*target,1,scene,&error));
                scene.source_owner.reset();owner.reset();
                CHECK_FALSE(lease.expired());
                renderer->cancelCommandBatch();
                CHECK(lease.expired());
                // Metal4 compute has an independent lifetime pin. An abandoned
                // consumer cannot recycle its source ICB/allocator prematurely.
                REQUIRE(renderer->renderSpotShadow(*target,1,scene,&error));
                REQUIRE(target->readback(*device,0,1,{},indirect.data(),width*sizeof(float),&error));
                CHECK(scalar==indirect);
            }
        }
    }
}

TEST_CASE("almetal: GPU visibility preserves conservative edges and ordered query runs",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        std::array<ALMetalShadowVisibilityInput,12> inputs{};
        for(auto& input:inputs)
        {
            for(std::size_t i=0;i<4;++i)input.matrix[i*4+i]=1.f;
            for(std::size_t i=0;i<3;++i){input.minimum[i]=-0.1f;input.maximum[i]=0.1f;}
            input.config[0]=1;
        }
        inputs[1].matrix[12]=4.f; // culled X
        inputs[2].matrix[12]=4.f;inputs[2].config[0]=0; // unknown stays visible
        inputs[3].matrix[14]=4.f; // clipped Z
        inputs[4].matrix[14]=4.f;inputs[4].config[1]=1; // depth clamp keeps it
        inputs[5].matrix[15]=-1.f; // negative W fails visible
        inputs[6].matrix[3]=20.f; // box crosses eye plane
        inputs[7].minimum[0]=std::numeric_limits<float>::quiet_NaN();
        inputs[8].minimum[0]=1.000001f;inputs[8].maximum[0]=1.000002f; // conservative margin
        inputs[9].minimum[0]=1.f;inputs[9].maximum[0]=-1.f; // unknown invalid bounds
        inputs[10].matrix[12]=-4.f;inputs[11].matrix[13]=4.f;
        const std::array<ALMetalShadowVisibilityRun,3> runs{{{0,4},{4,4},{8,4}}};
        const std::array<ALMetalShadowVisibilityRun,3> expected{{{0,2},{4,4},{8,2}}};
        const std::array<ALMetalShadowVisibilityDraw,12> empty_draws{};
        for(const bool metal4:{false,true})
        {
            CAPTURE(metal4);
            auto visibility=ALMetalShadowVisibility::create(*device,&error);INFO(error);REQUIRE(visibility);
            if(metal4 && [native supportsFamily:MTLGPUFamilyMetal4])
                REQUIRE(visibility->metal4Supported());
            auto descriptor=[MTLIndirectCommandBufferDescriptor new];
            descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;
            descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=NO;
            descriptor.maxVertexBufferBindCount=7;descriptor.maxFragmentBufferBindCount=2;
            id<MTLIndirectCommandBuffer> destination=[native newIndirectCommandBufferWithDescriptor:descriptor
                maxCommandCount:inputs.size() options:MTLResourceStorageModePrivate];
            REQUIRE(destination);
            auto command=device->makeCommandBuffer("Conservative ordered visibility",&error);REQUIRE(command);
            auto batch=visibility->encode(*command,(__bridge void*)destination,
                inputs,runs,{},empty_draws,metal4,&error);INFO(error);REQUIRE(batch);
            CHECK(batch->usedMetal4()==(metal4 && visibility->metal4Supported()));
            REQUIRE(command->commitAndWait(&error));
            id<MTLBuffer> ranges=(__bridge id<MTLBuffer>)batch->executionRanges();
            REQUIRE(ranges.contents);
            CHECK(std::memcmp(ranges.contents,expected.data(),sizeof(expected))==0);
            CHECK(visibility->failures()==0);
            if(batch->usedMetal4())
            {
                // Independent compute completion pins inputs, ICBs, pipelines
                // and allocator even if its legacy consumer is abandoned and
                // all public resource/helper owners disappear immediately.
                auto abandoned=device->makeCommandBuffer("Abandoned Metal4 consumer",&error);REQUIRE(abandoned);
                batch=visibility->encode(*abandoned,(__bridge void*)destination,
                    inputs,runs,{},empty_draws,true,&error);REQUIRE(batch);
                abandoned.reset();visibility.reset();destination=nil;
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while(!batch->computeComplete() && std::chrono::steady_clock::now()<deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                REQUIRE(batch->computeComplete());
                ranges=(__bridge id<MTLBuffer>)batch->executionRanges();
                CHECK(std::memcmp(ranges.contents,expected.data(),sizeof(expected))==0);
            }
        }
    }
}

TEST_CASE("almetal: borrowed palette publication retains one immutable identity",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto arena=ALMetalWorldFrameData::create(*device,&error);REQUIRE(arena);
        std::array<float,16> pose{};pose[4]=pose[9]=pose[14]=1.f;
        auto record=ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),1,&error);REQUIRE(record);
        CHECK(record.use_count()==1);
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> lifetime=record;
        auto first=device->makeCommandBuffer("Borrowed palette first reader",&error);REQUIRE(first);
        auto second=device->makeCommandBuffer("Borrowed palette second reader",&error);REQUIRE(second);
        arena->beginFrame(1);
        ALMetalWorldDataSlice slice,repeated;
        REQUIRE(arena->publishPaletteRecord(*first,record.get(),slice,&error));
        CHECK(record.use_count()==2);
        for(std::size_t i=0;i<100;++i)
            REQUIRE(arena->publishPaletteRecord(*second,record.get(),repeated,&error));
        CHECK(record.use_count()==2);
        CHECK(slice.buffer==repeated.buffer);CHECK(slice.offset==repeated.offset);
        CHECK(arena->stats().record_publications==1);CHECK(arena->stats().record_reuses==100);
        CHECK_FALSE(arena->publishPaletteRecord(*second,nullptr,repeated,&error));
        CHECK(repeated.buffer==nullptr);
        record.reset();CHECK_FALSE(lifetime.expired());
        first.reset();arena->beginFrame(2);
        CHECK(lifetime.expired());
        CHECK(arena->stats().retired_pages==1);
        second.reset();arena->beginFrame(3);
        CHECK(arena->stats().retired_pages==0);
    }
}

TEST_CASE("almetal: parallel command generation preserves tiled run order and partial tiles",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto indirect=ALMetalShadowIndirectCommands::create(*device,&error);REQUIRE(indirect);
        const char* shader=R"metal(
#include <metal_stdlib>
using namespace metal;
struct Constants { float4x4 mvp; uint4 config; };
struct Out { float4 position [[position]]; float value; };
vertex Out ordered_vertex(uint index [[vertex_id]],device const float4* positions [[buffer(0)]],
                         constant Constants& constants [[buffer(1)]])
{ return {constants.mvp*positions[index],float(constants.config.y)}; }
fragment float ordered_fragment(Out in [[stage_in]]) { return in.value; }
)metal";
        auto library=ALMetalShaderLibrary::compileSource(*device,shader,"Tiled stable command order",&error);REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;pd.library=library.get();pd.vertex_function="ordered_vertex";
        pd.fragment_function="ordered_fragment";pd.colors[0].format=ALMetalPixelFormat::r32_float;
        pd.depth_stencil_format=ALMetalPixelFormat::depth32_float;pd.support_indirect_commands=true;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error);REQUIRE(pipeline);
        ALMetalDepthStencilDescriptor dd;dd.depth_compare=ALMetalCompareFunction::always;
        auto depth=ALMetalDepthStencilState::create(*device,dd,&error);REQUIRE(depth);
        constexpr std::size_t count=1027;
        struct alignas(16) Constants {float mvp[16]{};std::uint32_t config[4]{};};
        static_assert(sizeof(Constants)==80);
        std::vector<Constants> values(count);
        for(std::size_t i=0;i<count;++i)
        {std::memcpy(values[i].mvp,alMetalShadowIdentity.data(),64);values[i].config[1]=std::uint32_t(i+1);}
        const std::array<float,12> triangle{{-1.f,-1.f,.5f,1.f,3.f,-1.f,.5f,1.f,-1.f,3.f,.5f,1.f}};
        const std::array<std::uint16_t,3> elements{{0,1,2}};
        const auto buffer=[&](const void* bytes,std::size_t length) {
            ALMetalBufferDescriptor bd;bd.length=length;
            auto result=ALMetalBuffer::create(*device,bd,&error);REQUIRE(result);
            REQUIRE(result->write(0,bytes,length,&error));return result;
        };
        auto positions=buffer(triangle.data(),sizeof(triangle));
        auto indices=buffer(elements.data(),sizeof(elements));
        auto constants=buffer(values.data(),values.size()*sizeof(Constants));
        std::vector<ALMetalShadowIndirectDraw> draws(count);
        std::vector<ALMetalShadowVisibilityInput> inputs(count);
        for(std::size_t i=0;i<count;++i)
        {
            auto& draw=draws[i];draw.positions=positions.get();draw.indices=indices.get();draw.constants=constants.get();
            draw.vertex_count=3;draw.index_count=3;draw.constant_offset=i*sizeof(Constants);
            std::memcpy(inputs[i].matrix,alMetalShadowIdentity.data(),64);
            inputs[i].minimum[0]=inputs[i].minimum[1]=inputs[i].minimum[2]=-.1f;
            inputs[i].maximum[0]=inputs[i].maximum[1]=inputs[i].maximum[2]=.1f;
            inputs[i].config[0]=1;
            // Classification is an independent helper contract here: actual
            // triangles all cover the target, so any incorrect cull or source
            // ordering is observable in the last source identity per run.
            if(i%5==1)inputs[i].matrix[12]=4.f;
        }
        const std::array<ALMetalShadowVisibilityRun,3> runs{{{0,257},{257,511},{768,259}}};
        std::array<float,3> expected_last{};
        for(std::size_t r=0;r<runs.size();++r)
        {
            for(std::size_t i=runs[r].first;i<runs[r].first+runs[r].count;++i)
                if(i%5!=1)expected_last[r]=float(i+1);
        }
        ALMetalRenderTargetDescriptor td;td.width=3;td.height=2;
        td.colors[0].format=ALMetalPixelFormat::r32_float;td.depth_stencil_format=ALMetalPixelFormat::depth32_float;
        auto target=ALMetalRenderTarget::create(*device,td,&error);REQUIRE(target);
        for(const bool metal4:{false,true})
        {
            CAPTURE(metal4);
            auto command=device->makeCommandBuffer("Parallel tiled run order",&error);REQUIRE(command);
            auto batch=indirect->prepare(*command,draws,&error,true);REQUIRE(batch);
            REQUIRE(indirect->prepareVisibility(*command,*batch,inputs,runs,metal4,&error));
            auto pass=target->makeRenderPass({},&error);REQUIRE(pass);
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Tiled run order output",&error);REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
            REQUIRE(encoder->setViewport({0.,0.,3.,2.,0.,1.},&error));REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
            for(std::size_t r=0;r<runs.size();++r)
            {
                REQUIRE(encoder->setScissor({std::uint32_t(r),0,1,2},&error));
                REQUIRE(indirect->execute(*encoder,*batch,runs[r].first,runs[r].count,&error));
            }
            REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));
            std::array<float,6> pixels{};
            REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},pixels.data(),3*sizeof(float),&error));
            for(std::size_t p=0;p<pixels.size();++p)CHECK(pixels[p]==expected_last[p%3]);
        }
    }
}

TEST_CASE("almetal: shadow frame admission and settlement follow native commit",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto renderer=ALMetalWorldGBufferRenderer::create(*device,&error);REQUIRE(renderer);
        const auto owner_thread=std::this_thread::get_id();
        unsigned committed=0,cancelled=0;
        CHECK_FALSE(renderer->observeCommandBatchSettlement([&](bool){FAIL("Unregistered observer must not run");},&error));
        for(unsigned view=0;view<6;++view)
        {
            CAPTURE(view);
            REQUIRE(renderer->beginShadowFrameBatch(100,"Six-view shadow frame",&error));
            REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){
                CHECK(std::this_thread::get_id()==owner_thread);
                CHECK_FALSE(renderer->commandBatchActive());
                CHECK(value);++committed;
            },&error));
            CHECK(committed==view);
            REQUIRE(renderer->commitCommandBatch(&error));
            CHECK(committed==view+1);
        }
        CHECK_FALSE(renderer->beginShadowFrameBatch(100,"Seventh view rejected",&error));
        CHECK(error.find("six-invocation")!=std::string::npos);
        // Reload must preserve both the native retirement deque and its frame
        // policy, including a completed frame's invocation bound.
        REQUIRE(renderer->reloadShaders(&error));
        CHECK_FALSE(renderer->beginShadowFrameBatch(100,"Reload preserves frame bound",&error));
        REQUIRE(renderer->beginCommandBatch("Full-world budget remains separate",&error));
        REQUIRE(renderer->commitCommandBatch(&error));
        CHECK_FALSE(renderer->beginShadowFrameBatch(100,"World batch does not reset shadows",&error));
        for(unsigned attempt=0;attempt<8;++attempt)
        {
            REQUIRE(renderer->beginShadowFrameBatch(101,"Cancelled reservation can retry",&error));
            REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){CHECK_FALSE(value);++cancelled;},&error));
            renderer->cancelCommandBatch();renderer->cancelCommandBatch();
            CHECK(cancelled==attempt+1);
        }
        REQUIRE(renderer->beginShadowFrameBatch(101,"Observer reentrant followup",&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){
            CHECK(value);
            REQUIRE(renderer->beginCommandBatch("Observer starts next transaction",&error));
            REQUIRE(renderer->observeCommandBatchSettlement([&](bool next){CHECK_FALSE(next);++cancelled;},&error));
        },&error));
        REQUIRE(renderer->commitCommandBatch(&error));
        CHECK(renderer->commandBatchActive());
        CHECK(cancelled==8);
        renderer->cancelCommandBatch();CHECK(cancelled==9);
        // Ordinary probe-like invocations can share an external display
        // serial. Admission consumes monotonic logical shadow groups, each
        // with its own complete four-view bound, rather than six views for
        // the whole display. The native API deliberately has no display ID.
        const auto probe_commands=ALMetalDevice::committedCommandBufferCount();
        for(std::uint64_t group=200;group<208;++group)
        for(unsigned view=0;view<4;++view)
        {
            REQUIRE(renderer->beginShadowFrameBatch(group,"Four-view logical probe group",&error));
            REQUIRE(renderer->commitCommandBatch(&error));
        }
        CHECK(ALMetalDevice::committedCommandBufferCount()-probe_commands==32);
        REQUIRE(renderer->beginShadowFrameBatch(208,"Destructor settles outstanding reservation",&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){CHECK_FALSE(value);++cancelled;},&error));
        renderer.reset();CHECK(cancelled==10);
    }
}

TEST_CASE("almetal: settlement observer exceptions cannot change committed status",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto renderer=ALMetalWorldGBufferRenderer::create(*device,&error);REQUIRE(renderer);
        unsigned callbacks=0;
        const auto before=renderer->shadowIndirectStats().settlement_observer_failures;
        REQUIRE(renderer->beginShadowFrameBatch(1,"Throwing commit observer",&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){
            CHECK(value);++callbacks;throw std::runtime_error("intentional observer failure");
        },&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){CHECK(value);++callbacks;},&error));
        REQUIRE(renderer->commitCommandBatch(&error));
        CHECK(callbacks==2);
        CHECK(renderer->shadowIndirectStats().settlement_observer_failures==before+1);
        REQUIRE(renderer->beginShadowFrameBatch(2,"Throwing cancellation observer",&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){CHECK_FALSE(value);++callbacks;throw 7;},&error));
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool value){CHECK_FALSE(value);++callbacks;},&error));
        renderer->cancelCommandBatch();
        CHECK(callbacks==4);
        CHECK(renderer->shadowIndirectStats().settlement_observer_failures==before+2);
        renderer->cancelCommandBatch();CHECK(callbacks==4);
    }
}

TEST_CASE("almetal: one shadow transaction preserves four sun and two spot slices",
          "[almetal][shadow-extended]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto renderer=ALMetalWorldGBufferRenderer::create(*device,&error);REQUIRE(renderer);
        auto arena=ALMetalWorldFrameData::create(*device,&error);REQUIRE(arena);
        renderer->setFrameData(arena);
        constexpr std::size_t width=24,height=24,views=6;
        const std::array<float,24> geometry{{
            -.7f,-.7f,0.f,1.f,.7f,-.7f,0.f,1.f,0.f,.7f,0.f,1.f,
            .5f,0.f,0.f,0.f,.5f,0.f,0.f,0.f,.5f,0.f,0.f,0.f}};
        const std::array<std::uint16_t,3> indices{{0,1,2}};
        ALMetalBufferDescriptor bd;bd.length=sizeof(geometry);
        auto vb=ALMetalBuffer::create(*device,bd,&error);REQUIRE(vb);
        REQUIRE(vb->write(0,geometry.data(),sizeof(geometry),&error));
        std::shared_ptr<const ALMetalBuffer> vertices=std::move(vb);
        bd.length=sizeof(indices);
        auto ib=ALMetalBuffer::create(*device,bd,&error);REQUIRE(ib);
        REQUIRE(ib->write(0,indices.data(),sizeof(indices),&error));
        std::shared_ptr<const ALMetalBuffer> elements=std::move(ib);
        std::array<float,16> pose{};pose[4]=pose[9]=pose[14]=1.f;pose[15]=.125f;
        auto palette=ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),1,&error);REQUIRE(palette);
        ALMetalWorldSpotShadowBatch prototype;
        prototype.vertices=vertices.get();prototype.indices=elements.get();
        prototype.vertex_count=3;prototype.index_count=3;prototype.weight_offset=48;
        prototype.skin_palette_record=palette;prototype.skin_palette=palette->data();
        prototype.skin_palette_float_count=palette->size();prototype.skin_joint_count=1;
        auto source=ALMetalShadowSourceRecord::create(1,prototype,vertices,elements,{},&error);REQUIRE(source);
        ALMetalTextureDescriptor td;td.type=ALMetalTextureType::texture_2d_array;
        td.format=ALMetalPixelFormat::r32_float;td.width=width;td.height=height;td.array_length=views;
        td.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
        auto reference=ALMetalTexture::create(*device,td,&error);REQUIRE(reference);
        auto candidate=ALMetalTexture::create(*device,td,&error);REQUIRE(candidate);
        std::array<ALMetalWorldVisibilityQuery,views> queries{};
        for(auto& query:queries)
        {
            query.batch_offset=1;query.count_samples=1;
            std::memcpy(query.modelview_projection,alMetalShadowIdentity.data(),64);
            query.center[0]=.55f;query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.35f;
        }
        std::array<std::uint64_t,views> expected_queries{};
        const auto draw_frame=[&](bool combined,ALMetalTexture& target,std::uint64_t frame) {
            arena->beginFrame(frame);
            auto packet=std::make_shared<std::vector<ALMetalShadowPrepareInput>>(views);
            std::weak_ptr<const void> packet_lifetime=packet;
            std::array<ALMetalWorldSpotShadowBatch,views> output;
            for(std::size_t view=0;view<views;++view)
            {
                auto& input=(*packet)[view];input.source=source;input.source_generation=1;
                input.epoch=frame;input.palette=palette;input.view[14]=float(view)*.0625f;
            }
            REQUIRE(alMetalPrepareShadowBatches(*packet,output,frame));
            for(const auto& batch:output)CHECK(batch.hasBorrowedInputs());
            std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,views> results;
            std::vector<std::shared_ptr<const ALMetalCommandCompletion>> completions;
            const auto before=ALMetalDevice::committedCommandBufferCount();
            unsigned settlements=0;
            if(combined)REQUIRE(renderer->beginShadowFrameBatch(frame,"Four sun and two spot passes",&error));
            for(std::size_t view=0;view<views;++view)
            {
                CAPTURE(combined,view);
                if(!combined)REQUIRE(renderer->beginShadowFrameBatch(frame,"Separate native shadow view",&error));
                REQUIRE(renderer->observeCommandBatchSettlement([&](bool committed){CHECK(committed);++settlements;},&error));
                ALMetalWorldSpotShadowScene scene;scene.source_owner=packet;
                scene.batches=&output[view];scene.batch_count=1;scene.width=width;scene.height=height;
                scene.depth_clamp=view<4;scene.reverse_z=view%2;
                scene.visibility_queries=&queries[view];scene.visibility_query_count=1;
                REQUIRE(renderer->renderSpotShadow(target,view,scene,&error,&results[view]));
                REQUIRE(results[view]);CHECK(results[view]->status()==ALMetalVisibilityResultBatch::Status::pending);
                if(combined&&view==2)
                {
                    auto rejected=scene;rejected.width=0;
                    CHECK_FALSE(renderer->renderSpotShadow(target,view,rejected,&error));
                    // Whole-scene preflight rejection has encoded nothing;
                    // it must preserve the healthy prefix (PPD-001 isolation).
                    CHECK(renderer->commandBatchActive());
                }
                std::uint64_t unavailable=123;
                CHECK_FALSE(results[view]->result(0,unavailable));
                if(!combined)
                {
                    completions.push_back(renderer->commandBatchCompletion());
                    REQUIRE(renderer->commitCommandBatch(&error));
                }
                else CHECK(settlements==0);
            }
            if(combined)
            {
                // The command owns the detached source packet even after its
                // caller releases all six borrowed views before submission.
                packet.reset();CHECK_FALSE(packet_lifetime.expired());
                completions.push_back(renderer->commandBatchCompletion());
                REQUIRE(renderer->commitCommandBatch(&error));
            }
            CHECK(settlements==views);
            CHECK(ALMetalDevice::committedCommandBufferCount()-before==(combined?1:views));
            for(std::size_t view=0;view<views;++view)
            {
                std::uint64_t value=0;REQUIRE(results[view]->wait(0,value,&error));
                CHECK(results[view]->status()==ALMetalVisibilityResultBatch::Status::ready);
                if(combined)CHECK(value==expected_queries[view]);
                else {expected_queries[view]=value;CHECK(value>0);}
            }
            for(const auto& completion:completions){REQUIRE(completion);CHECK(completion->isComplete());}
            packet.reset();
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!packet_lifetime.expired()&&std::chrono::steady_clock::now()<deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            CHECK(packet_lifetime.expired());
        };
        draw_frame(false,*reference,10);draw_frame(true,*candidate,11);
        std::array<float,width*height> expected{},actual{};
        for(std::uint32_t view=0;view<views;++view)
        {
            CAPTURE(view);
            REQUIRE(reference->readback(*device,0,view,{},expected.data(),width*sizeof(float),&error));
            REQUIRE(candidate->readback(*device,0,view,{},actual.data(),width*sizeof(float),&error));
            CHECK(expected==actual);
            CHECK(expected[height/2*width+width/2]!=(view%2?0.f:1.f));
        }
    }
}
