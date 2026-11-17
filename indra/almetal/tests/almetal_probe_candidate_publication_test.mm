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
#include <cstring>
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
    std::uint64_t library_epoch = 0;
    bool reverse_z = false;
    Fixture()
    {
        INFO(error); REQUIRE(device);
        data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        library = data->builtinLibrary(&error); INFO(error); REQUIRE(library);
        library_epoch = data->builtinLibraryEpoch();
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
            library_epoch, packet, probes, projection, inverse, reverse_z, binding, &error);
    }
    void key(unsigned value)
    {
        REQUIRE(value < 34);
        // Distinct actual kernel inputs, including the 33rd/34th resident
        // requests. SH scale is not a mask dependency and must permit reuse.
        packet.bounds[1] = {value,0,value+16,16};
    }
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

TEST_CASE("Completed probe masks reuse exact derived inputs across current lighting and view changes",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto producer = f.command(); Binding original;
        REQUIRE(f.prepare(*producer,original) == Result::ready);
        auto original_readback = f.copyMasks(*producer,original);
        REQUIRE(producer->commitAndWait(&f.error));
        const auto packet = f.packet;
        // The publication API receives a caller-certified packet. Its GPU
        // output must not depend on the old view/lighting metadata retained
        // by an earlier producer. Receiver certification is tested separately.
        f.probes.ref_params[1][0] = .75f; // SH scale
        f.probes.ref_params[1][1] = .625f; // radiance scale
        f.probes.ref_params[1][2] = .5f; // fade/distance weight
        f.probes.ref_params[1][3] = 2.f; // ordered-walk depth boundary
        f.probes.ref_index[1][0] = 1; // current radiance/SH layer
        f.probes.ref_index[1][1] = 0; f.probes.ref_index[1][2] = 1;
        f.probes.ref_neighbor[0] = 2;
        for (auto& bucket : f.probes.ref_bucket) bucket[0] = 1;
        f.probes.env_matrix[0][0] = 0.f; f.probes.env_matrix[0][1] = 1.f;
        f.probes.env_matrix[1][0] = -1.f; f.probes.env_matrix[1][1] = 0.f;
        f.probes.ref_index[2][3] = -1; // a manual box remains full-view
        for (unsigned i : {0u,5u,10u,15u}) f.probes.ref_box[2][i] = 1.f;
        f.projection[0] = 2.f; f.inverse[0] = .5f;
        f.reverse_z = true;
        CHECK(std::memcmp(&packet,&f.packet,sizeof(packet)) == 0);
        unsigned admissions = 0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
            ->std::shared_ptr<const void> { ++admissions; return {}; });
        auto consumer = f.command(); Binding reused;
        const auto allocations = alMetalResourceAllocationStats().buffers_created;
        REQUIRE(f.prepare(*consumer,reused) == Result::ready);
        CHECK(reused.owner == original.owner); CHECK(reused.masks == original.masks);
        CHECK_FALSE(reused.native_prefix); CHECK(admissions == 0);
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        CHECK(f.data->stats().candidate_preparations == 1);
        CHECK(f.data->stats().candidate_reuses == 1);
        // A matching cache key cannot bypass current input validation.
        f.probes.ref_index[1][3] = 1; Binding rejected;
        CHECK(f.prepare(*consumer,rejected) == Result::unavailable);
        CHECK_FALSE(rejected.owner); CHECK_FALSE(rejected.native_prefix);
        f.probes.ref_index[1][3] = 0;
        f.probes.config[0] = 4.f;
        CHECK(f.prepare(*consumer,rejected) == Result::unavailable);
        CHECK_FALSE(rejected.owner); CHECK_FALSE(rejected.native_prefix);
        f.probes.config[0] = 3.f; f.packet.config[3] = 0;
        CHECK(f.prepare(*consumer,rejected) == Result::unavailable);
        CHECK_FALSE(rejected.owner); CHECK_FALSE(rejected.native_prefix);
        f.packet = packet;
        CHECK(admissions == 0);
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations);
        auto reused_readback = f.copyMasks(*consumer,reused);
        REQUIRE(consumer->commitAndWait(&f.error));
        std::array<std::uint32_t,120> before{}, after{};
        REQUIRE(original_readback->read(0,before.data(),sizeof(before),&f.error));
        REQUIRE(reused_readback->read(0,after.data(),sizeof(after),&f.error));
        CHECK(after == before);
        for (unsigned tile = 0; tile < 15; ++tile)
            for (unsigned word = 0; word < 8; ++word)
                CHECK(after[tile*8+word] == (word ? 0u : (tile ? 5u : 7u)));
        CHECK(f.data->stats().candidate_failures == 0);
    }
}

