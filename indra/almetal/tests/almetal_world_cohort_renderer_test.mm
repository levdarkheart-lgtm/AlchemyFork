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
    ALMetalWorldSourceCohortView additionalGroup(unsigned family,std::size_t logical,std::size_t base,unsigned identity_base,
        unsigned count=2,std::uint64_t region=9,bool varying_cull=false)
    {
        auto owner=std::make_shared<DirectCohortCpu>();owner->rows.reserve(count);owner->keys.reserve(count);
        ALMetalWorldMaterialDataCache cache(*device);
        for(unsigned row=0;row<count;++row)
        {
            auto source=batch(family);source.texture_matrix[12]=row*.125f;
            if(varying_cull && family==2)source.double_sided=(identity_base+row)%2;
            if(family)
            {
                REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
                REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
                for(const auto& uniform:{source.native_material_source->materialLease(),source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                    if(uniform)resources->buffers.push_back(uniform);
            }
            owner->rows.push_back(std::move(source));
        }
        for(unsigned row=0;row<count;++row)owner->keys.push_back({{identity_base+row,family,1},&owner->rows[row]});
        auto view=group;view.logical_begin=logical;view.base_before=base;view.cpu_owner=owner;
        view.cohort=ALMetalWorldSourceCohort::seal(owner->keys,owner,resources,region,&error);INFO(error);REQUIRE(view.cohort);
        return view;
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
                CHECK(f.render(true,format)==expected);
                const auto after=f.renderer->worldCohortStats();
                CHECK(after.views-before.views==1);CHECK(after.groups-before.groups==1);
                CHECK(after.draws-before.draws==2);
            }
        }
    }
}


TEST_CASE("almetal: native cohorts preserve mixed simple material PBR raw and reference order",
          "[almetal][world-cohort-renderer]")
{
    @autoreleasepool
    {
        {
        DirectCohortFixture f(1);
        auto legacy=f.group;legacy.logical_begin=3;legacy.base_before=1;
        std::array<ALMetalWorldSourceCohortView,3> groups{
            f.additionalGroup(0,0,0,21),legacy,f.additionalGroup(2,6,2,11)};
        std::array<ALMetalWorldSimpleBatch,2> raw{f.batch(0),f.batch(0)};
        auto referenced=std::make_shared<ALMetalWorldSimpleBatch>(f.batch(0));ALMetalWorldSourceReference reference;reference.source=referenced.get();
        identity(reference.view.modelview_projection);identity(reference.view.modelview);
        reference.view.modelview_projection[12]=.2f;
        for(unsigned axis=0;axis<3;++axis)reference.view.normal_matrix[axis][axis]=reference.view.rigged_normal_matrix[axis][axis]=1.f;
        const std::array<ALMetalWorldDrawIndex,3> order{0,alMetalWorldDrawReferenceBit,1};
        auto grouped=f.scene();grouped.batches=raw.data();grouped.batch_count=raw.size();
        grouped.references=&reference;grouped.reference_count=1;grouped.draw_order=order.data();grouped.draw_order_count=order.size();
        grouped.cohorts=groups.data();grouped.cohort_count=groups.size();grouped.source_owner=f.resources;
        grouped.cpu_source_owner=referenced;
        std::vector<ALMetalWorldSimpleBatch> flat;flat.reserve(9);
        for(std::size_t row=0;row<grouped.drawCount();++row)
        {
            const auto draw=grouped.draw(row);auto value=draw.source();
            std::memcpy(value.modelview_projection,draw.modelviewProjection(),64);std::memcpy(value.modelview,draw.modelview(),64);
            std::memcpy(value.normal_matrix,draw.normalMatrix(),48);std::memcpy(value.rigged_normal_matrix,draw.riggedNormalMatrix(),48);
            std::memcpy(value.texture_matrix,draw.textureMatrix(),64);flat.push_back(std::move(value));
        }
        std::vector<ALMetalWorldVisibilityQuery> queries(10);
        for(unsigned row=0;row<queries.size();++row)
        {auto& q=queries[row];identity(q.modelview_projection);q.center[2]=.6f;q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.1f;
         q.batch_offset=row;q.fan=7;q.count_samples=1;}
        grouped.visibility_queries=queries.data();grouped.visibility_query_count=queries.size();
        for(unsigned format:{0u,3u,6u})
        {
            CAPTURE(format);grouped.hdr=format&1;grouped.normal_hdr=format&2;grouped.reverse_z=format&4;
            std::array<DirectCohortFixture::Output,2> outputs;
            for(unsigned mode=0;mode<2;++mode)
            {
                auto scene=grouped;
                if(!mode){scene.batches=flat.data();scene.batch_count=flat.size();scene.references=nullptr;scene.reference_count=0;
                    scene.draw_order=nullptr;scene.draw_order_count=0;scene.cohorts=nullptr;scene.cohort_count=0;}
                std::shared_ptr<ALMetalVisibilityResultBatch> result;
                auto target=f.renderer->render(scene,&f.error,&result);INFO(f.error);REQUIRE(target);REQUIRE(result);
                outputs[mode].pixels=f.read(*target);outputs[mode].queries.resize(queries.size());
                for(unsigned row=0;row<queries.size();++row)REQUIRE(result->wait(row,outputs[mode].queries[row],&f.error));
            }
            CHECK(outputs[0]==outputs[1]);
        }
        }
    }
}
