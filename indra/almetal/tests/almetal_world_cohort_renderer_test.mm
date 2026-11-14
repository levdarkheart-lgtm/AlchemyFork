/**
 * @file almetal_world_cohort_renderer_test.mm
 * @brief Direct-cohort renderer accounting and ordinary path regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include <cstdlib>
#include <filesystem>

namespace
{
struct DirectCohortCpu
{
    std::vector<ALMetalWorldSimpleBatch> rows;
    std::vector<ALMetalWorldKeyedSource> keys;
};
struct DirectCohortFixture : Fixture
{
    std::shared_ptr<DirectCohortCpu> cpu=std::make_shared<DirectCohortCpu>();
    std::shared_ptr<const ALMetalWorldSourceCohort> certificate;
    ALMetalWorldSourceCohortView group;
    std::array<float,4+110*12> large_pose{};
    explicit DirectCohortFixture(unsigned family)
    {
        ALMetalWorldMaterialDataCache cache(*device);
        cpu->rows.reserve(2);cpu->keys.reserve(2);
        for(unsigned i=0;i<2;++i)
        {
            auto source=batch(family);
            source.texcoord1_offset=source.texcoord2_offset=source.texcoord_offset;
            source.texture_matrix[12]=.125f*i;
            if(family==2)source.double_sided=i!=0;
            REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
            REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
            for(const auto& uniform:{source.native_material_source->materialLease(),
                source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                if(uniform)resources->buffers.push_back(uniform);
            cpu->rows.push_back(std::move(source));
        }
        for(unsigned i=0;i<cpu->rows.size();++i)cpu->keys.push_back({{i+1,family,1},&cpu->rows[i]});
        certificate=ALMetalWorldSourceCohort::seal(cpu->keys,cpu,resources,9,&error);
        INFO(error);REQUIRE(certificate);
        group.cohort=certificate;group.cpu_owner=cpu;group.native_owner=resources;
        group.logical_begin=group.base_before=1;
        identity(group.view.modelview_projection);identity(group.view.modelview);
        group.view.modelview_projection[12]=-.15f;
        for(unsigned axis=0;axis<3;++axis)
            group.view.normal_matrix[axis][axis]=group.view.rigged_normal_matrix[axis][axis]=1.f;
        // The group's UV is intentionally unused; every source keeps fixed UV.
        group.view.texture_matrix[12]=std::numeric_limits<float>::quiet_NaN();
        for(unsigned joint=0;joint<110;++joint)
            for(unsigned axis=0;axis<3;++axis)large_pose[4+joint*12+axis*5]=1.f;
    }
    std::vector<ALMetalWorldSimpleBatch> baseRows()
    {
        auto skin=batch(4);skin.skin_palette=large_pose.data();skin.skin_joint_count=110;
        skin.skin_palette_float_count=large_pose.size();
        return {skin,batch(7),skin};
    }
    ALMetalWorldSimpleBatch flattened(unsigned i) const
    {
        auto source=certificate->source(i);
        std::memcpy(source.modelview_projection,group.view.modelview_projection,64);
        std::memcpy(source.modelview,group.view.modelview,64);
        std::memcpy(source.normal_matrix,group.view.normal_matrix,48);
        std::memcpy(source.rigged_normal_matrix,group.view.rigged_normal_matrix,48);
        return source;
    }
    std::vector<ALMetalWorldVisibilityQuery> queries()
    {
        std::vector<ALMetalWorldVisibilityQuery> values(6);
        for(unsigned i=0;i<values.size();++i)
        {
            auto& q=values[i];identity(q.modelview_projection);q.center[2]=.6f;
            q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.1f;
            q.fan=7;q.count_samples=1;q.batch_offset=i;
        }
        return values;
    }
    struct Output{Pixels pixels;std::vector<std::uint64_t> queries;bool operator==(const Output&)const=default;};
    Output render(bool cohort,unsigned format)
    {
        auto raw=baseRows();auto q=queries();auto s=scene();
        if(!cohort){raw.insert(raw.begin()+1,flattened(0));raw.insert(raw.begin()+2,flattened(1));}
        s.batches=raw.data();s.batch_count=raw.size();s.source_owner=resources;
        s.hdr=format&1;s.normal_hdr=format&2;s.reverse_z=format&4;
        s.visibility_queries=q.data();s.visibility_query_count=q.size();
        if(cohort)
        {
            s.cohorts=&group;s.cohort_count=1;
            s.material_argument_buffers=true;s.source_command_banks=true;
            // An empty optional source-row view must not turn cohort rows into
            // GPU requests, key arrays or CPU-source completion retainers.
            s.source_rows=std::make_shared<ALMetalWorldSourceView>();
        }
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        auto target=renderer->render(s,&error,&results);INFO(error);REQUIRE(target);
        Output output{read(*target),{}};REQUIRE(results);output.queries.resize(q.size());
        for(unsigned i=0;i<q.size();++i)REQUIRE(results->wait(i,output.queries[i],&error));
        return output;
    }
};
}

TEST_CASE("almetal: direct cohorts keep accepted accounting and ordinary terrain palette paths",
          "[almetal][world-cohort-renderer]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);DirectCohortFixture f(family);
            // Exercise the standalone large-palette preparation path. Cohorts
            // add no palette/factor slots; terrain remains in logical order.
            f.renderer->setFrameData({});
            for(unsigned format=0;format<8;++format)
            {
                CAPTURE(format);const auto expected=f.render(false,format);
                const auto before=f.renderer->worldCohortStats();
                const auto resource=f.renderer->worldResourceStats();
                const auto rows=f.renderer->worldSourceStats();
                const auto banks=f.renderer->worldSourceCommandStats();
                CHECK(f.render(true,format)==expected);
                const auto after=f.renderer->worldCohortStats();
                CHECK(after.views-before.views==1);CHECK(after.groups-before.groups==1);
                CHECK(after.draws-before.draws==2);
                CHECK(f.renderer->worldResourceStats().requested_draws==resource.requested_draws);
                CHECK(f.renderer->worldResourceStats().bound_draws==resource.bound_draws);
                CHECK(f.renderer->worldResourceStats().fallback_draws==resource.fallback_draws);
                CHECK(f.renderer->worldSourceStats().requested==rows.requested);
                CHECK(f.renderer->worldSourceStats().bound==rows.bound);
                CHECK(f.renderer->worldSourceCommandStats().execute_calls==banks.execute_calls);
                CHECK(f.renderer->worldSourceCommandStats().cache.requests==banks.cache.requests);
            }
        }
    }
}

TEST_CASE("almetal: direct cohorts use the unchanged older ordinary shader library",
          "[almetal][world-cohort-renderer][.world-cohort-renderer-previous-shader]")
{
    @autoreleasepool
    {
        const char* path=std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY");
        REQUIRE(path);REQUIRE(std::filesystem::is_regular_file(path));
        for(unsigned family:{1u,2u})
        {
            DirectCohortFixture f(family);
            REQUIRE(f.renderer->reloadShaders(std::string(path),&f.error));
            REQUIRE_FALSE(f.renderer->shaderLibrary()->hasFunction("almetal_world_material_source_rows_vertex"));
            for(unsigned format:{0u,3u,6u})
            {
                const auto expected=f.render(false,format);const auto before=f.renderer->worldCohortStats();
                CHECK(f.render(true,format)==expected);
                CHECK(f.renderer->worldCohortStats().draws-before.draws==2);
            }
        }
    }
}
