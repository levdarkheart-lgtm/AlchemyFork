/**
 * @file almetal_retained_background_test.mm
 * @brief Real GPU regressions for retained pre-UI color/depth ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */
#include "../almetalretainedbackground.h"
#include "../almetaluirenderer.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
using Backing = ALMetalRetainedBackground;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> scheduler;
    std::unique_ptr<ALMetalUIRenderer> ui;
    std::unique_ptr<ALMetalBuffer> vertices, indices;
    std::unique_ptr<ALMetalShaderLibrary> pattern_library;
    std::unique_ptr<ALMetalRenderPipeline> pattern_pipeline;
    std::unique_ptr<ALMetalDepthStencilState> pattern_depth;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        scheduler = ALMetalFrameScheduler::create(*device, 3, &error); REQUIRE(scheduler);
        ui = ALMetalUIRenderer::createForTexture(*device, *scheduler,
            ALMetalPixelFormat::bgra8_unorm, &error); INFO(error); REQUIRE(ui);
        const float positions[3][4]={{-1,-1,0,1},{3,-1,0,1},{-1,3,0,1}};
        std::array<std::uint8_t, 84> data{};
        std::memcpy(data.data(), positions, sizeof(positions));
        std::memset(data.data()+72,255,12);
        ALMetalBufferDescriptor bd; bd.length=data.size();
        bd.storage=ALMetalStorageMode::shared; bd.label="Retained UI fullscreen SoA";
        vertices=ALMetalBuffer::create(*device,bd,&error); REQUIRE(vertices);
        REQUIRE(vertices->write(0,data.data(),data.size(),&error));
        const std::array<std::uint16_t,3> ids{0,1,2};
        bd.length=sizeof(ids); bd.label="Retained UI HUD indexed triangle";
        indices=ALMetalBuffer::create(*device,bd,&error); REQUIRE(indices);
        REQUIRE(indices->write(0,ids.data(),sizeof(ids),&error));
    }
    Backing::Context context(bool reverse=false, std::uint32_t w=32,
                             std::uint32_t h=16, std::uint64_t generation=1)
    {
        // A headless compatibility token, not a simulated CAMetalLayer resize.
        return {device.get(),this,generation,w,h,reverse};
    }
    ALMetalRenderTargetDescriptor descriptor(const Backing::Context& c)
    {
        ALMetalRenderTargetDescriptor d; d.width=c.width; d.height=c.height;
        d.colors[0].format=ALMetalPixelFormat::bgra8_unorm;
        d.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_shader_read=true; d.label="Retained immutable world pair";
        return d;
    }
    void clearWorld(ALMetalRenderTarget& target, bool reverse)
    {
        ALMetalRenderPassConfig pc;
        pc.colors[0].clear={0,0,1,1};
        pc.depth_store=ALMetalStoreAction::store;
        pc.stencil_store=ALMetalStoreAction::store;
        pc.clear_depth=reverse?.625:.375;
        auto pass=target.makeRenderPass(pc,&error); REQUIRE(pass);
        auto command=device->makeCommandBuffer("Retained world GPU producer",&error); REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"World clear",&error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commit(&error));
    }
    static std::array<std::uint8_t,4> pattern(unsigned x,unsigned y)
    {
        return {static_cast<std::uint8_t>((x%4)*64),
            static_cast<std::uint8_t>((y%4)*64),
            static_cast<std::uint8_t>(((x/4+y/4)%2)*128),255};
    }
    void patternWorld(ALMetalRenderTarget& target)
    {
        if (!pattern_pipeline)
        {
            pattern_library=ALMetalShaderLibrary::loadBuiltin(*device,&error); REQUIRE(pattern_library);
            ALMetalRenderPipelineDescriptor pd;
            pd.library=pattern_library.get(); pd.vertex_function="almetal_resample_vertex";
            pd.fragment_function="almetal_ui_depth_clear_fragment";
            pd.colors[0].format=ALMetalPixelFormat::bgra8_unorm;
            pd.colors[0].write_mask=ALMetalColorWriteMask::none;
            pd.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
            pd.label="Retained resize independent quadrant depth producer";
            pattern_pipeline=ALMetalRenderPipeline::create(*device,pd,&error); REQUIRE(pattern_pipeline);
            ALMetalDepthStencilDescriptor dd; dd.depth_compare=ALMetalCompareFunction::always;
            dd.label="Retained quadrant depth write";
            pattern_depth=ALMetalDepthStencilState::create(*device,dd,&error); REQUIRE(pattern_depth);
        }
        const auto w=target.descriptor().width,h=target.descriptor().height;
        std::vector<std::uint8_t> color(w*h*4);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
        {
            const auto p=pattern(x,y);
            std::copy(p.begin(),p.end(),color.begin()+(y*w+x)*4);
        }
        auto command=device->makeCommandBuffer("Retained patterned source",&error); REQUIRE(command);
        // This target owns mutable storage; the target accessor is read-only
        // because ordinary render consumers do not upload attachment pixels.
        REQUIRE(const_cast<ALMetalTexture*>(target.colorTexture(0))->upload(
            *command,0,0,{},color.data(),w*4,&error));
        ALMetalRenderPassConfig pc; pc.colors[0].load=ALMetalLoadAction::load;
        pc.depth_store=ALMetalStoreAction::store; pc.stencil_store=ALMetalStoreAction::store;
        auto pass=target.makeRenderPass(pc,&error); REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Quadrant raw depth",&error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pattern_pipeline,&error));
        REQUIRE(encoder->setDepthStencilState(pattern_depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,static_cast<double>(w),static_cast<double>(h),0,1},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        for(unsigned q=0;q<4;++q)
        {
            const float z=.125f+.25f*q;
            REQUIRE(encoder->setScissor({(q%2)*(w/2),(q/2)*(h/2),w/2,h/2},&error));
            REQUIRE(encoder->setFragmentBytes(0,&z,sizeof(z),&error));
            REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
        }
        REQUIRE(encoder->end(&error)); REQUIRE(command->commit(&error));
    }
    std::vector<std::uint8_t> compose(const Backing::Lease& record,
        ALMetalScissorRect region, const std::array<float,4>& color,
        bool hud=false, bool occlusion=false)
    {
        REQUIRE(record); const auto& c=record->context;
        REQUIRE(ui->setFrameBackground(record->colorTexture(),&error));
        REQUIRE(ui->setFrameDepth(record->depthTexture(),&error));
        const ALMetalScissorRect full{0,0,c.width,c.height};
        REQUIRE(ui->clearFrameDepthRects(&full,1,c.reverse_z?0.f:1.f,&error));
        REQUIRE(ui->restoreFrameDepthRects(&full,1,ALMetalCompareFunction::always,&error));
        ALMetalUIBatch batch; batch.vertices=vertices.get();
        batch.texcoord_offset=48; batch.color_offset=72; batch.vertex_count=3;
        batch.shader=ALMetalUIShader::uniform_color; batch.blend.enabled=false;
        batch.depth.enabled=true; batch.depth.write_enabled=false;
        batch.depth.reverse_z=c.reverse_z; batch.depth.compare=ALMetalCompareFunction::always;
        if(occlusion) batch.depth.compare=c.reverse_z
            ? ALMetalCompareFunction::greater_equal:ALMetalCompareFunction::less_equal;
        batch.target_width=c.width; batch.target_height=c.height;
        batch.viewport={0,0,static_cast<double>(c.width),static_cast<double>(c.height),0,1};
        batch.scissor=region; std::copy(color.begin(),color.end(),batch.solid_color);
        for(unsigned i=0;i<4;++i)
        {
            batch.matrices.modelview[i*4+i]=1; batch.matrices.projection[i*4+i]=1;
            batch.matrices.modelview_projection[i*4+i]=1;
            batch.matrices.inverse_projection[i*4+i]=1; batch.matrices.texture0[i*4+i]=1;
        }
        if(occlusion) batch.matrices.modelview_projection[14]=c.reverse_z?.5f:0.f;
        bool drawn=false;
        if (hud)
        {
            ALMetalHUDIndexedBatch attached;
            attached.vertices=vertices.get(); attached.vertex_count=3;
            attached.indices=indices.get(); attached.index_count=3;
            attached.shader=ALMetalHUDShader::debug;
            attached.matrices=batch.matrices; attached.blend=batch.blend;
            attached.depth=batch.depth; attached.viewport=batch.viewport;
            attached.scissor=batch.scissor; attached.target_width=c.width;
            attached.target_height=c.height; attached.double_sided=true;
            std::copy(color.begin(),color.end(),attached.highlight_color);
            drawn=ui->drawHUDIndexed(attached,&error);
        }
        else
        {
            drawn=ui->draw(batch,&error);
        }
        INFO(error);
        REQUIRE(drawn);
        REQUIRE(ui->endFrame(&error));
        std::vector<std::uint8_t> pixels(c.width*c.height*4);
        REQUIRE(ui->outputTexture()->readback(*device,0,0,{},pixels.data(),c.width*4,&error));
        std::vector<float> source_depth(c.width*c.height), output_depth(c.width*c.height);
        REQUIRE(record->depthTexture()->readback(*device,0,0,{},source_depth.data(),c.width*4,&error));
        REQUIRE(ui->outputDepthTexture()->readback(*device,0,0,{},output_depth.data(),c.width*4,&error));
        CHECK(output_depth==source_depth); // Actual localized clear+restore path, no epsilon.
        return pixels;
    }
    void check(const Backing::Context& c, const std::vector<std::uint8_t>& pixels,
        ALMetalScissorRect region, std::array<std::uint8_t,4> overlay,
        std::array<std::uint8_t,4> world={255,0,0,255})
    {
        for(unsigned y=0;y<c.height;++y) for(unsigned x=0;x<c.width;++x)
        {
            const auto expected=x>=region.x&&x<region.x+region.width&&
                y>=region.y&&y<region.y+region.height?overlay:world;
            for(unsigned k=0;k<4;++k) CHECK(pixels[(y*c.width+x)*4+k]==expected[k]);
        }
    }
};
}

