/**
 * @file alrender_resource_invariants_test.cpp
 * @brief Actual native transient target reuse, shared depth, poison and shadow caster stamps.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version2.1 only.
 */
#include "linden_common.h"
#include "../altransienttargetpool.h"
#include "../altransienttargets.h"
#include "../alspotshadowcache.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llvertexbuffer.h"
#include "llstrider.h"
#include "alrendercapabilities.h"
#include "almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
using Pool=ALTransientTargetPool;
using Depth=ALRenderTargetDesc::Depth;
constexpr U32 W=6,H=5;
constexpr ALRenderTargetDesc GBUFFER{.width=W,.height=H,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D32F,.stencil=true};
constexpr ALRenderTargetDesc LIGHT{.width=W,.height=H,.color={GL_RGBA16F}};
constexpr ALRenderTargetDesc DEPTH_COPY{.width=W,.height=H,.depth=Depth::D32F};
constexpr ALRenderTargetDesc OWN_DEPTH{.width=W,.height=H,.color={GL_RGBA8},.depth=Depth::D24,.stencil=true};
struct Names
{
    const ALMetalRenderTarget* target;
    std::vector<void*> color;
    void* depth;
    bool operator==(const Names&) const=default;
};
void* depth_handle(const LLRenderTarget& target)
{
    const auto* depth=target.getMetalDepthTexture();return depth?depth->nativeHandle():nullptr;
}
Names names_of(const LLRenderTarget& target)
{
    Names names{target.getMetalRenderTarget(),{},depth_handle(target)};
    for (U32 i=0;i<target.getNumTextures();++i)
    {
        REQUIRE(target.getMetalColorTexture(i));
        names.color.push_back(target.getMetalColorTexture(i)->nativeHandle());
    }
    return names;
}
bool close_to(F32 a,F32 b){return std::abs(a-b)<1e-3f;}
bool is_poison(const LLColor4& c)
{
    for(U32 i=0;i<4;++i) if(!close_to(c.mV[i],Pool::POISON_COLOUR[i])) return false;
    return true;
}
struct NativeResources
{
    ALRenderCapabilities previous_capabilities=gRenderCapabilities;
    ALMetalDevice* previous_device=LLImageGL::getMetalDevice();
    std::unique_ptr<ALMetalDevice> device;
    std::string error;
    LLRenderTarget owner;
    Pool pool;
    NativeResources()
    {
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
    }
    ~NativeResources()
    {
        pool.clear();owner.release();
        LLImageGL::setMetalDevice(previous_device);
        gRenderCapabilities=previous_capabilities;
    }
    void fill(LLRenderTarget& target,const LLColor4& colour,F64 depth)
    {
        REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,colour,depth,0));
    }
    LLColor4 colour_of(LLRenderTarget& target)
    {
        const auto* texture=target.getMetalColorTexture(0);REQUIRE(texture);
        REQUIRE(texture->descriptor().width==W);REQUIRE(texture->descriptor().height==H);
        LLColor4 result;
        if(texture->descriptor().format==ALMetalPixelFormat::rgba16_float)
        {
            std::array<U16,W*H*4> pixels;
            REQUIRE(texture->readback(*device,0,0,{},pixels.data(),W*8,&error));INFO(error);
            for(U32 i=0;i<4;++i){__fp16 value;std::memcpy(&value,&pixels[(H/2*W+W/2)*4+i],2);result.mV[i]=value;}
        }
        else
        {
            REQUIRE(texture->descriptor().format==ALMetalPixelFormat::rgba8_unorm);
            std::array<U8,W*H*4> pixels;
            REQUIRE(texture->readback(*device,0,0,{},pixels.data(),W*4,&error));INFO(error);
            for(U32 i=0;i<4;++i)result.mV[i]=pixels[(H/2*W+W/2)*4+i]/255.f;
        }
        return result;
    }
    F32 depth_of(LLRenderTarget& target)
    {
        const auto* depth=target.getMetalDepthTexture();REQUIRE(depth);
        REQUIRE(depth->descriptor().width==W);REQUIRE(depth->descriptor().height==H);
        std::array<F32,W*H> pixels;
        REQUIRE(depth->readback(*device,0,0,{},pixels.data(),W*sizeof(F32),&error));INFO(error);
        return pixels[H/2*W+W/2];
    }
};
    TEST_CASE_METHOD(NativeResources, "Native transient targets: a pooled target is the same GL objects at each acquisition",
                     "[almetal][native-alrender-resources]")
    {
        for (const ALRenderTargetDesc& desc : { LIGHT, DEPTH_COPY, OWN_DEPTH })
        {
            CAPTURE(desc.colorCount(), (U32)desc.depth);
            LLRenderTarget* first = pool.acquire(desc, "first");
            REQUIRE(first);
            REQUIRE(first->isComplete());
            CHECK(first->desc() == desc);
            const Names names = names_of(*first);
            CHECK(names.target != nullptr);
            CHECK(pool.release(first));

            const U32 bytes = LLRenderTarget::sBytesAllocated;
            LLRenderTarget* again = pool.acquire(desc, "again");
            REQUIRE(again == first);
            UNSCOPED_INFO("nothing built: the names it had, and no byte more");
            CHECK(names_of(*again) == names);
            CHECK(LLRenderTarget::sBytesAllocated == bytes);
            CHECK(again->desc() == desc);
            CHECK(pool.release(again));
        }
        CHECK(pool.stats().mMisses == 3);
        CHECK(pool.stats().mHits == 3);
        CHECK(pool.stats().mBytes ==
              LLRenderTarget::bytesFor(LIGHT) + LLRenderTarget::bytesFor(DEPTH_COPY) + LLRenderTarget::bytesFor(OWN_DEPTH));

    }

    TEST_CASE_METHOD(NativeResources, "Native transient targets: a pooled target keeps the depth it shares attached",
                     "[almetal][native-alrender-resources]")
    {
        REQUIRE(owner.ensure(GBUFFER) == LLRenderTarget::EnsureResult::Built);

        LLRenderTarget* light = nullptr;
        {
            ALTransientTargets scope(pool);
            light = scope.acquire(LIGHT, owner, "main.light");
            REQUIRE(light);
            CHECK(light->sharesDepth());
            CHECK(depth_handle(*light) == depth_handle(owner));
            CHECK(light->getMetalDepthTexture()->descriptor().format == ALMetalPixelFormat::depth32_float_stencil8);
            fill(owner, LLColor4::black, 0.25);
            CHECK(close_to(depth_of(*light), 0.25f));
        }

        UNSCOPED_INFO("acquired again, under another name: still attached, and what the owner writes reads through it");
        const Names names = names_of(*light);
        LLRenderTarget* water = pool.acquire(LIGHT, owner, "main.water.refraction");
        REQUIRE(water == light);
        CHECK(names_of(*water) == names);
        CHECK(depth_handle(*water) == depth_handle(owner));
        fill(owner, LLColor4::black, 0.625);
        CHECK(close_to(depth_of(*water), 0.625f));
        CHECK(pool.release(water));

        UNSCOPED_INFO("the owner's depth built again: the old sharer dropped, and the next attaches the new depth");
        REQUIRE(owner.ensure({ .width = W, .height = H, .color = { GL_RGBA8, GL_RGBA16F }, .depth = Depth::D24,
                               .stencil = true }) == LLRenderTarget::EnsureResult::Built);
        pool.dropStaleDepth();
        CHECK(pool.stats().mTargets == 0);
        LLRenderTarget* rebuilt = pool.acquire(LIGHT, owner, "main.light");
        REQUIRE(rebuilt);
        CHECK(depth_handle(*rebuilt) == depth_handle(owner));
        fill(owner, LLColor4::black, 0.375);
        CHECK(close_to(depth_of(*rebuilt), 0.375f));
        CHECK(pool.release(rebuilt));

    }

    TEST_CASE_METHOD(NativeResources, "Native transient targets: under validation a release poisons what the target holds of its own",
                     "[almetal][native-alrender-resources]")
    {
        REQUIRE(owner.ensure(GBUFFER) == LLRenderTarget::EnsureResult::Built);
        pool.setValidation(true);

        // One actual native poison implementation replaces both old GL paths.
        {

            LLRenderTarget* sharer = pool.acquire(LIGHT, owner, "main.light");
            LLRenderTarget* own    = pool.acquire(OWN_DEPTH, "smaa.edges");
            LLRenderTarget* depth  = pool.acquire(DEPTH_COPY, "main.depth.copy");
            REQUIRE(sharer);
            REQUIRE(own);
            REQUIRE(depth);
            fill(owner, LLColor4::black, 0.25);
            fill(*sharer, LLColor4(0.f, 1.f, 0.f, 1.f), 0.25);
            fill(*own, LLColor4(0.f, 0.f, 1.f, 1.f), 0.75);
            fill(*depth, LLColor4::black, 0.875);
            REQUIRE(!is_poison(colour_of(*sharer)));

            bool before_mask[4];S32 before_scissor[4];
            gGL.getColorMask(before_mask);gGL.getScissor(before_scissor);
            for (LLRenderTarget* target : { sharer, own, depth })
            {
                CHECK(pool.release(target));
            }

            CHECK(is_poison(colour_of(*sharer)));
            CHECK(is_poison(colour_of(*own)));
            CHECK(close_to(depth_of(*own), Pool::POISON_DEPTH));
            CHECK(close_to(depth_of(*depth), Pool::POISON_DEPTH));
            UNSCOPED_INFO("the depth the sharer shares is the owner's, and keeps what the owner wrote");
            CHECK(close_to(depth_of(owner), 0.25f));
            // Dedicated native poison encoders leave the caller's logical
            // color/scissor state untouched; no retained driver state exists.
            bool after_mask[4];S32 after_scissor[4];
            gGL.getColorMask(after_mask);gGL.getScissor(after_scissor);
            CHECK(std::equal(std::begin(before_mask),std::end(before_mask),std::begin(after_mask)));
            CHECK(std::equal(std::begin(before_scissor),std::end(before_scissor),std::begin(after_scissor)));


        }

        UNSCOPED_INFO("without validation a release leaves the contents be");
        pool.setValidation(false);
        LLRenderTarget* kept = pool.acquire(OWN_DEPTH, "smaa.edges");
        REQUIRE(kept);
        fill(*kept, LLColor4(0.f, 0.f, 1.f, 1.f), 0.75);
        CHECK(pool.release(kept));
        CHECK(!is_poison(colour_of(*kept)));
        CHECK(close_to(depth_of(*kept), 0.75f));
    }

