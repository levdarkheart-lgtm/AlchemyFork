/**
 * @file alpipeline_compile_cost_attribution_native.inl
 * @brief Contained owner CPU attribution for retained publication, not FPS.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
TEST_CASE("Actual material source compilation separates capability and group publication CPU costs",
          "[almetal][.native-cohort-compile-attribution]")
{
    using namespace Inputs::ALScenePublicationNative;
    constexpr U32 count=512,samples=12,warmups=2;
    const auto cpu_ns=[]
    {
        timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;
    };
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);
        std::vector<Inputs::LLDrawInfo*> sources;sources.reserve(count);
        for(U32 i=0;i<count;++i)sources.push_back(&f.materialDraw());
        std::array<std::array<double,samples>,4> costs{};
        for(U32 sample=0;sample<samples+warmups;++sample)
        {
            CAPTURE(sample);++Inputs::gFrameCount;
            f.group->invalidate();f.group_layout.reset();f.group_dependencies.reset();
            for(auto* info:sources)info->state->invalidate();
            auto ordinary=f.flavor;ordinary.native_resources=false;ordinary.cohort_source=false;
            auto resources=f.flavor;resources.native_resources=true;resources.cohort_source=false;
            auto record=f.flavor;record.native_resources=false;record.cohort_source=true;
            const auto before_records=cohort_source_publications;
            const auto before_serial=Inputs::sNativeSceneSerial;
            std::array<double,4> value{};
            auto begin=cpu_ns();
            for(auto* info:sources)
            {
                auto source=world(*info,ordinary,Inputs::sNativeSceneSources,
                    Inputs::sNativeSceneSerial,Inputs::gFrameCount);
                REQUIRE(source);CHECK_FALSE(source.prototype->native_resource_source);
            }
            auto end=cpu_ns();value[0]=(end-begin)/1e6;
            CHECK(cohort_source_publications==before_records);
            CHECK(Inputs::sNativeSceneSerial==before_serial+count);
            begin=cpu_ns();
            for(auto* info:sources)
            {
                auto source=world(*info,resources,Inputs::sNativeSceneSources,
                    Inputs::sNativeSceneSerial,Inputs::gFrameCount);
                REQUIRE(source);REQUIRE(source.prototype->native_resource_source);
            }
            end=cpu_ns();value[1]=(end-begin)/1e6;
            CHECK(cohort_source_publications==before_records);
            CHECK(Inputs::sNativeSceneSerial==before_serial+2*count);
            begin=cpu_ns();
            for(auto* info:sources)
            {
                auto source=world(*info,record,Inputs::sNativeSceneSources,
                    Inputs::sNativeSceneSerial,Inputs::gFrameCount);
                REQUIRE(source);
                const auto* variant=findWorldVariant(*source.owner,record.key());
                REQUIRE(variant);REQUIRE(variant->cohort_source);
            }
            end=cpu_ns();value[2]=(end-begin)/1e6;
            CHECK(cohort_source_publications==before_records+count);
            CHECK(Inputs::sNativeSceneSerial==before_serial+3*count);
            const auto before_group_serial=Inputs::sNativeSceneSerial;
            begin=cpu_ns();
            auto packet=Inputs::ALSceneGroupPublicationNative::sealMaterial(f.group,
                f.group_dependencies,f.group_layout,f.model,f.region.snapshot(),sources,
                f.flavor,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
            REQUIRE(packet);REQUIRE(packet->cohort);
            end=cpu_ns();value[3]=(end-begin)/1e6;
            CHECK(packet->cohort->size()==count);
            CHECK(cohort_source_publications==before_records+count);
            CHECK(Inputs::sNativeSceneSerial==before_group_serial+1);
            if(sample>=warmups)for(U32 stage=0;stage<value.size();++stage)
                costs[stage][sample-warmups]=value[stage];
            std::cout<<"NATIVE_COHORT_COMPILE_STAGE_RAW family="<<(family==1?"legacy":"pbr")
                <<" sources="<<count<<" sample="<<sample<<" warmup="<<(sample<warmups)
                <<" ordinary_source_cpu_ms="<<value[0]<<" resource_promotion_cpu_ms="<<value[1]
                <<" record_promotion_cpu_ms="<<value[2]<<" group_from_completed_records_cpu_ms="<<value[3]<<'\n';
            gGL.frameFences().release();
        }
        std::cout<<"NATIVE_COHORT_COMPILE_STAGE family="<<(family==1?"legacy":"pbr")
            <<" sources="<<count<<" samples="<<samples<<" warmups="<<warmups
            <<" ordinary_source_cpu_median_ms="<<median(costs[0])
            <<" resource_promotion_cpu_median_ms="<<median(costs[1])
            <<" record_promotion_cpu_median_ms="<<median(costs[2])
            <<" group_from_completed_records_cpu_median_ms="<<median(costs[3])
            <<" attribution_only=1 separate_promotions_copy_parent_twice=1 do_not_sum_as_full_path=1"
            <<" geometry_creation_excluded=1 no_render_or_gpu_wait=1 whole_viewer_fps_claim=0\n";
    }
}
