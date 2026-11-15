/**
 * @file alpipeline_terrain_preflight_native.inl
 * @brief Actual classic-terrain ordered preflight with explicit owner inputs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the existing native capture fixtures. Terrain packing,
 * tessellation and the hero transaction itself are outside this frontier.
 */
#include <stdexcept>
#include <sstream>
#include <type_traits>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
namespace TerrainPreflightTest
{
#include "../../newview/pipeline_terrain_preflight_types_native.inl"
static_assert(std::is_trivially_copyable_v<TerrainPreflightSnapshot>);
U32 gFrameCount=17;
struct LLVOSurfacePatch {static constexpr U32 PATCH_CORNERS=4;};
struct Drawable {bool dead=false;bool isDead() const{return dead;}};
struct PatchObject {};
struct Patch {};
struct Surface {};
struct Region {};
struct Face
{
    U32 count=4,index=0,count_reads=0,index_reads=0;
    bool forbid_geometry=false;
    U32 getGeomCount(){++count_reads;if(forbid_geometry)throw std::logic_error("Unexpected terrain corner read");return count;}
    U32 getGeomIndex(){++index_reads;if(forbid_geometry)throw std::logic_error("Unexpected terrain range read");return index;}
};
struct Buffer
{
    LLPointer<LLVertexBuffer> actual;
    U32 prepare_calls=0,position_reads=0,vertex_reads=0;
    bool require_indices=false,forbid_prepare=false,forbid_later=false;
    bool prepareMetalBuffers(bool indices)
    {++prepare_calls;require_indices=indices;if(forbid_prepare)throw std::logic_error("Unexpected terrain prepare");return actual->prepareMetalBuffers(indices);}
    bool hasDataType(LLVertexBuffer::AttributeType type)
    {++position_reads;if(forbid_later)throw std::logic_error("Unexpected terrain position read");return actual->hasDataType(type);}
    U32 getNumVerts()
    {++vertex_reads;if(forbid_later)throw std::logic_error("Unexpected terrain vertex-range read");return actual->getNumVerts();}
    bool hasPublishedMetalVertexBuffer() const noexcept{return actual->hasPublishedMetalVertexBuffer();}
    bool hasNativeVertexWriteFailure() const noexcept{return actual->hasNativeVertexWriteFailure();}
    U64 getMetalVertexPublicationRevision() const noexcept{return actual->getMetalVertexPublicationRevision();}
};
struct Capture : Inputs::Capture
{
    TerrainPreflightSnapshot first_terrain_preflight_failure,recent_terrain_preflight_failure;
    U64 terrain_preflight_failures=0;
    U32 incomplete_owner_line=0;
    void markIncomplete(U32 line){incomplete_owner_line=line;Inputs::Capture::markIncomplete(line);}
    bool hero_probe_face_capture=false,reflection_probe_face_capture=false;
    U32 hero_probe_face=0,reflection_probe_face=0;
};
struct OwnerInputs
{
    Capture capture;
    Face face_value;Face* face=&face_value;
    Drawable drawable_value;Drawable* drawable=&drawable_value;
    PatchObject object_value;PatchObject* object=&object_value;
    Patch patch_value;Patch* patch=&patch_value;
    Surface surface_value;Surface* surface=&surface_value;
    Region region_value;Region* region=&region_value;
    Buffer buffer_value;Buffer* buffer=&buffer_value;
    Inputs::LLViewerTexture texture;
    std::array<Inputs::LLViewerTexture*,4> details{&texture,&texture,&texture,&texture};
    Inputs::LLViewerTexture* const* detail_textures=details.data();
    Inputs::LLViewerTexture* alpha_ramp=&texture;
    std::array<F32,2> phases{.125f,.25f};const F32* detail_phase=phases.data();
    F32 detail_scale=1.f,tess_density=8.f,triplanar_blend=.5f;
    bool tessellation_pinned=false,triplanar=true;
    U32 ready_fallthroughs=0;
    U32 preflight_owner_line=0;
    explicit OwnerInputs(LLVertexBuffer* storage)
    {buffer_value.actual=storage;capture.batches.emplace_back().index_count=91;capture.draw_order.push_back(0);}
    bool isMainWorldPass() const{return true;}
    void evaluate()
    {
        // The identical function-local production include owns all predicates,
        // snapshot updates, incomplete marking and actual clearDraws().
        const U32 terrain_preflight_source_line = __LINE__;
        preflight_owner_line=terrain_preflight_source_line;
#include "../../newview/pipeline_classic_terrain_preflight_native.inl"
        ++ready_fallthroughs;
    }
};
LLPointer<LLVertexBuffer> terrainBuffer(bool require_published=true)
{
    LLPointer<LLVertexBuffer> result=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
    REQUIRE(result->allocateBuffer(4,0));
    std::array<LLVector4a,4> positions;
    positions[0].set(0,0,0,0);positions[1].set(1,0,0,0);
    positions[2].set(1,1,0,0);positions[3].set(0,1,0,0);
    result->setPositionData(positions.data());result->unmapBuffer();
    if(require_published)REQUIRE(result->hasPublishedMetalVertexBuffer());
    return result;
}
struct GuardedPhases
{
    void* mapping=MAP_FAILED;std::size_t page=0;
    GuardedPhases()
    {
        const long size=sysconf(_SC_PAGESIZE);REQUIRE(size>0);page=static_cast<std::size_t>(size);
        mapping=mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);REQUIRE(mapping!=MAP_FAILED);
        REQUIRE(mprotect(static_cast<char*>(mapping)+page,page,PROT_NONE)==0);
    }
    ~GuardedPhases(){if(mapping!=MAP_FAILED)munmap(mapping,page*2);}
    F32* lastFloat() const{return reinterpret_cast<F32*>(static_cast<char*>(mapping)+page-sizeof(F32));}
    const F32* unreadable() const{return reinterpret_cast<const F32*>(static_cast<char*>(mapping)+page);}
};
}
}

