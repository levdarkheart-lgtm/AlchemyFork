/**
 * @file llpolymesh_test.cpp
 * @brief Real .llm loading, optional ordering, and preserved avatar attributes.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "llpolymesh.h"
#include "lldir.h"
#include <meshoptimizer.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <new>
#include <vector>

namespace
{
using Triangle=std::array<U16,3>;
constexpr unsigned SIDE=9, VERTICES=SIDE*SIDE;

class FixtureDir final : public LLDir
{
public:
    explicit FixtureDir(const std::string& path) { mAppRODataDir=path; mDirDelimiter="/"; }
    void initAppDirs(const std::string&,const std::string&) override {}
    std::string getCurPath() override { return mAppRODataDir; }
    std::string getLLPluginFilename(std::string) override { return {}; }
};
struct Fixture
{
    std::filesystem::path root;
    FixtureDir directory;
    LLDir* previous;
    Fixture() : root(std::filesystem::temp_directory_path()/ ("alchemy-polymesh-"+LLUUID::generateNewID().asString())),
        directory(root.string()),previous(gDirUtilp)
    {
        std::filesystem::create_directories(root/"character");
        gDirUtilp=&directory;
    }
    ~Fixture()
    {
        // Instances are local variables declared after the fixture and die
        // before the shared-name cache. No profile directory is initialized.
        LLPolyMesh::freeAllMeshes();
        gDirUtilp=previous;
        std::error_code error; std::filesystem::remove_all(root,error);
    }
    std::unique_ptr<LLPolyMesh> load(const std::string& name,bool keep=false,LLPolyMesh* reference=nullptr)
    { return std::unique_ptr<LLPolyMesh>(LLPolyMesh::getMesh(name,reference,keep)); }
};

std::vector<Triangle> authored()
{
    std::vector<Triangle> grid;
    for(unsigned y=0;y+1<SIDE;++y) for(unsigned x=0;x+1<SIDE;++x)
    {
        const U16 a=U16(y*SIDE+x),b=U16(a+1),c=U16(a+SIDE),d=U16(c+1);
        grid.push_back({a,b,c}); grid.push_back({b,d,c});
    }
    std::vector<Triangle> shuffled;
    for(unsigned i=0;i<grid.size();++i) shuffled.push_back(grid[(i*37)%grid.size()]);
    return shuffled;
}
using Bytes=std::vector<U8>;
void u16(Bytes& bytes,U16 value) { bytes.push_back(U8(value)); bytes.push_back(U8(value>>8)); }
void u32(Bytes& bytes,U32 value) { for(unsigned n=0;n<4;++n) bytes.push_back(U8(value>>(8*n))); }
void f32(Bytes& bytes,F32 value) { u32(bytes,std::bit_cast<U32>(value)); }
void text(Bytes& bytes,const char* value,unsigned width)
{
    const auto start=bytes.size(); bytes.resize(start+width,0);
    std::memcpy(bytes.data()+start,value,std::min<std::size_t>(std::strlen(value),width));
}
std::array<F32,3> position(unsigned vertex)
{ return {F32(vertex%SIDE),F32(vertex/SIDE),F32(vertex)*.125f}; }
std::array<F32,2> uv(unsigned vertex)
{ return {F32(vertex%SIDE)/8.f,F32(vertex/SIDE)/8.f}; }
F32 weight(unsigned vertex) { return .25f+F32(vertex)/160.f; }

// Valid binary format, including weights, skin names, End Morphs and remaps.
// A detail-UV fixture embeds a benign End Morphs sentinel where the broken
// reader would start its morph section. Every resulting UV float is finite;
// this keeps the old control a clean assertion failure, not a bad allocation.
std::vector<std::array<F32,2>> write(Fixture& fixture,const std::string& name,
    const std::vector<Triangle>& faces,bool lod=false,bool detail=false)
{
    Bytes bytes; text(bytes,"Linden Binary Mesh 1.0",24);
    bytes.push_back(detail?0:1); bytes.push_back(detail?1:0);
    for(float value : {0.f,0.f,0.f,0.f,0.f,0.f}) f32(bytes,value);
    bytes.push_back(0); for(float value : {1.f,1.f,1.f}) f32(bytes,value);
    std::vector<std::array<F32,2>> detail_uv;
    if(!lod)
    {
        u16(bytes,VERTICES);
        for(unsigned v=0;v<VERTICES;++v) for(float value:position(v)) f32(bytes,value);
        for(unsigned v=0;v<VERTICES;++v) for(float value:{0.f,0.f,1.f}) f32(bytes,value);
        for(unsigned v=0;v<VERTICES;++v) for(float value:{0.f,1.f,0.f}) f32(bytes,value);
        for(unsigned v=0;v<VERTICES;++v) for(float value:uv(v)) f32(bytes,value);
        if(detail)
        {
            Bytes data(VERTICES*2*sizeof(F32),0);
            const char sentinel[]="End Morphs";
            std::memcpy(data.data()+2,sentinel,sizeof(sentinel));
            detail_uv.resize(VERTICES);
            for(unsigned v=0;v<VERTICES;++v) for(unsigned c=0;c<2;++c)
            {
                U32 bits=0; for(unsigned b=0;b<4;++b) bits|=U32(data[(v*2+c)*4+b])<<(8*b);
                detail_uv[v][c]=std::bit_cast<F32>(bits);
            }
            bytes.insert(bytes.end(),data.begin(),data.end());
        }
        else for(unsigned v=0;v<VERTICES;++v) f32(bytes,weight(v));
    }
    u16(bytes,U16(faces.size()));
    for(const auto& face:faces) for(U16 vertex:face) u16(bytes,vertex);
    if(!lod)
    {
        if(!detail) { u16(bytes,2); text(bytes,"mPelvis",64); text(bytes,"mTorso",64); }
        text(bytes,"End Morphs",64); u32(bytes,0);
    }
    std::ofstream stream(fixture.root/"character"/name,std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    REQUIRE(stream.good());
    return detail_uv;
}
std::vector<Triangle> faces(LLPolyMesh& mesh)
{
    std::vector<Triangle> result;
    for(S32 i=0;i<mesh.getNumFaces();++i)
        result.push_back({U16(mesh.getFaces()[i][0]),U16(mesh.getFaces()[i][1]),U16(mesh.getFaces()[i][2])});
    return result;
}
void sameTriangles(std::vector<Triangle> actual,std::vector<Triangle> expected)
{
    std::sort(actual.begin(),actual.end()); std::sort(expected.begin(),expected.end());
    CHECK(actual==expected); // triplets never rotate or reverse winding
}
void attributes(LLPolyMesh& mesh,bool weights=true)
{
    REQUIRE(mesh.getNumVertices()==VERTICES);
    CHECK(mesh.hasWeights()==weights);
    if(weights && !mesh.getSharedData()->isLOD())
    {
        REQUIRE(mesh.getNumJointNames()==2);
        CHECK(mesh.getJointNames()[0]=="mPelvis");
        CHECK(mesh.getJointNames()[1]=="mTorso");
    }
    for(unsigned v=0;v<VERTICES;++v)
    {
        for(unsigned c=0;c<3;++c) CHECK(mesh.getCoords()[v].getF32ptr()[c]==position(v)[c]);
        for(unsigned c=0;c<3;++c)
        {
            CHECK(mesh.getNormals()[v].getF32ptr()[c]==(c==2?1.f:0.f));
            CHECK(mesh.getBaseBinormals()[v].getF32ptr()[c]==(c==1?1.f:0.f));
            CHECK(mesh.getBinormals()[v].getF32ptr()[c]==(c==1?1.f:0.f));
            CHECK(mesh.getScaledBinormals()[v].getF32ptr()[c]==(c==1?1.f:0.f));
        }
        CHECK(mesh.getTexCoords()[v].mV[0]==uv(v)[0]); CHECK(mesh.getTexCoords()[v].mV[1]==uv(v)[1]);
        if(weights) CHECK(mesh.getWeights()[v]==weight(v));
    }
}
unsigned attempted_allocations=0, failure_at=1;
void* (*original_allocate)(size_t)=nullptr;
void* fail_ordering(size_t bytes)
{
    if(++attempted_allocations==failure_at) throw std::bad_alloc();
    return original_allocate(bytes);
}
struct FailOrdering
{
    meshopt_Allocator::Storage previous=meshopt_Allocator::storage();
    explicit FailOrdering(unsigned nth=1)
    {
        attempted_allocations=0;
        failure_at=nth;
        original_allocate=previous.allocate;
        meshopt_setAllocator(fail_ordering,previous.deallocate);
    }
    ~FailOrdering() { meshopt_setAllocator(previous.allocate,previous.deallocate); }
};
}

TEST_CASE("LLPolyMesh loads ordered triangle triplets without changing vertex UV or skin data", "[llpolymesh]")
{
    Fixture fixture; const auto original=authored(); write(fixture,"opaque.llm",original);
    auto mesh=fixture.load("opaque.llm"); REQUIRE(mesh);
    const auto reordered=faces(*mesh); sameTriangles(reordered,original); CHECK(reordered!=original); attributes(*mesh);
    auto cached=fixture.load("opaque.llm"); REQUIRE(cached);
    CHECK(cached->getSharedData()==mesh->getSharedData()); CHECK(faces(*cached)==reordered);
    mesh->getSharedData()->genIndices(17);
    for(unsigned face=0;face<reordered.size();++face) for(unsigned c=0;c<3;++c)
        CHECK(mesh->getIndices()[face*3+c]==U32(reordered[face][c])+17);
}

TEST_CASE("LLPolyMesh blended hair skirt and eyelash requests keep authored triangle order exactly", "[llpolymesh]")
{
    Fixture fixture; const auto original=authored();
    for(const char* name: {"hair.llm","skirt.llm","eyelash.llm"})
    {
        write(fixture,name,original); FailOrdering fail;
        auto mesh=fixture.load(name,true); REQUIRE(mesh); CHECK(attempted_allocations==0);
        CHECK(faces(*mesh)==original); attributes(*mesh);
    }
}

TEST_CASE("LLPolyMesh optional ordering allocation failure retains the complete loaded mesh", "[llpolymesh][llpolymesh-allocation]")
{
    Fixture fixture; const auto original=authored();
    write(fixture,"allocation_control.llm",original);
    unsigned allocation_count=0;
    {
        FailOrdering count(std::numeric_limits<unsigned>::max());
        auto mesh=fixture.load("allocation_control.llm"); REQUIRE(mesh);
        CHECK(faces(*mesh)!=original);
        allocation_count=attempted_allocations;
    }
    REQUIRE(allocation_count>0);
    REQUIRE(allocation_count<32);
    CAPTURE(allocation_count);
    for(unsigned nth=1;nth<=allocation_count;++nth)
    {
        const std::string name="allocation_"+std::to_string(nth)+".llm";
        write(fixture,name,original);
        std::unique_ptr<LLPolyMesh> mesh;
        {
            FailOrdering fail(nth);
            REQUIRE_NOTHROW(mesh=fixture.load(name));
            CHECK(attempted_allocations==nth);
        }
        REQUIRE(mesh); CHECK(faces(*mesh)==original); attributes(*mesh);
    }
}

TEST_CASE("LLPolyMesh retains invalid-index order without calling the optional optimizer", "[llpolymesh]")
{
    Fixture fixture; auto original=authored(); original[0][2]=VERTICES;
    write(fixture,"out_of_range.llm",original); FailOrdering fail;
    auto mesh=fixture.load("out_of_range.llm"); REQUIRE(mesh); CHECK(attempted_allocations==0);
    CHECK(faces(*mesh)==original); attributes(*mesh);
}

TEST_CASE("LLPolyMesh LOD faces own their order while vertex and skin data remain referenced", "[llpolymesh]")
{
    Fixture fixture; const auto original=authored(); write(fixture,"base.llm",original);
    auto base=fixture.load("base.llm",true); REQUIRE(base);
    write(fixture,"lod.llm",original,true);
    auto lod=fixture.load("lod.llm",false,base.get()); REQUIRE(lod);
    sameTriangles(faces(*lod),original); CHECK(faces(*lod)!=original);
    CHECK(lod->getCoords()==base->getCoords()); CHECK(lod->getWeights()==base->getWeights());
    CHECK(lod->getBinormals()==base->getBinormals());
    CHECK(lod->getScaledBinormals()==base->getScaledBinormals());
    attributes(*lod);
}

TEST_CASE("LLPolyMesh consumes the valid detail UV stream and preserves it through LOD references", "[llpolymesh][llpolymesh-detail]")
{
    Fixture fixture; const auto original=authored(); const auto detail=write(fixture,"detail.llm",original,false,true);
    auto mesh=fixture.load("detail.llm",true); REQUIRE(mesh);
    REQUIRE(mesh->hasDetailTexCoords()); CHECK(faces(*mesh)==original); attributes(*mesh,false);
    for(unsigned v=0;v<VERTICES;++v) for(unsigned c=0;c<2;++c)
        CHECK(mesh->getDetailTexCoords()[v].mV[c]==detail[v][c]);
    write(fixture,"detail_lod.llm",original,true,false); // LOD file flag0 must keep reference flag1.
    auto lod=fixture.load("detail_lod.llm",true,mesh.get()); REQUIRE(lod);
    CHECK(lod->hasDetailTexCoords()); CHECK(lod->getDetailTexCoords()==mesh->getDetailTexCoords());
    CHECK(faces(*lod)==original);
}
