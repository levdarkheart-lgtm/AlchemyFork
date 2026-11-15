/**
 * @file almetal_empty_light_pass_native.inl
 * @brief Validated empty deferred-light owners and complete chain costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalstagetiming.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <time.h>
#include <vector>

namespace
{
namespace EmptyLightPass
{
constexpr std::uint32_t side=16;
using Pixels=std::array<std::uint16_t,side*side*4>;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
    std::unique_ptr<ALMetalRenderTarget> gbuffer;
    std::unique_ptr<ALMetalTexture> cookie;
    bool hdr=false,normal_hdr=true,reverse=false;
    Fixture()
    {
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        data=ALMetalWorldFrameData::create(*device,&error);REQUIRE(data);
        auto library=data->builtinLibrary(&error);REQUIRE(library);
        main=ALMetalWorldGBufferRenderer::create(*device,1,library,&error);
        probe=ALMetalWorldGBufferRenderer::create(*device,1,library,&error);
        INFO(error);REQUIRE(main);REQUIRE(probe);main->setFrameData(data);probe->setFrameData(data);
        ALMetalTextureDescriptor texture;texture.width=texture.height=1;
        texture.format=ALMetalPixelFormat::rgba8_unorm;
        texture.usage=ALMetalTextureUsage::shader_read;
        cookie=ALMetalTexture::create(*device,texture,&error);REQUIRE(cookie);
        auto upload=device->makeCommandBuffer("Empty-light fixed cookie",&error);REQUIRE(upload);
        const std::array<std::uint8_t,4> white{255,255,255,255};
        REQUIRE(cookie->upload(*upload,0,0,{},white.data(),white.size(),&error));REQUIRE(upload->commitAndWait(&error));
        configure(false,true,false);
    }
    void configure(bool high_dynamic,bool high_normal,bool reverse_z)
    {
        hdr=high_dynamic;normal_hdr=high_normal;reverse=reverse_z;
        ALMetalRenderTargetDescriptor target;target.width=target.height=side;target.color_attachment_count=4;
        target.colors[0].format=ALMetalPixelFormat::rgba8_unorm_srgb;
        target.colors[1].format=ALMetalPixelFormat::rgba8_unorm;
        target.colors[2].format=normal_hdr?ALMetalPixelFormat::rgba16_unorm:ALMetalPixelFormat::rgb10a2_unorm;
        target.colors[3].format=hdr?ALMetalPixelFormat::rg11b10_float:ALMetalPixelFormat::rgba8_unorm;
        target.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;target.depth_stencil_shader_read=true;
        gbuffer=ALMetalRenderTarget::create(*device,target,&error);REQUIRE(gbuffer);
        ALMetalRenderPassConfig clear;clear.colors[0].clear={.5,.25,.125,0};
        clear.colors[2].clear={.5,.5,0,0};clear.clear_depth=reverse?.75:.25;
        clear.depth_store=clear.stencil_store=ALMetalStoreAction::store;
        clearTarget(*gbuffer,clear);
    }
    std::unique_ptr<ALMetalRenderTarget> target(ALMetalPixelFormat format=ALMetalPixelFormat::rgba16_float)
    {
        ALMetalRenderTargetDescriptor descriptor;descriptor.width=descriptor.height=side;descriptor.colors[0].format=format;
        auto result=ALMetalRenderTarget::create(*device,descriptor,&error);REQUIRE(result);return result;
    }
    void clearTarget(ALMetalRenderTarget& target,const ALMetalRenderPassConfig& config)
    {
        auto pass=target.makeRenderPass(config,&error);REQUIRE(pass);
        auto command=device->makeCommandBuffer("Authored lighting input frontier",&error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Authored lighting input clear",&error);REQUIRE(encoder);
        REQUIRE(encoder->end(&error));REQUIRE(command->commit(&error));
    }
    void authorBase(ALMetalRenderTarget& target)
    {ALMetalRenderPassConfig config;config.colors[0].clear={.125,.25,.5,.75};clearTarget(target,config);}
    Pixels read(const ALMetalRenderTarget& target)
    {Pixels result{};REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},result.data(),side*8,&error));return result;}
    ALMetalWorldLocalLightingScene local(ALMetalWorldLocalLight* light=nullptr)
    {
        ALMetalWorldLocalLightingScene scene;scene.gbuffer=gbuffer.get();scene.lights=light;scene.light_count=light?1:0;
        scene.hdr=hdr;scene.reverse_z=reverse;
        for(std::size_t at=0;at<4;++at)scene.inverse_projection[at*4+at]=1.f;
        return scene;
    }
    ALMetalWorldProjectorLightingScene projector(ALMetalWorldProjectorLight* light=nullptr)
    {
        ALMetalWorldProjectorLightingScene scene;scene.gbuffer=gbuffer.get();scene.lights=light;scene.light_count=light?1:0;
        scene.hdr=hdr;scene.normal_hdr=normal_hdr;scene.reverse_z=reverse;
        for(std::size_t at=0;at<4;++at)scene.inverse_projection[at*4+at]=1.f;
        return scene;
    }
    ALMetalWorldLocalLight point() const
    {
        ALMetalWorldLocalLight light;light.position_radius[2]=1.5f;light.position_radius[3]=4.f;
        light.color_falloff[0]=.2f;light.color_falloff[1]=.1f;light.color_falloff[2]=.05f;
        return light;
    }
    ALMetalWorldProjectorLight spot() const
    {
        ALMetalWorldProjectorLight light;const auto source=point();light.cookie_texture=cookie.get();
        std::copy_n(source.position_radius,4,light.position_radius);std::copy_n(source.color_falloff,4,light.color_falloff);
        light.projector_matrix[0]=light.projector_matrix[5]=.5f;
        light.projector_matrix[12]=light.projector_matrix[13]=.5f;light.projector_matrix[14]=light.projector_matrix[15]=1.f;
        light.projector_normal[2]=-1.f;light.projector_normal[3]=4.f;
        light.projector_origin[2]=1.5f;light.projector_origin[3]=2.f;light.sampling[0]=1.f;
        return light;
    }
};
}
}

TEST_CASE("almetal: validated zero light owners preserve authored pixels without a pass or submission",
          "[almetal][empty-light-pass][PPD-250]")
{
    @autoreleasepool
    {
        EmptyLightPass::Fixture f;
        for(bool hdr:{false,true})for(bool normal:{false,true})for(bool reverse:{false,true})
        {
            CAPTURE(hdr,normal,reverse);f.configure(hdr,normal,reverse);auto target=f.target();f.authorBase(*target);
            const auto before=f.read(*target);auto window=ALMetalStageTimingWindow::create(*f.device,8,&f.error);REQUIRE(window);
            const auto commits=ALMetalDevice::committedCommandBufferCount();const auto allocations=alMetalResourceAllocationStats();
            {
                ALMetalStageTimingScope scope(window.get(),1,1,-1);
                REQUIRE(f.main->accumulateLocalLights(*target,f.local(),&f.error));
                REQUIRE(f.main->accumulateProjectorLights(*target,f.projector(),&f.error));
            }
            CHECK(ALMetalDevice::committedCommandBufferCount()==commits);CHECK(window->preparedPassCount()==0);
            CHECK(window->rejectedPrepareCount()==0);const auto after=alMetalResourceAllocationStats();
            CHECK(after.buffers_created==allocations.buffers_created);CHECK(after.textures_created==allocations.textures_created);
            REQUIRE(window->seal(&f.error));std::vector<ALMetalStageTimingResult> stages;REQUIRE(window->poll(stages,&f.error));
            CHECK(stages.empty());CHECK(window->isFinished());CHECK(f.read(*target)==before);

            auto bad_target=f.target(ALMetalPixelFormat::rgba8_unorm);
            const auto invalid_commits=ALMetalDevice::committedCommandBufferCount();
            for(unsigned invalid=0;invalid<5;++invalid)
            {
                CAPTURE(invalid);auto local=f.local();auto projector=f.projector();f.error.clear();
                if(invalid==0){local.gbuffer=nullptr;projector.gbuffer=nullptr;}
                if(invalid==1){local.inverse_projection[0]=std::numeric_limits<float>::quiet_NaN();projector.inverse_projection[0]=local.inverse_projection[0];}
                if(invalid==2){local.specular_exponent=0.f;projector.specular_exponent=0.f;}
                if(invalid==3){local.gbuffer=bad_target.get();projector.gbuffer=bad_target.get();}
                auto& destination=invalid==4?*bad_target:*target;
                CHECK_FALSE(f.main->accumulateLocalLights(destination,local,&f.error));CHECK_FALSE(f.error.empty());f.error.clear();
                CHECK_FALSE(f.main->accumulateProjectorLights(destination,projector,&f.error));CHECK_FALSE(f.error.empty());
            }
            auto invalid_bounded=f.local();invalid_bounded.bounded_light_count=1;
            CHECK_FALSE(f.main->accumulateLocalLights(*target,invalid_bounded,&f.error));
            CHECK(ALMetalDevice::committedCommandBufferCount()==invalid_commits);
        }
    }
}

TEST_CASE("almetal: empty light owners leave an enclosing batch usable for nonempty local and projector draws",
          "[almetal][empty-light-pass]")
{
    @autoreleasepool
    {
        EmptyLightPass::Fixture f;auto point=f.point();auto spot=f.spot();
        auto reference=f.target();auto combined=f.target();f.authorBase(*reference);f.authorBase(*combined);
        REQUIRE(f.main->accumulateLocalLights(*reference,f.local(&point),&f.error));
        REQUIRE(f.main->accumulateProjectorLights(*reference,f.projector(&spot),&f.error));
        const auto expected=f.read(*reference);const auto initial=f.read(*combined);
        REQUIRE_FALSE(expected==initial); // Nonempty controls really contribute.
        auto window=ALMetalStageTimingWindow::create(*f.device,8,&f.error);REQUIRE(window);
        const auto commits=ALMetalDevice::committedCommandBufferCount();
        REQUIRE(f.main->beginCommandBatch("Validated no-work owners inside a real lighting chain",&f.error));
        {
            ALMetalStageTimingScope scope(window.get(),2,1,-1);
            REQUIRE(f.main->accumulateLocalLights(*combined,f.local(),&f.error));
            REQUIRE(f.main->accumulateProjectorLights(*combined,f.projector(),&f.error));
            CHECK(window->preparedPassCount()==0);CHECK(f.main->commandBatchActive());
            REQUIRE(f.main->accumulateLocalLights(*combined,f.local(&point),&f.error));
            REQUIRE(f.main->accumulateProjectorLights(*combined,f.projector(&spot),&f.error));
        }
        CHECK(window->preparedPassCount()==2);REQUIRE(f.main->commitCommandBatch(&f.error));
        CHECK(ALMetalDevice::committedCommandBufferCount()==commits+1);REQUIRE(window->seal(&f.error));
        REQUIRE(f.device->waitUntilIdle(&f.error));std::vector<ALMetalStageTimingResult> stages;REQUIRE(window->poll(stages,&f.error));
        CHECK(stages.size()==2);CHECK(window->isFinished());CHECK(f.read(*combined)==expected);
    }
}

TEST_CASE("almetal: empty and nonempty local projector complete chain costs",
          "[almetal][.native-empty-light-cost]")
{
    @autoreleasepool
    {
        EmptyLightPass::Fixture f;f.configure(true,true,true);
        constexpr unsigned samples=12,warmups=2;
        struct Sample{double cpu=0,encode=0,settle=0,gpu=0;std::uint64_t commits=0,draws=0,buffers=0,gpu_ready=0;};
        struct Restore
        {bool timing=ALMetalDevice::commandTimingEnabled();~Restore(){ALMetalDevice::setCommandTimingEnabled(timing);ALMetalDevice::beginRenderInfoFrame(false);}} restore;
        ALMetalDevice::setCommandTimingEnabled(true);
        const auto cpu_ns=[](){timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
            return std::uint64_t(value.tv_sec)*1'000'000'000ULL+std::uint64_t(value.tv_nsec);};
        const auto median=[](std::array<double,samples> values){std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
        for(auto& target:targets)target=f.target();
        std::uint64_t epoch=0;
        for(unsigned views:{1u,3u})
        {
            std::array<std::array<Sample,samples>,4> recorded{};
            std::array<EmptyLightPass::Pixels,4> final_pixels{};
            for(unsigned sample=0;sample<samples+warmups;++sample)for(unsigned position=0;position<4;++position)
            {
                const unsigned workload=(sample+position)%4;CAPTURE(views,sample,workload);
                REQUIRE(std::chrono::steady_clock::now()<deadline);
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                const auto commits=ALMetalDevice::committedCommandBufferCount();const auto resources=alMetalResourceAllocationStats();
                ALMetalDevice::beginRenderInfoFrame(true);Sample value;
                const auto started=std::chrono::steady_clock::now();const auto cpu_started=cpu_ns();f.data->beginFrame(++epoch);
                for(unsigned view=0;view<views;++view)
                {
                    // The base-light producer is a typed nonzero clear here,
                    // not a claim to time the whole viewer/base shader.
                    f.authorBase(*targets[view]);auto point=f.point();auto spot=f.spot();
                    point.position_radius[0]=spot.position_radius[0]=.01f*view;
                    auto local=f.local((workload&1)?&point:nullptr);auto projector=f.projector((workload&2)?&spot:nullptr);
                    auto& renderer=view?*f.probe:*f.main;
                    REQUIRE(renderer.beginCommandBatch("Complete local/projector workload",&f.error));
                    REQUIRE(renderer.accumulateLocalLights(*targets[view],local,&f.error));
                    REQUIRE(renderer.accumulateProjectorLights(*targets[view],projector,&f.error));
                    completions[view]=renderer.commandBatchCompletion();REQUIRE(completions[view]);REQUIRE(renderer.commitCommandBatch(&f.error));
                }
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                const auto settled=std::chrono::steady_clock::now();value.cpu=(cpu_ns()-cpu_started)/1e6;
                value.encode=std::chrono::duration<double,std::milli>(encoded-started).count();
                value.settle=std::chrono::duration<double,std::milli>(settled-started).count();
                value.commits=ALMetalDevice::committedCommandBufferCount()-commits;value.draws=ALMetalDevice::renderInfo().draw_calls;
                value.buffers=alMetalResourceAllocationStats().buffers_created-resources.buffers_created;
                for(unsigned view=0;view<views;++view){ALMetalCommandGPUTime time;
                    if(completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready){value.gpu+=time.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                CHECK(value.draws==views*((workload&1?1u:0u)+(workload&2?1u:0u)));
                // PPD252: waitUntilIdle itself commits one settlement barrier.
                CHECK(value.commits==views*2+1); // Base + chain per view, then barrier.
                if(sample>=warmups)recorded[workload][sample-warmups]=value;
                if(sample==samples+warmups-1)final_pixels[workload]=f.read(*targets[0]);
                std::cout<<"NATIVE_EMPTY_LIGHT_RAW workload="<<workload<<" views="<<views<<" sample="<<sample<<" warmup="<<(sample<warmups)
                    <<" scope=local_projector_chain base_authoring=typed_clear_frontier hdr=1 normal_hdr=1 reverse_z=1"
                    <<" cpu_ms="<<value.cpu<<" encode_commit_ms="<<value.encode<<" settle_ms="<<value.settle<<" lighting_gpu_sum_ms="<<value.gpu
                    <<" gpu_ready="<<value.gpu_ready<<" commits="<<value.commits<<" settlement_barrier_commands=1"
                    <<" direct_draws="<<value.draws<<" buffers_created="<<value.buffers<<'\n';
            }
            CHECK_FALSE(final_pixels[0]==final_pixels[1]);CHECK_FALSE(final_pixels[0]==final_pixels[2]);
            for(unsigned workload=0;workload<4;++workload)
            {
                std::array<double,samples> cpu,encode,settle,gpu;
                for(unsigned sample=0;sample<samples;++sample){const auto& value=recorded[workload][sample];
                    cpu[sample]=value.cpu;encode[sample]=value.encode;settle[sample]=value.settle;gpu[sample]=value.gpu;}
                std::cout<<"NATIVE_EMPTY_LIGHT_COST workload="<<workload<<" views="<<views<<" samples="<<samples<<" warmups="<<warmups
                    <<" scope=local_projector_chain base_authoring=typed_clear_frontier same_source_workloads_not_before_after=1 settlement_barrier_commands_per_sample=1"
                    <<" cpu_median_ms="<<median(cpu)<<" cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                    <<" encode_median_ms="<<median(encode)<<" encode_max_ms="<<*std::max_element(encode.begin(),encode.end())
                    <<" settle_median_ms="<<median(settle)<<" settle_max_ms="<<*std::max_element(settle.begin(),settle.end())
                    <<" lighting_gpu_median_ms="<<median(gpu)<<" lighting_gpu_max_ms="<<*std::max_element(gpu.begin(),gpu.end())<<'\n';
            }
        }
    }
}
