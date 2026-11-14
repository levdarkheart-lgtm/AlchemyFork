/**
 * @file almetal_shadow_generation_benchmark.mm
 * @brief Complete production shadow preparation, encoding and GPU cost gates.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowprepare.h"
#include "../almetalshadowindirect.h"
#include "../almetalshadowmaterials.h"
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
using Clock=std::chrono::steady_clock;
using Batch=ALMetalWorldSpotShadowBatch;
using Record=ALMetalShadowSourceRecord;
using Input=ALMetalShadowPrepareInput;
enum Mode { scalar,bulk,cpu_icb,gpu_classic,gpu_metal4,mode_count };
constexpr const char* modes[]{"scalar","bulk_direct","cpu_icb","gpu_generated_classic","gpu_generated_metal4"};
constexpr const char* families[]{"static_opaque","rigged_opaque","rigged_pbr_mask","opaque_pbr_mixed"};
struct Scenario {std::size_t draws;unsigned family;bool mixed;};
struct Sample
{
    double cpu=0,commit=0,release=0,wall=0,legacy_gpu=0,legacy_gpu_span=0,metal4_gpu=0,receipt_wait=0;
    std::uint64_t generated=0,visibility=0,metal4=0,metal4_timed=0,publications=0,reuses=0;
    std::uint64_t commands=0;
    std::uint64_t stable_publications=0,stable_reuses=0,stable_fallbacks=0,stable_palettes=0,stable_bytes=0;
    std::uint64_t material_hits=0,material_misses=0,material_creates=0;
    std::uint64_t opaque_prepared=0,opaque_executed=0,opaque_runs=0,opaque_skipped=0;
};
double ms(Clock::time_point first,Clock::time_point last)
{return std::chrono::duration<double,std::milli>(last-first).count();}
bool enabled(const char* name)
{const char* value=std::getenv(name);return value&&*value&&std::strcmp(value,"0")!=0;}
double percentile(std::vector<double> values,double fraction)
{
    std::sort(values.begin(),values.end());
    return values[std::max<std::size_t>(1,std::size_t(std::ceil(values.size()*fraction)))-1];
}

struct Fixture
{
    static constexpr std::size_t width=48,height=48,views=6,pose_count=16,joints=110;
    static constexpr std::size_t weight_offset=48,uv_offset=96,color_offset=120;
    struct Packet
    {
        std::vector<Input> inputs;
        std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> poses;
    };
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> arena;
    std::shared_ptr<const ALMetalBuffer> vertices,indices16,indices32;
    std::shared_ptr<const ALMetalTexture> texture;
    std::array<std::array<std::shared_ptr<const Record>,2>,3> sources;
    std::unique_ptr<ALMetalTexture> target;
    std::vector<Batch> outputs;
    std::vector<ALMetalShadowBounds> bounds;
    std::uint64_t generation=0;
    bool native_metal4=false;
    std::array<std::uint64_t,views*2> last_queries{};

    Fixture()
    {
        device=ALMetalDevice::create(&error);REQUIRE(device);
        native_metal4=[(__bridge id<MTLDevice>)device->nativeHandle() supportsFamily:MTLGPUFamilyMetal4];
        renderer=ALMetalWorldGBufferRenderer::create(*device,&error);INFO(error);REQUIRE(renderer);
        arena=ALMetalWorldFrameData::create(*device,&error);REQUIRE(arena);renderer->setFrameData(arena);
        const std::array<float,30> data{{
            -.75f,-.75f,0.f,1.f,.75f,-.75f,0.f,1.f,0.f,.75f,0.f,1.f,
            .5f,0.f,0.f,0.f,.5f,0.f,0.f,0.f,.5f,0.f,0.f,0.f,
            0.f,0.f,1.f,0.f,.5f,1.f}};
        std::array<std::uint8_t,132> bytes{};
        std::memcpy(bytes.data(),data.data(),sizeof(data));
        std::fill(bytes.begin()+color_offset,bytes.end(),255);
        ALMetalBufferDescriptor bd;bd.length=bytes.size();
        auto vb=ALMetalBuffer::create(*device,bd,&error);REQUIRE(vb);
        REQUIRE(vb->write(0,bytes.data(),bytes.size(),&error));vertices=std::move(vb);
        const std::array<std::uint16_t,3> i16{{0,1,2}};bd.length=sizeof(i16);
        auto ib16=ALMetalBuffer::create(*device,bd,&error);REQUIRE(ib16);
        REQUIRE(ib16->write(0,i16.data(),sizeof(i16),&error));indices16=std::move(ib16);
        const std::array<std::uint32_t,3> i32{{0,1,2}};bd.length=sizeof(i32);
        auto ib32=ALMetalBuffer::create(*device,bd,&error);REQUIRE(ib32);
        REQUIRE(ib32->write(0,i32.data(),sizeof(i32),&error));indices32=std::move(ib32);
        ALMetalTextureDescriptor td;td.width=td.height=4;td.format=ALMetalPixelFormat::rgba8_unorm;
        td.usage=ALMetalTextureUsage::shader_read;
        auto image=ALMetalTexture::create(*device,td,&error);REQUIRE(image);
        std::array<std::uint8_t,64> pixels{};
        for(std::size_t p=0;p<16;++p)
        {pixels[p*4]=pixels[p*4+1]=pixels[p*4+2]=255;pixels[p*4+3]=(p%4==0?0:255);}
        auto upload=device->makeCommandBuffer("GPU generation cutout fixture",&error);REQUIRE(upload);
        REQUIRE(image->upload(*upload,0,0,{},pixels.data(),16,&error));REQUIRE(upload->commitAndWait(&error));
        texture=std::move(image);
        std::vector<float> pose(4+joints*12,0.f);
        for(std::size_t j=0;j<joints;++j){pose[4+j*12]=pose[9+j*12]=pose[14+j*12]=1.f;}
        auto palette=ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),joints,&error);REQUIRE(palette);
        for(unsigned family=0;family<3;++family)
        for(unsigned wide=0;wide<2;++wide)
        {
            Batch prototype;prototype.vertices=vertices.get();prototype.indices=(wide?indices32:indices16).get();
            prototype.vertex_count=3;prototype.index_count=3;prototype.double_sided=true;
            prototype.index_type=wide?ALMetalIndexType::uint32:ALMetalIndexType::uint16;
            if(family)
            {
                prototype.weight_offset=weight_offset;prototype.skin_palette_record=palette;
                prototype.skin_palette=palette->data();prototype.skin_palette_float_count=palette->size();
                prototype.skin_joint_count=joints;
            }
            std::vector<std::shared_ptr<const ALMetalTexture>> textures;
            if(family==2)
            {
                prototype.coverage=ALMetalWorldSpotShadowCoverage::pbr_mask;
                prototype.texture_count=1;prototype.texcoord_offset=uv_offset;prototype.color_offset=color_offset;
                prototype.coverageData().textures[0]=texture.get();prototype.coverageData().alpha_cutoffs[0]=.5f;
                prototype.coverageData().pbr_base_color_transforms[0][0][0]=1.f;
                prototype.coverageData().pbr_base_color_transforms[0][0][1]=1.f;
                textures.push_back(texture);
            }
            sources[family][wide]=Record::create(1,prototype,vertices,wide?indices32:indices16,std::move(textures),&error);
            INFO(error);REQUIRE(sources[family][wide]);
        }
        td={};td.width=width;td.height=height;td.array_length=views;
        td.type=ALMetalTextureType::texture_2d_array;td.format=ALMetalPixelFormat::r32_float;
        td.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
        target=ALMetalTexture::create(*device,td,&error);REQUIRE(target);
    }

    Sample frame(const Scenario& scenario,Mode mode,unsigned phase,
                 bool standalone=false,bool allow_dispatch=false,bool stable=false,bool opaque_only=false)
    {
        Sample sample;
        const auto before=renderer->shadowIndirectStats();
        const auto data_before=arena->stats();
        const auto materials_before=renderer->shadowMaterialStats();
        const auto commands_before=ALMetalDevice::committedCommandBufferCount();
        const auto started=Clock::now();
        Clock::time_point release_started;
        @autoreleasepool
        {
            // Retirement prepares the following generation. Reuse that same
            // generation here; advancing twice would evict otherwise hot
            // material keys between consecutive rendered frames (PPD-102).
            const auto frame=generation?generation:++generation;arena->beginFrame(frame);
            auto packet=std::make_shared<Packet>();
            std::weak_ptr<const Packet> lifetime=packet;
            packet->inputs.resize(views*scenario.draws);
            if(scenario.family)
            {
                packet->poses.reserve(pose_count);
                for(std::size_t p=0;p<pose_count;++p)
                {
                    std::vector<float> pose(4+joints*12,0.f);
                    for(std::size_t j=0;j<joints;++j){pose[4+j*12]=pose[9+j*12]=pose[14+j*12]=1.f;}
                    pose[15]=float(p)*.002f+float(phase%7)*.003f;
                    auto record=ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),joints,&error);
                    if(!record)throw std::runtime_error(error);
                    packet->poses.push_back(std::move(record));
                }
            }
            outputs.resize(views*scenario.draws);bounds.resize(views*scenario.draws);
            std::vector<std::shared_ptr<const ALMetalCommandCompletion>> completions;
            completions.reserve(views);
            std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,views> query_results;
            std::array<ALMetalWorldVisibilityQuery,2> queries{};
            if(scenario.family==3)
            {
                for(std::size_t q=0;q<queries.size();++q)
                {
                    queries[q].batch_offset=q?2560:2048;queries[q].count_samples=1;
                    std::memcpy(queries[q].modelview_projection,alMetalShadowIdentity.data(),64);
                    queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.3f;
                }
            }
            if(!standalone&&!renderer->beginShadowFrameBatch(frame,"Production GPU shadow generation full cost",&error))
                throw std::runtime_error(error);
            for(std::size_t view=0;view<views;++view)
            {
                if(standalone&&!renderer->beginShadowFrameBatch(frame,"Standalone native shadow view full cost",&error))
                    throw std::runtime_error(error);
                const auto offset=view*scenario.draws;
                for(std::size_t n=0;n<scenario.draws;++n)
                {
                    auto& input=packet->inputs[offset+n];
                    const unsigned family=scenario.family<3?scenario.family:(n<2048?0:n<2560?2:1);
                    input.source=sources[family][n%2];
                    input.source_generation=input.source->generation();input.epoch=frame;input.has_model=true;
                    input.model[0]=input.model[5]=.06f;
                    input.model[12]=scenario.mixed&&n%4==0?4.f:(float(n%16)-7.5f)*.11f;
                    input.model[13]=(float((n/16)%16)-7.5f)*.11f;
                    input.view[14]=float(view)*.015f+float(phase%7)*.004f;
                    if(family)input.palette=packet->poses[n%pose_count];
                    auto& bound=bounds[offset+n];bound.conservative=scenario.mixed;
                    bound.min[0]=bound.min[1]=-.75f;bound.max[0]=bound.max[1]=.75f;
                    bound.min[2]=bound.max[2]=family?float(n%pose_count)*.002f+float(phase%7)*.003f:0.f;
                }
                if(!alMetalPrepareShadowBatches(std::span<const Input>(packet->inputs).subspan(offset,scenario.draws),
                    std::span<Batch>(outputs).subspan(offset,scenario.draws),frame,nullptr,allow_dispatch,nullptr,&error))
                    throw std::runtime_error(error);
                ALMetalWorldSpotShadowScene scene;scene.source_owner=packet;
                scene.batches=outputs.data()+offset;scene.batch_count=scenario.draws;
                scene.width=width;scene.height=height;scene.depth_clamp=view<4;scene.reverse_z=view%2;
                scene.bulk_draws=mode!=scalar;scene.indirect_draws=mode>=cpu_icb;
                scene.stable_shadow_data=stable;
                scene.opaque_indirect_draws=opaque_only;
                if(scenario.family==3){scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();}
                scene.gpu_visibility=mode>=gpu_classic;scene.metal4_visibility=mode==gpu_metal4;
                if(scene.gpu_visibility){scene.visibility_bounds=bounds.data()+offset;scene.visibility_bounds_count=scenario.draws;}
                if(!renderer->renderSpotShadow(*target,view,scene,&error,scenario.family==3?&query_results[view]:nullptr))throw std::runtime_error(error);
                if(standalone)
                {
                    completions.push_back(renderer->commandBatchCompletion());
                    const auto commit_started=Clock::now();
                    if(!renderer->commitCommandBatch(&error))throw std::runtime_error(error);
                    sample.commit+=ms(commit_started,Clock::now());
                }
            }
            if(!standalone)completions.push_back(renderer->commandBatchCompletion());
            for(const auto& completion:completions)
                if(!completion)throw std::runtime_error("Missing full-cost command completion");
            const auto encoded=Clock::now();sample.cpu=ms(started,encoded)-sample.commit;
            if(!standalone)
            {
                if(!renderer->commitCommandBatch(&error))throw std::runtime_error(error);
                sample.commit+=ms(encoded,Clock::now());
            }
            const auto committed=Clock::now();
            const auto deadline=committed+std::chrono::seconds(10);
            while(!std::all_of(completions.begin(),completions.end(),
                [](const auto& completion){return completion->isComplete();})&&Clock::now()<deadline)std::this_thread::yield();
            const auto completed=Clock::now();sample.wall=ms(started,completed);
            double gpu_start=0,gpu_end=0;
            for(const auto& completion:completions)
            {
                if(!completion->isComplete())throw std::runtime_error("GPU full-cost completion timed out");
                ALMetalCommandGPUTime gpu;
                if(completion->gpuTime(gpu)!=ALMetalCommandTimingStatus::ready)
                    throw std::runtime_error("GPU full-cost timestamps failed");
                sample.legacy_gpu+=double(gpu.elapsed_nanoseconds)*1.e-6;
                if(!gpu_start||gpu.start_seconds<gpu_start)gpu_start=gpu.start_seconds;
                gpu_end=std::max(gpu_end,gpu.end_seconds);
            }
            sample.legacy_gpu_span=(gpu_end-gpu_start)*1000.;
            if(scenario.family==3)
            {
                for(std::size_t view=0;view<views;++view)
                for(std::size_t q=0;q<queries.size();++q)
                    if(!query_results[view]||!query_results[view]->result(q,last_queries[view*2+q]))
                        throw std::runtime_error("Mixed shadow query receipt failed");
            }
            const auto submitted=renderer->shadowIndirectStats();
            sample.generated=submitted.gpu_generated_draws-before.gpu_generated_draws;
            sample.visibility=submitted.visibility_dispatches-before.visibility_dispatches;
            sample.metal4=submitted.metal4_visibility_dispatches-before.metal4_visibility_dispatches;
            // Feedback delivery can lag the event observed by the consumer.
            // Collect that receipt outside both CPU and completion-wall gates.
            const auto receipt_started=Clock::now();
            while(renderer->shadowIndirectStats().metal4_timed_dispatches<before.metal4_timed_dispatches+sample.metal4&&Clock::now()<deadline)
                std::this_thread::yield();
            sample.receipt_wait=ms(receipt_started,Clock::now());
            const auto timed=renderer->shadowIndirectStats();
            sample.stable_publications=timed.stable_data_publications-before.stable_data_publications;
            sample.stable_reuses=timed.stable_data_reuses-before.stable_data_reuses;
            sample.stable_fallbacks=timed.stable_data_fallbacks-before.stable_data_fallbacks;
            sample.stable_palettes=timed.stable_palette_copies-before.stable_palette_copies;
            sample.stable_bytes=timed.stable_data_bytes-before.stable_data_bytes;
            sample.opaque_prepared=timed.opaque_indirect_prepared_draws-before.opaque_indirect_prepared_draws;
            sample.opaque_executed=timed.opaque_indirect_executed_draws-before.opaque_indirect_executed_draws;
            sample.opaque_runs=timed.opaque_indirect_runs-before.opaque_indirect_runs;
            sample.opaque_skipped=timed.opaque_indirect_skipped_draws-before.opaque_indirect_skipped_draws;
            sample.metal4_timed=timed.metal4_timed_dispatches-before.metal4_timed_dispatches;
            sample.metal4_gpu=double(timed.metal4_gpu_time_nanoseconds-before.metal4_gpu_time_nanoseconds)*1.e-6;
            if(sample.metal4_timed!=sample.metal4||timed.visibility_failures)
                throw std::runtime_error("Independent Metal4 GPU feedback failed");
            release_started=Clock::now();packet.reset();
            for(auto& output:outputs)output.clearInputOwnership();
            arena->beginFrame(++generation);completions.clear();
            while(!lifetime.expired()&&Clock::now()<deadline)std::this_thread::yield();
            if(!lifetime.expired())throw std::runtime_error("Native packet retirement timed out");
        }
        sample.release=ms(release_started,Clock::now());
        const auto data_after=arena->stats();
        sample.publications=data_after.record_publications-data_before.record_publications;
        sample.reuses=data_after.record_reuses-data_before.record_reuses;
        const auto materials_after=renderer->shadowMaterialStats();
        sample.material_hits=materials_after.cache_hits-materials_before.cache_hits;
        sample.material_misses=materials_after.cache_misses-materials_before.cache_misses;
        sample.material_creates=materials_after.table_creates-materials_before.table_creates;
        sample.commands=ALMetalDevice::committedCommandBufferCount()-commands_before;
        if(sample.commands!=(standalone?views:1))throw std::runtime_error("Native shadow submission arm did not activate");
        if(scenario.draws>=128&&mode>=gpu_classic&&sample.generated!=views*scenario.draws)
            throw std::runtime_error("GPU generation arm did not activate");
        if(scenario.draws<128&&sample.visibility)
            throw std::runtime_error("Light control unexpectedly activated GPU generation");
        if(native_metal4&&mode==gpu_metal4&&scenario.draws>=128&&sample.metal4!=views)
            throw std::runtime_error("Supported native Metal4 arm did not activate");
        const auto stable_expected=opaque_only&&scenario.family==3?views*2:
            stable&&(mode<cpu_icb||scenario.draws>=128)?views:0;
        if(sample.stable_publications!=stable_expected||sample.stable_fallbacks)
            throw std::runtime_error("Completion-owned stable shadow data arm did not activate");
        if(sample.stable_palettes!=(stable_expected&&scenario.family?views*pose_count:0))
            throw std::runtime_error("Completion-owned certified palette publication did not activate");
        if(scenario.family==3&&(sample.opaque_prepared!=(opaque_only?views*4096:0)||
            sample.opaque_executed!=sample.opaque_prepared||sample.opaque_runs!=(opaque_only?views*2:0)||
            sample.opaque_skipped||sample.material_hits||sample.material_misses||sample.material_creates||sample.visibility))
            throw std::runtime_error("Selective opaque admission changed mixed covered paths or failed activation");
        return sample;
    }

    std::vector<float> pixels()
    {
        std::vector<float> result(width*height*views);
        for(std::uint32_t view=0;view<views;++view)
            REQUIRE(target->readback(*device,0,view,{},result.data()+view*width*height,width*sizeof(float),&error));
        return result;
    }

    void preflight()
    {
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,256u,2048u})
        for(const bool mixed:{false,true})
        {
            CAPTURE(family,draws,mixed);
            const Scenario scenario{draws,family,mixed};
            frame(scenario,scalar,0);const auto reference=pixels();
            REQUIRE(std::all_of(reference.begin(),reference.end(),[](float value){return std::isfinite(value);}));
            REQUIRE(std::any_of(reference.begin(),reference.begin()+width*height,[](float value){return value<.99f;}));
            for(unsigned mode=1;mode<mode_count;++mode)
            {CAPTURE(mode);frame(scenario,Mode(mode),0);CHECK(pixels()==reference);}
            frame(scenario,scalar,1);CHECK(pixels()!=reference);
        }
    }
};
}

TEST_CASE("almetal: complete GPU generation cost fixture preserves production pixels",
          "[almetal][shadow-generation-cost-parity]")
{@autoreleasepool {Fixture fixture;fixture.preflight();}}

TEST_CASE("almetal: stable shadow slabs observe current pose and exact material keys",
          "[almetal][shadow-stable-data-parity]")
{
    @autoreleasepool
    {
        Fixture fixture;
        const Scenario scenario{256,2,false};
        fixture.frame(scenario,scalar,0);const auto first=fixture.pixels();
        fixture.frame(scenario,cpu_icb,0,false,false,true);CHECK(fixture.pixels()==first);
        fixture.frame(scenario,scalar,1);const auto changed_pose=fixture.pixels();CHECK(changed_pose!=first);
        fixture.frame(scenario,cpu_icb,1,false,false,true);CHECK(fixture.pixels()==changed_pose);
        const auto hot=fixture.frame(scenario,cpu_icb,1,false,false,true);
        CHECK(hot.stable_reuses==Fixture::views);CHECK(hot.stable_palettes==Fixture::views*Fixture::pose_count);
        CHECK(hot.material_hits>0);CHECK(hot.material_misses==0);CHECK(hot.material_creates==0);

        ALMetalTextureDescriptor td;td.width=td.height=1;td.format=ALMetalPixelFormat::rgba8_unorm;
        td.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> image=ALMetalTexture::create(*fixture.device,td,&fixture.error);REQUIRE(image);
        const auto upload_alpha=[&](std::uint8_t alpha) {
            const std::array<std::uint8_t,4> bytes{{255,255,255,alpha}};
            auto command=fixture.device->makeCommandBuffer("Mutable material content control",&fixture.error);REQUIRE(command);
            REQUIRE(image->upload(*command,0,0,{},bytes.data(),4,&fixture.error));REQUIRE(command->commitAndWait(&fixture.error));
        };
        upload_alpha(0);
        std::vector<float> pose(4+Fixture::joints*12,0.f);
        for(std::size_t j=0;j<Fixture::joints;++j){pose[4+j*12]=pose[9+j*12]=pose[14+j*12]=1.f;}
        auto palette=ALMetalAttachmentPaletteRecord::create(pose.data(),pose.size(),Fixture::joints,&fixture.error);REQUIRE(palette);
        for(std::size_t wide=0;wide<2;++wide)
        {
            auto prototype=fixture.sources[2][wide]->prototype();
            prototype.coverageData().textures[0]=image.get();
            prototype.skin_palette_record=palette;prototype.skin_palette=palette->data();
            prototype.skin_palette_float_count=palette->size();prototype.skin_joint_count=palette->jointCount();
            auto source=Record::create(2,prototype,fixture.vertices,wide?fixture.indices32:fixture.indices16,{image},&fixture.error);
            INFO(fixture.error);REQUIRE(source);fixture.sources[2][wide]=std::move(source);
        }
        CHECK(fixture.renderer->shadowMaterialStats().cached_tables==1);
        const auto new_key=fixture.frame(scenario,cpu_icb,1,false,false,true);
        CHECK(new_key.material_misses==1);CHECK(new_key.material_creates==1);
        CHECK(fixture.renderer->shadowMaterialStats().cached_tables==1);
        const auto transparent=fixture.pixels();CHECK(transparent!=changed_pose);
        CHECK(std::all_of(transparent.begin(),transparent.begin()+Fixture::width*Fixture::height,
            [](float value){return value==1.f;}));
        upload_alpha(255); // Same exact native view: key stays hot, bytes change.
        const auto same_key=fixture.frame(scenario,cpu_icb,2,false,false,true);
        CHECK(same_key.material_hits>0);CHECK(same_key.material_misses==0);CHECK(same_key.material_creates==0);
        const auto opaque=fixture.pixels();CHECK(opaque!=transparent);
        fixture.frame(scenario,scalar,2);CHECK(fixture.pixels()==opaque);
        const auto slots=fixture.renderer->shadowIndirectStats().stable_data_slots;
        REQUIRE(fixture.renderer->reloadShaders(&fixture.error));
        CHECK(fixture.renderer->shadowIndirectStats().stable_data_slots==slots);
        const auto reloaded=fixture.frame(scenario,cpu_icb,2,false,false,true);
        CHECK(reloaded.stable_reuses==Fixture::views);CHECK(fixture.pixels()==opaque);

        // Both GPU generators consume the same completion-owned packed ABI.
        for(const auto mode:{gpu_classic,gpu_metal4})
        {fixture.frame(scenario,mode,2,false,false,true);CHECK(fixture.pixels()==opaque);}
    }
}

TEST_CASE("almetal: stable shadow payload ABI preserves all production cost fixture pixels",
          "[almetal][shadow-stable-data-parity]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,256u,2048u})
        for(const bool mixed:{false,true})
        {
            CAPTURE(family,draws,mixed);
            const Scenario scenario{draws,family,mixed};
            fixture.frame(scenario,scalar,2);const auto reference=fixture.pixels();
            for(const auto mode:{bulk,cpu_icb,gpu_classic,gpu_metal4})
            {CAPTURE(mode);fixture.frame(scenario,mode,2,false,false,true);CHECK(fixture.pixels()==reference);}
        }
    }
}

TEST_CASE("almetal: stable shadow slab reuse follows actual render completion and cancellation",
          "[almetal][shadow-stable-data-parity]")
{
    @autoreleasepool
    {
        Fixture fixture;
        id<MTLDevice> native=(__bridge id<MTLDevice>)fixture.device->nativeHandle();
        id<MTLSharedEvent> event=[native newSharedEvent];REQUIRE(event);
        struct ReleaseGate
        {
            id<MTLSharedEvent> event;
            std::shared_ptr<std::atomic<int>> state=std::make_shared<std::atomic<int>>(0);
            explicit ReleaseGate(id<MTLSharedEvent> value):event(value)
            {
                const auto phase=state;id<MTLSharedEvent> retained=event;
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),
                    dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
                        int expected=0;if(phase->compare_exchange_strong(expected,2))retained.signaledValue=1;
                    });
            }
            void release(){int expected=0;state->compare_exchange_strong(expected,1);event.signaledValue=1;}
            bool tripped()const{return state->load()==2;}
            ~ReleaseGate(){release();}
        } gate(event);
        auto blocker=fixture.device->makeCommandBuffer("Hold stable slab readers",&fixture.error);REQUIRE(blocker);
        id<MTLCommandBuffer> blocking_command=(__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
        [blocking_command encodeWaitForEvent:event value:1];
        id<MTLBuffer> positions=(__bridge id<MTLBuffer>)fixture.vertices->nativeHandle();
        CHECK(positions.hazardTrackingMode==MTLHazardTrackingModeTracked);
        id<MTLBuffer> staging=[native newBufferWithBytes:positions.contents length:positions.length options:MTLResourceStorageModeShared];
        REQUIRE(staging);
        auto blit=[blocking_command blitCommandEncoder];REQUIRE(blit);
        [blit copyFromBuffer:staging sourceOffset:0 toBuffer:positions destinationOffset:0 size:positions.length];
        [blit endEncoding];
        REQUIRE(blocker->commit(&fixture.error));
        fixture.arena->beginFrame(1);
        std::vector<Batch> output(256);
        const auto packet=[&](float z) {
            auto result=std::make_shared<std::vector<Input>>(output.size());
            std::vector<float> values(4+Fixture::joints*12,0.f);
            for(std::size_t j=0;j<Fixture::joints;++j){values[4+j*12]=values[9+j*12]=values[14+j*12]=1.f;}
            values[15]=z;
            auto pose=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),Fixture::joints,&fixture.error);REQUIRE(pose);
            for(std::size_t i=0;i<result->size();++i)
            {auto& input=(*result)[i];input.source=fixture.sources[1][i%2];input.source_generation=1;input.epoch=1;input.palette=pose;}
            return result;
        };
        const auto encode=[&](const auto& inputs,std::uint32_t slice) {
            REQUIRE(alMetalPrepareShadowBatches(*inputs,output,1));
            ALMetalWorldSpotShadowScene scene;scene.source_owner=inputs;
            scene.batches=output.data();scene.batch_count=output.size();scene.width=Fixture::width;scene.height=Fixture::height;
            scene.indirect_draws=true;scene.stable_shadow_data=true;
            REQUIRE(fixture.renderer->renderSpotShadow(*fixture.target,slice,scene,&fixture.error));
        };
        auto first=packet(.2f),second=packet(.3f);
        std::weak_ptr<const void> first_lifetime=first,second_lifetime=second;
        REQUIRE(fixture.renderer->beginShadowFrameBatch(1,"Two pending immutable shadow slabs",&fixture.error));
        auto completion=fixture.renderer->commandBatchCompletion();REQUIRE(completion);
        encode(first,0);encode(second,1);
        CHECK(fixture.renderer->shadowIndirectStats().stable_data_slots==2);
        REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
        first.reset();second.reset();CHECK_FALSE(first_lifetime.expired());CHECK_FALSE(second_lifetime.expired());
        CHECK_FALSE(completion->isComplete());
        // Completed-reload transfer/reuse is tested separately above. Reload
        // submits synchronous lookup uploads, so placing it behind this gated
        // writer would make the test wait for its own gate (PPD-104).
        auto cancelled=packet(.4f);std::weak_ptr<const void> cancelled_lifetime=cancelled;
        REQUIRE(fixture.renderer->beginShadowFrameBatch(2,"Cancelled stable slab reader",&fixture.error));
        encode(cancelled,2);cancelled.reset();CHECK_FALSE(cancelled_lifetime.expired());
        CHECK(fixture.renderer->shadowIndirectStats().stable_data_slots==3);
        fixture.renderer->cancelCommandBatch();CHECK(cancelled_lifetime.expired());
        auto retry=packet(.5f);
        REQUIRE(fixture.renderer->beginShadowFrameBatch(2,"Abandoned slab can be reused",&fixture.error));
        const auto reuses=fixture.renderer->shadowIndirectStats().stable_data_reuses;
        encode(retry,2);CHECK(fixture.renderer->shadowIndirectStats().stable_data_reuses==reuses+1);
        fixture.renderer->cancelCommandBatch();retry.reset();
        CHECK_FALSE(gate.tripped());CHECK_FALSE(completion->isComplete());
        CHECK_FALSE(first_lifetime.expired());CHECK_FALSE(second_lifetime.expired());
        fixture.renderer.reset(); // Submitted commands independently own slabs.
        CHECK_FALSE(gate.tripped());CHECK_FALSE(completion->isComplete());
        CHECK_FALSE(first_lifetime.expired());CHECK_FALSE(second_lifetime.expired());
        gate.release();
        REQUIRE(fixture.device->waitUntilIdle(&fixture.error));REQUIRE(completion->isComplete());
        const auto deadline=Clock::now()+std::chrono::seconds(2);
        while((!first_lifetime.expired()||!second_lifetime.expired())&&Clock::now()<deadline)std::this_thread::yield();
        CHECK(first_lifetime.expired());CHECK(second_lifetime.expired());
        std::array<float,Fixture::width*Fixture::height> first_pixels{},second_pixels{};
        REQUIRE(fixture.target->readback(*fixture.device,0,0,{},first_pixels.data(),Fixture::width*sizeof(float),&fixture.error));
        REQUIRE(fixture.target->readback(*fixture.device,0,1,{},second_pixels.data(),Fixture::width*sizeof(float),&fixture.error));
        const auto center=Fixture::height/2*Fixture::width+Fixture::width/2;
        CHECK(std::abs(first_pixels[center]-.6f)<1.e-6f);CHECK(std::abs(second_pixels[center]-.65f)<1.e-6f);
    }
}

TEST_CASE("almetal: opt-in complete native shadow GPU generation cost",
          "[.][almetal][shadow-generation-cost]")
{
    @autoreleasepool
    {
        if(!enabled("ALCHEMY_METAL_SHADOW_GENERATION_BENCHMARK"))
            SKIP("Set ALCHEMY_METAL_SHADOW_GENERATION_BENCHMARK=1 for the isolated cost gate");
#ifndef NDEBUG
        SKIP("Native shadow generation timing requires Release");
#endif
        REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
        REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
        Fixture fixture;fixture.preflight();
        std::cout<<std::setprecision(12)<<"SHADOW_GENERATION_COST source="
            <<(std::getenv("ALCHEMY_BENCHMARK_SOURCE")?std::getenv("ALCHEMY_BENCHMARK_SOURCE"):"unspecified")
            <<" build=Release views=4_directional_clamp_plus_2_spot same_native_renderer=1"
              " includes=packet_pose_seal_serial_prepare_validation_publication_encode_commit_retirement"
              " legacy_gpu=whole_legacy_command_interval metal4_gpu=independent_compute_feedback_sum"
              " wall=publication_start_to_actual_consumer_completion retirement=charged_separately"
              " feedback_receipt_delivery_wait=oracle_only_reported_separately"
              " excludes=cull_pose_solve_initial_PSO_source_record_creation\n";
        unsigned sample_id=0;
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,256u,2048u})
        for(const bool mixed:{false,true})
        {
            const Scenario scenario{draws,family,mixed};
            for(unsigned warm=0;warm<3;++warm)
                for(unsigned mode=0;mode<mode_count;++mode)fixture.frame(scenario,Mode(mode),warm);
            std::array<std::vector<double>,mode_count> cpu,wall,gpu,metal4;
            for(unsigned block=0;block<10;++block)
            for(unsigned repetition=0;repetition<10;++repetition)
            for(unsigned arm=0;arm<mode_count;++arm)
            {
                // Every block rotates the first arm; alternate direction to
                // balance first/last and thermal drift across all five arms.
                const unsigned mode=(block+(block%2?mode_count-1-arm:arm))%mode_count;
                const auto sample=fixture.frame(scenario,Mode(mode),block*10+repetition);
                const double cpu_total=sample.cpu+sample.commit+sample.release;
                cpu[mode].push_back(cpu_total);wall[mode].push_back(sample.wall+sample.release);
                gpu[mode].push_back(sample.legacy_gpu);metal4[mode].push_back(sample.metal4_gpu);
                std::cout<<"SHADOW_GENERATION_SAMPLE sample="<<sample_id++<<" family="<<families[family]
                    <<" draws="<<draws<<" visibility="<<(mixed?"mixed_known_25percent_reject":"all_visible_unknown_bounds")
                    <<" mode="<<modes[mode]<<" block="<<block<<" repetition="<<repetition<<" arm="<<arm
                    <<" cpu_total_ms="<<cpu_total<<" prepare_encode_ms="<<sample.cpu
                    <<" commit_ms="<<sample.commit<<" release_ms="<<sample.release
                    <<" wall_completed_release_ms="<<sample.wall+sample.release
                    <<" legacy_gpu_ms="<<sample.legacy_gpu<<" metal4_gpu_ms="<<sample.metal4_gpu
                    <<" feedback_receipt_wait_ms="<<sample.receipt_wait<<" native_commands="<<sample.commands
                    <<" generation_inputs="<<sample.generated<<" visibility_dispatches="<<sample.visibility
                    <<" metal4_dispatches="<<sample.metal4<<" metal4_timed="<<sample.metal4_timed
                    <<" record_publications="<<sample.publications<<" record_reuses="<<sample.reuses<<'\n';
            }
            for(unsigned mode=0;mode<mode_count;++mode)
            {
                REQUIRE(cpu[mode].size()==100);
                std::cout<<"SHADOW_GENERATION_SUMMARY family="<<families[family]<<" draws="<<draws
                    <<" visibility="<<(mixed?"mixed_known_25percent_reject":"all_visible_unknown_bounds")
                    <<" mode="<<modes[mode]<<" frames="<<cpu[mode].size()
                    <<" p50_cpu_total_ms="<<percentile(cpu[mode],.5)<<" p95_cpu_total_ms="<<percentile(cpu[mode],.95)
                    <<" p50_wall_ms="<<percentile(wall[mode],.5)<<" p95_wall_ms="<<percentile(wall[mode],.95)
                    <<" p50_legacy_gpu_ms="<<percentile(gpu[mode],.5)<<" p95_legacy_gpu_ms="<<percentile(gpu[mode],.95)
                    <<" p50_metal4_gpu_ms="<<percentile(metal4[mode],.5)<<" p95_metal4_gpu_ms="<<percentile(metal4[mode],.95)<<'\n';
            }
        }
        std::cout<<"SHADOW_GENERATION_LIMIT headless_mechanism_only;not_live_FPS;R32_parity_not_private_D32;"
            "GPU_intervals_are_queue_envelopes_not_busy_time;independent_intervals_may_overlap_do_not_sum_as_frame_span\n";
    }
}

TEST_CASE("almetal: selective opaque commands preserve mixed covered pixels and query boundaries",
          "[almetal][shadow-opaque-only-parity]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for(const bool mixed:{false,true})
        for(unsigned phase=0;phase<2;++phase)
        {
            CAPTURE(mixed,phase);
            const Scenario scenario{4608,3,mixed};
            fixture.frame(scenario,bulk,phase,true);const auto reference=fixture.pixels();
            const auto queries=fixture.last_queries;
            const auto candidate=fixture.frame(scenario,bulk,phase,true,false,false,true);
            CHECK(fixture.pixels()==reference);CHECK(fixture.last_queries==queries);
            CHECK(candidate.opaque_prepared==Fixture::views*4096);
            CHECK(candidate.opaque_executed==Fixture::views*4096);CHECK(candidate.opaque_runs==Fixture::views*2);
            CHECK(candidate.stable_publications==Fixture::views*2);CHECK(candidate.stable_palettes==Fixture::views*16);
            CHECK(candidate.material_misses==0);CHECK(candidate.material_creates==0);CHECK(candidate.visibility==0);
        }
        for(unsigned family=0;family<3;++family)
        {
            const Scenario scenario{32,family,false};
            fixture.frame(scenario,bulk,0,true);const auto reference=fixture.pixels();
            const auto candidate=fixture.frame(scenario,bulk,0,true,false,false,true);
            CHECK(fixture.pixels()==reference);CHECK(candidate.opaque_prepared==0);CHECK(candidate.stable_publications==0);
            CHECK(candidate.material_creates==0);CHECK(candidate.material_misses==0);
        }
    }
}

TEST_CASE("almetal: selective opaque limits and missing certification preserve every direct caster",
          "[almetal][shadow-opaque-only-parity]")
{
    @autoreleasepool
    {
        Fixture fixture;std::uint64_t frame=0;
        for(unsigned kind=0;kind<3;++kind)
        {
            CAPTURE(kind);
            const std::size_t count=kind==0?16385:2048;
            std::vector<Batch> batches(count,fixture.sources[kind==2?1:0][0]->prototype());
            std::array<float,16> pose{};pose[4]=pose[9]=pose[14]=1.f;pose[15]=.2f;
            for(std::size_t n=0;n<count;++n)
            {
                auto& batch=batches[n];std::memcpy(batch.modelview_projection,alMetalShadowIdentity.data(),64);
                if(kind==0)batch.modelview_projection[14]=n+1==count?-.5f:.5f;
                if(kind==2){batch.skin_palette=pose.data();batch.skin_palette_float_count=pose.size();batch.skin_joint_count=1;}
            }
            ALMetalWorldVisibilityQuery query;query.batch_offset=1024;query.count_samples=1;
            std::memcpy(query.modelview_projection,alMetalShadowIdentity.data(),64);
            query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.3f;
            ALMetalWorldSpotShadowScene scene;scene.batches=batches.data();scene.batch_count=count;
            scene.width=Fixture::width;scene.height=Fixture::height;
            if(kind==1){scene.visibility_queries=&query;scene.visibility_query_count=1;}
            std::array<std::vector<float>,2> pixels;
            std::array<std::uint64_t,2> samples{};
            for(unsigned candidate=0;candidate<2;++candidate)
            {
                fixture.arena->beginFrame(++frame);scene.opaque_indirect_draws=candidate;
                const auto before=fixture.renderer->shadowIndirectStats();
                std::shared_ptr<ALMetalVisibilityResultBatch> results;
                REQUIRE(fixture.renderer->renderSpotShadow(*fixture.target,candidate,scene,&fixture.error,&results));
                pixels[candidate].resize(Fixture::width*Fixture::height);
                REQUIRE(fixture.target->readback(*fixture.device,0,candidate,{},pixels[candidate].data(),Fixture::width*sizeof(float),&fixture.error));
                if(kind==1){REQUIRE(results);REQUIRE(results->wait(0,samples[candidate],&fixture.error));CHECK(samples[candidate]>0);}
                const auto after=fixture.renderer->shadowIndirectStats();
                CHECK(after.encoded_records-before.encoded_records==count);
                CHECK(after.opaque_indirect_prepared_draws==before.opaque_indirect_prepared_draws);
                CHECK(after.stable_data_publications==before.stable_data_publications);
                CHECK(after.opaque_indirect_skipped_draws-before.opaque_indirect_skipped_draws==(candidate?count:0));
            }
            CHECK(pixels[0]==pixels[1]);CHECK(samples[0]==samples[1]);
            const auto center=Fixture::height/2*Fixture::width+Fixture::width/2;
            CHECK(std::abs(pixels[1][center]-(kind==0?.25f:kind==2?.6f:.5f))<1.e-6f);
        }
    }
}

TEST_CASE("almetal: opt-in complete mixed selective opaque command cost",
          "[.][almetal][shadow-opaque-only-cost]")
{
    @autoreleasepool
    {
        if(!enabled("ALCHEMY_METAL_SHADOW_OPAQUE_ONLY_BENCHMARK"))
            SKIP("Set ALCHEMY_METAL_SHADOW_OPAQUE_ONLY_BENCHMARK=1 for the isolated mixed opaque gate");
#ifndef NDEBUG
        SKIP("Native mixed opaque timing requires Release");
#endif
        REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
        REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
        std::array<std::unique_ptr<Fixture>,2> fixtures{{std::make_unique<Fixture>(),std::make_unique<Fixture>()}};
        std::cout<<std::setprecision(12)<<"SHADOW_OPAQUE_ONLY_COST source="
            <<(std::getenv("ALCHEMY_BENCHMARK_SOURCE")?std::getenv("ALCHEMY_BENCHMARK_SOURCE"):"unspecified")
            <<" build=Release counts=2048_static_opaque_512_rigged_PBR_2048_rigged_opaque views=6_streamed"
              " queries=2_original_boundaries_per_view quality=identical_rendering caches=independent_per_arm"
              " control=unchanged_bulk_direct global_stable_fullindirect_materialtables_GPUgeneration=off"
              " includes=packet_pose_seal_serial_prepare_validation_encode_all_commits_retirement\n";
        unsigned sample_id=0;
        for(const bool mixed:{false,true})
        {
            const Scenario scenario{4608,3,mixed};
            for(unsigned phase=0;phase<2;++phase)
            {
                fixtures[0]->frame(scenario,bulk,phase,true);const auto pixels=fixtures[0]->pixels();
                const auto queries=fixtures[0]->last_queries;
                fixtures[1]->frame(scenario,bulk,phase,true,false,false,true);
                REQUIRE(fixtures[1]->pixels()==pixels);REQUIRE(fixtures[1]->last_queries==queries);
            }
            for(unsigned warm=0;warm<4;++warm)
            {fixtures[0]->frame(scenario,bulk,warm,true);fixtures[1]->frame(scenario,bulk,warm,true,false,false,true);}
            std::array<std::vector<double>,2> cpu,wall,gpu;
            for(unsigned block=0;block<10;++block)
            for(unsigned repetition=0;repetition<5;++repetition)
            {
                const std::array<bool,4> order=block%2?std::array<bool,4>{{true,false,false,true}}:
                    std::array<bool,4>{{false,true,true,false}};
                for(unsigned position=0;position<order.size();++position)
                {
                    const bool candidate=order[position];
                    const auto sample=fixtures[candidate]->frame(scenario,bulk,block*5+repetition,true,false,false,candidate);
                    const double total=sample.cpu+sample.commit+sample.release;
                    cpu[candidate].push_back(total);wall[candidate].push_back(sample.wall+sample.release);gpu[candidate].push_back(sample.legacy_gpu);
                    std::cout<<"SHADOW_OPAQUE_ONLY_SAMPLE sample="<<sample_id++<<" candidate="<<candidate
                        <<" visibility="<<(mixed?"mixed_25percent_offscreen":"all_visible")
                        <<" block="<<block<<" repetition="<<repetition<<" position="<<position
                        <<" cpu_total_ms="<<total<<" prepare_encode_ms="<<sample.cpu<<" commit_ms="<<sample.commit
                        <<" release_ms="<<sample.release<<" wall_completed_release_ms="<<sample.wall+sample.release
                        <<" legacy_gpu_sum_ms="<<sample.legacy_gpu<<" native_commands="<<sample.commands
                        <<" opaque_prepared="<<sample.opaque_prepared<<" opaque_executed="<<sample.opaque_executed
                        <<" opaque_runs="<<sample.opaque_runs<<" opaque_skipped="<<sample.opaque_skipped
                        <<" stable_publications="<<sample.stable_publications<<" stable_palettes="<<sample.stable_palettes
                        <<" stable_fallbacks="<<sample.stable_fallbacks<<" material_creates="<<sample.material_creates
                        <<" material_misses="<<sample.material_misses<<" generation_inputs="<<sample.generated<<'\n';
                }
            }
            for(unsigned candidate=0;candidate<2;++candidate)
            {
                REQUIRE(cpu[candidate].size()==100);
                std::cout<<"SHADOW_OPAQUE_ONLY_SUMMARY visibility="<<(mixed?"mixed_25percent_offscreen":"all_visible")
                    <<" candidate="<<candidate<<" frames="<<cpu[candidate].size()
                    <<" p50_cpu_total_ms="<<percentile(cpu[candidate],.5)<<" p95_cpu_total_ms="<<percentile(cpu[candidate],.95)
                    <<" p50_wall_ms="<<percentile(wall[candidate],.5)<<" p95_wall_ms="<<percentile(wall[candidate],.95)
                    <<" p50_gpu_sum_ms="<<percentile(gpu[candidate],.5)<<" p95_gpu_sum_ms="<<percentile(gpu[candidate],.95)<<'\n';
            }
        }
        std::cout<<"SHADOW_OPAQUE_ONLY_LIMIT headless_mechanism_only;not_live_FPS;R32_parity_not_private_D32;"
            "sum_of_overlapping_GPU_envelopes_is_not_GPU_busy_or_frame_span\n";
    }
}

TEST_CASE("almetal: opt-in complete six-commit versus one-transaction shadow cost",
          "[.][almetal][shadow-frame-batch-cost]")
{
    @autoreleasepool
    {
        if(!enabled("ALCHEMY_METAL_SHADOW_FRAME_BATCH_BENCHMARK"))
            SKIP("Set ALCHEMY_METAL_SHADOW_FRAME_BATCH_BENCHMARK=1 for the isolated transaction cost gate");
#ifndef NDEBUG
        SKIP("Native shadow transaction timing requires Release");
#endif
        REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
        REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
        Fixture fixture;
        // Actual six-slice output parity, including both depth directions,
        // before isolated timing. Query publication and borrowed ownership
        // are additionally exercised by the ordinary extended regression.
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,2048u})
        {
            const Scenario scenario{draws,family,false};
            fixture.frame(scenario,bulk,0,true,draws>=2048);const auto reference=fixture.pixels();
            fixture.frame(scenario,bulk,0,false,draws>=2048);REQUIRE(fixture.pixels()==reference);
        }
        std::cout<<std::setprecision(12)<<"SHADOW_FRAME_BATCH_COST source="
            <<(std::getenv("ALCHEMY_BENCHMARK_SOURCE")?std::getenv("ALCHEMY_BENCHMARK_SOURCE"):"unspecified")
            <<" build=Release views=4_directional_clamp_plus_2_spot quality=identical_current_bulk_direct"
              " includes=packet_pose_seal_prepare_validation_publication_all_commits_retirement"
              " wall=publication_start_to_actual_final_consumer_completion_plus_retirement"
              " dispatch=same_both_arms_only_at2048 excludes=cull_pose_solve_initial_PSO_source_record_creation\n";
        unsigned sample_id=0;
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,2048u})
        {
            const Scenario scenario{draws,family,false};
            for(unsigned warm=0;warm<4;++warm)
            {fixture.frame(scenario,bulk,warm,true,draws>=2048);fixture.frame(scenario,bulk,warm,false,draws>=2048);}
            std::array<std::vector<double>,2> cpu,wall,gpu_span;
            for(unsigned block=0;block<10;++block)
            for(unsigned repetition=0;repetition<5;++repetition)
            {
                const std::array<bool,4> order=block%2?std::array<bool,4>{{true,false,false,true}}:
                    std::array<bool,4>{{false,true,true,false}};
                for(unsigned arm=0;arm<order.size();++arm)
                {
                    const bool standalone=order[arm];
                    const auto sample=fixture.frame(scenario,bulk,block*5+repetition,standalone,draws>=2048);
                    const double total_cpu=sample.cpu+sample.commit+sample.release;
                    cpu[standalone].push_back(total_cpu);wall[standalone].push_back(sample.wall+sample.release);
                    gpu_span[standalone].push_back(sample.legacy_gpu_span);
                    std::cout<<"SHADOW_FRAME_BATCH_SAMPLE sample="<<sample_id++<<" family="<<families[family]
                        <<" draws="<<draws<<" mode="<<(standalone?"six_commits":"one_transaction")
                        <<" block="<<block<<" repetition="<<repetition<<" arm="<<arm
                        <<" native_commands="<<sample.commands<<" cpu_total_ms="<<total_cpu
                        <<" prepare_encode_ms="<<sample.cpu<<" all_commit_ms="<<sample.commit
                        <<" release_ms="<<sample.release<<" wall_completed_release_ms="<<sample.wall+sample.release
                        <<" legacy_gpu_sum_ms="<<sample.legacy_gpu<<" legacy_gpu_span_ms="<<sample.legacy_gpu_span<<'\n';
                }
            }
            for(unsigned standalone=0;standalone<2;++standalone)
            {
                REQUIRE(cpu[standalone].size()==100);
                std::cout<<"SHADOW_FRAME_BATCH_SUMMARY family="<<families[family]<<" draws="<<draws
                    <<" mode="<<(standalone?"six_commits":"one_transaction")<<" frames="<<cpu[standalone].size()
                    <<" p50_cpu_total_ms="<<percentile(cpu[standalone],.5)<<" p95_cpu_total_ms="<<percentile(cpu[standalone],.95)
                    <<" p50_wall_ms="<<percentile(wall[standalone],.5)<<" p95_wall_ms="<<percentile(wall[standalone],.95)
                    <<" p50_gpu_span_ms="<<percentile(gpu_span[standalone],.5)<<" p95_gpu_span_ms="<<percentile(gpu_span[standalone],.95)<<'\n';
            }
        }
        std::cout<<"SHADOW_FRAME_BATCH_LIMIT headless_mechanism_only;not_live_FPS;R32_parity_not_private_D32;"
            "native_GPU_span_includes_queue_gaps_and_is_not_GPU_busy_time\n";
    }
}

TEST_CASE("almetal: opt-in completion-owned direct and indirect shadow slab cost",
          "[.][almetal][shadow-stable-data-cost]")
{
    @autoreleasepool
    {
        if(!enabled("ALCHEMY_METAL_SHADOW_STABLE_DATA_BENCHMARK"))
            SKIP("Set ALCHEMY_METAL_SHADOW_STABLE_DATA_BENCHMARK=1 for the isolated stable data gate");
#ifndef NDEBUG
        SKIP("Native shadow slab timing requires Release");
#endif
        REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
        REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
        // Separate steady-state caches prevent another arm's direct/scalar
        // frames from ageing this arm's material keys or changing its arenas.
        std::array<std::unique_ptr<Fixture>,8> fixtures;
        for(auto& fixture:fixtures)fixture=std::make_unique<Fixture>();
        std::cout<<std::setprecision(12)<<"SHADOW_STABLE_DATA_COST source="
            <<(std::getenv("ALCHEMY_BENCHMARK_SOURCE")?std::getenv("ALCHEMY_BENCHMARK_SOURCE"):"unspecified")
            <<" build=Release views=4_directional_clamp_plus_2_spot caches=independent_per_arm"
              " generation=once_per_rendered_frame quality=identical_native_direct_or_indirect_PSO"
              " includes=packet_pose_seal_serial_prepare_validation_payload_copy_encode_commit_retirement"
              " legacy_gpu=command_interval metal4_gpu=independent_feedback_sum"
              " feedback_receipt_wait=reported_oracle_only excludes=cull_pose_solve_initial_PSO_source_record_creation\n";
        unsigned sample_id=0;
        for(unsigned family=0;family<3;++family)
        for(const auto draws:{32u,2048u})
        for(const bool mixed:{false,true})
        {
            const Scenario scenario{draws,family,mixed};
            std::vector<float> reference;
            for(unsigned arm=0;arm<fixtures.size();++arm)
            {
                fixtures[arm]->frame(scenario,Mode(bulk+arm/2),0,true,false,arm%2);
                const auto pixels=fixtures[arm]->pixels();
                if(!arm)reference=pixels;else REQUIRE(pixels==reference);
            }
            for(unsigned warm=0;warm<3;++warm)
                for(unsigned arm=0;arm<fixtures.size();++arm)
                    fixtures[arm]->frame(scenario,Mode(bulk+arm/2),warm,true,false,arm%2);
            std::array<std::vector<double>,8> cpu,wall,gpu,metal4;
            for(unsigned block=0;block<10;++block)
            for(unsigned repetition=0;repetition<10;++repetition)
            for(unsigned position=0;position<fixtures.size();++position)
            {
                const unsigned arm=(block+(block%2?fixtures.size()-1-position:position))%fixtures.size();
                const auto mode=Mode(bulk+arm/2);const bool stable=arm%2;
                const auto sample=fixtures[arm]->frame(scenario,mode,block*10+repetition,true,false,stable);
                const double cpu_total=sample.cpu+sample.commit+sample.release;
                cpu[arm].push_back(cpu_total);wall[arm].push_back(sample.wall+sample.release);
                gpu[arm].push_back(sample.legacy_gpu);metal4[arm].push_back(sample.metal4_gpu);
                std::cout<<"SHADOW_STABLE_DATA_SAMPLE sample="<<sample_id++<<" family="<<families[family]
                    <<" draws="<<draws<<" visibility="<<(mixed?"mixed_known_25percent_reject":"all_visible_unknown_bounds")
                    <<" mode="<<modes[mode]<<" stable="<<stable<<" block="<<block
                    <<" repetition="<<repetition<<" position="<<position<<" arm="<<arm
                    <<" cpu_total_ms="<<cpu_total<<" prepare_encode_ms="<<sample.cpu
                    <<" commit_ms="<<sample.commit<<" release_ms="<<sample.release
                    <<" wall_completed_release_ms="<<sample.wall+sample.release
                    <<" legacy_gpu_ms="<<sample.legacy_gpu<<" metal4_gpu_ms="<<sample.metal4_gpu
                    <<" feedback_receipt_wait_ms="<<sample.receipt_wait<<" native_commands="<<sample.commands
                    <<" generation_inputs="<<sample.generated<<" visibility_dispatches="<<sample.visibility
                    <<" metal4_dispatches="<<sample.metal4<<" metal4_timed="<<sample.metal4_timed
                    <<" stable_publications="<<sample.stable_publications<<" stable_reuses="<<sample.stable_reuses
                    <<" stable_fallbacks="<<sample.stable_fallbacks<<" stable_palettes="<<sample.stable_palettes
                    <<" stable_bytes="<<sample.stable_bytes<<" material_hits="<<sample.material_hits
                    <<" material_misses="<<sample.material_misses<<" material_creates="<<sample.material_creates<<'\n';
            }
            for(unsigned arm=0;arm<fixtures.size();++arm)
            {
                REQUIRE(cpu[arm].size()==100);
                std::cout<<"SHADOW_STABLE_DATA_SUMMARY family="<<families[family]<<" draws="<<draws
                    <<" visibility="<<(mixed?"mixed_known_25percent_reject":"all_visible_unknown_bounds")
                    <<" mode="<<modes[bulk+arm/2]<<" stable="<<arm%2<<" frames="<<cpu[arm].size()
                    <<" p50_cpu_total_ms="<<percentile(cpu[arm],.5)<<" p95_cpu_total_ms="<<percentile(cpu[arm],.95)
                    <<" p50_wall_ms="<<percentile(wall[arm],.5)<<" p95_wall_ms="<<percentile(wall[arm],.95)
                    <<" p50_legacy_gpu_ms="<<percentile(gpu[arm],.5)<<" p95_legacy_gpu_ms="<<percentile(gpu[arm],.95)
                    <<" p50_metal4_gpu_ms="<<percentile(metal4[arm],.5)<<" p95_metal4_gpu_ms="<<percentile(metal4[arm],.95)<<'\n';
            }
        }
        std::cout<<"SHADOW_STABLE_DATA_LIMIT headless_mechanism_only;not_live_FPS;R32_parity_not_private_D32;"
            "GPU_intervals_not_busy_time;independent_intervals_may_overlap_do_not_sum_as_frame_span;"
            "body_cloth_terrain_keep_existing_bindings\n";
    }
}
