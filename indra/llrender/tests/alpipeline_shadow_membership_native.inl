/**
 * @brief Actual Scene::shadow factory frontier, ownership and preparation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include AFTER alpipeline_dependency_topology_native.inl in the producer
 * test TU. almetalshadowpublication.h must be included at GLOBAL scope first.
 */
namespace ShadowMembershipFrontier
{
static_assert(LL_DARWIN==1,"Frontier must execute the actual native dependency visitor");
namespace Scene=Inputs::ALScenePublicationNative;
struct Info:DependencyTopology::Owner,LLRefCount
{
    mutable U64 observation_calls=0;
    void observeMetalSourceDependencies()const{++observation_calls;observeShared();}
};
namespace QualifiedScene
{
using namespace Inputs::ALScenePublicationNative;
using LLDrawInfo=Info;
#include "alpipeline_shadow_factory_frontier_native.inl"
}
using Coverage=ALMetalWorldSpotShadowCoverage;
using Source=ALMetalShadowMembershipSource;
#include "../../newview/pipeline_shadow_membership_native.inl"
namespace Bridge=ALSceneShadowMembershipNative;
struct Lease
{
    // Same old source-owner operations as pipeline.cpp529–571. Required raw
    // owner frontier only; no extra synthetic work in the baseline factory.
    std::vector<std::shared_ptr<const void>> publications;
    std::vector<std::shared_ptr<const ALMetalShadowSourceRecord>> sources;
    std::unordered_set<const ALMetalShadowSourceRecord*> source_set;
    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> palettes;
    std::unordered_set<const ALMetalAttachmentPaletteRecord*> palette_set;
    std::unordered_set<const void*> resource_set;
    void retainResource(std::shared_ptr<const void> resource)
    {
        if(!resource)return;
        const auto [at,inserted]=resource_set.insert(resource.get());
        if(inserted){try{publications.push_back(std::move(resource));}catch(...){resource_set.erase(at);throw;}}
    }
    const ALMetalShadowSourceRecord* retain(const std::shared_ptr<const ALMetalShadowSourceRecord>& source)
    {if(source_set.insert(source.get()).second)sources.push_back(source);return source.get();}
    const ALMetalAttachmentPaletteRecord* retain(const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& pose)
    {if(palette_set.insert(pose.get()).second)palettes.push_back(pose);return pose.get();}
};
struct CPUOwners{std::vector<std::shared_ptr<const ALMetalShadowMembership>> memberships;};
struct Handoff
{
    std::shared_ptr<Lease> native;
    std::shared_ptr<CPUOwners> cpu;
    void reset(){cpu.reset();native.reset();}
};
struct Driver
{
    Publication& fixture;
    Coverage coverage;
    bool rigged;
    Inputs::LLVOAvatar avatar;
    std::vector<LLPointer<Info>> infos;
    // Both production versions already use strong LLPointer admission. The
    // new path adds only span stamping/positive translation to that operation.
    // Source maps and admitted arrays retain capacity across views.
    std::vector<Info*> raw_map;
    std::vector<LLPointer<Info>> admitted;
    std::vector<Bridge::AdmittedSpan> raw_spans,admitted_spans;
    std::array<std::shared_ptr<ALNativePublication::State>,3> groups;
    // Separate producer heuristic, never main compilation state/tryAcquire.
    std::array<ALNativeCohortCompilationAdmission,3> stability;
    ALNativeSourceWorkingSet residency{8192,64ULL*1024*1024,2};
    ALNativeFrameBudget budget;
    ALMetalShadowMembershipCache memberships;
    std::vector<ALMetalWorldSpotShadowBatch> output;
    LLMatrix4a model=LLMatrix4a::identity();LLMatrix4 uv;
    std::uint64_t frame=0,epoch=0,factories=0,builders=0,raw_rows=0,positive_rows=0,stamped_spans=0,preparation_deferrals=0;
    std::string error;
    Driver(Publication& f,U32 count,Coverage c,const LLPointer<LLVertexBuffer>& geometry,bool use_skin=false):fixture(f),coverage(c),rigged(use_skin)
    {
        for(auto& group:groups)group=std::make_shared<ALNativePublication::State>();
        avatar.palette.mMatrixPalette={0};avatar.palette.mGLMp.assign(16,0.f);avatar.palette.mGLMp[4]=avatar.palette.mGLMp[9]=avatar.palette.mGLMp[14]=1.f;
        for(U32 i=0;i<count;++i){LLPointer<Info> info=new Info;info->mVertexBuffer=geometry;info->mTexture.value=&f.image;info->mModelMatrix=&model;if(c!=Coverage::opaque)info->mTextureMatrix=&uv;if(rigged){info->mAvatar=&avatar;info->mSkinInfo=&avatar;}infos.push_back(info);}
        output.resize(count);
    }
    U64 observations()const{U64 total=0;for(const auto& info:infos)total+=info->observation_calls;return total;}
    void begin(bool dirty,bool denied,bool child_only=false)
    {
        ++frame;Inputs::gFrameCount=U32(frame);budget.beginFrame({.frame=frame});residency.trim(U32(frame));
        avatar.palette.mGLMp[0]=.017f*F32(frame%7);
        if(dirty)
        {
            if(!child_only)for(auto& group:groups)group->invalidate();
            for(auto& info:infos)info->state->invalidate();
            // Authoritative source/group mutation: reorder and change the
            // positive-count subset, not a camera-keyed cache fiction.
            if(!child_only&&infos.size()>1){std::rotate(infos.begin(),infos.begin()+1,infos.end());for(auto& info:infos)info->mCount=3;infos[frame%infos.size()]->mCount=0;}
        }
        if(denied)budget.beginDrain();
    }
    bool capture(bool cached,U32 view,bool cold,Handoff& lease)
    {
        const auto group_index=view%groups.size();const auto& group=groups[group_index];if(cold)group->invalidate();
        const U32 pass=coverage==Coverage::opaque?7:9;
        // Same authored group/pass admission and positive-count translation
        // operations as postSort/admitMetalShadowSources. No cull traversal,
        // scene queries, sorting, merging or GPU encoding is simulated here.
        raw_map.clear();
        if(cached)
        {
            raw_spans.clear();admitted_spans.clear();
            const auto snapshot=budget.snapshot();
            // Exact integrated guard order: global drain, raw size, separate
            // group stamp heuristic; positive-size qualification occurs below.
            if(!snapshot.draining&&infos.size()>=Bridge::minimum_members&&
                Bridge::candidateReady(stability[group_index],group,snapshot.frame,infos.size()))
                raw_spans.push_back(Bridge::admit(group,pass,Coverage::opaque,0,infos.size(),true));
        }
        for(const auto& info:infos)raw_map.push_back(info.get());
        raw_rows+=raw_map.size();
        admitted.clear();
        std::size_t raw_index=0,span_index=0,positive_first=0;const bool optional_spans=cached&&!raw_spans.empty();
        for(auto* info:raw_map)
        {
            if(optional_spans&&span_index<raw_spans.size()&&raw_spans[span_index].first==raw_index)positive_first=admitted.size();
            if(info&&info->mCount)admitted.emplace_back(info);
            ++raw_index;
            if(optional_spans&&span_index<raw_spans.size()&&raw_index==raw_spans[span_index].first+raw_spans[span_index].count)
            {
                auto span=raw_spans[span_index++];span.first=positive_first;span.count=admitted.size()-positive_first;
                if(span.count>=Bridge::minimum_members)admitted_spans.push_back(std::move(span));
            }
        }
        const std::size_t count=admitted.size();positive_rows+=count;stamped_spans+=admitted_spans.size();output.resize(count);
        const auto infoAt=[&](std::size_t i)->const Info&{return *admitted[i];};
        auto stamped=admitted_spans.empty()?Bridge::AdmittedSpan{}:admitted_spans.front();stamped.coverage=coverage;
        auto owners=std::make_shared<Lease>();
        std::shared_ptr<CPUOwners> cpu_owners;
        auto hit=cached&&!admitted_spans.empty()?Bridge::current(memberships,stamped,residency,U32(frame),budget.snapshot().frame):ALMetalShadowMembershipPublisher::Result{};
        preparation_deferrals+=hit.defer_preparation;
        const bool collect_members=cached&&!admitted_spans.empty()&&!hit.defer_preparation;
        ALMetalShadowPrepareContext context;context.view[12]=.013f*F32(view+1);context.projection[5]=.79f;
        model=LLMatrix4a::translation(.021f*F32(frame%7),.017f*F32(view),0.f);uv.setTranslation(LLVector3(.031f*F32(view),.013f*F32(frame%5),0.f));
        LLMatrix4a view_matrix(context.view.data()),projection(context.projection.data());
        const auto prepareSmall=[&](const ALMetalShadowSourceRecord& source,std::size_t i)
        {
            // EXACT existing small-family shape: scalar prototype/MVP/UV,
            // ordinary validation, no new ViewInput/scratch/job copies.
            const auto& info=infoAt(i);auto& batch=output[i];batch={};
            static_cast<ALMetalWorldSpotShadowValues&>(batch)=source.prototype();const ALMetalAttachmentPaletteRecord* palette=nullptr;
            if(source.requiresPalette()){const auto& current=info.mAvatar->updateSkinInfoMatrixPalette(info.mSkinInfo).getMetalAttachmentPalette();if(!current)return false;palette=owners->retain(current);batch.skin_palette=palette->data();batch.skin_palette_float_count=palette->size();batch.skin_joint_count=palette->jointCount();}
            batch.borrowPreparedInputs(nullptr,palette,source.prototype().coverageDataIdentity());
            LLMatrix4a mv=view_matrix,mvp;if(info.mModelMatrix)mv.setMul(*info.mModelMatrix,view_matrix);mvp.setMul(mv,projection);std::copy_n(mvp.getF32ptr(),16,batch.modelview_projection);
            if(info.mTextureMatrix&&coverage!=Coverage::opaque&&source.prototype().texture_count<=1)std::memcpy(batch.texture_matrix,info.mTextureMatrix->mMatrix,sizeof(batch.texture_matrix));
            return true;
        };
        if(hit)
        {
            cpu_owners=std::make_shared<CPUOwners>();cpu_owners->memberships.push_back(hit.membership);owners->retainResource(hit.membership->nativeOwner());
            for(std::size_t i=0;i<count;++i)if(!prepareSmall(*hit.membership->members()[i].record,i))return false;
        }
        else
        {
            // The real integrated per-family pending vector is local. Charge
            // its cold reserve/destruction here rather than hiding it in a
            // persistent pre-reserved fixture scratch allocation.
            std::vector<Source> pending;if(collect_members)pending.reserve(stamped.count);
            for(std::size_t i=0;i<count;++i)
            {
                const auto& info=infoAt(i);++factories;
                auto published=QualifiedScene::shadow(info,pass,coverage,residency,Inputs::sNativeSceneSerial,U32(frame),[&](auto& prototype){++builders;prototype=coverage==Coverage::opaque?SourceRecipes::shadow(info):SourceRecipes::covered(info,fixture.white->getMetalTexture());if(rigged){const auto& pose=info.mAvatar->updateSkinInfoMatrixPalette(info.mSkinInfo).getMetalAttachmentPalette();if(!pose)return false;prototype.weight_offset=info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4);prototype.skin_palette_record=pose;prototype.skin_palette=pose->data();prototype.skin_palette_float_count=pose->size();prototype.skin_joint_count=pose->jointCount();}return true;});
                if(!published)return false;
                owners->publications.push_back(published.owner);const auto* source=owners->retain(*published.record);
                if(!prepareSmall(*source,i))return false;
                if(collect_members){const auto& producer=info.metalSourcePublication();pending.push_back({producer,producer->identity(),published.owner->producer_revision,*published.record});}
            }
            if(collect_members)
            {
                // Optional cache failure does not replay or discard the already
                // complete required old-path outputs.
                (void)Bridge::seal(memberships,stamped,pending,residency,budget,Inputs::sNativeSceneSerial,U32(frame),frame);
                pending.clear(); // Scratch is not an unbudgeted native/State cache.
            }
        }
        lease.native=std::move(owners);lease.cpu=std::move(cpu_owners);return true;
    }
};
inline void equal(const Driver& a,const Driver& b)
{
    REQUIRE(a.output.size()==b.output.size());
    for(std::size_t i=0;i<a.output.size();++i)
    {const auto& x=a.output[i];const auto& y=b.output[i];REQUIRE(x.vertices);CHECK(x.vertices==y.vertices);CHECK(x.indices==y.indices);CHECK(x.index_count==y.index_count);CHECK(x.coverage==y.coverage);CHECK(std::memcmp(x.modelview_projection,y.modelview_projection,64)==0);CHECK(std::memcmp(x.texture_matrix,y.texture_matrix,64)==0);if(x.texture_count)CHECK(x.coverageData().alpha_cutoffs[0]==y.coverageData().alpha_cutoffs[0]);CHECK(x.skin_joint_count==y.skin_joint_count);CHECK(x.skin_palette_float_count==y.skin_palette_float_count);if(x.skin_palette_float_count){REQUIRE(x.skin_palette);REQUIRE(y.skin_palette);CHECK(std::memcmp(x.skin_palette,y.skin_palette,x.skin_palette_float_count*sizeof(float))==0);}}
}
}

