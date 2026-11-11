/**
 * @file almetal_buffer_storage_test.cpp
 * @brief Typed native buffer storage, publication, accounting and GPU leases.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "almetaldevice.h"
#include "almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
constexpr U32 words_count=64;
std::vector<U32> pattern(U32 base)
{
    std::vector<U32> values(words_count);
    for (U32 i=0;i<words_count;++i) values[i]=base+i*3u;
    return values;
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    Fixture()
    {
        device=ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    }
    std::unique_ptr<ALMetalBuffer> buffer(ALMetalStorageMode storage,
                                          const std::vector<U32>* initial=nullptr,
                                          ALMetalCpuCacheMode cache=ALMetalCpuCacheMode::default_cache)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length=words_count*sizeof(U32);
        descriptor.storage=storage;
        descriptor.cpu_cache=cache;
        descriptor.label="Native typed storage invariant";
        auto result=ALMetalBuffer::create(*device,descriptor,&error); REQUIRE(result);
        CHECK(result->nativeHandle());
        CHECK(result->length()==descriptor.length);
        CHECK(result->storageMode()==storage);
        if (initial)
        {
            auto command=device->makeCommandBuffer("Native typed initial upload",&error); REQUIRE(command);
            REQUIRE(result->upload(*command,0,initial->data(),initial->size()*sizeof(U32),&error));
            REQUIRE(command->commitAndWait(&error));
        }
        return result;
    }
    std::vector<U32> read(const ALMetalBuffer& buffer)
    {
        std::vector<U32> result(buffer.length()/sizeof(U32),0xffffffffu);
        REQUIRE(buffer.readback(*device,0,result.data(),result.size()*sizeof(U32),&error));
        return result;
    }
};
}

TEST_CASE("Native buffer storage preserves private uploads partial fills and wrapper accounting",
          "[almetal][native-buffer-storage]")
{
    Fixture fixture;
    const auto baseline=alMetalResourceAllocationStats().buffer_bytes_live;
    {
        const auto initial=pattern(11);
        auto buffer=fixture.buffer(ALMetalStorageMode::private_gpu,&initial);
        CHECK(buffer->mappedBytes()==nullptr);
        CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline+buffer->length());
        CHECK(fixture.read(*buffer)==initial);
        const std::vector<U32> replacement(8,0x12345678u);
        auto upload=fixture.device->makeCommandBuffer("Native typed partial upload",&fixture.error); REQUIRE(upload);
        REQUIRE(buffer->upload(*upload,4*sizeof(U32),replacement.data(),replacement.size()*sizeof(U32),&fixture.error));
        REQUIRE(upload->commitAndWait(&fixture.error));
        auto expected=initial;
        std::copy(replacement.begin(),replacement.end(),expected.begin()+4);
        CHECK(fixture.read(*buffer)==expected);
        auto fill=fixture.device->makeCommandBuffer("Native typed partial word fill",&fixture.error); REQUIRE(fill);
        REQUIRE(buffer->fillWords(*fill,20*sizeof(U32),12*sizeof(U32),0xaabbccddu,&fixture.error));
        REQUIRE(fill->commitAndWait(&fixture.error));
        std::fill(expected.begin()+20,expected.begin()+32,0xaabbccddu);
        CHECK(fixture.read(*buffer)==expected);
        std::vector<U32> partial(4);
        REQUIRE(buffer->readback(*fixture.device,23*sizeof(U32),partial.data(),partial.size()*sizeof(U32),&fixture.error));
        CHECK(partial==std::vector<U32>(expected.begin()+23,expected.begin()+27));
        auto clear=fixture.device->makeCommandBuffer("Native typed whole nonzero fill",&fixture.error); REQUIRE(clear);
        REQUIRE(buffer->fillWords(*clear,0,buffer->length(),7,&fixture.error));
        REQUIRE(clear->commitAndWait(&fixture.error));
        CHECK(fixture.read(*buffer)==std::vector<U32>(words_count,7));
    }
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline);
}

TEST_CASE("Native buffer storage copies static and disjoint self ranges without altering its source",
          "[almetal][native-buffer-storage]")
{
    Fixture fixture;
    const auto baseline=alMetalResourceAllocationStats().buffer_bytes_live;
    const auto initial=pattern(100);
    const std::vector<U32> zeroes(words_count,0);
    auto source=fixture.buffer(ALMetalStorageMode::private_gpu,&initial);
    auto target=fixture.buffer(ALMetalStorageMode::private_gpu,&zeroes);
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline+source->length()+target->length());
    auto copy=fixture.device->makeCommandBuffer("Native typed static and self copies",&fixture.error); REQUIRE(copy);
    REQUIRE(target->copyFrom(*copy,*source,4*sizeof(U32),32*sizeof(U32),10*sizeof(U32),&fixture.error));
    REQUIRE(target->copyFrom(*copy,*target,32*sizeof(U32),0,10*sizeof(U32),&fixture.error));
    REQUIRE(copy->commitAndWait(&fixture.error));
    auto expected=zeroes;
    std::copy(initial.begin()+4,initial.begin()+14,expected.begin()+32);
    std::copy(expected.begin()+32,expected.begin()+42,expected.begin());
    CHECK(fixture.read(*target)==expected);
    CHECK(fixture.read(*source)==initial);
    auto clear=fixture.device->makeCommandBuffer("Native typed static nonzero fill",&fixture.error); REQUIRE(clear);
    REQUIRE(target->fillWords(*clear,0,target->length(),0xa5a5a5a5u,&fixture.error));
    REQUIRE(clear->commitAndWait(&fixture.error));
    CHECK(fixture.read(*target)==std::vector<U32>(words_count,0xa5a5a5a5u));
    CHECK(fixture.read(*source)==initial);
    source.reset(); target.reset();
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline);
}

TEST_CASE("Native buffer storage publishes coherent mapped ranges and retains encoded sources after owner retirement",
          "[almetal][native-buffer-storage]")
{
    Fixture fixture;
    const auto baseline=alMetalResourceAllocationStats().buffer_bytes_live;
    const auto initial=pattern(900);
    auto shared=fixture.buffer(ALMetalStorageMode::shared,&initial,ALMetalCpuCacheMode::write_combined);
    auto gpu=fixture.buffer(ALMetalStorageMode::private_gpu);
    REQUIRE(shared->mappedBytes());
    auto first=fixture.device->makeCommandBuffer("Native coherent shared-to-private copy",&fixture.error); REQUIRE(first);
    REQUIRE(gpu->copyFrom(*first,*shared,0,0,shared->length(),&fixture.error));
    REQUIRE(first->commitAndWait(&fixture.error));
    CHECK(fixture.read(*gpu)==initial);
    const std::vector<U32> replacement(8,9000);
    std::memcpy(static_cast<U8*>(shared->mappedBytes())+16*sizeof(U32),replacement.data(),replacement.size()*sizeof(U32));
    auto expected=initial;
    std::copy(replacement.begin(),replacement.end(),expected.begin()+16);
    // Apple shared storage is coherent. The CPU write precedes submission;
    // no obsolete flush flag or synthetic publication API is needed.
    auto second=fixture.device->makeCommandBuffer("Native mapped partial publication",&fixture.error); REQUIRE(second);
    REQUIRE(gpu->copyFrom(*second,*shared,0,0,shared->length(),&fixture.error));
    REQUIRE(second->commitAndWait(&fixture.error));
    CHECK(fixture.read(*gpu)==expected);
    auto readback=fixture.buffer(ALMetalStorageMode::shared);
    REQUIRE(readback->mappedBytes());
    auto result=fixture.device->makeCommandBuffer("Native coherent GPU-to-CPU copy",&fixture.error); REQUIRE(result);
    REQUIRE(readback->copyFrom(*result,*gpu,0,0,gpu->length(),&fixture.error));
    REQUIRE(result->commitAndWait(&fixture.error));
    std::vector<U32> actual(words_count);
    REQUIRE(readback->read(0,actual.data(),readback->length(),&fixture.error));
    CHECK(actual==expected);

    first.reset(); second.reset(); result.reset();
    // A fresh source has no preceding encoded users that could mask a missing
    // lease on this still-unsubmitted command.
    const auto retired_expected=pattern(7000);
    auto retiring=fixture.buffer(ALMetalStorageMode::shared,&retired_expected);
    const std::vector<U32> poison(words_count,0);
    REQUIRE(readback->write(0,poison.data(),readback->length(),&fixture.error));
    auto pending=fixture.device->makeCommandBuffer("Native source and device-owner retirement",&fixture.error); REQUIRE(pending);
    REQUIRE(readback->copyFrom(*pending,*retiring,0,0,retiring->length(),&fixture.error));
    CHECK_FALSE(pending->isCommitted());
    shared.reset(); gpu.reset(); retiring.reset();
    // This is live typed-wrapper byte accounting, not native allocation or
    // residency: the encoded command still retains its source Metal buffer.
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline+readback->length());
    fixture.device.reset();
    REQUIRE(pending->commitAndWait(&fixture.error));
    CHECK(pending->status()==ALMetalCommandStatus::completed);
    REQUIRE(readback->read(0,actual.data(),readback->length(),&fixture.error));
    CHECK(actual==retired_expected);
    readback.reset();
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live==baseline);
}
