/** Native probe tile-lighting tests. Included by the registered base-light fixture. */
#include "almetal_world_inputs_fixture.h"
#include "../almetalstagetiming.h"
#include <cstdlib>
#include <filesystem>

namespace
{
struct ProbeLightingFixture
{
    Fixture f;
    std::shared_ptr<ALMetalTexture> environment, cascades, noise;
    ALMetalWorldDirectionalShadows shadows;
    explicit ProbeLightingFixture(unsigned width = side)
    {
        ALMetalTextureDescriptor d;
        d.type=ALMetalTextureType::texture_cube;d.format=ALMetalPixelFormat::rgba8_unorm;
        d.usage=ALMetalTextureUsage::shader_read;
        environment=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(environment);
        d.type=ALMetalTextureType::texture_2d_array;d.format=ALMetalPixelFormat::r32_float;
        d.width=d.height=8;d.array_length=4;
        cascades=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(cascades);
        d.type=ALMetalTextureType::texture_2d;d.format=ALMetalPixelFormat::rg16_float;
        d.width=d.height=128;d.array_length=1;
        noise=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(noise);
        auto upload=f.device->makeCommandBuffer("Probe tile fixed lighting inputs",&f.error);REQUIRE(upload);
        const std::array<std::uint8_t,4> color{{24,96,48,255}};
        for(unsigned face=0;face<6;++face)
            REQUIRE(environment->upload(*upload,0,face,{},color.data(),4,&f.error));
        std::array<float,64> depth{};
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
            depth[y*8+x]=(x+y)%2 ? .8f : .2f;
        for(unsigned slice=0;slice<4;++slice)
            REQUIRE(cascades->upload(*upload,0,slice,{},depth.data(),8*sizeof(float),&f.error));
        std::vector<std::uint16_t> random(128*128*2,0x3800);
        REQUIRE(noise->upload(*upload,0,0,{},random.data(),128*4,&f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        shadows.depth_texture=cascades.get();
        for(auto& matrix:shadows.constants.shadow_matrix)
        {
            matrix[0]=matrix[5]=.35f;matrix[12]=matrix[13]=matrix[14]=.5f;matrix[15]=1;
        }
        const float clips[4]{4,8,12,16};std::copy_n(clips,4,shadows.constants.shadow_clip);
        shadows.constants.resolution[0]=shadows.constants.resolution[1]=8;
        shadows.constants.resolution[2]=shadows.constants.resolution[3]=static_cast<float>(width);
        shadows.constants.parameters[2]=200;shadows.constants.parameters[3]=static_cast<float>(width);
        shadows.constants.config[0]=4;shadows.constants.config[3]=2; // Original multi-tap filter/derivatives.
    }
    ALMetalWorldLightMapScene lightMap(const ALMetalRenderTarget& input,bool reverse,bool normal_hdr,bool shadow)
    {
        ALMetalWorldLightMapScene s;s.gbuffer=&input;identity(s.constants.inverse_projection);
        s.constants.inverse_projection[14]=-1;s.constants.light_direction[2]=1;
        s.constants.screen[0]=static_cast<float>(input.descriptor().width);
        s.constants.screen[1]=static_cast<float>(input.descriptor().height);
        s.constants.config[0]=reverse;s.constants.config[1]=shadow;s.constants.config[3]=normal_hdr;
        if(shadow)s.directional_shadows=&shadows;
        return s;
    }
    ALMetalWorldBaseLightingScene base(const ALMetalRenderTarget& input,bool reverse,bool hdr,bool shadow)
    {
        ALMetalWorldBaseLightingScene s;s.gbuffer=&input;s.legacy_environment=environment.get();s.hdr=hdr;
        identity(s.constants.inverse_projection);s.constants.inverse_projection[14]=-1;
        s.constants.light_direction[2]=1;s.constants.environment_config[0]=1;
        s.constants.config[0]=reverse;s.constants.config[2]=shadow;
        for(unsigned channel=0;channel<3;++channel)
        {
            s.constants.atmos_sunlit[channel]=.7f;
            s.constants.atmos_sunlit_linear[channel]=.7f;
            s.constants.atmos_amblit[channel]=.15f;
        }
        s.constants.lighting[0]=s.constants.lighting[1]=1;
        s.carry_depth=true;s.borrow_depth=true;s.depth_stencil_shader_read=true;
        s.diagnostic_view.view_kind=3;
        return s;
    }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(unsigned family,bool reverse,bool hdr,bool normal_hdr,unsigned width=side)
    {
        auto batch=f.batch(family);
        std::shared_ptr<const ALMetalAttachmentPaletteRecord> palette;
        if(family==4)
        {
            palette=ALMetalAttachmentPaletteRecord::create(batch.skin_palette,batch.skin_palette_float_count,
                batch.skin_joint_count,&f.error);REQUIRE(palette);
            batch.skin_palette_record=palette.get();batch.skin_palette=palette->data();
        }
        auto scene=f.scene();scene.width=scene.height=width;
        scene.batches=&batch;scene.batch_count=1;scene.reverse_z=reverse;scene.hdr=hdr;scene.normal_hdr=normal_hdr;
        // Real attachment palette callers use the same combined-library G-buffer owner.
        auto result=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(result);return result;
    }
    std::vector<std::uint16_t> read(ALMetalRenderTarget& target)
    {
        const auto width=target.descriptor().width,height=target.descriptor().height;
        std::vector<std::uint16_t> result(width*height*4);
        REQUIRE(target.colorTexture(0)->readback(*f.device,0,0,{},result.data(),width*8,&f.error));
        return result;
    }
};

void settleProbe(const std::shared_ptr<const ALMetalCommandCompletion>& completion)
{
    REQUIRE(completion);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!completion->isComplete() && std::chrono::steady_clock::now()<deadline)
        std::this_thread::yield();
    REQUIRE(completion->isComplete());
}
}

TEST_CASE("almetal: probe tile lighting preserves hardware RG8 shadows and existing G-buffer variants",
          "[almetal][probe-lighting-fusion]")
{
    @autoreleasepool
    {
        ProbeLightingFixture p;auto& f=p.f;INFO(f.error);REQUIRE(f.renderer->supportsProbeLightingFusion());
        for(unsigned family:{1u,2u,4u})
        for(bool hdr:{false,true})for(bool normal_hdr:{false,true})
        for(bool reverse:{false,true})for(bool shadow:{false,true})
        {
            CAPTURE(family,hdr,normal_hdr,reverse,shadow);
            auto input=p.gbuffer(family,reverse,hdr,normal_hdr);const auto original=f.read(*input);
            auto lm=p.lightMap(*input,reverse,normal_hdr,shadow);auto base=p.base(*input,reverse,hdr,shadow);
            auto map=f.renderer->renderLightMap(lm,&f.error);INFO(f.error);REQUIRE(map);
            base.light_map=map->colorTexture(0);
            auto separate=f.renderer->renderBaseLighting(base,&f.error);INFO(f.error);REQUIRE(separate);
            const auto expected=p.read(*separate);base.light_map=nullptr;
            ALMetalProbeLightingPath path=ALMetalProbeLightingPath::none;
            auto fused=f.renderer->renderProbeLighting(lm,base,&f.error,&path);INFO(f.error);REQUIRE(fused);
            CHECK(path==ALMetalProbeLightingPath::memoryless_tile);
            CHECK(p.read(*fused)==expected);
            CHECK(fused->depthStencilTexture()==input->depthStencilTexture());
            CHECK(f.read(*input)==original);
            CHECK(std::any_of(expected.begin(),expected.end(),[](auto v){return (v&0x7fffu)!=0;}));
        }
    }
}

TEST_CASE("almetal: probe tile pass census, sky transfer and ineligible inputs keep native behavior",
          "[almetal][probe-lighting-fusion]")
{
    @autoreleasepool
    {
        ProbeLightingFixture p;auto& f=p.f;REQUIRE(f.renderer->supportsProbeLightingFusion());
        auto input=p.gbuffer(1,false,true,true);auto lm=p.lightMap(*input,false,true,true);
        auto base=p.base(*input,false,true,true);
        // Generate the lazy BRDF LUT before counting physical render passes.
        auto warm=f.renderer->renderProbeLighting(lm,base,&f.error);REQUIRE(warm);(void)p.read(*warm);warm.reset();
        for(bool fused:{false,true})
        {
            auto window=ALMetalStageTimingWindow::create(*f.device,8,&f.error);INFO(f.error);REQUIRE(window);
            const auto pool_before=f.renderer->renderTargetPoolStats();
            std::unique_ptr<ALMetalRenderTarget> map,output;
            REQUIRE(f.renderer->beginCommandBatch("Probe tile physical encoder census",&f.error));
            {
                ALMetalStageTimingScope scope(window.get(),1,3,0);
                if(fused)output=f.renderer->renderProbeLighting(lm,base,&f.error);
                else
                {
                    map=f.renderer->renderLightMap(lm,&f.error);REQUIRE(map);
                    auto scene=base;scene.light_map=map->colorTexture(0);
                    output=f.renderer->renderBaseLighting(scene,&f.error);
                }
            }
            INFO(f.error);REQUIRE(output);
            auto completion=f.renderer->commandBatchCompletion();REQUIRE(f.renderer->commitCommandBatch(&f.error));
            settleProbe(completion);CHECK(window->preparedPassCount()==(fused?1u:2u));
            const auto pool_after=f.renderer->renderTargetPoolStats();
            // Pool acquisitions count only reusable owned targets, not the
            // borrowed-depth pass wrapper. Stage census above counts encoders.
            CHECK(pool_after.acquisitions-pool_before.acquisitions==(fused?1u:2u));
            REQUIRE(window->seal(&f.error));std::vector<ALMetalStageTimingResult> results;
            REQUIRE(window->poll(results,&f.error));CHECK(window->isFinished());
        }
        ALMetalProbeLightingPath path;
        auto external=f.renderer->renderLightMap(lm,&f.error);REQUIRE(external);
        base.light_map=external->colorTexture(0);
        auto output=f.renderer->renderProbeLighting(lm,base,&f.error,&path);REQUIRE(output);
        CHECK(path==ALMetalProbeLightingPath::separate_passes);base.light_map=nullptr;
        base.diagnostic_view.view_kind=0;
        output=f.renderer->renderProbeLighting(lm,base,&f.error,&path);REQUIRE(output);
        CHECK(path==ALMetalProbeLightingPath::separate_passes);base.diagnostic_view.view_kind=3;
        // SSAO/blur retains the original light-map owner rather than a tile variant.
        lm.noise_texture=p.noise.get();lm.constants.config[2]=1;
        lm.constants.ssao[0]=.25f;lm.constants.ssao[1]=1;lm.constants.ssao[2]=1;lm.constants.ssao[3]=1;
        lm.constants.blur_taps[0][1]=1;lm.constants.blur[0]=1;base.constants.config[3]=1;
        output=f.renderer->renderProbeLighting(lm,base,&f.error,&path);INFO(f.error);REQUIRE(output);
        CHECK(path==ALMetalProbeLightingPath::separate_passes);
        lm.constants.config[2]=0;base.constants.config[3]=0;
        // All clear-depth pixels are actual sky. The unchanged sky shader must
        // execute after light visibility without overwriting the tile attachment.
        ALMetalRenderPassConfig clear;clear.clear_depth=1;clear.depth_store=ALMetalStoreAction::store;
        clear.stencil_store=ALMetalStoreAction::store;clear.colors[3].clear={.125,.25,.5,0};
        clear.colors[2].clear={0,0,0,1}; // Exact HDRI G-buffer family marker.
        auto pass=input->makeRenderPass(clear,&f.error);REQUIRE(pass);
        auto command=f.device->makeCommandBuffer("Probe tile clear-sky input",&f.error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Probe tile sky input",&f.error);REQUIRE(encoder);
        REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));
        base.constants.sky[0]=base.constants.sky[1]=1;
        auto map=f.renderer->renderLightMap(lm,&f.error);REQUIRE(map);base.light_map=map->colorTexture(0);
        auto separate=f.renderer->renderBaseLighting(base,&f.error);REQUIRE(separate);const auto sky=p.read(*separate);
        base.light_map=nullptr;output=f.renderer->renderProbeLighting(lm,base,&f.error,&path);REQUIRE(output);
        CHECK(path==ALMetalProbeLightingPath::memoryless_tile);CHECK(p.read(*output)==sky);
        CHECK(halfValue(sky[0])>0.f);CHECK(halfValue(sky[2])>0.f);
    }
}