TEST_CASE("almetal: half and double world resolutions preserve color rows and nearest HUD depth",
          "[almetal][retained-background]")
{
    Fixture f; Backing cache;
    for(bool reverse:{false,true}) for(bool doubled:{false,true})
    {
        const auto c=f.context(reverse);
        const auto source_context=f.context(reverse,doubled?64:16,doubled?32:8);
        std::shared_ptr<ALMetalRenderTarget> source=
            ALMetalRenderTarget::create(*f.device,f.descriptor(source_context),&f.error);
        REQUIRE(source); f.patternWorld(*source);
        REQUIRE(cache.publish(c,100,source,source,&f.error));
        auto retained=cache.find(c); REQUIRE(retained);
        CHECK(retained->colorTexture()->descriptor().width==c.width);
        CHECK(retained->depthTexture()->descriptor().height==c.height);
        CHECK(retained->colorTexture()->nativeHandle()!=source->colorTexture(0)->nativeHandle());
        source.reset(); // The scaler command owns the producer until completion.
        const auto w=source_context.width,h=source_context.height;
        std::vector<std::uint8_t> color(c.width*c.height*4);
        std::vector<float> depth(c.width*c.height);
        REQUIRE(retained->colorTexture()->readback(*f.device,0,0,{},color.data(),c.width*4,&f.error));
        REQUIRE(retained->depthTexture()->readback(*f.device,0,0,{},depth.data(),c.width*4,&f.error));
        const auto mirror=[](int n,int size){return n<0?-n-1:n>=size?2*size-n-1:n;};
        bool exact=true;
        for(unsigned y=0;y<c.height;++y) for(unsigned x=0;x<c.width;++x)
        {
            const float sx=(x+.5f)*w/c.width-.5f,sy=(y+.5f)*h/c.height-.5f;
            const int ix=static_cast<int>(std::floor(sx)),iy=static_cast<int>(std::floor(sy));
            const float fx=sx-ix,fy=sy-iy;
            const auto a=f.pattern(mirror(ix,w),mirror(iy,h));
            const auto b=f.pattern(mirror(ix+1,w),mirror(iy,h));
            const auto d=f.pattern(mirror(ix,w),mirror(iy+1,h));
            const auto e=f.pattern(mirror(ix+1,w),mirror(iy+1,h));
            for(unsigned k=0;k<4;++k)
            {
                const auto expected=static_cast<std::uint8_t>(std::lround(
                    (a[k]*(1-fx)+b[k]*fx)*(1-fy)+(d[k]*(1-fx)+e[k]*fx)*fy));
                exact=exact&&color[(y*c.width+x)*4+k]==expected;
            }
            const unsigned nx=static_cast<unsigned>((x+.5f)*w/c.width);
            const unsigned ny=static_cast<unsigned>((y+.5f)*h/c.height);
            const float expected_z=.125f+.25f*((nx>=w/2)+2*(ny>=h/2));
            exact=exact&&depth[y*c.width+x]==expected_z;
        }
        CHECK(exact); // All bytes and depth values, no tolerance or Y flip.
        const auto composed=f.compose(retained,{0,0,c.width,c.height},{1,0,0,1},true,true);
        unsigned visible=0,hidden=0; bool hud_exact=true;
        for(unsigned p=0;p<c.width*c.height;++p)
        {
            const bool passes=reverse?.5f>=depth[p]:.5f<=depth[p];
            passes?++visible:++hidden;
            for(unsigned k=0;k<4;++k)
                hud_exact=hud_exact&&composed[p*4+k]==(passes?std::array<std::uint8_t,4>{0,0,255,255}[k]:color[p*4+k]);
        }
        CHECK(visible>0); CHECK(hidden>0); CHECK(hud_exact);
    }
}