TEST_CASE("Actual classic terrain preflight guards pointers before native preparation or phase reads",
          "[almetal][pipeline-buffer-publication][terrain-preflight]")
{
    using namespace TerrainPreflightTest;
    Publication f;const auto storage=terrainBuffer();GuardedPhases inaccessible;
    for(bool missing_face:{false,true})
    {
        OwnerInputs inputs(storage.get());
        if(missing_face)inputs.face=nullptr;else inputs.detail_textures=nullptr;
        inputs.detail_phase=inaccessible.unreadable();inputs.buffer_value.forbid_prepare=true;
        inputs.face_value.forbid_geometry=true;const auto revision=storage->getMetalVertexPublicationRevision();
        REQUIRE_NOTHROW(inputs.evaluate());
        CHECK_FALSE(inputs.capture.complete);CHECK(inputs.capture.batches.empty());CHECK(inputs.capture.draw_order.empty());
        CHECK(inputs.ready_fallthroughs==0);CHECK(inputs.buffer_value.prepare_calls==0);
        CHECK(inputs.face_value.count_reads==0);CHECK(inputs.face_value.index_reads==0);
        CHECK(storage->getMetalVertexPublicationRevision()==revision);
        CHECK(inputs.capture.first_terrain_preflight_failure.reason==(missing_face?TerrainPreflightReason::missing_face:TerrainPreflightReason::missing_details));
        CHECK(inputs.capture.first_terrain_preflight_failure.observed==0);CHECK(inputs.capture.terrain_preflight_failures==1);
        CHECK(inputs.capture.incomplete_owner_line==inputs.preflight_owner_line);
    }
}

TEST_CASE("Actual classic terrain preflight records failed native publication without evaluating later guards",
          "[almetal][pipeline-buffer-publication][terrain-preflight]")
{
    using namespace TerrainPreflightTest;
    Publication f;LLPointer<LLVertexBuffer> storage;GuardedPhases inaccessible;
    {
        MissingPublisher missing(f.device.get());storage=terrainBuffer(false);REQUIRE_FALSE(storage->hasPublishedMetalVertexBuffer());
        OwnerInputs inputs(storage.get());inputs.detail_phase=inaccessible.unreadable();inputs.buffer_value.forbid_later=true;
        inputs.face_value.forbid_geometry=true;
        REQUIRE_NOTHROW(inputs.evaluate());
        CHECK(inputs.buffer_value.prepare_calls==1);CHECK_FALSE(inputs.buffer_value.require_indices);
        CHECK(inputs.buffer_value.position_reads==0);CHECK(inputs.buffer_value.vertex_reads==0);
        CHECK(inputs.face_value.count_reads==0);CHECK(inputs.face_value.index_reads==0);
        CHECK(inputs.capture.first_terrain_preflight_failure.reason==TerrainPreflightReason::native_vertex_not_current);
        CHECK(inputs.capture.first_terrain_preflight_failure.observed==1);
        CHECK_FALSE(inputs.capture.first_terrain_preflight_failure.prepared);CHECK_FALSE(inputs.capture.first_terrain_preflight_failure.vertex_current);
        CHECK_FALSE(inputs.capture.complete);CHECK(inputs.capture.batches.empty());CHECK(inputs.ready_fallthroughs==0);
    }
    // Required publication can recover once its real device publisher returns.
    OwnerInputs retry(storage.get());REQUIRE_NOTHROW(retry.evaluate());
    CHECK(retry.ready_fallthroughs==1);CHECK(retry.capture.complete);CHECK(retry.capture.terrain_preflight_failures==0);
}