TEST_CASE("almetal: old explicit world library retains separate probe lighting", "[almetal][.probe-lighting-old-library]")
{
    const char* filename=std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY");
    if(!filename || !*filename)SKIP("Requires the preserved owner-selected pre-fusion metallib");
    @autoreleasepool
    {
        ProbeLightingFixture p;auto& f=p.f;const std::filesystem::path path(filename);
        REQUIRE(path.is_absolute());REQUIRE(std::filesystem::is_regular_file(path));
        std::shared_ptr<const ALMetalShaderLibrary> old=ALMetalShaderLibrary::loadFile(*f.device,path.string(),&f.error);REQUIRE(old);
        REQUIRE_FALSE(old->hasFunction("almetal_world_probe_tile_light_map_fragment"));
        REQUIRE_FALSE(old->hasFunction("almetal_world_probe_tile_base_light_fragment"));
        auto previous=ALMetalWorldGBufferRenderer::create(*f.device,1,old,&f.error);INFO(f.error);REQUIRE(previous);
        CHECK_FALSE(previous->supportsProbeLightingFusion());
        auto input=p.gbuffer(4,false,false,true);auto lm=p.lightMap(*input,false,true,true);
        auto base=p.base(*input,false,false,true);ALMetalProbeLightingPath selected;
        auto output=previous->renderProbeLighting(lm,base,&f.error,&selected);INFO(f.error);REQUIRE(output);
        CHECK(selected==ALMetalProbeLightingPath::separate_passes);
        auto current=f.renderer->renderProbeLighting(lm,base,&f.error,&selected);REQUIRE(current);
        CHECK(selected==ALMetalProbeLightingPath::memoryless_tile);CHECK(p.read(*output)==p.read(*current));
    }
}