TEST_CASE("almetal: retained world remains immutable while fresh UI moves",
          "[almetal][retained-background]")
{
    Fixture f;
    for(bool reverse:{false,true})
    {
        const auto c=f.context(reverse);
        auto pool=ALMetalRenderTargetPool::create(*f.device,4*1024*1024); REQUIRE(pool);
        std::shared_ptr<ALMetalRenderTarget> world=pool->acquire(f.descriptor(c),&f.error); REQUIRE(world);
        f.clearWorld(*world,reverse);
        const void* original=world->colorTexture(0)->nativeHandle();
        std::weak_ptr<ALMetalRenderTarget> weak=world;
        Backing cache; REQUIRE(cache.publish(c,42,world,world,&f.error));
        auto retained=cache.find(c); REQUIRE(retained);
        CHECK(retained->colorTexture()->nativeHandle()==original);
        world.reset(); CHECK_FALSE(weak.expired()); CHECK(pool->cachedTargetCount()==0);
        // A retained pool lease cannot be recycled for an unrelated frame.
        auto other=pool->acquire(f.descriptor(c),&f.error); REQUIRE(other);
        CHECK(other->colorTexture(0)->nativeHandle()!=original);
        const ALMetalScissorRect a{0,0,8,16},b{24,0,8,16},d{12,0,8,16};
        f.check(c,f.compose(retained,a,{1,0,0,1}),a,{0,0,255,255});
        // No new world publication: the previous UI MUST disappear.
        f.check(c,f.compose(cache.find(c),b,{0,1,0,1}),b,{0,255,0,255});
        f.check(c,f.compose(cache.find(c),d,{1,1,0,1}),d,{0,255,255,255});
        auto mismatch=c; ++mismatch.surface_generation; CHECK_FALSE(cache.find(mismatch));
        mismatch=c; ++mismatch.width; CHECK_FALSE(cache.find(mismatch));
        mismatch=c; mismatch.reverse_z=!reverse; CHECK_FALSE(cache.find(mismatch));
        CHECK_FALSE(cache.publish(c,99,{}, {},&f.error));
        CHECK(cache.find(c)==retained);
        cache.clear(); CHECK_FALSE(weak.expired());
        retained.reset(); CHECK(weak.expired());
        CHECK(pool->cachedTargetCount()==1);
        const auto reuses=pool->stats().reuses;
        auto recycled=pool->acquire(f.descriptor(c),&f.error); REQUIRE(recycled);
        CHECK(recycled->colorTexture(0)->nativeHandle()==original);
        CHECK(pool->stats().reuses==reuses+1);
    }
}

