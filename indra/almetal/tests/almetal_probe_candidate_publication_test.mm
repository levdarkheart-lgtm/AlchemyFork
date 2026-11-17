/**
 * @file almetal_probe_candidate_publication_test.mm
 * @brief Native candidate publication, readiness, admission and lifetime.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetalprobecandidates.h"
#include "../almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace
{
using Result = ALMetalProbeCandidatePreparation;
using Binding = ALMetalProbeCandidateBinding;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::shared_ptr<const ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalComputePipeline> pipeline;
    ALMetalProbeCandidatePacket packet;
    ALMetalWorldReflectionProbeConstants probes;
    std::array<float,16> projection{}, inverse{};
    Fixture()
    {
        INFO(error); REQUIRE(device);
        data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        library = data->builtinLibrary(&error); INFO(error); REQUIRE(library);
        REQUIRE(library->hasFunction("almetal_world_probe_candidate_tiles"));
        ALMetalComputePipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.function = "almetal_world_probe_candidate_tiles";
        descriptor.label = "Probe publication correctness kernel";
        pipeline = ALMetalComputePipeline::create(*device, descriptor, &error);
        INFO(error); REQUIRE(pipeline);
        std::printf("probe_candidate_publication pipeline_validation=%ld\n",
            long([(__bridge id<MTLComputePipelineState>)pipeline->nativeHandle() shaderValidation]));
        data->beginFrame(1);
        packet.config[0] = 3; packet.config[1] = 65; packet.config[2] = 33; packet.config[3] = 1;
        packet.bounds[0] = {0,0,65,33};
        packet.bounds[1] = {0,0,16,16};
        packet.bounds[2] = {0,0,65,33};
        probes.config[0] = 3.f;
        probes.ref_index[2][3] = 1; // manual: always present in every tile
        for (unsigned i : {0u,5u,10u,15u}) projection[i] = inverse[i] = 1.f;
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    auto command()
    {
        auto result = device->makeCommandBuffer("Probe publication correctness command", &error);
        INFO(error); REQUIRE(result); return result;
    }
    Result prepare(ALMetalCommandBuffer& command, Binding& binding)
    {
        return data->prepareProbeCandidates(command, pipeline.get(), library,
            data->builtinLibraryEpoch(), packet, probes, projection, inverse, false, binding, &error);
    }
    void key(unsigned value) { probes.ref_params[1][0] = float(value + 1); }
    void resize(std::uint32_t width, std::uint32_t height)
    {
        packet.config[1] = width; packet.config[2] = height;
        packet.bounds[0] = packet.bounds[2] = {0,0,width,height};
    }
    std::unique_ptr<ALMetalBuffer> copyMasks(ALMetalCommandBuffer& command, const Binding& binding)
    {
        REQUIRE(binding.masks);
        ALMetalBufferDescriptor descriptor;
        descriptor.length = binding.masks->length();
        descriptor.storage = ALMetalStorageMode::shared;
        auto readback = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(readback);
        id<MTLBlitCommandEncoder> blit = [(__bridge id<MTLCommandBuffer>)command.nativeHandle() blitCommandEncoder];
        REQUIRE(blit);
        [blit copyFromBuffer:(__bridge id<MTLBuffer>)binding.masks->nativeHandle() sourceOffset:0
            toBuffer:(__bridge id<MTLBuffer>)readback->nativeHandle() destinationOffset:0 size:descriptor.length];
        [blit endEncoding];
        return readback;
    }
};

template<class Predicate> bool eventually(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do { if (predicate()) return true; std::this_thread::yield(); }
    while (std::chrono::steady_clock::now() < deadline);
    return predicate();
}
}

TEST_CASE("Probe publication declines before native work for missing or unusable inputs",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto command = f.command();
        const auto buffers = alMetalResourceAllocationStats().buffers_created;
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        Binding binding;
        CHECK(f.data->prepareProbeCandidates(*command, nullptr, f.library,
            f.data->builtinLibraryEpoch(), f.packet, f.probes, f.projection, f.inverse,
            false, binding, &f.error) == Result::unavailable);
        CHECK_FALSE(binding.masks); CHECK_FALSE(binding.owner); CHECK_FALSE(binding.native_prefix);
        auto original = f.packet;
        f.packet.config[3] = 0;
        CHECK(f.prepare(*command,binding) == Result::unavailable);
        f.packet = original; f.packet.config[0] = 257;
        CHECK(f.prepare(*command,binding) == Result::unavailable);
        f.packet = original; f.packet.bounds[0].x_min = 1;
        CHECK(f.prepare(*command,binding) == Result::unavailable);
        f.packet = original; f.packet.bounds[1] = f.packet.bounds[0];
        CHECK(f.prepare(*command,binding) == Result::unavailable); // all full
        f.packet = original; f.packet.bounds[2] = {0,0,1,1};
        CHECK(f.prepare(*command,binding) == Result::unavailable); // manual cannot be bounded
        f.packet = original; f.resize(65536,65536);
        CHECK(f.prepare(*command,binding) == Result::unavailable); // per-record byte cap
        CHECK_FALSE(binding.masks); CHECK_FALSE(binding.owner); CHECK_FALSE(binding.native_prefix);
        CHECK(f.error.empty());
        CHECK(f.data->stats().candidate_preparations == 0);
        CHECK(f.data->stats().candidate_failures == 0);
        CHECK(alMetalResourceAllocationStats().buffers_created == buffers);
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
    }
}

TEST_CASE("Probe publication reuses one command and retains output through completion",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto command = f.command();
        std::weak_ptr<const void> credit;
        unsigned admissions = 0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork& work)
            ->std::shared_ptr<const void>
        {
            ++admissions; CHECK(work.allocation_bytes > 0);
            CHECK(work.copied_page_bytes > 0); CHECK(work.encoded_mapping_bytes == 480);
            auto owner = std::make_shared<int>(42); credit = owner; return owner;
        });
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        Binding first;
        REQUIRE(f.prepare(*command,first) == Result::ready);
        REQUIRE(first.masks); REQUIRE(first.owner); CHECK(first.native_prefix);
        CHECK((first.grid == std::array<std::uint32_t,4>{5,3,65,33}));
        CHECK(first.masks->length() == 480);
        CHECK_FALSE(credit.expired());
        const auto buffers = alMetalResourceAllocationStats().buffers_created;
        Binding second;
        REQUIRE(f.prepare(*command,second) == Result::ready);
        CHECK(second.masks == first.masks); CHECK(second.owner == first.owner); CHECK(second.native_prefix);
        CHECK(admissions == 1);
        CHECK(f.data->stats().candidate_preparations == 1);
        CHECK(f.data->stats().candidate_reuses == 1);
        CHECK(alMetalResourceAllocationStats().buffers_created == buffers);
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        auto readback = f.copyMasks(*command, first);
        std::weak_ptr<const void> publication = first.owner;
        first = {}; second = {}; f.data.reset();
        CHECK_FALSE(publication.expired()); // only the command now owns publication/credit
        CHECK_FALSE(credit.expired());
        REQUIRE(command->commitAndWait(&f.error));
        CHECK(command->completionToken()->status() == ALMetalCommandStatus::completed);
        std::array<std::uint32_t,120> words{};
        REQUIRE(readback->read(0, words.data(), sizeof(words), &f.error));
        for (unsigned tile = 0; tile < 15; ++tile)
            for (unsigned word = 0; word < 8; ++word)
                CHECK(words[tile*8+word] == (word ? 0u : (tile ? 5u : 7u)));
        command.reset();
        CHECK(eventually([&] { return publication.expired() && credit.expired(); }));
    }
}

TEST_CASE("Probe publication rejects another pending producer and reuses completed output",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f;
        auto first_command = f.command(); Binding first;
        REQUIRE(f.prepare(*first_command,first) == Result::ready);
        auto pending_command = f.command(); Binding pending;
        REQUIRE(f.prepare(*pending_command,pending) == Result::ready);
        CHECK(pending.owner != first.owner); CHECK(pending.masks != first.masks);
        CHECK(pending.native_prefix);
        CHECK(f.data->stats().candidate_preparations == 2);
        CHECK(f.data->stats().candidate_reuses == 0);
        REQUIRE(first_command->commitAndWait(&f.error));
        pending_command.reset(); // abandon the duplicate producer, retaining its binding
        f.data->beginFrame(2);
        auto consumer = f.command(); Binding completed;
        const auto allocations = alMetalResourceAllocationStats().buffers_created;
        REQUIRE(f.prepare(*consumer,completed) == Result::ready);
        CHECK(completed.owner == first.owner); CHECK(completed.masks == first.masks);
        CHECK_FALSE(completed.native_prefix);
        CHECK(f.data->stats().candidate_preparations == 2);
        CHECK(f.data->stats().candidate_reuses == 1);
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        REQUIRE(consumer->commitAndWait(&f.error));
    }
}

TEST_CASE("Abandoned probe publication is never certified by generic completion",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto abandoned = f.command(); Binding old;
        REQUIRE(f.prepare(*abandoned,old) == Result::ready);
        const auto token = abandoned->completionToken();
        abandoned.reset();
        CHECK(token->isComplete());
        CHECK(token->status() == ALMetalCommandStatus::unknown);
        auto replacement = f.command(); Binding fresh;
        REQUIRE(f.prepare(*replacement,fresh) == Result::ready);
        CHECK(fresh.owner != old.owner); CHECK(fresh.masks != old.masks);
        CHECK(fresh.native_prefix);
        CHECK(f.data->stats().candidate_preparations == 2);
        CHECK(f.data->stats().candidate_reuses == 0);
        REQUIRE(replacement->commitAndWait(&f.error));
    }
}

TEST_CASE("Probe publication denial preserves generic work and completed hits need no admission",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto producer = f.command(); Binding original;
        REQUIRE(f.prepare(*producer,original) == Result::ready);
        REQUIRE(producer->commitAndWait(&f.error));
        unsigned admissions = 0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
            ->std::shared_ptr<const void> { ++admissions; return {}; });
        auto consumer = f.command(); Binding hit;
        const auto allocations = alMetalResourceAllocationStats().buffers_created;
        REQUIRE(f.prepare(*consumer,hit) == Result::ready);
        CHECK(hit.owner == original.owner); CHECK_FALSE(hit.native_prefix);
        CHECK(admissions == 0);
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        f.key(1); Binding denied;
        CHECK(f.prepare(*consumer,denied) == Result::unavailable);
        CHECK_FALSE(denied.masks); CHECK_FALSE(denied.owner); CHECK_FALSE(denied.native_prefix);
        CHECK(admissions == 1); CHECK(f.error.empty());
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        CHECK(f.data->stats().candidate_preparations == 1);
        CHECK(f.data->stats().candidate_reuses == 1);
        CHECK(f.data->stats().candidate_failures == 0);
        REQUIRE(consumer->commitAndWait(&f.error));
    }
}

TEST_CASE("Reentrant probe publication makes outer denial fatal after a native prefix",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto command = f.command();
        auto nested_packet = f.packet;
        nested_packet.bounds[1] = {16,0,32,16}; // valid, distinct immutable key
        auto nested_probes = f.probes;
        nested_probes.ref_params[1][0] = 99.f;
        Binding nested;
        Result nested_result = Result::unavailable;
        bool inside_callback = false;
        unsigned admissions = 0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
            ->std::shared_ptr<const void>
        {
            ++admissions;
            if (inside_callback) return std::make_shared<int>(1);
            inside_callback = true;
            nested_result = f.data->prepareProbeCandidates(*command, f.pipeline.get(), f.library,
                f.data->builtinLibraryEpoch(), nested_packet, nested_probes,
                f.projection, f.inverse, false, nested, &f.error);
            inside_callback = false;
            return {}; // Outer admission denied AFTER nested encoding on the same command.
        });
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        Binding outer;
        const auto result = f.prepare(*command,outer);
        REQUIRE(nested_result == Result::ready);
        REQUIRE(nested.masks); REQUIRE(nested.owner); CHECK(nested.native_prefix);
        CHECK(admissions == 2);
        CHECK(result == Result::failed);
        CHECK(outer.native_prefix); // Generic replay could duplicate the existing native prefix.
        CHECK_FALSE(command->isCommitted());
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        CHECK(f.data->stats().candidate_preparations == 1);
        CHECK(f.data->stats().candidate_failures == 1);
        const auto token = command->completionToken();
        command.reset(); // This negative case never executes the GPU prefix.
        CHECK(token->status() == ALMetalCommandStatus::unknown);
        nested = {};
        f.data->beginFrame(2);
    }
}

TEST_CASE("Probe publication pressure preserves an already known command prefix without failure",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto command = f.command(); Binding original;
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(f.prepare(*command,original) == Result::ready);
        REQUIRE(original.masks); REQUIRE(original.owner); CHECK(original.native_prefix);
        const auto allocations = alMetalResourceAllocationStats().buffers_created;
        unsigned admissions = 0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
            ->std::shared_ptr<const void> { ++admissions; return {}; });
        f.key(1); // A distinct mask request after the caller's existing prefix.
        Binding denied;
        CHECK(f.prepare(*command,denied) == Result::unavailable);
        CHECK(denied.native_prefix); // Preserve the prefix; ordinary same-command generic work remains valid.
        CHECK_FALSE(denied.masks); CHECK_FALSE(denied.owner);
        CHECK(admissions == 1);
        CHECK(f.data->stats().candidate_preparations == 1);
        CHECK(f.data->stats().candidate_failures == 0);
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        CHECK_FALSE(command->isCommitted());
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        const auto token = command->completionToken();
        command.reset();
        CHECK(token->status() == ALMetalCommandStatus::unknown);
    }
}

TEST_CASE("Probe publication resident bounds include cache-evicted command and binding owners",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f;
        SECTION("32 resident records remain charged through abandonment and external bindings")
        {
            auto producer = f.command();
            std::vector<Binding> held(32);
            for (unsigned i = 0; i < held.size(); ++i)
            {
                f.key(i);
                REQUIRE(f.prepare(*producer,held[i]) == Result::ready);
                REQUIRE(held[i].owner); REQUIRE(held[i].native_prefix);
                if (i) CHECK(held[i].owner != held[i-1].owner);
            }
            CHECK(f.data->stats().candidate_preparations == 32);
            const auto allocations = alMetalResourceAllocationStats().buffers_created;
            f.key(32); Binding denied;
            CHECK(f.prepare(*producer,denied) == Result::unavailable);
            CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
            // The failed room request can evict every idle-cache entry, but
            // the original command and external bindings still own 32 records.
            held[0] = {};
            CHECK(f.prepare(*producer,denied) == Result::unavailable);
            CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
            producer.reset();
            // One record now expires; the other31 remain externally retained.
            auto next = f.command(); Binding admitted;
            REQUIRE(f.prepare(*next,admitted) == Result::ready);
            CHECK(f.data->stats().candidate_preparations == 33);
            CHECK(alMetalResourceAllocationStats().buffers_created == allocations + 2);
            f.key(33);
            CHECK(f.prepare(*next,denied) == Result::unavailable);
            CHECK(alMetalResourceAllocationStats().buffers_created == allocations + 2);
            REQUIRE(next->commitAndWait(&f.error));
        }
        SECTION("resident bytes cannot be hidden by evicting externally held large records")
        {
            f.resize(8192,8192); // 8MiB mask per record plus packet/key bookkeeping
            auto producer = f.command(); std::array<Binding,3> held;
            for (unsigned i = 0; i < held.size(); ++i)
            {
                f.key(i); REQUIRE(f.prepare(*producer,held[i]) == Result::ready);
                REQUIRE(held[i].masks); CHECK(held[i].masks->length() == 8u*1024u*1024u);
            }
            const auto allocations = alMetalResourceAllocationStats().buffers_created;
            f.key(3); Binding denied;
            CHECK(f.prepare(*producer,denied) == Result::unavailable);
            CHECK(denied.native_prefix); // Earlier preparations on this same command remain intact.
            CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
            CHECK(f.data->stats().candidate_preparations == 3);
            producer.reset(); // No GPU dispatch execution needed to prove resident accounting.
        }
    }
}