TEST_CASE("Shadow producer membership skips actual warm source factories without caching view transforms", "[almetal][pipeline-buffer-publication][shadow-membership-frontier]")
{
    using namespace ShadowMembershipFrontier;SourceRecipes::Policy policy;Publication f;
    for(auto coverage:{Coverage::opaque,Coverage::texture_mask})for(bool rigged:{false,true})
    {
        const auto geometry=f.buffer(true,rigged?LLVertexBuffer::MAP_WEIGHT4:0);Driver old(f,8,coverage,geometry,rigged),now(f,8,coverage,geometry,rigged);Handoff old_lease,new_lease;
        for(unsigned iteration=0;iteration<5;++iteration)
        {
            old.begin(iteration==3,false);now.begin(iteration==3,false);
            for(unsigned view=0;view<7;++view){REQUIRE(old.capture(false,view,false,old_lease));REQUIRE(now.capture(true,view,false,new_lease));equal(old,now);old_lease.reset();new_lease.reset();}
            if(iteration==1||iteration==4){const auto factories=now.factories,observations=now.observations(),unions=now.memberships.stats().resource_unions;for(unsigned view=0;view<7;++view){REQUIRE(now.capture(true,view,false,new_lease));new_lease.reset();}CHECK(now.factories==factories);CHECK(now.observations()==observations);CHECK(now.memberships.stats().resource_unions==unions);}
        }
        CHECK(old.factories>now.factories);CHECK(old.observations()>now.observations());CHECK(old.raw_rows>old.positive_rows);CHECK(now.raw_rows>now.positive_rows);CHECK(now.stamped_spans>0);CHECK(old.stamped_spans==0);
    }
}