TEST_CASE("Probe mask publications remain isolated by actual library identity and epoch",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        Fixture f; auto producer = f.command(); Binding original;
        REQUIRE(f.prepare(*producer,original) == Result::ready);
        REQUIRE(producer->commitAndWait(&f.error));
        const auto old_library = f.library;
        const auto old_epoch = f.library_epoch;
        auto old_pipeline = std::move(f.pipeline);
        SECTION("same native library with a new publication epoch")
        {
            ++f.library_epoch;
            f.pipeline = std::move(old_pipeline);
        }
        SECTION("distinct actual bundled library with the same epoch")
        {
            f.library = ALMetalShaderLibrary::loadBuiltin(*f.device,&f.error);
            INFO(f.error); REQUIRE(f.library);
            REQUIRE(f.library->nativeHandle() != old_library->nativeHandle());
            ALMetalComputePipelineDescriptor descriptor;
            descriptor.library = f.library.get();
            descriptor.function = "almetal_world_probe_candidate_tiles";
            descriptor.label = "Probe publication replacement library";
            f.pipeline = ALMetalComputePipeline::create(*f.device,descriptor,&f.error);
            INFO(f.error); REQUIRE(f.pipeline);
        }
        auto command = f.command(); Binding replacement;
        REQUIRE(f.prepare(*command,replacement) == Result::ready);
        CHECK(replacement.owner != original.owner); CHECK(replacement.masks != original.masks);
        CHECK(replacement.native_prefix);
        auto readback = f.copyMasks(*command,replacement);
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::uint32_t,120> words{};
        REQUIRE(readback->read(0,words.data(),sizeof(words),&f.error));
        for (unsigned tile = 0; tile < 15; ++tile)
            for (unsigned word = 0; word < 8; ++word)
                CHECK(words[tile*8+word] == (word ? 0u : (tile ? 5u : 7u)));
        CHECK(f.data->stats().candidate_preparations == 2);
        CHECK(f.data->stats().candidate_reuses == 0);
        // Returning to an earlier epoch/library must not resurrect an idle
        // cache entry invalidated by the coherent library switch.
        if (old_pipeline) f.pipeline = std::move(old_pipeline);
        f.library = old_library; f.library_epoch = old_epoch;
        auto returned_command = f.command(); Binding returned;
        REQUIRE(f.prepare(*returned_command,returned) == Result::ready);
        CHECK(returned.owner != original.owner); CHECK(returned.owner != replacement.owner);
        CHECK(returned.native_prefix);
        CHECK(f.data->stats().candidate_preparations == 3);
        CHECK(f.data->stats().candidate_reuses == 0);
        REQUIRE(returned_command->commitAndWait(&f.error));
    }
}

TEST_CASE("Probe publication rechecks full borrowed input snapshots after miss admission",
          "[almetal][probe-candidate-publication]")
{
    @autoreleasepool
    {
        for (unsigned mutation = 0; mutation < 4; ++mutation)
            for (bool nested_prefix : {false,true})
            {
                CAPTURE(mutation,nested_prefix);
                Fixture f; auto command = f.command();
                auto nested_packet = f.packet; nested_packet.bounds[1] = {16,0,32,16};
                const auto nested_probes = f.probes;
                const auto nested_projection = f.projection, nested_inverse = f.inverse;
                Binding nested; Result nested_result = Result::unavailable;
                bool inside_callback = false;
                unsigned admissions = 0;
                f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
                    ->std::shared_ptr<const void>
                {
                    ++admissions;
                    if (inside_callback) return std::make_shared<int>(1);
                    inside_callback = true;
                    if (nested_prefix)
                        nested_result = f.data->prepareProbeCandidates(*command,f.pipeline.get(),f.library,
                            f.library_epoch,nested_packet,nested_probes,nested_projection,nested_inverse,
                            false,nested,&f.error);
                    // Even a non-mask lighting field must remain guarded for
                    // this in-progress request; the persistent key is separate.
                    if (mutation == 0) f.packet.bounds[1].x_max = 15;
                    if (mutation == 1) f.probes.ref_params[1][0] = .75f;
                    if (mutation == 2) f.projection[0] = 2.f;
                    if (mutation == 3) f.inverse[0] = .5f;
                    inside_callback = false;
                    return std::make_shared<int>(2);
                });
                const auto allocations = alMetalResourceAllocationStats().buffers_created;
                const auto commits = ALMetalDevice::committedCommandBufferCount();
                Binding outer;
                CHECK(f.prepare(*command,outer) == (nested_prefix ? Result::failed : Result::unavailable));
                CHECK_FALSE(outer.masks); CHECK_FALSE(outer.owner);
                CHECK(outer.native_prefix == nested_prefix);
                CHECK(admissions == (nested_prefix ? 2u : 1u));
                CHECK(f.data->stats().candidate_preparations == (nested_prefix ? 1u : 0u));
                CHECK(f.data->stats().candidate_failures == (nested_prefix ? 1u : 0u));
                CHECK(alMetalResourceAllocationStats().buffers_created == allocations+(nested_prefix ? 2u : 0u));
                CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
                CHECK_FALSE(command->isCommitted());
                if (nested_prefix)
                {
                    REQUIRE(nested_result == Result::ready);
                    REQUIRE(nested.owner); REQUIRE(nested.masks); CHECK(nested.native_prefix);
                }
                const auto token = command->completionToken();
                command.reset(); // Never execute these intentionally invalidated requests.
                CHECK(token->status() == ALMetalCommandStatus::unknown);
                nested = {}; f.data->beginFrame(2);
            }
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
