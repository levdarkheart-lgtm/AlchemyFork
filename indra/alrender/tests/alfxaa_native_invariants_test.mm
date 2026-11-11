/**
 * @file alfxaa_native_invariants_test.mm
 * @brief Original FXAA10-bit preservation, color-edge and luma-alpha invariants.
 * Copyright (C)2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version2.1 only.
 */
#include "../../almetal/almetalpipeline.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
constexpr unsigned W=64,H=16,edge=48;
std::array<unsigned,3> source_texel(unsigned x)
{
    return x<edge?std::array<unsigned,3>{300+x,400+x,500+x}:
                  std::array<unsigned,3>{50,50,50};
}
float half_value(std::uint16_t bits)
{
    __fp16 value;std::memcpy(&value,&bits,sizeof(bits));return value;
}
float luma(const std::array<unsigned,3>& c)
{
    return (c[0]*.299f+c[1]*.587f+c[2]*.114f)/1023.f;
}
}

TEST_CASE("Native production FXAA preserves10-bit flat color and derives alpha from RGB",
          "[almetal][native-fxaa-invariants]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        auto library=ALMetalShaderLibrary::loadBuiltin(*device,&error);INFO(error);REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;
        pd.library=library.get();pd.vertex_function="almetal_resample_vertex";
        pd.fragment_function="almetal_world_fxaa_fragment";
        // The original fixture also used a16F destination to observe the
        // kernel's alpha before the production post target's2-bit storage.
        pd.colors[0].format=ALMetalPixelFormat::rgba16_float;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error);INFO(error);REQUIRE(pipeline);
        ALMetalTextureDescriptor sd;
        sd.width=W;sd.height=H;sd.format=ALMetalPixelFormat::rgb10a2_unorm;
        sd.usage=ALMetalTextureUsage::shader_read;
        auto source=ALMetalTexture::create(*device,sd,&error);REQUIRE(source);
        std::array<std::uint32_t,W*H> texels;
        for(unsigned y=0;y<H;++y)
            for(unsigned x=0;x<W;++x)
            {
                const auto c=source_texel(x);
                texels[y*W+x]=c[0]|(c[1]<<10u)|(c[2]<<20u); // Deliberately flat alpha0.
            }
        auto upload=device->makeCommandBuffer("Original FXAA authored10-bit ramp",&error);REQUIRE(upload);
        REQUIRE(source->upload(*upload,0,0,{},texels.data(),W*4,&error));
        REQUIRE(upload->commitAndWait(&error));
        ALMetalSamplerDescriptor sam;
        sam.min_filter=sam.mag_filter=ALMetalSamplerFilter::linear;
        sam.address_s=sam.address_t=ALMetalSamplerAddressMode::mirror_repeat;
        auto sampler=ALMetalSamplerState::create(*device,sam,&error);REQUIRE(sampler);
        ALMetalRenderTargetDescriptor td;
        td.width=W;td.height=H;td.colors[0].format=ALMetalPixelFormat::rgba16_float;
        auto target=ALMetalRenderTarget::create(*device,td,&error);REQUIRE(target);
        for(unsigned quality=0;quality<4;++quality)
        {
            CAPTURE(quality);
            const std::array<std::uint32_t,4> config{quality,0,0,0};
            auto command=device->makeCommandBuffer("Actual FXAA precision invariant",&error);REQUIRE(command);
            auto pass=target->makeRenderPass({},&error);REQUIRE(pass);
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Actual builtin FXAA",&error);REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline,&error));
            REQUIRE(encoder->setViewport({0,0,W,H,0,1},&error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
            REQUIRE(encoder->setFragmentTexture(0,source.get(),&error));
            REQUIRE(encoder->setFragmentSampler(0,sampler.get(),&error));
            REQUIRE(encoder->setFragmentBytes(0,config.data(),sizeof(config),&error));
            REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
            REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));INFO(error);
            std::array<std::uint16_t,W*H*4> output;
            REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},output.data(),W*8,&error));INFO(error);
            const auto at=[&](unsigned x,unsigned y,unsigned channel){return half_value(output[(y*W+x)*4+channel]);};
            unsigned blended=0;
            for(unsigned y=2;y<H-2;++y)
                for(unsigned x=2;x<W-2;++x)
                {
                    CAPTURE(x,y);
                    const auto expected=source_texel(x);
                    if(x<edge-4)
                        for(unsigned c=0;c<3;++c)
                        {
                            CAPTURE(c);
                            CHECK(std::abs(at(x,y,c)*1023.f-float(expected[c]))<.25f);
                        }
                    if(x==edge-1||x==edge)
                        blended+=std::abs(at(x,y,1)*1023.f-float(expected[1]))>2.f;
                    CHECK(std::abs(at(x,y,3)-luma(expected))<.002f);
                }
            CHECK(blended>0);
        }
    }
}
