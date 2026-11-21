/** Published CPU visibility and scalar native rendering contracts. */
#include "almetal_world_inputs_fixture.h"
#include "../almetalworldvisibility.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalresource.h"
#include <bit>

namespace
{
ALMetalWorldConservativeBounds triangleBounds(bool qualified=true)
{
    ALMetalWorldConservativeBounds b;b.minimum[0]=b.minimum[1]=-.8f;
    b.maximum[0]=b.maximum[1]=.8f;b.minimum[2]=b.maximum[2]=.25f;b.qualified=qualified;return b;
}
struct VisibilityCohortCpu {std::vector<ALMetalWorldSimpleBatch> sources;};
ALMetalWorldViewMatrices visibilityMatrices(const ALMetalWorldSimpleBatch& source)
{
    ALMetalWorldViewMatrices result;
    std::memcpy(result.modelview_projection,source.modelview_projection,64);
    std::memcpy(result.modelview,source.modelview,64);
    std::memcpy(result.normal_matrix,source.normal_matrix,48);
    std::memcpy(result.rigged_normal_matrix,source.rigged_normal_matrix,48);
    return result;
}
ALMetalWorldSimpleBatch withVisibilityMatrices(ALMetalWorldSimpleBatch source,const ALMetalWorldViewMatrices& view)
{
    std::memcpy(source.modelview_projection,view.modelview_projection,64);
    std::memcpy(source.modelview,view.modelview,64);
    std::memcpy(source.normal_matrix,view.normal_matrix,48);
    std::memcpy(source.rigged_normal_matrix,view.rigged_normal_matrix,48);
    return source; // Fixed source UV is deliberately independent of the view.
}
struct RetainedVisibilityFixture : Fixture
{
    ALMetalWorldMaterialDataCache material_cache;
    std::shared_ptr<VisibilityCohortCpu> cpu=std::make_shared<VisibilityCohortCpu>();
    std::shared_ptr<Resources> native;
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
    std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
    explicit RetainedVisibilityFixture(unsigned family):material_cache(*device)
    {
        const std::array<std::uint8_t,16> pattern{255,30,20,255,20,255,30,255,30,20,255,255,220,180,30,255};
        auto diffuse=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pattern.data(),8);
        auto alternate=pattern;std::swap_ranges(alternate.begin(),alternate.begin()+8,alternate.begin()+8);
        auto indexed_diffuse=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,alternate.data(),8);
        cpu->sources.reserve(3);records.reserve(3);
        for(unsigned row=0;row<3;++row)
        {
            auto mesh=geometry;
            for(unsigned corner=0;corner<4;++corner)
            {
                mesh.positions[corner][0]+=row==1?4.f:0.f;
                mesh.positions[corner][3]=std::bit_cast<float>(family?1u:0u);
                mesh.uv[corner][0]=corner==1?.8f:.1f;mesh.uv[corner][1]=corner==2?.8f:.1f;
            }
            auto source=batch(family);source.vertices=buffer(&mesh,sizeof(mesh));
            source.texcoord1_offset=source.texcoord2_offset=offsetof(Geometry,uv);
            source.diffuse_textures[0]=diffuse;source.texture_matrix[12]=row*.125f;
            if(family)
            {
                source.texture_count=2;source.diffuse_textures[1]=indexed_diffuse;
                if(family==1)
                {source.material_slot_count=2;const auto slot=source.materialSlots()[0];source.materialSlots()[1]=slot;source.material_alpha_mode=2;}
                else
                {source.pbr_slot_count=2;const auto slot=source.pbrSlots()[0];source.pbrSlots()[1]=slot;source.pbrSlots()[1].roughness_factor=.7f;source.alpha_mask=true;}
                source.alpha_cutoff=.2f;
                REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,material_cache,&error));
                for(const auto& buffer:{source.native_material_source->materialLease(),source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                    if(buffer)resources->buffers.push_back(buffer);
            }
            cpu->sources.push_back(std::move(source));
            auto bounds=triangleBounds();bounds.minimum[0]+=row==1?4.f:0.f;bounds.maximum[0]+=row==1?4.f:0.f;
            auto record=ALMetalWorldCohortSource::seal({{100+row,family,1},&cpu->sources.back()},&error,row==2?nullptr:&bounds);
            INFO(error);REQUIRE(record);records.push_back(std::move(record));
        }
        native=std::make_shared<Resources>(*resources);
        cohort=ALMetalWorldSourceCohort::seal(records,cpu,native,7,&error);INFO(error);REQUIRE(cohort);
        CHECK(records[0]->hasBoundsPublication());CHECK_FALSE(records[2]->hasBoundsPublication());
    }
    ALMetalWorldSourceCohortView view() const
    {
        ALMetalWorldSourceCohortView value;value.cohort=cohort;value.cpu_owner=cpu;value.native_owner=native;
        value.view=visibilityMatrices(cpu->sources.front());return value;
    }
};
}

#include "almetal_world_published_cpu_visibility_test.inl"
#include "almetal_world_cpu_bound_intervals_test.inl"