TEST_CASE("almetal: odd-size probe tile lighting matches separate RG8 sampling exactly",
          "[almetal][probe-lighting-fusion]")
{
    @autoreleasepool
    {
        constexpr unsigned width=31;
        ProbeLightingFixture p(width);auto& f=p.f;
        REQUIRE(f.renderer->supportsProbeLightingFusion());
        auto input=p.gbuffer(2,true,true,true,width);
        REQUIRE(input->descriptor().width==width);REQUIRE(input->descriptor().height==width);
        auto lm=p.lightMap(*input,true,true,true);auto base=p.base(*input,true,true,true);
        auto map=f.renderer->renderLightMap(lm,&f.error);INFO(f.error);REQUIRE(map);
        REQUIRE(map->descriptor().width==width);REQUIRE(map->descriptor().height==width);
        base.light_map=map->colorTexture(0);
        auto separate=f.renderer->renderBaseLighting(base,&f.error);INFO(f.error);REQUIRE(separate);
        const auto expected=p.read(*separate);base.light_map=nullptr;
        ALMetalProbeLightingPath path=ALMetalProbeLightingPath::none;
        auto fused=f.renderer->renderProbeLighting(lm,base,&f.error,&path);INFO(f.error);REQUIRE(fused);
        CHECK(path==ALMetalProbeLightingPath::memoryless_tile);
        CHECK(fused->descriptor().width==width);CHECK(fused->descriptor().height==width);
        CHECK(fused->depthStencilTexture()==input->depthStencilTexture());
        CHECK(expected.size()==width*width*4);
        CHECK(p.read(*fused)==expected);
        CHECK(std::any_of(expected.begin(),expected.end(),[](auto value){return (value&0x7fffu)!=0;}));
    }
}