TEST_CASE("Native spot shadow geometry folds actual vertex publications and buffer identity",
          "[almetal][native-alrender-resources][native-shadow-fold]")
{
    NativeResources native;
    const bool authority=LLVertexBuffer::isMetalBufferAuthority();
    LLVertexBuffer::setMetalDevice(native.device.get());
    LLVertexBuffer::setMetalBufferAuthority(false);
    LLVertexBuffer::initClass(nullptr);
    struct Reset
    {
        bool authority;ALMetalDevice* device;
        ~Reset(){LLVertexBuffer::flushBuffers();LLVertexBuffer::cleanupClass();LLVertexBuffer::setMetalBufferAuthority(authority);LLVertexBuffer::setMetalDevice(device);}
    } reset{authority,native.previous_device};
    const auto make=[]
    {
        LLPointer<LLVertexBuffer> buffer=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(buffer->allocateBuffer(4,6));return buffer;
    };
    const auto fold=[](const LLVertexBuffer* buffer)
    {
        ALSpotShadowCache::Hash hash;
        ALSpotShadowCache::foldGeometry(hash,buffer,0,3,6,0,0);
        return hash.value();
    };
    auto buffer=make();const U64 original=fold(buffer);
    CHECK(fold(buffer)==original);
    LLStrider<LLVector3> position;REQUIRE(buffer->getVertexStrider(position,0,4));
    for(S32 i=0;i<4;++i)position[i].set(F32(i),0,0);
    buffer->unmapBuffer();
    REQUIRE(buffer->hasPublishedMetalVertexBuffer());
    CHECK(fold(buffer)!=original);
    // Prove the new stamp corresponds to actual published native bytes.
    REQUIRE(buffer->getMetalVertexBuffer());
    std::vector<U8> bytes(buffer->getMetalVertexBuffer()->length());
    REQUIRE(buffer->getMetalVertexBuffer()->read(0,bytes.data(),bytes.size(),&native.error));
    REQUIRE(bytes.size()>=4*sizeof(LLVector4a));
    for(S32 i=0;i<4;++i)
    {
        F32 x;std::memcpy(&x,bytes.data()+i*sizeof(LLVector4a),sizeof(x));
        CHECK(x==F32(i));
    }
    auto other=make();CHECK(fold(other)!=fold(buffer));
}
}