TEST_CASE("almetal: retained aliases outlive borrowed wrappers and resize uses fresh clear depth",
          "[almetal][retained-background]")
{
    Fixture f; const auto c=f.context();
    auto external=ALMetalRenderTarget::create(*f.device,f.descriptor(c),&f.error); REQUIRE(external);
    f.clearWorld(*external,false);
    auto d=f.descriptor(c); d.colors[0].texture=external->colorTexture(0);
    d.depth_stencil_texture=external->depthStencilTexture();
    std::shared_ptr<ALMetalRenderTarget> borrowed=ALMetalRenderTarget::create(*f.device,d,&f.error); REQUIRE(borrowed);
    Backing cache; REQUIRE(cache.publish(c,7,borrowed,borrowed,&f.error));
    auto lease=cache.find(c); REQUIRE(lease);
    borrowed.reset(); external.reset(); // Raw borrowed pointers are now dead.
    const ALMetalScissorRect block{0,0,8,16};
    f.check(c,f.compose(lease,block,{1,0,0,1}),block,{0,0,255,255});
    for(bool reverse:{false,true})
    {
        const auto resized=f.context(reverse,40,20,2);
        CHECK_FALSE(cache.find(resized));
        auto cleared=Backing::makeCleared(resized,&f.error); INFO(f.error); REQUIRE(cleared);
        CHECK(cleared->source_frame==std::numeric_limits<std::uint64_t>::max());
        CHECK(cache.find(c)==lease); // Cleared backing does not replace world publication.
        const ALMetalScissorRect right{32,0,8,20},left{0,0,8,20};
        f.check(resized,f.compose(cleared,right,{0,1,0,1}),right,{0,255,0,255},{0,0,0,255});
        f.check(resized,f.compose(cleared,left,{1,0,0,1},true),left,{0,0,255,255},{0,0,0,255});
        std::vector<float> depth(resized.width*resized.height);
        REQUIRE(cleared->depthTexture()->readback(*f.device,0,0,{},depth.data(),resized.width*4,&f.error));
        for(float z:depth) CHECK(z==(reverse?0.f:1.f));
    }
}