TEST_CASE("almetal: complete separate and tile probe-lighting costs", "[almetal][.probe-lighting-fusion-cost]")
{
    // Synthetic retained G-buffer frontier: asset upload and geometry are
    // excluded equally. Both actual lighting APIs, target acquisition, native
    // commit, output retirement and exact-command settlement are timed.
    for(unsigned width:{256u,512u})for(unsigned views:{1u,3u})for(bool shadow:{false,true})
    @autoreleasepool
    {
        ProbeLightingFixture p(width);auto& f=p.f;
        std::array<std::unique_ptr<ALMetalWorldGBufferRenderer>,2> owners;
        std::array<std::shared_ptr<ALMetalWorldFrameData>,2> data;
        for(unsigned mode=0;mode<2;++mode)
        {
            data[mode]=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data[mode]);
            owners[mode]=ALMetalWorldGBufferRenderer::create(*f.device,1,f.renderer->shaderLibrary(),&f.error);
            INFO(f.error);REQUIRE(owners[mode]);owners[mode]->setFrameData(data[mode]);
            REQUIRE(owners[mode]->supportsProbeLightingFusion());
        }
        auto input=p.gbuffer(2,true,true,true,width);
        auto lm=p.lightMap(*input,true,true,shadow);auto base=p.base(*input,true,true,shadow);
        for(unsigned sample=0;sample<10;++sample)
        {
            std::array<std::vector<std::uint16_t>,2> colors;
            // Alternate arm order; no shader/target-pool sharing between arms.
            for(unsigned order=0;order<2;++order)
            {
                const unsigned mode=order^(sample&1u);auto& renderer=*owners[mode];
                const auto resources_before=alMetalResourceAllocationStats();
                const auto started=std::chrono::steady_clock::now();
                data[mode]->beginFrame(sample+1);
                REQUIRE(renderer.beginCommandBatch("Complete probe-lighting cost",&f.error));
                std::vector<std::unique_ptr<ALMetalRenderTarget>> outputs,maps;
                for(unsigned view=0;view<views;++view)
                {
                    auto view_base=base;auto view_lm=lm;
                    view_base.constants.inverse_projection[12]=view_lm.constants.inverse_projection[12]=view*.03125f;
                    if(mode)
                    {
                        ALMetalProbeLightingPath selected;
                        auto output=renderer.renderProbeLighting(view_lm,view_base,&f.error,&selected);
                        INFO(f.error);REQUIRE(output);REQUIRE(selected==ALMetalProbeLightingPath::memoryless_tile);
                        outputs.push_back(std::move(output));
                    }
                    else
                    {
                        auto map=renderer.renderLightMap(view_lm,&f.error);INFO(f.error);REQUIRE(map);
                        view_base.light_map=map->colorTexture(0);
                        auto output=renderer.renderBaseLighting(view_base,&f.error);INFO(f.error);REQUIRE(output);
                        outputs.push_back(std::move(output));maps.push_back(std::move(map));
                    }
                }
                // Retain only the last native color for an out-of-band parity
                // read. Retire every CPU target before native settlement, just
                // as the producing viewer owner does at this frontier.
                id<MTLTexture> parity_texture=(__bridge id<MTLTexture>)outputs.back()->colorTexture(0)->nativeHandle();
                auto completion=renderer.commandBatchCompletion();REQUIRE(renderer.commitCommandBatch(&f.error));
                outputs.clear();maps.clear();
                const auto encoded=std::chrono::steady_clock::now();
                settleProbe(completion);const auto settled=std::chrono::steady_clock::now();
                ALMetalCommandGPUTime gpu;REQUIRE(completion->gpuTime(gpu)==ALMetalCommandTimingStatus::ready);
                const auto resources_after=alMetalResourceAllocationStats();
                // No parity encoder/readback is included in either timed
                // command. This extra native reference cannot make a typed
                // target-pool entry a real CPU reader or cross-warm the arms.
                id<MTLDevice> native=(__bridge id<MTLDevice>)f.device->nativeHandle();
                id<MTLBuffer> pixels=[native newBufferWithLength:width*width*8 options:MTLResourceStorageModeShared];
                REQUIRE(pixels);
                auto parity=[(__bridge id<MTLCommandQueue>)f.device->nativeCommandQueue() commandBuffer];
                auto blit=[parity blitCommandEncoder];REQUIRE(blit);
                [blit copyFromTexture:parity_texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                    sourceSize:MTLSizeMake(width,width,1) toBuffer:pixels destinationOffset:0
                    destinationBytesPerRow:width*8 destinationBytesPerImage:width*width*8];
                [blit endEncoding];[parity commit];[parity waitUntilCompleted];
                REQUIRE(parity.status==MTLCommandBufferStatusCompleted);
                colors[mode].resize(width*width*4);std::memcpy(colors[mode].data(),pixels.contents,width*width*8);
                std::cout<<"probe_lighting_cost width="<<width<<" views="<<views<<" shadow="<<shadow
                        <<" mode="<<mode<<" sample="<<sample
                        <<" phase="<<(sample==0?"cold":sample==1?"warmup":"warm")
                        <<" cpu_encode_ms="<<std::chrono::duration<double,std::milli>(encoded-started).count()
                        <<" settle_ms="<<std::chrono::duration<double,std::milli>(settled-started).count()
                        <<" gpu_envelope_ms="<<gpu.elapsed_nanoseconds/1e6
                        <<" expected_render_passes="<<views*(mode?1u:2u)
                        <<" textures_created="<<resources_after.textures_created-resources_before.textures_created
                        <<" live_wrapper_logical_texture_bytes="<<resources_after.texture_bytes_live<<'\n';
            }
            CHECK(colors[0]==colors[1]);
        }
    }
}
