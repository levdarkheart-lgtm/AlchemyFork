/**
 * @file alrender_resource_invariants_test.cpp
 * @brief Native shadow caster geometry publications and identity.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version2.1 only.
 */
#include "linden_common.h"
#include "../alspotshadowcache.h"
#include "llimagegl.h"
#include "llvertexbuffer.h"
#include "llstrider.h"
#include "alrendercapabilities.h"
#include "almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <vector>

namespace
{
struct NativeResources
{
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    std::unique_ptr<ALMetalDevice> device;
    std::string error;
    NativeResources()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
    }
    ~NativeResources()
    {
        LLImageGL::setMetalDevice(previous_device);
        gRenderCapabilities = previous_capabilities;
    }
};
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
