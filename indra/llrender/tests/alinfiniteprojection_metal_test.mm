/**
 * @file alinfiniteprojection_metal_test.mm
 * @brief Original infinite/forward projection invariants on actual offscreen Metal depth.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project.
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llgl.h"
#include "../llrender.h"
#include "llcamera.h"
#include "../../almetal/almetalpipeline.h"
#import <Metal/Metal.h>
#include "altestmatchers.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr S32 SIZE = 8;
constexpr F32 NEAR_PLANE = .1f, FOV = 1.f;
constexpr F32 DISTANCES[] = {1000.f, 8192.f, 32000.f, 100000.f};
        LLMatrix4a infinite()
        {
            return al_perspective(FOV, 1.f, NEAR_PLANE, std::numeric_limits<F32>::infinity());
        }

        // What deferredUtil.glsl's getPositionWithNDC does, in float: inv_proj * (0, 0, d, 1), w floored.
        F32 reconstruct(const LLMatrix4a& inv, F32 d)
        {
            LLVector4a pos;
            inv.transform4(LLVector4a(0.f, 0.f, d, 1.f), pos);
            return -pos[2] / llmax(pos[3], 1.f / MAX_RECONSTRUCT_DISTANCE);
        }

        // True metres between the depth stored for `dist` and the next representable (farther) one.
        F64 depthStep(F32 d)
        {
            const F32 next = std::nextafter(d, 0.f);
            return (F64)NEAR_PLANE / next - (F64)NEAR_PLANE / d;
        }

        F64 bound(F32 dist, F32 d)
        {
            return llmax(4.0 * depthStep(d), 4.0 * (F64)(std::nextafter(dist, 2.f * dist) - dist));
        }


// Original1eb44 al_perspective/deferredUtil/sky depth contracts. Only the API
// transport changes: a typed D32F owner, explicit compare/write state, real
// visibility counters and native readback replace GL context/FBO/queries.
struct NativeInfiniteProjection
{
    bool previous_reverse = LLRender::sReverseZ;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalRenderTarget> target;
    ALMetalViewport viewport{0,0,SIZE,SIZE,0,1};
    std::string error;
    NativeInfiniteProjection() { LLRender::sReverseZ = true; }
    ~NativeInfiniteProjection() { LLRender::sReverseZ = previous_reverse; }
    void setUp()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, R"(
#include <metal_stdlib>
using namespace metal;
struct Params { float4x4 projection; float4 geometry; uint4 flags; };
vertex float4 native_depth(uint i [[vertex_id]], constant Params& p [[buffer(0)]]) {
    float2 xy = float2((i << 1) & 2, i & 2) * 2.0 - 1.0;
    float4 clip = p.projection * float4(xy * p.geometry.yz * p.geometry.x, -p.geometry.x, 1.0);
    if (p.flags.x) clip.z = (clip.z + clip.w) * 0.5;
    if (p.geometry.w != 0.0) clip.z = p.flags.x ? clip.w : 0.0;
    return clip;
}
)", "Original projection depth transport", &error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;
        pd.library = library.get(); pd.vertex_function = "native_depth";
        pd.color_attachment_count = 0; pd.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        pipeline = ALMetalRenderPipeline::create(*device, pd, &error); INFO(error); REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor td; td.width = td.height = SIZE;
        td.color_attachment_count = 0; td.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        target = ALMetalRenderTarget::create(*device, td, &error); INFO(error); REQUIRE(target);
        clear();
    }
    void clear()
    {
        auto command = device->makeCommandBuffer("Native projection depth clear", &error); REQUIRE(command);
        ALMetalRenderPassConfig pc; pc.clear_depth = LLRender::sReverseZ ? 0.0 : 1.0;
        pc.depth_store = ALMetalStoreAction::store;
        auto pass = target->makeRenderPass(pc, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Depth clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
    }
    U64 draw(const LLMatrix4a& projection, F32 distance, bool far_plane = false,
             ALMetalCompareFunction compare = ALMetalCompareFunction::greater,
             bool write = true, bool count = false)
    {
        struct alignas(16) Params { F32 matrix[16]; F32 geometry[4]; U32 flags[4]; } p{};
        std::memcpy(p.matrix, projection.getF32ptr(), sizeof(p.matrix));
        p.geometry[0] = distance; p.geometry[1] = 1.f/projection.getRow<0>()[0];
        p.geometry[2] = 1.f/projection.getRow<1>()[1]; p.geometry[3] = far_plane;
        p.flags[0] = !LLRender::sReverseZ;
        ALMetalDepthStencilDescriptor dd;
        dd.depth_compare = !LLRender::sReverseZ && compare == ALMetalCompareFunction::greater
            ? ALMetalCompareFunction::less : compare;
        dd.depth_write_enabled = write;
        auto depth = ALMetalDepthStencilState::create(*device, dd, &error); REQUIRE(depth);
        auto command = device->makeCommandBuffer("Native projection draw", &error); REQUIRE(command);
        id<MTLBuffer> result = nil;
        if (count)
        {
            result = [(__bridge id<MTLDevice>)device->nativeHandle()
                newBufferWithLength:sizeof(U64) options:MTLResourceStorageModeShared];
            REQUIRE(result); std::memset(result.contents,0,sizeof(U64));
        }
        ALMetalRenderPassConfig pc; pc.depth_load = ALMetalLoadAction::load;
        pc.depth_store = ALMetalStoreAction::store;
        auto pass = target->makeRenderPass(pc, &error); REQUIRE(pass);
        // The public batch's completion association belongs to the world
        // owner. This standalone transport owns its actual counter directly.
        if (result) ((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer = result;
        auto encoder = ALMetalRenderEncoder::begin(*command,*pass,"Projection depth",&error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error)); REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setVertexBytes(0,&p,sizeof(p),&error)); REQUIRE(encoder->setViewport(viewport,&error));
        if (count) REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,0,&error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        U64 samples = 0;
        if (result) std::memcpy(&samples,result.contents,sizeof(samples));
        return samples;
    }
    U64 samples(const LLMatrix4a& projection, ALMetalCompareFunction compare)
    { return draw(projection,1000.f,true,compare,false,true); }
    F32 centreDepth()
    {
        std::array<F32,SIZE*SIZE> values;
        REQUIRE(target->depthStencilTexture()->readback(*device,0,0,{},values.data(),SIZE*sizeof(F32),&error));
        return values[(SIZE/2)*SIZE+SIZE/2];
    }
};

    // The matrix: recognised as infinite, stores near / distance, and its inverse gives w = 0 at the cleared
    // depth, which the reconstruction floor turns into MAX_RECONSTRUCT_DISTANCE. Finite and forward matrices are not.
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: the infinite matrix stores near over distance and its far end is floored", "[almetal][native-infinite-projection]")
    {
        const LLMatrix4a proj = infinite();
        CHECK(al_projection_is_infinite(proj));
        UNSCOPED_INFO("finite reversed is not");
        CHECK_FALSE(al_projection_is_infinite(al_perspective(FOV, 1.f, NEAR_PLANE, 1024.f)));
        CHECK_FALSE(al_projection_is_infinite(al_ortho(-1.f, 1.f, -1.f, 1.f, 0.f, 10.f)));

        for (F32 dist : DISTANCES)
        {
            CAPTURE(dist);
            LLVector4a clip;
            proj.transform4(LLVector4a(0.f, 0.f, -dist, 1.f), clip);
            REQUIRE(clip[2] == NEAR_PLANE);
            REQUIRE(clip[3] == dist);
        }

        LLMatrix4a inv = proj;
        inv.invert();
        LLVector4a far_point;
        inv.transform4(LLVector4a(0.f, 0.f, 0.f, 1.f), far_point);
        UNSCOPED_INFO("cleared depth unprojects to w = 0");
        CHECK(fabsf(far_point[3]) < 1e-12f);
        CHECK_THAT(reconstruct(inv, 0.f), ll_test::WithinBits(MAX_RECONSTRUCT_DISTANCE, 8));

        // The floor leaves a finite projection alone: its cleared depth still reconstructs at its far plane.
        LLMatrix4a inv_finite = al_perspective(FOV, 1.f, NEAR_PLANE, 1024.f);
        inv_finite.invert();
        LLVector4a raw;
        inv_finite.transform4(LLVector4a(0.f, 0.f, 0.f, 1.f), raw);
        CHECK(reconstruct(inv_finite, 0.f) == -raw[2] / raw[3]);
        CHECK(fabsf(-raw[2] / raw[3] - 1024.f) < 1.f);

        LLRender::sReverseZ = false;
        const LLMatrix4a forward = al_perspective(FOV, 1.f, NEAR_PLANE, std::numeric_limits<F32>::infinity());
        CHECK_FALSE(al_projection_is_infinite(forward));
        CHECK(std::isfinite(forward.getColumn<2>()[2]));
        LLRender::sReverseZ = true;
    }

    // Draw at each distance: the stored depth is near / distance (oracle), nothing is clipped far, and the
    // reconstructed distance is within the oracle's bound (0.29 mm, 3.9 mm, 9.3 mm, 45 mm).
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: drawn depth is near over distance and round-trips within the bound", "[almetal][native-infinite-projection]")
    {
        setUp();
        const LLMatrix4a proj = infinite();
        LLMatrix4a inv = proj;
        inv.invert();
        const F32 oracle_depth[] = { 1.0e-4f, 1.2207031e-5f, 3.125e-6f, 1.0e-6f };

        for (size_t i = 0; i < std::size(DISTANCES); ++i)
        {
            const F32 dist = DISTANCES[i];
            CAPTURE(dist);
            clear();
            draw(proj, dist);
            const F32 d = centreDepth();
            UNSCOPED_INFO("drawn");
            REQUIRE(d > 0.f);
            REQUIRE(fabsf(d - oracle_depth[i]) <= oracle_depth[i] * 1e-6f);
            const F64 err = fabs((F64)reconstruct(inv, d) - dist);
            UNSCOPED_INFO("round trip error " << err * 1000.0 << " mm");
            REQUIRE(err <= bound(dist, d));
        }

        // Past every reach and still not clipped: 1000 km.
        clear();
        draw(proj, 1.0e6f);
        UNSCOPED_INFO("1000 km is not clipped");
        CHECK(centreDepth() > 0.f);
    }

    // Two surfaces four depth steps apart at each distance: the nearer one wins whichever is drawn first.
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: surfaces four depth steps apart resolve in either draw order", "[almetal][native-infinite-projection]")
    {
        setUp();
        const LLMatrix4a proj = infinite();

        for (F32 dist : DISTANCES)
        {
            CAPTURE(dist);
            const F32 gap = (F32)(4.0 * depthStep(NEAR_PLANE / dist)) + (std::nextafter(dist, 2.f * dist) - dist);
            const F32 nearer = dist - gap;

            clear();
            draw(proj, dist);
            const F32 d_far = centreDepth();
            draw(proj, nearer);
            const F32 d_near = centreDepth();
            UNSCOPED_INFO("nearer drawn second wins");
            REQUIRE(d_near > d_far);

            clear();
            draw(proj, nearer);
            draw(proj, dist);
            UNSCOPED_INFO("farther drawn second loses");
            REQUIRE(centreDepth() == d_near);
        }
    }

    // Forward-Z cannot hold an infinite plane in 24-bit depth, so an infinite request falls back to the old
    // fixed plane rather than MAX_FAR_PLANE, and depth beyond it is clipped as before.
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: forward-Z falls back to the old fixed plane and clips past it", "[almetal][native-infinite-projection]")
    {
        LLRender::sReverseZ = false;
        const LLMatrix4a forward = al_perspective(FOV, 1.f, NEAR_PLANE, std::numeric_limits<F32>::infinity());
        const LLMatrix4a legacy = al_perspective(FOV, 1.f, NEAR_PLANE, FINITE_PROJECTION_FAR);
        LLRender::sReverseZ = true;
        for (S32 i = 0; i < 16; ++i)
        {
            CAPTURE(i);
            REQUIRE(forward.getF32ptr()[i] == legacy.getF32ptr()[i]);
        }
        LLVector4a clip;
        forward.transform4(LLVector4a(0.f, 0.f, -FINITE_PROJECTION_FAR, 1.f), clip);
        UNSCOPED_INFO("far plane at the old fixed plane");
        CHECK(fabsf(clip[2] / clip[3] - 1.f) < 1e-5f);
        forward.transform4(LLVector4a(0.f, 0.f, -2.f * FINITE_PROJECTION_FAR, 1.f), clip);
        UNSCOPED_INFO("past it is clipped");
        CHECK(clip[2] / clip[3] > 1.f);

        // Metal receives the same forward matrix with its native [-1,1] -> [0,1] depth conversion.
        LLRender::sReverseZ = false;
        setUp(); clear(); draw(forward, FINITE_PROJECTION_FAR * .5f);
        REQUIRE(centreDepth() < 1.f);
        clear(); draw(forward, FINITE_PROJECTION_FAR * 2.f);
        CHECK(centreDepth() == 1.f);
    }

    // The sky draws last, on the far plane, writing no depth, and the depth test spares it every pixel the world
    // covered. Under the infinite projection the far plane is the cleared depth exactly and geometry as
    // far out as reconstruction reaches still stores more, so the sky passes where nothing was drawn and nowhere else.
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: the sky on the far plane passes only where nothing was drawn", "[almetal][native-infinite-projection]")
    {
        setUp();
        const LLMatrix4a proj = infinite();
        const auto sky_samples = [&]() { return samples(proj, ALMetalCompareFunction::greater_equal); };

        clear();
        UNSCOPED_INFO("the sky fills an empty frame");
        CHECK(sky_samples() == (U32)(SIZE * SIZE));
        UNSCOPED_INFO("the sky leaves the cleared depth");
        CHECK(centreDepth() == 0.f);

        for (F32 dist : { 1000.f, 100000.f, MAX_RECONSTRUCT_DISTANCE })
        {
            CAPTURE(dist);
            clear();
            draw(proj, dist);
            UNSCOPED_INFO("geometry stores more than the far plane");
            REQUIRE(centreDepth() > 0.f);
            UNSCOPED_INFO("the sky stays behind geometry");
            REQUIRE(sky_samples() == (U32)0);
        }

    }

    // Original drawn-only lighting passes use a far-plane triangle that
    // tests in front of the clear: every geometry pixel passes, however far
    // out, and no sky pixel does.
    TEST_CASE_METHOD(NativeInfiniteProjection, "alinfiniteprojection_native: a far-plane drawn-only pass covers geometry and no sky", "[almetal][native-infinite-projection]")
    {
        setUp();
        const LLMatrix4a proj = infinite();
        UNSCOPED_INFO("in front of the far plane is LESS under reverse-Z");
        CHECK((U32)LLGLDepthTest::remap(GL_GREATER) == (U32)GL_LESS);
        const auto drawn_samples = [&]() { return samples(proj, ALMetalCompareFunction::less); };

        clear();
        UNSCOPED_INFO("no drawn-only samples on an empty frame");
        CHECK(drawn_samples() == (U32)0);

        for (F32 dist : { 1.f, 1000.f, 100000.f, MAX_RECONSTRUCT_DISTANCE })
        {
            CAPTURE(dist);
            clear();
            draw(proj, dist);
            UNSCOPED_INFO("the far-plane pass covers all geometry");
            REQUIRE(drawn_samples() == (U32)(SIZE * SIZE));
        }

        // Pixel by pixel: geometry over the left half only, sky over the right.
        clear();
        viewport.width = SIZE / 2;
        draw(proj, MAX_RECONSTRUCT_DISTANCE);
        viewport.width = SIZE;
        UNSCOPED_INFO("the far-plane pass covers the geometry's half alone");
        CHECK(drawn_samples() == (U32)(SIZE * SIZE / 2));

    }

}
