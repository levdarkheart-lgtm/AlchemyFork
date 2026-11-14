/**
 * @file alnative_vertex_publication_test.cpp
 * @brief Fresh immediate shared stores preserve native/CPU layout and readers.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <vector>

namespace
{
constexpr U32 layout = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR |
                       LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXTURE_INDEX;
const LLColor4U red(255, 0, 0, 255), green(0, 255, 0, 255), blue(0, 0, 255, 255);

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_device = LLImageGL::getMetalDevice();
    bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
    Fixture()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
        gGL.frameFences().release();
    }
    ~Fixture()
    {
        gGL.frameFences().release();
        LLVertexBuffer::flushBuffers();
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalDevice(old_device);
        LLVertexBuffer::setMetalBufferAuthority(old_authority);
        gRenderCapabilities = old_capabilities;
    }
    LLPointer<LLVertexBuffer> fresh(U32 mask = layout)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(mask);
        REQUIRE(result->allocateNativeSharedBuffer(4));
        return result;
    }
    static void fill(LLVertexBuffer& buffer)
    {
        const std::array<LLVector4a, 4> positions{{
            LLVector4a(-1.f, -1.f, 0.f, 0.f), LLVector4a(1.f, -1.f, 0.f, 0.f),
            LLVector4a(-1.f, 1.f, 0.f, 0.f), LLVector4a(1.f, 1.f, 0.f, 0.f)}};
        const std::array<LLVector2, 4> uv{{
            LLVector2(0.f, 0.f), LLVector2(1.f, 0.f), LLVector2(0.f, 1.f), LLVector2(1.f, 1.f)}};
        const std::array<LLColor4U, 4> colors{{red, red, red, red}};
        buffer.setPositionData(positions.data());
        if (buffer.hasDataType(LLVertexBuffer::TYPE_TEXCOORD0)) buffer.setTexCoord0Data(uv.data());
        if (buffer.hasDataType(LLVertexBuffer::TYPE_COLOR)) buffer.setColorData(colors.data());
        buffer.unmapBuffer();
    }
    std::vector<U8> bytes(const ALMetalBuffer& buffer)
    {
        std::vector<U8> result(buffer.length());
        REQUIRE(buffer.read(0, result.data(), result.size(), &error));
        return result;
    }
};
}

TEST_CASE("Fresh immediate native shared publication uses the CPU layout without a second copy",
          "[almetal][native-publication][direct-shared]")
{
    Fixture f;
    const auto before = LLVertexBuffer::metalUploadStats();
    auto buffer = f.fresh();
    REQUIRE(buffer->usesNativeSharedVertexStorage());
    REQUIRE(buffer->getMetalVertexBuffer());
    CHECK(buffer->getMappedData() == buffer->getMetalVertexBuffer()->mappedBytes());
    CHECK(reinterpret_cast<std::uintptr_t>(buffer->getMappedData()) % 16 == 0);
    CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
    Fixture::fill(*buffer);
    REQUIRE(buffer->hasPublishedMetalVertexBuffer());
    const auto after = LLVertexBuffer::metalUploadStats();
    CHECK(after.direct_shared_allocations == before.direct_shared_allocations + 1);
    CHECK(after.direct_shared_publications == before.direct_shared_publications + 1);
    CHECK(after.uploaded_bytes == before.uploaded_bytes);
    CHECK(after.full_uploads == before.full_uploads);
    CHECK(after.partial_uploads == before.partial_uploads);
    CHECK(after.direct_shared_written_bytes > before.direct_shared_written_bytes);
    const auto native = f.bytes(*buffer->getMetalVertexBuffer());
    CHECK(std::memcmp(native.data(), buffer->getMappedData(), buffer->getSize()) == 0);
    CHECK_FALSE(buffer->allocateNativeSharedBuffer(4));
}

TEST_CASE("Fresh native shared texture-index writes alias only the position w lanes",
          "[almetal][native-publication][direct-shared]")
{
    Fixture f;
    auto buffer = f.fresh();
    Fixture::fill(*buffer);
    const auto previous = f.bytes(*buffer->getMetalVertexBuffer());
    auto* indices = buffer->mapVertexBuffer(LLVertexBuffer::TYPE_TEXTURE_INDEX, 3, 1);
    REQUIRE(indices);
    const U32 slot = 7;
    std::memcpy(indices, &slot, sizeof(slot));
    buffer->unmapBuffer();
    const auto current = f.bytes(*buffer->getMetalVertexBuffer());
    const auto offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX) + 3 * sizeof(LLVector4a) + 12;
    for (std::size_t byte = 0; byte < current.size(); ++byte)
        CHECK(current[byte] == (byte >= offset && byte < offset + sizeof(slot)
            ? reinterpret_cast<const U8*>(&slot)[byte - offset] : previous[byte]));
}

TEST_CASE("Fresh native shared stores rotate before writes borrowed by queued readers",
          "[almetal][native-publication][direct-shared]")
{
    Fixture f;
    auto buffer = f.fresh();
    Fixture::fill(*buffer);
    auto publisher = std::make_shared<ALNativePublication::State>();
    buffer->nativePublication().observe(publisher);
    REQUIRE(publisher->publish(publisher->revision(), buffer->getMetalVertexBufferLease()));
    // This retained lease represents an unencoded display-list/frame reader;
    // frame completion alone cannot make its generation writable.
    auto queued_reader = publisher->publication();
    auto old_native = std::static_pointer_cast<const ALMetalBuffer>(queued_reader);
    const auto old_bytes = f.bytes(*old_native);
    const auto before = LLVertexBuffer::metalUploadStats();
    buffer->setColorData(&green, 1, 1);
    CHECK_FALSE(publisher->publication());
    CHECK(buffer->getMappedData() != old_native->mappedBytes());
    CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
    buffer->unmapBuffer();
    CHECK(buffer->usesNativeSharedVertexStorage());
    CHECK(f.bytes(*old_native) == old_bytes);
    CHECK(LLVertexBuffer::metalUploadStats().direct_shared_preserved_bytes ==
          before.direct_shared_preserved_bytes + buffer->getSize());
    const auto current = f.bytes(*buffer->getMetalVertexBuffer());
    const auto color = buffer->getOffset(LLVertexBuffer::TYPE_COLOR) + sizeof(LLColor4U);
    CHECK(std::memcmp(current.data() + color, &green, sizeof(green)) == 0);

    LLPointer<LLVertexBuffer> clone = new LLVertexBuffer(layout);
    REQUIRE(buffer->clone(*clone));
    CHECK_FALSE(clone->usesNativeSharedVertexStorage());
    CHECK(clone->getMappedData() != buffer->getMappedData());
    CHECK(clone->getOffset(LLVertexBuffer::TYPE_TEXTURE_INDEX) == buffer->getOffset(LLVertexBuffer::TYPE_TEXTURE_INDEX));
    CHECK(clone->getSize() == buffer->getSize());
}

TEST_CASE("Fresh native shared fallback preserves CPU edits and publication retry",
          "[almetal][native-publication][direct-shared]")
{
    Fixture f;
    auto buffer = f.fresh();
    Fixture::fill(*buffer);
    auto queued_reader = buffer->getMetalVertexBufferLease();
    const auto old_bytes = f.bytes(*queued_reader);
    const auto before = buffer->nativePublication().publishedRevision();
    LLVertexBuffer::setMetalDevice(nullptr);
    buffer->setColorData(&blue, 2, 1);
    CHECK_FALSE(buffer->usesNativeSharedVertexStorage());
    CHECK(buffer->getMappedData() != queued_reader->mappedBytes());
    buffer->unmapBuffer();
    CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->nativePublication().publishedRevision() == before);
    CHECK(f.bytes(*queued_reader) == old_bytes);
    LLVertexBuffer::setMetalDevice(f.device.get());
    REQUIRE(buffer->prepareMetalBuffers(false));
    CHECK(buffer->nativePublication().publishedRevision() == before + 1);
    const auto current = f.bytes(*buffer->getMetalVertexBuffer());
    const auto color = buffer->getOffset(LLVertexBuffer::TYPE_COLOR) + 2 * sizeof(LLColor4U);
    CHECK(std::memcmp(current.data() + color, &blue, sizeof(blue)) == 0);

    auto unsupported = f.fresh(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL);
    CHECK_FALSE(unsupported->usesNativeSharedVertexStorage());
    Fixture::fill(*unsupported);
    CHECK(unsupported->hasPublishedMetalVertexBuffer());
}

TEST_CASE("Lost direct-shared producer edits fail closed until their bytes are actually retried",
          "[almetal][native-publication][direct-shared][PPD-130]")
{
    Fixture f;
    auto buffer = f.fresh();
    Fixture::fill(*buffer);
    auto reader = buffer->getMetalVertexBufferLease();
    REQUIRE(reader);
    const auto old = f.bytes(*reader);
    const auto revision = buffer->nativePublication().publishedRevision();
    const auto counters = LLVertexBuffer::metalUploadStats();
    {
        LLVertexBuffer::ScopedNativeSharedWriteAllocationFailure fail_both(true, true);
        buffer->setColorData(&blue, 2, 1);
        CHECK(buffer->hasNativeVertexWriteFailure());
        CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
        buffer->unmapBuffer();
        CHECK_FALSE(buffer->prepareMetalBuffers(false));
        CHECK(buffer->nativePublication().publishedRevision() == revision);
        CHECK(f.bytes(*reader) == old);
    }
    // Allocation recovery and automatic flush cannot reconstruct lost bytes.
    CHECK_FALSE(buffer->prepareMetalBuffers(false));
    buffer->unmapBuffer();
    CHECK(buffer->hasNativeVertexWriteFailure());
    CHECK(buffer->nativePublication().publishedRevision() == revision);
    CHECK(LLVertexBuffer::metalUploadStats().direct_shared_write_failures ==
          counters.direct_shared_write_failures + 1);

    // A different valid partial edit must not clear the missing blue edit.
    buffer->setColorData(&green, 0, 1);
    buffer->unmapBuffer();
    CHECK(buffer->hasNativeVertexWriteFailure());
    CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->nativePublication().publishedRevision() == revision);
    CHECK(f.bytes(*reader) == old);

    buffer->setColorData(&blue, 2, 1); // actual producer retry of the rejected bytes
    buffer->unmapBuffer();
    CHECK_FALSE(buffer->hasNativeVertexWriteFailure());
    REQUIRE(buffer->prepareMetalBuffers(false));
    CHECK(buffer->nativePublication().publishedRevision() == revision + 1);
    const auto current = f.bytes(*buffer->getMetalVertexBuffer());
    const auto color = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
    CHECK(std::memcmp(current.data() + color, &green, sizeof(green)) == 0);
    CHECK(std::memcmp(current.data() + color + 2 * sizeof(blue), &blue, sizeof(blue)) == 0);
    CHECK(f.bytes(*reader) == old);
}

TEST_CASE("Retried lost edits in CPU shadow resume ordinary upload retry without another producer write",
          "[almetal][native-publication][direct-shared][PPD-130]")
{
    Fixture f;
    auto buffer = f.fresh();
    Fixture::fill(*buffer);
    auto reader = buffer->getMetalVertexBufferLease();
    const auto old = f.bytes(*reader);
    const auto revision = buffer->nativePublication().publishedRevision();
    {
        LLVertexBuffer::ScopedNativeSharedWriteAllocationFailure fail_both(true, true);
        buffer->setColorData(&blue, 1, 1);
        buffer->unmapBuffer();
    }
    REQUIRE(buffer->hasNativeVertexWriteFailure());
    {
        LLVertexBuffer::ScopedNativeSharedWriteAllocationFailure fail_native(true, false);
        buffer->setColorData(&blue, 1, 1);
        CHECK_FALSE(buffer->usesNativeSharedVertexStorage());
        CHECK_FALSE(buffer->hasNativeVertexWriteFailure());
        LLVertexBuffer::setMetalDevice(nullptr);
        buffer->unmapBuffer();
        CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
        CHECK(buffer->nativePublication().publishedRevision() == revision);
    }
    LLVertexBuffer::setMetalDevice(f.device.get());
    REQUIRE(buffer->prepareMetalBuffers(false));
    CHECK(buffer->nativePublication().publishedRevision() == revision + 1);
    const auto current = f.bytes(*buffer->getMetalVertexBuffer());
    const auto color = buffer->getOffset(LLVertexBuffer::TYPE_COLOR) + sizeof(blue);
    CHECK(std::memcmp(current.data() + color, &blue, sizeof(blue)) == 0);
    CHECK(f.bytes(*reader) == old);
}

TEST_CASE("Empty direct-shared mappings preserve borrowed incomplete generations and physical bounds",
          "[almetal][native-publication][direct-shared][PPD-131]")
{
    Fixture f;
    auto buffer = f.fresh(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXTURE_INDEX);
    Fixture::fill(*buffer);
    auto reader = buffer->getMetalVertexBufferLease();
    REQUIRE(reader);
    CHECK_FALSE(gGL.frameFences().isComplete(gGL.frameFences().frame()));
    const auto old = f.bytes(*reader);
    const auto* storage = buffer->getMappedData();
    const auto stamp = buffer->getContentStamp();
    const auto revision = buffer->nativePublication().publishedRevision();
    const auto before = LLVertexBuffer::metalUploadStats();
    CHECK(buffer->mapVertexBuffer(LLVertexBuffer::TYPE_VERTEX, 1, 0) == storage + 16);
    CHECK(buffer->mapVertexBuffer(LLVertexBuffer::TYPE_VERTEX, 4, -1) == storage + buffer->getSize());
    CHECK(buffer->mapVertexBuffer(LLVertexBuffer::TYPE_TEXTURE_INDEX, 4, 0) == storage + buffer->getSize());
    CHECK_FALSE(buffer->mapVertexBuffer(LLVertexBuffer::TYPE_VERTEX, 5, 0));
    buffer->unmapBuffer();
    CHECK(buffer->getMappedData() == storage);
    CHECK(buffer->getMetalVertexBuffer() == reader.get());
    CHECK(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->getContentStamp() == stamp);
    CHECK(buffer->nativePublication().publishedRevision() == revision);
    const auto after = LLVertexBuffer::metalUploadStats();
    CHECK(after.direct_shared_preserved_bytes == before.direct_shared_preserved_bytes);
    CHECK(after.direct_shared_publications == before.direct_shared_publications);
    CHECK(after.uploaded_bytes == before.uploaded_bytes);
    CHECK(f.bytes(*reader) == old);
}