TEST_CASE("Actual classic terrain phase-U failure never reads protected V and copies first and recent snapshots",
          "[almetal][pipeline-buffer-publication][terrain-preflight]")
{
    using namespace TerrainPreflightTest;
    Publication f;TerrainPreflightSnapshot first,recent;
    {
        const auto storage=terrainBuffer();OwnerInputs inputs(storage.get());GuardedPhases phases;
        *phases.lastFloat()=std::numeric_limits<F32>::quiet_NaN();inputs.detail_phase=phases.lastFloat();
        inputs.capture.hero_probe_face_capture=true;inputs.capture.hero_probe_face=3;
        REQUIRE_NOTHROW(inputs.evaluate());
        CHECK(inputs.capture.first_terrain_preflight_failure.reason==TerrainPreflightReason::phase_u);
        CHECK(inputs.capture.first_terrain_preflight_failure.observed==31);
        CHECK(inputs.buffer_value.prepare_calls==1);CHECK(inputs.buffer_value.position_reads==1);CHECK(inputs.buffer_value.vertex_reads==1);
        CHECK(inputs.face_value.count_reads==1);CHECK(inputs.face_value.index_reads==1);
        CHECK_FALSE(inputs.capture.complete);CHECK(inputs.capture.batches.empty());
        // Re-invoke the exact evaluator with an existing first diagnostic.
        // This checks its state policy, not a full viewer retry or hero begin.
        inputs.capture.complete=true;inputs.detail_phase=inputs.phases.data();inputs.triplanar_blend=0.f;
        REQUIRE_NOTHROW(inputs.evaluate());
        CHECK(inputs.capture.terrain_preflight_failures==2);
        CHECK(inputs.capture.first_terrain_preflight_failure.reason==TerrainPreflightReason::phase_u);
        CHECK(inputs.capture.recent_terrain_preflight_failure.reason==TerrainPreflightReason::triplanar_blend);
        *phases.lastFloat()=42.f;first=inputs.capture.first_terrain_preflight_failure;recent=inputs.capture.recent_terrain_preflight_failure;
    }
    // All phase memory and caller frontier objects are now out of scope.
    CHECK(std::isnan(first.phase_u));CHECK(first.frame==17);CHECK(first.view==2);CHECK(first.face==3);
    CHECK(recent.phase_u==.125f);CHECK(recent.phase_v==.25f);CHECK(recent.blend==0.f);
    std::ostringstream report;report<<first;CHECK(report.str().find("first_predicate=phase_u")!=std::string::npos);
}

TEST_CASE("Actual ready classic terrain preflight preserves current native capture and the accepted prefix",
          "[almetal][pipeline-buffer-publication][terrain-preflight]")
{
    using namespace TerrainPreflightTest;
    Publication f;const auto storage=terrainBuffer();OwnerInputs inputs(storage.get());
    const auto revision=storage->getMetalVertexPublicationRevision();
    REQUIRE_NOTHROW(inputs.evaluate());
    CHECK(inputs.ready_fallthroughs==1);CHECK(inputs.capture.complete);REQUIRE(inputs.capture.batches.size()==1);
    CHECK(inputs.capture.batches[0].index_count==91);CHECK(inputs.capture.draw_order==std::vector<ALMetalWorldDrawIndex>{0});
    CHECK(inputs.capture.terrain_preflight_failures==0);
    CHECK(inputs.capture.first_terrain_preflight_failure.reason==TerrainPreflightReason::not_evaluated);
    CHECK(inputs.buffer_value.prepare_calls==1);CHECK_FALSE(inputs.buffer_value.require_indices);
    CHECK(inputs.face_value.count_reads==1);CHECK(inputs.face_value.index_reads==1);
    CHECK(storage->getMetalVertexPublicationRevision()==revision);
}
