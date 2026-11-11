/**
 * @file almetal_linear_screen_boundary_test.mm
 * @brief Original float-screen to SSR/probe storage boundaries on native Metal.
 * Copyright (C)2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version2.1 only.
 */
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
using Pixel=std::array<float,4>;
using Pixels=std::vector<Pixel>;
struct Readback {std::vector<std::uint8_t> bytes;Pixels pixels;};
float half_value(std::uint16_t bits)
{__fp16 value;std::memcpy(&value,&bits,2);return value;}
float unsigned_float(unsigned bits,unsigned mantissa)
{
    const unsigned e=bits>>mantissa,m=bits&((1u<<mantissa)-1);
    return e?std::ldexp(1.f+float(m)/float(1u<<mantissa),int(e)-15):
             std::ldexp(float(m),-14-int(mantissa));
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalShaderLibrary> control_library;
    std::unique_ptr<ALMetalComputePipeline> control_pipeline,sample_pipeline;
    Fixture()
    {
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        renderer=ALMetalWorldGBufferRenderer::create(*device,1,&error);INFO(error);REQUIRE(renderer);
        auto arena=ALMetalWorldFrameData::create(*device,&error);REQUIRE(arena);
        arena->beginFrame(1);renderer->setFrameData(std::move(arena));
        // Gaussian weighting is independent CPU arithmetic. These controls
        // measure only native format stores and sampling at explicit CPU
        // coordinates, so hardware interpolation precision is not invented.
        // No convolution or probe-selection kernel is copied into the test.
        const char* source=R"metal(
#include <metal_stdlib>
using namespace metal;
kernel void format_store(device const float4* values [[buffer(0)]],
    texture2d<float,access::write> destination [[texture(0)]],uint2 id [[thread_position_in_grid]])
{
    if(id.x<destination.get_width()&&id.y<destination.get_height())
        destination.write(values[id.y*destination.get_width()+id.x],id);
}
kernel void format_sample(texture2d<float> source [[texture(0)]],
    device const float2* coordinates [[buffer(0)]],device float4* values [[buffer(1)]],
    constant uint& count [[buffer(2)]],uint id [[thread_position_in_grid]])
{
    constexpr sampler linear_sampler(coord::normalized,filter::linear,address::clamp_to_edge);
    if(id<count) values[id]=source.sample(linear_sampler,coordinates[id],level(0.f));
}
)metal";
        control_library=ALMetalShaderLibrary::compileSource(*device,source,"Native format conversion control",&error);
        INFO(error);REQUIRE(control_library);
        ALMetalComputePipelineDescriptor descriptor;
        descriptor.library=control_library.get();descriptor.function="format_store";
        control_pipeline=ALMetalComputePipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(control_pipeline);
        descriptor.function="format_sample";
        sample_pipeline=ALMetalComputePipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(sample_pipeline);
    }
    std::unique_ptr<ALMetalRenderTarget> source(unsigned size,const Pixels& values,bool depth=false,bool shader_read=true)
    {
        REQUIRE(values.size()==size*size);
        ALMetalRenderTargetDescriptor d;d.width=d.height=size;
        d.colors[0].format=ALMetalPixelFormat::rgba16_float;
        d.colors[0].shader_read=shader_read;
        if(depth){d.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;d.depth_stencil_shader_read=true;}
        auto target=ALMetalRenderTarget::create(*device,d,&error);INFO(error);REQUIRE(target);
        std::vector<std::uint16_t> half(values.size()*4);
        for(unsigned i=0;i<half.size();++i)
        {
            const __fp16 v=values[i/4][i%4];std::memcpy(&half[i],&v,2);
        }
        auto command=device->makeCommandBuffer("Float world screen boundary input",&error);REQUIRE(command);
        if(depth)
        {
            ALMetalRenderPassConfig config;config.clear_depth=.375;
            config.depth_store=ALMetalStoreAction::store;config.stencil_store=ALMetalStoreAction::store;
            auto pass=target->makeRenderPass(config,&error);REQUIRE(pass);
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Original screen shared depth",&error);REQUIRE(encoder);
            REQUIRE(encoder->end(&error));
        }
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(*command,0,0,{},half.data(),size*8,&error));
        REQUIRE(command->commitAndWait(&error));INFO(error);
        return target;
    }
    Readback read(const ALMetalTexture& texture,unsigned slice=0)
    {
        const auto& d=texture.descriptor();
        REQUIRE(d.width>0);REQUIRE(d.height>0);REQUIRE_FALSE(texture.isCompressed());
        Readback result;result.bytes.resize(std::size_t(d.width)*d.height*texture.bytesPerBlock());
        REQUIRE(texture.readback(*device,0,slice,{},result.bytes.data(),d.width*texture.bytesPerBlock(),&error));INFO(error);
        result.pixels.resize(std::size_t(d.width)*d.height);
        for(unsigned p=0;p<result.pixels.size();++p)
        {
            if(d.format==ALMetalPixelFormat::rgba8_unorm)
                for(unsigned c=0;c<4;++c)result.pixels[p][c]=result.bytes[p*4+c]/255.f;
            else if(d.format==ALMetalPixelFormat::rgba16_float)
                for(unsigned c=0;c<4;++c){std::uint16_t bits;std::memcpy(&bits,result.bytes.data()+(p*4+c)*2,2);result.pixels[p][c]=half_value(bits);}
            else
            {
                REQUIRE(d.format==ALMetalPixelFormat::rg11b10_float);
                std::uint32_t bits;std::memcpy(&bits,result.bytes.data()+p*4,4);
                result.pixels[p]={unsigned_float(bits&0x7ffu,6),unsigned_float((bits>>11)&0x7ffu,6),unsigned_float(bits>>22,5),1.f};
            }
        }
        return result;
    }
    Pixels sample(const ALMetalTexture& texture,unsigned output_size)
    {
        // CPU supplies coordinates; this is a sampler-precision control only.
        // It neither computes a convolution nor selects a probe/mip policy.
        const unsigned count=output_size*output_size;
        std::vector<std::array<float,2>> coordinates(count);
        for(unsigned y=0;y<output_size;++y)for(unsigned x=0;x<output_size;++x)
            coordinates[y*output_size+x]={(x+.5f)/output_size,(y+.5f)/output_size};
        const auto buffer=[&](const void* bytes,std::size_t length)
        {
            ALMetalBufferDescriptor d;d.length=length;
            auto value=ALMetalBuffer::create(*device,d,&error);REQUIRE(value);
            REQUIRE(value->write(0,bytes,length,&error));return value;
        };
        auto positions=buffer(coordinates.data(),coordinates.size()*sizeof(coordinates[0]));
        Pixels result(count,Pixel{NAN,NAN,NAN,NAN});
        auto output=buffer(result.data(),result.size()*sizeof(Pixel));
        auto command=device->makeCommandBuffer("Independent native sampler-precision control",&error);REQUIRE(command);
        auto encoder=ALMetalComputeEncoder::begin(*command,"Sample CPU-supplied coordinates",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*sample_pipeline,&error));
        REQUIRE(encoder->setTexture(0,&texture,&error));
        REQUIRE(encoder->setBuffer(0,*positions,0,&error));REQUIRE(encoder->setBuffer(1,*output,0,&error));
        REQUIRE(encoder->setBytes(2,&count,sizeof(count),&error));
        REQUIRE(encoder->dispatch(count,1,1,8,1,1,&error));REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));INFO(error);
        REQUIRE(output->read(0,result.data(),result.size()*sizeof(Pixel),&error));
        for(const auto& pixel:result)for(float value:pixel)REQUIRE(std::isfinite(value));
        return result;
    }
    Readback store(const Pixels& values,unsigned size,ALMetalPixelFormat format,unsigned sampled_size=0)
    {
        REQUIRE(values.size()==size*size);
        ALMetalTextureDescriptor td;td.width=td.height=size;td.format=format;
        td.usage=ALMetalTextureUsage::shader_read|ALMetalTextureUsage::shader_write;
        auto target=ALMetalTexture::create(*device,td,&error);REQUIRE(target);
        ALMetalBufferDescriptor bd;bd.length=values.size()*sizeof(Pixel);
        auto buffer=ALMetalBuffer::create(*device,bd,&error);REQUIRE(buffer);
        REQUIRE(buffer->write(0,values.data(),bd.length,&error));
        auto command=device->makeCommandBuffer("Independent native format-store control",&error);REQUIRE(command);
        auto encoder=ALMetalComputeEncoder::begin(*command,"Store scalar model without filtering",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*control_pipeline,&error));
        REQUIRE(encoder->setBuffer(0,*buffer,0,&error));REQUIRE(encoder->setTexture(0,target.get(),&error));
        REQUIRE(encoder->dispatch(size,size,1,4,4,1,&error));REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));INFO(error);
        auto result=read(*target);
        result.pixels=sample(*target,sampled_size?sampled_size:size);
        return result;
    }
};
Pixels authored(unsigned size)
{
    Pixels result(size*size);
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x)
    {
        for(unsigned c=0;c<3;++c)
        {
            unsigned h=x*747796405u+y*2891336453u+c*277803737u+37u;
            h^=h>>13;h*=0x5bd1e995u;h^=h>>15;
            const __fp16 value=.05f+float(h&0xffffu)/65535.f*std::array<float,3>{3.75f,1.6f,.93f}[c];
            result[y*size+x][c]=value; // The actual16F source stores precisely these values.
        }
        result[y*size+x][3]=.875f;
    }
    return result;
}
Pixels gaussian(const Pixels& source,unsigned size,bool horizontal)
{
    // Original interface/gaussianF.glsl's9 taps. SS2 uses one source texel
    // per resScale step. Conversion happens AFTER each weighted horizontal
    // sum, not to the input pixels before their large values are mixed.
    constexpr float weights[9]={.0002f,.0060f,.0606f,.2417f,.3829f,.2417f,.0606f,.0060f,.0002f};
    Pixels result(size*size);
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x)
        for(unsigned c=0;c<3;++c)
        {
            float value=0;
            for(int i=0;i<9;++i)
            {
                const unsigned sx=std::clamp(int(x)+(horizontal?i-4:0),0,int(size)-1);
                const unsigned sy=std::clamp(int(y)+(horizontal?0:i-4),0,int(size)-1);
                value+=source[sy*size+sx][c]*weights[i];
            }
            result[y*size+x][c]=std::max(value,0.f);
        }
    return result;
}
void alpha_zero(const Readback& data)
{for(const auto& pixel:data.pixels)CHECK(pixel[3]==0.f);}
}