TEST_CASE("Shadow membership frontier preserves actual admission across small ranges and pressure", "[almetal][pipeline-buffer-publication][shadow-membership-frontier]")
{
    using namespace ShadowMembershipFrontier;
    SourceRecipes::Policy policy;Publication f;
    for(U32 rows:{1u,2u,4u,8u,16u,64u,256u})for(unsigned mode=0;mode<5;++mode)
    {
        const auto geometry=f.buffer(true);Driver old(f,rows,Coverage::opaque,geometry),now(f,rows,Coverage::opaque,geometry);Handoff a,b;
        for(unsigned iteration=0;iteration<3;++iteration)
        {
            const bool dirty=(mode==2||mode==4)&&iteration>0;old.begin(dirty,false,mode==4);now.begin(dirty,mode==3,mode==4);
            for(U32 view=0;view<7;++view)
            {
                REQUIRE(old.capture(false,view,mode==1,a));REQUIRE(now.capture(true,view,mode==1,b));equal(old,now);
                CHECK(old.admitted.size()==now.admitted.size());
                const bool eligible=rows>=Bridge::minimum_members&&iteration>0&&(mode==0||mode==4);
                CHECK(now.admitted_spans.size()==std::size_t(eligible));
                if(eligible){CHECK(now.admitted_spans[0].count==now.output.size());CHECK(now.admitted_spans[0].whole_group);}
                CHECK(a.cpu==nullptr);
                if(b.cpu){CHECK(b.cpu->memberships.size()==1);CHECK(b.native->resource_set.size()==1);CHECK(b.native->sources.empty());}
                a.reset();b.reset();
            }
        }
        if(mode==3||mode==1||mode==2||rows<Bridge::minimum_members)
        {CHECK(now.memberships.stats().resource_unions==0);CHECK(now.memberships.stats().hits==0);CHECK(old.factories==now.factories);CHECK(now.memberships.stats().entries==0);}
        if(mode==3||rows<Bridge::minimum_members)for(const auto& value:now.stability){CHECK_FALSE(value.pendingSince());CHECK_FALSE(value.lastConsideredFrame());}
        if(mode==0&&rows>=Bridge::minimum_members){CHECK(now.memberships.stats().resource_unions==3);CHECK(now.factories<old.factories);}
        if(mode==4&&rows>=Bridge::minimum_members)
        {
            CHECK(now.memberships.stats().resource_unions==3);CHECK(now.preparation_deferrals==7);
            CHECK(now.factories<old.factories);
            // Once real source writes stop, the same current records may seal
            // on a later outer frame and then reuse without caching pose/UV.
            old.begin(false,false);now.begin(false,false);
            for(U32 view=0;view<7;++view){REQUIRE(old.capture(false,view,false,a));REQUIRE(now.capture(true,view,false,b));equal(old,now);a.reset();b.reset();}
            CHECK(now.memberships.stats().resource_unions==6);
            const auto before=now.factories;
            old.begin(false,false);now.begin(false,false);
            for(U32 view=0;view<7;++view){REQUIRE(old.capture(false,view,false,a));REQUIRE(now.capture(true,view,false,b));equal(old,now);a.reset();b.reset();}
            CHECK(now.factories==before);CHECK(now.memberships.stats().resource_unions==6);
        }
    }
}

