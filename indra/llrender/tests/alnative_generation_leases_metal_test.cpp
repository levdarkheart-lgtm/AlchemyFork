/**
 * @file alnative_generation_leases_metal_test.cpp
 * @brief Queued CPU/native leases prevent reuse; completed stores publish ranges.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
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
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
const LLColor4U red(255, 0, 0, 255), green(0, 255, 0, 255), blue(0, 0, 255, 255);
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_device = LLImageGL::getMetalDevice();
    bool old_image_authority = LLImageGL::isMetalTextureAuthority();
    bool old_vertex_authority = LLVertexBuffer::isMetalBufferAuthority();
    Fixture()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
    }
    ~Fixture()
    {
        // Tests explicitly complete/abandon every native command first.
        gGL.frameFences().release();
        LLVertexBuffer::flushBuffers();
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalDevice(old_device);
        LLVertexBuffer::setMetalBufferAuthority(old_vertex_authority);
        LLImageGL::setMetalDevice(old_device);
        LLImageGL::setMetalTextureAuthority(old_image_authority);
        gRenderCapabilities = old_capabilities;
    }
    LLPointer<LLVertexBuffer> buffer(U32 vertices = 4, U32 indices = 6)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(
            LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR);
        REQUIRE(result->allocateBuffer(vertices, indices));
        REQUIRE_FALSE(result->getMetalVertexBufferLease());
        std::memset(result->getMappedData(), 0, result->getSize());
        if (indices) std::memset(result->getMappedIndices(), 0, result->getIndicesSize());
        const std::vector<LLColor4U> colours(vertices, red);
        result->setColorData(colours.data());
        result->unmapBuffer();
        REQUIRE(result->hasPublishedMetalVertexBuffer());
        return result;
    }
    std::vector<U8> bytes(const ALMetalBuffer& value)
    {
        std::vector<U8> result(value.length());
        REQUIRE(value.read(0, result.data(), result.size(), &error));
        return result;
    }
    std::vector<U8> pixels(const ALMetalTexture& value)
    {
        std::vector<U8> result(value.width() * value.height() * 4);
        REQUIRE(value.readback(*device, 0, 0, {}, result.data(), value.width() * 4, &error));
        return result;
    }
};
}

TEST_CASE("Native vertex publication copies only declared completed unleased ranges and retries them",
          "[almetal][native-generation-leases][range-publication]")
{
    Fixture f;
    auto vb = f.buffer();
    auto* identity = vb->getMetalVertexBuffer();
    gGL.frameFences().release();
    const auto before = LLVertexBuffer::metalUploadStats();
    vb->setColorData(&green, 1, 1);
    vb->setColorData(&blue, 3, 1); // separate ranges: never copy the gap
    vb->unmapBuffer();
    const auto after = LLVertexBuffer::metalUploadStats();
    CHECK(after.full_uploads == before.full_uploads);
    CHECK(after.partial_uploads == before.partial_uploads + 1);
    CHECK(after.uploaded_bytes == before.uploaded_bytes + 2 * sizeof(LLColor4U));
    CHECK(vb->getMetalVertexBuffer() == identity);
    auto data = f.bytes(*identity);
    CHECK(std::memcmp(data.data() + vb->getOffset(LLVertexBuffer::TYPE_COLOR) + 4,
                      &green, 4) == 0);
    CHECK(std::memcmp(data.data() + vb->getOffset(LLVertexBuffer::TYPE_COLOR) + 8,
                      &red, 4) == 0);
    CHECK(std::memcmp(data.data() + vb->getOffset(LLVertexBuffer::TYPE_COLOR) + 12,
                      &blue, 4) == 0);

    gGL.frameFences().release();
    LLStrider<LLColor4U> suffix;
    REQUIRE(vb->getColorStrider(suffix, 2)); // default count=-1 means remaining two
    suffix[0] = green; suffix[1] = green;
    const auto retry_before = LLVertexBuffer::metalUploadStats();
    LLVertexBuffer::setMetalDevice(nullptr);
    vb->unmapBuffer();
    CHECK_FALSE(vb->hasPublishedMetalVertexBuffer());
    CHECK_FALSE(vb->getMetalVertexBufferLease());
    LLVertexBuffer::setMetalDevice(f.device.get());
    vb->unmapBuffer(); // no additional write: queued ranges survive failure
    const auto retry_after = LLVertexBuffer::metalUploadStats();
    CHECK(vb->hasPublishedMetalVertexBuffer());
    CHECK(retry_after.uploaded_bytes == retry_before.uploaded_bytes + 8);
    CHECK(retry_after.full_uploads == retry_before.full_uploads);
    CHECK(vb->mapVertexBuffer(LLVertexBuffer::TYPE_COLOR, 4, 1) == nullptr);
    CHECK(vb->mapIndexBuffer(6, 1) == nullptr);

    const auto zero_before = LLVertexBuffer::metalUploadStats();
    REQUIRE(vb->mapVertexBuffer(LLVertexBuffer::TYPE_COLOR, 4, 0));
    vb->unmapBuffer();
    CHECK(LLVertexBuffer::metalUploadStats().uploaded_bytes == zero_before.uploaded_bytes);

    auto wide = f.buffer(4, 12); // 24 allocated bytes become six U32 indices
    const std::array<U32, 6> indices{{0, 1, 2, 2, 1, 3}};
    wide->setIndexData(indices.data()); wide->unmapBuffer();
    CHECK(wide->getNumIndices() == 6);
    auto* index_identity = wide->getMetalIndexBuffer();
    gGL.frameFences().release();
    const std::array<U32, 2> replacement{{3, 2}};
    const auto index_before = LLVertexBuffer::metalUploadStats();
    wide->setIndexData(replacement.data(), 2, 2); wide->unmapBuffer();
    CHECK(wide->getNumIndices() == 6);
    CHECK(wide->getMetalIndexBuffer() == index_identity);
    CHECK(LLVertexBuffer::metalUploadStats().uploaded_bytes == index_before.uploaded_bytes + 8);
    const auto actual = f.bytes(*index_identity);
    auto expected = indices; expected[2] = 3; expected[3] = 2;
    CHECK(std::memcmp(actual.data(), expected.data(), sizeof(expected)) == 0);
}

TEST_CASE("Native mapped texture-index lane ranges end at the last position lane",
          "[almetal][native-generation-leases][texture-index-range]")
{
    Fixture f;
    LLPointer<LLVertexBuffer> vb = new LLVertexBuffer(
        LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXTURE_INDEX);
    REQUIRE(vb->allocateBuffer(4, 0));
    std::array<LLVector4a, 4> positions;
    for (auto& value : positions) value.set(1.f, 2.f, 3.f, 0.f);
    vb->setPositionData(positions.data()); vb->unmapBuffer();
    REQUIRE(vb->getSize() == 4 * 16);
    auto* identity = vb->getMetalVertexBuffer();
    gGL.frameFences().release();
    auto before = LLVertexBuffer::metalUploadStats();
    U8* mapped = vb->mapVertexBuffer(LLVertexBuffer::TYPE_TEXTURE_INDEX, 0);
    REQUIRE(mapped);
    const std::array<U32, 4> values{{1, 2, 3, 4}};
    for (U32 index = 0; index < values.size(); ++index)
        std::memcpy(mapped + index * 16, &values[index], sizeof(U32));
    vb->unmapBuffer();
    CHECK(vb->hasPublishedMetalVertexBuffer());
    CHECK(LLVertexBuffer::metalUploadStats().uploaded_bytes == before.uploaded_bytes + 52);
    CHECK(vb->getMetalVertexBuffer() == identity);
    auto bytes = f.bytes(*identity);
    for (U32 index = 0; index < values.size(); ++index)
    {
        CHECK(std::memcmp(bytes.data() + index * 16, positions[index].getF32ptr(), 12) == 0);
        CHECK(std::memcmp(bytes.data() + index * 16 + 12, &values[index], sizeof(U32)) == 0);
    }
    gGL.frameFences().release();
    before = LLVertexBuffer::metalUploadStats();
    mapped = vb->mapVertexBuffer(LLVertexBuffer::TYPE_TEXTURE_INDEX, 3, -1);
    REQUIRE(mapped);
    const U32 last = 7; std::memcpy(mapped, &last, sizeof(last));
    vb->unmapBuffer();
    CHECK(vb->hasPublishedMetalVertexBuffer());
    CHECK(LLVertexBuffer::metalUploadStats().uploaded_bytes == before.uploaded_bytes + sizeof(last));
    bytes = f.bytes(*identity);
    CHECK(std::memcmp(bytes.data() + 60, &last, sizeof(last)) == 0);
}

TEST_CASE("Native CPU leases prevent completed generation reuse through update resize and owner destruction",
          "[almetal][native-generation-leases][retirement]")
{
    Fixture f;
    auto vb = f.buffer();
    auto lease = vb->getMetalVertexBufferLease(); REQUIRE(lease);
    const auto original = f.bytes(*lease);
    std::weak_ptr<const ALMetalBuffer> weak = lease;
    for (unsigned generation = 0; generation < 5; ++generation)
    {
        gGL.frameFences().release(); // frame completion alone is insufficient
        vb->setColorData(&green, generation % 4, 1); vb->unmapBuffer();
        CHECK(vb->getMetalVertexBuffer() != lease.get());
        CHECK(f.bytes(*lease) == original);
    }
    REQUIRE(vb->allocateBuffer(8, 12));
    vb = nullptr;
    gGL.frameFences().release(); LLVertexBuffer::flushBuffers();
    CHECK_FALSE(weak.expired());
    CHECK(f.bytes(*lease) == original);
    lease.reset(); LLVertexBuffer::flushBuffers();
    CHECK(weak.expired());
}

TEST_CASE("Native command pins protect unencoded and submitted readers beyond logical frame completion",
          "[almetal][native-generation-leases][command-pin]")
{
    Fixture f;
    auto vb = f.buffer();
    auto lease = vb->getMetalVertexBufferLease(); REQUIRE(lease);
    const auto original = f.bytes(*lease);
    ALMetalBufferDescriptor descriptor;
    descriptor.length = original.size(); descriptor.storage = ALMetalStorageMode::shared;
    auto output = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(output);
    auto command = f.device->makeCommandBuffer("Lease test delayed CPU packet", &f.error); REQUIRE(command);
    REQUIRE(command->retainUntilCompletion(lease, &f.error));
    REQUIRE(output->copyFrom(*command, *lease, 0, 0, original.size(), &f.error));
    auto* held = lease.get(); lease.reset();
    gGL.frameFences().release();
    vb->setColorData(&green, 0, 1); vb->unmapBuffer();
    CHECK(vb->getMetalVertexBuffer() != held);
    // Old bytes must survive CPU update before this command is even committed.
    REQUIRE(command->commitAndWait(&f.error));
    CHECK(f.bytes(*output) == original);

    auto abandoned_lease = vb->getMetalVertexBufferLease();
    std::weak_ptr<const ALMetalBuffer> abandoned = abandoned_lease;
    auto abandoned_command = f.device->makeCommandBuffer("Lease test abandoned packet", &f.error);
    REQUIRE(abandoned_command);
    REQUIRE(abandoned_command->retainUntilCompletion(abandoned_lease, &f.error));
    abandoned_lease.reset(); vb = nullptr;
    gGL.frameFences().release(); LLVertexBuffer::flushBuffers();
    CHECK_FALSE(abandoned.expired());
    abandoned_command.reset(); LLVertexBuffer::flushBuffers();
    CHECK(abandoned.expired());
}

TEST_CASE("Native texture leases retain old pixels through partial writes pending publication and destruction",
          "[almetal][native-generation-leases][texture-cow]")
{
    Fixture f;
    std::vector<U8> red_pixels(4 * 4 * 4);
    for (std::size_t pixel = 0; pixel < red_pixels.size(); pixel += 4)
        std::memcpy(red_pixels.data() + pixel, &red, 4);
    LLPointer<LLImageGL> image = new LLImageGL(4, 4, 4, false);
    image->setNeedsAlphaAndPickMask(false);
    image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, red_pixels.data()));
    CHECK_FALSE(image->hasMetalTextureLeases());
    auto lease = image->getMetalTextureLease(); REQUIRE(lease);
    CHECK(image->hasMetalTextureLeases());
    const U64 old_stamp = image->getContentStamp();
    auto updated = red_pixels;
    std::memcpy(updated.data() + (1 * 4 + 2) * 4, &green, 4);
    REQUIRE(image->uploadMetalSubImage(updated.data(), 4, 4, 2, 1, 1, 1));
    CHECK(image->getContentStamp() != old_stamp);
    CHECK(image->getMetalTexture() != lease.get());
    CHECK(f.pixels(*lease) == red_pixels);
    CHECK(f.pixels(*image->getMetalTextureLease()) == updated);

    auto published = image->getMetalTextureLease();
    image->beginUpload();
    REQUIRE(image->createMetalTexture(0, red_pixels.data()));
    CHECK(image->getMetalTextureLease() == published);
    image->cancelUpload();
    CHECK(image->getMetalTextureLease() == published);
    CHECK(f.pixels(*published) == updated);
    image = nullptr;
    CHECK(f.pixels(*lease) == red_pixels);
    CHECK(f.pixels(*published) == updated);
}

TEST_CASE("Native texture generation leases retain every old mip during replacement",
          "[almetal][native-generation-leases][texture-mips]")
{
    Fixture f;
    std::vector<U8> pixels(8 * 8 * 4);
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
        std::memcpy(pixels.data() + pixel, &red, 4);
    LLPointer<LLImageGL> image = new LLImageGL(8, 8, 4, true);
    image->setNeedsAlphaAndPickMask(false);
    image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, pixels.data()));
    auto old = image->getMetalTextureLease(); REQUIRE(old);
    REQUIRE(old->descriptor().mipmap_level_count > 1);
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
        std::memcpy(pixels.data() + pixel, &blue, 4);
    REQUIRE(image->createMetalTexture(0, pixels.data()));
    auto current = image->getMetalTextureLease(); REQUIRE(current);
    CHECK(old != current);
    for (U32 mip = 0; mip < old->descriptor().mipmap_level_count; ++mip)
    {
        CAPTURE(mip);
        std::vector<U8> old_pixels(old->width(mip) * old->height(mip) * 4);
        std::vector<U8> new_pixels(old_pixels.size());
        REQUIRE(old->readback(*f.device, mip, 0, {}, old_pixels.data(), old->width(mip) * 4, &f.error));
        REQUIRE(current->readback(*f.device, mip, 0, {}, new_pixels.data(), current->width(mip) * 4, &f.error));
        for (std::size_t pixel = 0; pixel < old_pixels.size(); pixel += 4)
        {
            CHECK(std::memcmp(old_pixels.data() + pixel, &red, 4) == 0);
            CHECK(std::memcmp(new_pixels.data() + pixel, &blue, 4) == 0);
        }
    }
}

TEST_CASE("Native publication counters expose dirty traffic versus full-copy leased generations",
          "[almetal][native-generation-leases][publication-byte-cost]")
{
    Fixture f;
    auto vb = f.buffer(4096, 0);
    gGL.frameFences().release();
    const std::array<LLColor4U, 16> changed{green, blue, green, blue, green, blue, green, blue,
                                         green, blue, green, blue, green, blue, green, blue};
    auto before = LLVertexBuffer::metalUploadStats();
    for (unsigned repetition = 0; repetition < 32; ++repetition)
    {
        vb->setColorData(changed.data(), repetition * 16, changed.size());
        vb->unmapBuffer();
    }
    auto after = LLVertexBuffer::metalUploadStats();
    const U64 ranged_bytes = after.uploaded_bytes - before.uploaded_bytes;
    CHECK(ranged_bytes == 32 * sizeof(changed));
    CHECK(after.full_uploads == before.full_uploads);
    CHECK(after.partial_uploads == before.partial_uploads + 32);
    before = after;
    for (unsigned repetition = 0; repetition < 32; ++repetition)
    {
        auto lease = vb->getMetalVertexBufferLease(); REQUIRE(lease);
        gGL.frameFences().release();
        vb->setColorData(changed.data(), repetition * 16, changed.size());
        vb->unmapBuffer();
    }
    after = LLVertexBuffer::metalUploadStats();
    const U64 leased_bytes = after.uploaded_bytes - before.uploaded_bytes;
    CHECK(leased_bytes == 32 * vb->getSize());
    CHECK(after.full_uploads == before.full_uploads + 32);
    CHECK(after.leased_orphans == before.leased_orphans + 32);
    std::cout << "NATIVE_GENERATION_BYTE_COST updates_per_arm=32 vertices=4096 dirty_bytes="
              << ranged_bytes << " leased_full_bytes=" << leased_bytes
              << " claim=publication_bytes_only_no_viewer_timing\n";
}
