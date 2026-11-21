/** Owned scalar world input parity and publication tests. */
#include "almetal_world_inputs_fixture.h"
#include <time.h>

TEST_CASE("almetal: owned scalar world consumes every G-buffer family with exact direct pixels", "[almetal][world-inputs]")
{
    @autoreleasepool
    {
        Fixture f;
        for(unsigned family=0;family<10;++family)
        {
            CAPTURE(family);
            auto s=f.scene();auto b=f.batch(family);s.batches=&b;s.batch_count=1;
            if(family<7)s.hdr=true;
            auto direct=f.renderer->render(s,&f.error);INFO(f.error);REQUIRE(direct);const auto expected=f.read(*direct);
            auto ready=f.adopt(s,{b});
            auto owned_input=f.renderer->renderOwnedInputs(*ready,&f.error);INFO(f.error);REQUIRE(owned_input);
            const auto actual=f.read(*owned_input);CHECK(actual==expected);
            CHECK(std::any_of(actual.depth.begin(),actual.depth.end(),[](auto x){return x!=0x3f800000u;}));
        }
    }
}

TEST_CASE("almetal: small mixed owned views preserve query and special-state boundaries", "[almetal][world-inputs]")
{
    @autoreleasepool
    {
        Fixture f;
        for(unsigned mode=0;mode<4;++mode)
        {
            CAPTURE(mode);
            auto s=f.scene();s.reverse_z=mode&1;s.wireframe=mode==2;s.stamp_opaque_coverage=mode==3;
            s.join_adjacent_static_draws=true;
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned family=0;family<10;++family)batches.push_back(f.batch(family));
            batches.back().impostor_debug_outline=mode==2;batches.back().impostor_debug_line_width=mode==2?2.f:0.f;
            std::vector<ALMetalWorldVisibilityQuery> queries(3);
            for(unsigned q=0;q<queries.size();++q)
            {
                identity(queries[q].modelview_projection);queries[q].center[2]=.75f;
                queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.05f;
                queries[q].batch_offset=q==0?0:q==1?5:batches.size();queries[q].fan=7;queries[q].count_samples=1;
            }
            s.batches=batches.data();s.batch_count=batches.size();s.visibility_queries=queries.data();s.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,owned_queries;
            auto direct=f.renderer->render(s,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);std::array<std::uint64_t,3> results{};
            for(unsigned q=0;q<3;++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
            auto ready=f.adopt(s,batches,queries);
            auto owned_input=f.renderer->renderOwnedInputs(*ready,&f.error,&owned_queries);INFO(f.error);REQUIRE(owned_input);REQUIRE(owned_queries);
            CHECK(f.read(*owned_input)==expected);
            for(unsigned q=0;q<3;++q){std::uint64_t value=0;REQUIRE(owned_queries->wait(q,value,&f.error));CHECK(value==results[q]);}
            CHECK(results.front()>0);
        }
    }
}