TEST_CASE("Shadow source factory frontier retains cold changing cull and denied small-range costs", "[.][almetal][shadow-membership-frontier-cost]")
{
    using namespace ShadowMembershipFrontier;
    struct Cell{U32 rows;Coverage coverage;bool rigged=false;};
    constexpr std::array cells{Cell{1,Coverage::opaque},Cell{2,Coverage::opaque},Cell{4,Coverage::opaque},Cell{8,Coverage::opaque},Cell{16,Coverage::opaque},Cell{64,Coverage::opaque},Cell{256,Coverage::opaque},Cell{1,Coverage::texture_mask},Cell{64,Coverage::texture_mask,true}};
    std::cout.precision(17);
    for(unsigned cell=0;cell<cells.size();++cell)for(unsigned mode=0;mode<6;++mode)
    {
        SourceRecipes::Policy policy;Publication f;const auto geometry=f.buffer(true,cells[cell].rigged?LLVertexBuffer::MAP_WEIGHT4:0);Driver old(f,cells[cell].rows,cells[cell].coverage,geometry,cells[cell].rigged),now(f,cells[cell].rows,cells[cell].coverage,geometry,cells[cell].rigged);
        Handoff owner;
        // Correctness precheck on the exact timed topology, including fresh
        // owner matrices/UVs and three group identities in seven-view order.
        for(unsigned iteration=0;iteration<3;++iteration)
        {
            const bool dirty=(mode==2||mode==4)&&iteration>0;old.begin(dirty,false,mode==4);now.begin(dirty,mode==3,mode==4);
            for(U32 view=0;view<7;++view){REQUIRE(old.capture(false,view,mode==1,owner));owner.reset();REQUIRE(now.capture(true,view,mode==1,owner));owner.reset();equal(old,now);}
        }
        constexpr unsigned warm=8,blocks=20;const unsigned iterations=mode==5?8:10;
        for(unsigned block=0;block<warm+blocks;++block)
        {
            const std::array<unsigned,4> order=block%2?std::array<unsigned,4>{1,0,0,1}:std::array<unsigned,4>{0,1,1,0};
            for(unsigned position=0;position<order.size();++position)
            {
                // Fresh producer window: first cold frame plus seven later
                // frames, with independent source/cache ownership per arm.
                // Construction is setup; final teardown is measured separately.
                auto fresh=mode==5?std::make_unique<Driver>(f,cells[cell].rows,cells[cell].coverage,geometry,cells[cell].rigged):nullptr;
                auto& driver=fresh?*fresh:(order[position]?now:old);const auto factories=driver.factories,observations=driver.observations(),unions=driver.memberships.stats().resource_unions,raw=driver.raw_rows,positive=driver.positive_rows,spans=driver.stamped_spans,deferrals=driver.preparation_deferrals;
                std::int64_t nanos=0,retirement=0;bool complete=true;
                for(unsigned iteration=0;iteration<iterations;++iteration)
                {
                    auto start=std::chrono::steady_clock::now();driver.begin(mode==2||mode==4,mode==3,mode==4);auto stop=std::chrono::steady_clock::now();nanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();
                    for(U32 view=0;view<7;++view){start=std::chrono::steady_clock::now();complete=driver.capture(order[position]!=0,view,mode==1,owner)&&complete;stop=std::chrono::steady_clock::now();nanos+=std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();start=std::chrono::steady_clock::now();owner.reset();stop=std::chrono::steady_clock::now();retirement+=std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();}
                }
                REQUIRE(complete);REQUIRE(!driver.output.empty());REQUIRE(driver.output[0].vertices);
                const auto factory_delta=driver.factories-factories,observation_delta=driver.observations()-observations,union_delta=driver.memberships.stats().resource_unions-unions,raw_delta=driver.raw_rows-raw,positive_delta=driver.positive_rows-positive,span_delta=driver.stamped_spans-spans,deferral_delta=driver.preparation_deferrals-deferrals,debt=driver.memberships.stats().metadata_bytes;
                const auto teardown_start=std::chrono::steady_clock::now();fresh.reset();const auto teardown=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-teardown_start).count();
                if(block>=warm)std::cout<<"shadow_frontier_cost cell="<<cell<<" rows="<<cells[cell].rows<<" coverage="<<U32(cells[cell].coverage)<<" mode="<<mode<<" block="<<block-warm<<" position="<<position<<" arm="<<order[position]<<" seven_view_iterations="<<iterations<<" handoff_ns="<<nanos<<" owner_retirement_ns="<<retirement<<" producer_teardown_ns="<<teardown<<" factories="<<factory_delta<<" observation_calls="<<observation_delta<<" successful_unions="<<union_delta<<" raw_rows="<<raw_delta<<" positive_rows="<<positive_delta<<" stamped_spans="<<span_delta<<" preparation_deferrals="<<deferral_delta<<" cpu_debt="<<debt<<std::endl;
            }
        }
    }
}