TEST_CASE("Float screen preserves explicit non-HDR SSR history storage and raw depth",
          "[almetal][linear-screen-boundaries]")
{
    @autoreleasepool
    {
        Fixture f;Pixels pixels(16,Pixel{4.f,.25f,.75f,.875f});
        auto source=f.source(4,pixels,true);
        std::unique_ptr<ALMetalRenderTarget> history;
        REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*source,false,history,&f.error));INFO(f.error);REQUIRE(history);
        REQUIRE(history->descriptor().width==4);REQUIRE(history->descriptor().height==4);
        REQUIRE(history->descriptor().colors[0].format==ALMetalPixelFormat::rgba8_unorm);
        const auto observed=f.read(*history->colorTexture(0));
        for(const auto& pixel:observed.pixels)
        {
            CHECK(pixel[0]==1.f);CHECK(std::abs(pixel[1]-.25f)<=.5f/255);CHECK(std::abs(pixel[2]-.75f)<=.5f/255);
            CHECK(std::abs(pixel[3]-.875f)<=.5f/255);
        }
        // Original non-HDR history stores RGBA8 and preserves the copied
        // alpha, even though reflection consumers use only its RGB.
        REQUIRE(history->colorTexture(1));
        REQUIRE(history->colorTexture(1)->descriptor().format==ALMetalPixelFormat::r32_float);
        REQUIRE(history->colorTexture(1)->descriptor().width==4);
        REQUIRE(history->colorTexture(1)->descriptor().height==4);
        std::array<float,16> depth;
        REQUIRE(history->colorTexture(1)->readback(*f.device,0,0,{},depth.data(),4*sizeof(float),&f.error));
        for(float d:depth)CHECK(d==.375f);
    }
}
TEST_CASE("Float hero screen converts at mip-zero publication before later filtering",
          "[almetal][linear-screen-boundaries]")
{
    @autoreleasepool
    {
        Fixture f;const auto pixels=authored(4);auto source=f.source(4,pixels);
        std::unique_ptr<ALMetalTexture> cube;unsigned valid=0;
        for(bool hdr:{false,true})
        {
            CAPTURE(hdr);
            REQUIRE(f.renderer->updateHeroProbeFace(*source,0,1,hdr,cube,valid,&f.error));INFO(f.error);REQUIRE(cube);
            const auto format=hdr?ALMetalPixelFormat::rgba16_float:ALMetalPixelFormat::rgba8_unorm;
            CHECK(cube->descriptor().format==format);CHECK(valid==1);
            auto rgb=pixels;for(auto& pixel:rgb)pixel[3]=0;
            const auto expected=f.store(rgb,4,format);const auto observed=f.read(*cube);
            CHECK(observed.bytes==expected.bytes);alpha_zero(observed);

            // Hero publication now samples the source to convert at mip zero.
            // A render-only source must fail before encoding and leave the
            // already-published face and its identity available to readers.
            auto render_only=f.source(4,pixels,false,false);
            REQUIRE_FALSE(alMetalHasUsage(render_only->colorTexture(0)->descriptor().usage,
                                         ALMetalTextureUsage::shader_read));
            const auto* published=cube.get();
            const auto native=cube->nativeHandle();
            const auto published_faces=valid;
            f.error.clear();
            CHECK_FALSE(f.renderer->updateHeroProbeFace(*render_only,1,1,hdr,cube,valid,&f.error));
            CHECK_FALSE(f.error.empty());
            CHECK(cube.get()==published);CHECK(cube->nativeHandle()==native);
            CHECK(valid==published_faces);
            CHECK(f.read(*cube).bytes==observed.bytes);
        }
    }
}
TEST_CASE("Float ordinary probes retain original horizontal and vertical conversion boundaries",
          "[almetal][linear-screen-boundaries]")
{
    @autoreleasepool
    {
        Fixture f;
        for(bool hdr:{false,true})for(unsigned supersample:{1u,2u})
        {
            CAPTURE(hdr,supersample);
            constexpr unsigned resolution=4;
            const unsigned size=resolution*supersample;
            const auto pixels=authored(size);auto source=f.source(size,pixels);
            const auto format=hdr?ALMetalPixelFormat::rg11b10_float:ALMetalPixelFormat::rgba8_unorm;
            std::unique_ptr<ALMetalTexture> cube;unsigned valid=0;
            REQUIRE(f.renderer->updateReflectionProbeFace(*source,0,resolution,1,hdr,cube,valid,&f.error));INFO(f.error);REQUIRE(cube);
            CHECK(cube->descriptor().format==format);CHECK(valid==1);
            Pixels expected_input=pixels;
            for(auto& pixel:expected_input)pixel[3]=0;
            std::vector<std::uint8_t> wrong_vertical_bytes;
            if(supersample==2)
            {
                const auto horizontal=f.store(gaussian(pixels,size,true),size,format);
                const auto vertical=gaussian(horizontal.pixels,size,false);
                const auto vertical_float=f.store(vertical,size,ALMetalPixelFormat::rgba16_float,resolution);
                expected_input=vertical_float.pixels;
                // Both controls must actually differ from incorrect early
                // conversion boundaries; a constant scene cannot pass these.
                auto clipped=pixels;for(auto& pixel:clipped)for(unsigned c=0;c<3;++c)pixel[c]=std::min(pixel[c],1.f);
                const auto early_h=f.store(gaussian(clipped,size,true),size,format);
                const auto early_v=f.store(gaussian(early_h.pixels,size,false),size,ALMetalPixelFormat::rgba16_float,resolution);
                const auto wrong_early=f.store(early_v.pixels,resolution,format);
                const auto wrong_v=f.store(vertical,size,format,resolution);
                const auto wrong_vertical=f.store(wrong_v.pixels,resolution,format);
                wrong_vertical_bytes=wrong_vertical.bytes;
                const auto expected=f.store(expected_input,resolution,format);
                CHECK(expected.bytes!=wrong_early.bytes);
                CHECK(expected.bytes!=wrong_vertical.bytes);
            }
            const auto expected=f.store(expected_input,resolution,format);
            const auto observed=f.read(*cube);
            CHECK(observed.bytes==expected.bytes);
            if(!hdr&&supersample==2)
            {
                CHECK(observed.bytes!=wrong_vertical_bytes);
                for(unsigned i=0;i<observed.bytes.size();++i)
                    if(observed.bytes[i]!=expected.bytes[i])
                    {
                        CAPTURE(i,unsigned(observed.bytes[i]),unsigned(expected.bytes[i]),unsigned(wrong_vertical_bytes[i]));
                        CHECK(observed.bytes[i]==expected.bytes[i]);
                    }
            }
            if(!hdr)alpha_zero(observed); // R11 has no stored alpha channel.
        }
    }
}