TEST_CASE("almetal: repeated scalar common ranges restore after terrain and query boundaries", "[almetal][world-inputs]")
{
    @autoreleasepool
    {
        Fixture f;
        // Distinct triangles live in one region/agent-space buffer. Repeated
        // draws genuinely share matrices; per-object transforms do not hide
        // the common-constant reuse that the live viewer frequently exhibits.
        std::array<Fixture::Geometry,11> geometry;
        for(unsigned tile=0;tile<geometry.size();++tile)
        {
            geometry[tile]=f.geometry;
            const float x=(static_cast<float>(tile%4)-1.5f)*.45f;
            const float y=(static_cast<int>(tile/4)-1)*.6f;
            for(unsigned vertex=0;vertex<4;++vertex)
            {
                geometry[tile].positions[vertex][0]=f.geometry.positions[vertex][0]*.24f+x;
                geometry[tile].positions[vertex][1]=f.geometry.positions[vertex][1]*.24f+y;
                geometry[tile].colors[vertex][0]=static_cast<std::uint8_t>(55+tile*21);
                geometry[tile].colors[vertex][1]=static_cast<std::uint8_t>(225-tile*17);
                geometry[tile].colors[vertex][2]=static_cast<std::uint8_t>(75+tile*13);
            }
        }
        const auto* vertices=f.buffer(geometry.data(),sizeof(geometry));
        const auto tile_batch=[&](unsigned tile,unsigned family,bool vertex_color)
        {
            auto batch=f.batch(family);
            const auto base=tile*sizeof(Fixture::Geometry);
            batch.vertices=vertices;
            batch.position_offset+=base;batch.normal_offset+=base;
            batch.texcoord_offset+=base;batch.color_offset+=base;batch.tangent_offset+=base;
            batch.use_vertex_color=vertex_color;
            return batch;
        };
        for(bool reverse:{false,true})
        {
            CAPTURE(reverse);
            auto s=f.scene();s.reverse_z=reverse;
            auto terrain=f.batch(7),pbr_terrain=f.batch(8);
            for(auto* batch:{&terrain,&pbr_terrain})
            {
                batch->modelview_projection[0]=batch->modelview_projection[5]=.12f;
                batch->modelview_projection[12]=-.98f;
                batch->modelview_projection[13]=-.98f;
                batch->modelview_projection[14]=.75f;
            }
            pbr_terrain.modelview_projection[12]=.72f;
            auto impostor=f.batch(9);
            impostor.impostor_center[0]=impostor.impostor_center[1]=.95f;
            impostor.impostor_center[2]=.75f;
            impostor.impostor_left[0]=impostor.impostor_up[1]=.03f;
            // A,A,terrain,A,impostor,A,A | A,B,B,terrain,B,A,A.
            // Specials retain the last
            // simple value; changed B and the final A require new ranges.
            std::vector<ALMetalWorldSimpleBatch> batches{
                tile_batch(0,0,true),tile_batch(1,1,true),terrain,
                tile_batch(2,2,true),impostor,tile_batch(3,3,true),tile_batch(4,0,true),
                tile_batch(5,1,true),tile_batch(6,0,false),tile_batch(7,2,false),pbr_terrain,
                tile_batch(8,1,false),tile_batch(9,0,true),tile_batch(10,2,true)};
            std::vector<ALMetalWorldVisibilityQuery> queries(4);
            const std::array<std::size_t,4> offsets{0,3,9,batches.size()};
            for(unsigned q=0;q<queries.size();++q)
            {
                identity(queries[q].modelview_projection);
                queries[q].center[0]=queries[q].center[1]=-.6f;
                queries[q].center[2]=.75f;
                queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.03f;
                queries[q].batch_offset=offsets[q];queries[q].fan=7;queries[q].count_samples=1;
            }
            s.batches=batches.data();s.batch_count=batches.size();
            s.visibility_queries=queries.data();s.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,owned_queries;
            auto direct=f.renderer->render(s,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);
            std::array<std::uint64_t,4> results{};
            for(unsigned q=0;q<queries.size();++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
            REQUIRE(results.front()>0);
            auto ready=f.adopt(s,batches,queries);

            const auto before=f.renderer->constantStats();
            auto owned_input=f.renderer->renderOwnedInputs(*ready,&f.error,&owned_queries);
            INFO(f.error);REQUIRE(owned_input);REQUIRE(owned_queries);
            CHECK(f.renderer->constantStats().reused_bindings>before.reused_bindings);
            CHECK(f.read(*owned_input)==expected);
            for(unsigned q=0;q<queries.size();++q)
            {
                std::uint64_t value=0;REQUIRE(owned_queries->wait(q,value,&f.error));CHECK(value==results[q]);
            }
            const auto clear=reverse?0u:0x3f800000u;
            CHECK(std::count_if(expected.depth.begin(),expected.depth.end(),[&](auto value){return value!=clear;})>30);
        }
    }
}

#include "almetal_terrain_pass_publication_test.inl"
