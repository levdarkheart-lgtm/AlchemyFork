/**
 * @file almetal_cohort_declaration_test.mm
 * @brief Actual cohort declaration certificates and encoder-lifetime proofs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_source_pages_fixture.h"
#include "../almetalworldcohort.h"
#include <unordered_set>

namespace
{
struct DeclarationFixture : PageFixture
{
    struct Descriptions
    {
        std::vector<ALMetalWorldSimpleBatch> sources;
        std::vector<ALMetalWorldKeyedSource> keys;
    };
    std::shared_ptr<Descriptions> descriptions=std::make_shared<Descriptions>();
    std::array<std::shared_ptr<const ALMetalWorldSourceCohort>,2> cohorts;
    std::array<ALMetalWorldSourceCohortView,2> selections;
    std::array<ALMetalWorldConservativeBounds,8> expected_bounds;

    static std::shared_ptr<const ALMetalTexture> resolve(const std::shared_ptr<const void>& owner,
        const ALMetalTexture* texture) noexcept
    {
        const auto& values=*std::static_pointer_cast<const Resources>(owner);
        const auto at=std::find_if(values.textures.begin(),values.textures.end(),
            [&](const auto& value){return value.get()==texture;});
        return at==values.textures.end()?nullptr:*at;
    }
    DeclarationFixture()
    {
        descriptions->sources.reserve(8);descriptions->keys.reserve(8);
        for(unsigned index=0;index<8;++index)
        {
            const std::uint8_t pixel[]{std::uint8_t(40+index*20),80,160,255};
            auto source=batch(1);source.texcoord1_offset=source.texcoord2_offset=offsetof(Geometry,uv);
            source.diffuse_textures[0]=texture(ALMetalPixelFormat::rgba8_unorm_srgb,1,pixel,4);
            source.native_material_source.reset();source.native_resource_source.reset();publish(source);
            for(const auto& buffer:{source.native_material_source->materialLease(),
                source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                if(buffer)resources->buffers.push_back(buffer);
            descriptions->sources.push_back(std::move(source));
            auto& bounds=expected_bounds[index];
            bounds.minimum[0]=-1.f-.1f*index;bounds.minimum[1]=-1.f;bounds.minimum[2]=.25f;
            bounds.maximum[0]=1.f+.1f*index;bounds.maximum[1]=1.f;bounds.maximum[2]=.25f;
            bounds.qualified=(index<4?index%2==0:index%2!=0);
        }
        for(unsigned index=0;index<8;++index)
            descriptions->keys.push_back({{index+1,1,1},&descriptions->sources[index]});
        for(unsigned group=0;group<2;++group)
        {
            std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;
            for(unsigned local=0;local<4;++local)
            {
                const auto index=group*4+local;
                auto proof=ALMetalWorldCohortSource::seal(descriptions->keys[index],&error,&expected_bounds[index]);
                INFO(error);REQUIRE(proof);proofs.push_back(std::move(proof));
            }
            cohorts[group]=ALMetalWorldSourceCohort::seal(proofs,descriptions,resources,7+group,&error,&resolve);
            INFO(error);REQUIRE(cohorts[group]);
            auto& selection=selections[group];selection.cohort=cohorts[group];selection.cpu_owner=descriptions;
            selection.native_owner=resources;selection.logical_begin=group*6;selection.base_before=group*2;
            identity(selection.view.modelview_projection);identity(selection.view.modelview);
            for(unsigned axis=0;axis<3;++axis)
                selection.view.normal_matrix[axis][axis]=selection.view.rigged_normal_matrix[axis][axis]=1;
        }
    }
    void releaseDescriptions()
    {selections={};cohorts={};descriptions.reset();cpu.reset();}
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeCohorts(
        const std::shared_ptr<ALMetalWorldSourcePages>& pages,ALMetalCommandBuffer& command,bool direct=false)
    {
        auto result=pages->freezeCohortsForCommand(command,selections,globals,&error,data.get(),direct);
        INFO(error);REQUIRE(result);return result;
    }
};

struct DeclarationEncoder
{
    std::unique_ptr<ALMetalRenderTarget> target;
    std::unique_ptr<ALMetalRenderEncoder> encoder;
    DeclarationEncoder(PageFixture& f,ALMetalCommandBuffer& command)
    {
        ALMetalRenderTargetDescriptor td;td.width=td.height=side;td.color_attachment_count=1;
        td.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
        target=ALMetalRenderTarget::create(*f.device,td,&f.error);REQUIRE(target);
        auto pass=target->makeRenderPass({},&f.error);REQUIRE(pass);
        encoder=ALMetalRenderEncoder::begin(command,*pass,"Exact declaration lifetime",&f.error);REQUIRE(encoder);
    }
};
}

TEST_CASE("almetal: direct cohort membership carries exact flattened qualified and unknown bounds",
          "[almetal][cohort-declaration][world-source-pages]")
{
    DeclarationFixture f;auto pages=f.pages();
    auto command=f.device->makeCommandBuffer("Cold direct bounds certificate",&f.error);REQUIRE(command);
    auto first=f.freezeCohorts(pages,*command,true);
    const auto membership=first->commandMembership();REQUIRE(membership);
    REQUIRE(membership->directBindings().size()==f.expected_bounds.size());
    for(std::size_t index=0;index<f.expected_bounds.size();++index)
    {
        const auto& binding=membership->directBindings()[index];const auto& expected=f.expected_bounds[index];
        CHECK(std::equal(binding.bounds_minimum.begin(),binding.bounds_minimum.end(),expected.minimum));
        CHECK(std::equal(binding.bounds_maximum.begin(),binding.bounds_maximum.end(),expected.maximum));
        CHECK(binding.bounds_qualified==expected.qualified);
    }
    const auto published=pages->stats();
    first.reset();command.reset();
    auto next=f.device->makeCommandBuffer("Warm direct bounds certificate",&f.error);REQUIRE(next);
    auto reused=f.freezeCohorts(pages,*next,true);
    CHECK(reused->commandMembership()==membership);
    CHECK(pages->stats().row_publications==published.row_publications);
    CHECK(pages->stats().cohort_hits==published.cohort_hits+1);
    CHECK(pages->stats().cached_cohort_metadata_bytes==published.cached_cohort_metadata_bytes);
}

TEST_CASE("almetal: cohort table closure declares once per real encoder and retains ordinary table semantics",
          "[almetal][cohort-declaration][world-source-pages]")
{
    DeclarationFixture f;auto pages=f.pages();
    auto command=f.device->makeCommandBuffer("Cohort declaration exact command",&f.error);REQUIRE(command);
    auto view=f.freezeCohorts(pages,*command);
    std::unordered_set<const ALMetalWorldResourceTable*> tables;
    for(unsigned index=0;index<8;++index)
    {auto table=view->tableGenerationForTesting(index).lock();REQUIRE(table);tables.insert(table.get());}
    CHECK(tables.size()==8);
    auto first=f.encode(*view,f.descriptions->sources[0],std::move(command));
    const auto once=pages->stats();CHECK(once.cohort_declaration_calls==1);
    CHECK(once.cohort_declared_resources>tables.size());
    // Every table repeats the same normal/specular images. One immutable
    // closure must omit those duplicates as well as duplicate diffuse roles.
    CHECK(once.cohort_declared_resources<tables.size()*4);
    auto second=f.encode(*view,f.descriptions->sources[0],std::move(first.command));
    CHECK(pages->stats().cohort_declaration_calls==once.cohort_declaration_calls+1);
    CHECK(pages->stats().cohort_declared_resources==2*once.cohort_declared_resources);
    auto foreign=f.device->makeCommandBuffer("Different command cannot inherit cohort declaration",&f.error);REQUIRE(foreign);
    {
        DeclarationEncoder wrong(f,*foreign);
        CHECK_FALSE(view->bind(*wrong.encoder,*foreign,&f.error));
        CHECK(pages->stats().cohort_declaration_calls==2);
        REQUIRE(wrong.encoder->end(&f.error));
        CHECK_FALSE(view->bind(*wrong.encoder,*foreign,&f.error));
    }
    REQUIRE(second.command->commitAndWait(&f.error));
    CHECK(f.read(*first.target)==f.read(*second.target));
    // This tests two actual encoder lifetimes. It does not claim that Metal
    // reused an Objective-C object's address during this execution.
}

TEST_CASE("almetal: ordinary resource tables redeclare after encoder end without cohort marker sharing",
          "[almetal][cohort-declaration][world-resources]")
{
    PageFixture f;auto tables=ALMetalWorldResources::create(*f.device,*f.library,{},&f.error);REQUIRE(tables);
    ALMetalSamplerDescriptor sd;auto sampler=ALMetalSamplerState::create(*f.device,sd,&f.error);REQUIRE(sampler);
    auto table=tables->prepare(f.cpu->source.native_resource_source,*sampler,&f.error);REQUIRE(table);
    auto command=f.device->makeCommandBuffer("Ordinary table encoder lifetime",&f.error);REQUIRE(command);
    U64 once=0;
    {
        DeclarationEncoder first(f,*command);
        REQUIRE(tables->bind(*first.encoder,*command,f.cpu->source.native_resource_source,table,&f.error));
        once=tables->stats().declared_resources;REQUIRE(once>0);
        REQUIRE(tables->bind(*first.encoder,*command,f.cpu->source.native_resource_source,table,&f.error));
        CHECK(tables->stats().declared_resources==once);
        REQUIRE(first.encoder->end(&f.error));
        CHECK_FALSE(tables->declareResources(*first.encoder,*command,table,&f.error));
    }
    tables->clearLookup(); // Lookup withdrawal does not invalidate an exact older reader table.
    {
        DeclarationEncoder second(f,*command);
        REQUIRE(tables->declareResources(*second.encoder,*command,table,&f.error));
        CHECK(tables->stats().declared_resources==2*once);
        REQUIRE(tables->declareResources(*second.encoder,*command,table,&f.error));
        CHECK(tables->stats().declared_resources==2*once);
        REQUIRE(second.encoder->end(&f.error));
    }
    REQUIRE(command->commitAndWait(&f.error));
    auto next=f.device->makeCommandBuffer("Ordinary table next command",&f.error);REQUIRE(next);
    DeclarationEncoder third(f,*next);
    REQUIRE(tables->declareResources(*third.encoder,*next,table,&f.error));
    CHECK(tables->stats().declared_resources==3*once);
    REQUIRE(third.encoder->end(&f.error));
}

TEST_CASE("almetal: native cohort declaration owners survive pending work and release abandoned readers",
          "[almetal][cohort-declaration][world-source-pages]")
{
    for(bool commit:{false,true})
    {
        CAPTURE(commit);DeclarationFixture f;auto pages=f.pages();
        auto command=f.device->makeCommandBuffer("Cohort closure retirement",&f.error);REQUIRE(command);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct Open {id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{gate};
        if(commit)[(__bridge id<MTLCommandBuffer>)command->nativeHandle() encodeWaitForEvent:gate value:1];
        auto view=f.freezeCohorts(pages,*command);
        auto table=view->tableGenerationForTesting().lock();REQUIRE(table);
        std::weak_ptr<const ALMetalWorldResourceTable> weak_table=table;table.reset();
        std::weak_ptr<Resources> weak_native=f.resources;
        std::weak_ptr<DeclarationFixture::Descriptions> weak_cpu=f.descriptions;
        std::weak_ptr<const ALMetalWorldSourceFrameView> weak_view=view;
        auto encoded=f.encode(*view,f.descriptions->sources[0],std::move(command));
        const auto committed=ALMetalDevice::committedCommandBufferCount();
        if(commit)REQUIRE(encoded.command->commit(&f.error));
        pages->clear();f.releaseDescriptions();f.resources.reset();view.reset();
        CHECK(weak_cpu.expired());CHECK_FALSE(weak_table.expired());CHECK_FALSE(weak_native.expired());
        CHECK_FALSE(weak_view.expired());
        if(commit)
        {
            CHECK_FALSE(encoded.command->completionToken()->isComplete());
            gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        }
        else
        {
            encoded.command.reset();
            CHECK(ALMetalDevice::committedCommandBufferCount()==committed);
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while((!weak_table.expired()||!weak_native.expired()||!weak_view.expired())&&
              std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(weak_view.expired());CHECK(weak_table.expired());CHECK(weak_native.expired());
    }
}
