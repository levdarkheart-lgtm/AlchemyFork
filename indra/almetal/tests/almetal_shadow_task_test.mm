/**
 * @file almetal_shadow_task_test.mm
 * @brief Production shadow command adoption, native ownership and complete cost.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowtask.h"
#include "../almetalshadowprepare.h"
#include "llmath.h"
#include "llmatrix4a.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace
{
using Batch = ALMetalWorldSpotShadowBatch;
using Record = ALMetalShadowSourceRecord;
using Packet = ALMetalCapturedShadowView;
using Completion = std::shared_ptr<const ALMetalCommandCompletion>;
std::uint64_t tick()
{ return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count(); }
void settle(const Completion& completion)
{
    REQUIRE(completion);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!completion->isComplete() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    REQUIRE(completion->isComplete());
}
// A native GPU dependency makes uncompleted-generation assertions deterministic.
// This gate is used only by correctness tests, never by the cost frontier.
struct GPUGate
{
    id<MTLSharedEvent> event;
    explicit GPUGate(ALMetalDevice& device)
    {
        event = [(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent]; REQUIRE(event);
        auto command = [(__bridge id<MTLCommandQueue>)device.nativeCommandQueue() commandBuffer];
        [command encodeWaitForEvent:event value:1]; [command commit];
    }
    void release() { event.signaledValue = 1; }
    ~GPUGate() { release(); }
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    std::shared_ptr<const ALMetalTexture> texture;
    std::array<std::shared_ptr<const ALMetalAttachmentPaletteRecord>, 3> poses;
    static constexpr std::size_t weight_offset = 48, uv_offset = 96, color_offset = 120, body_offset = 132;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error); INFO(error); REQUIRE(renderer);
        data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        renderer->setFrameData(data); data->beginFrame(1);
        const std::array<std::array<float, 4>, 3> positions{{
            {{-.75f,-.75f,0.f,1.f}}, {{.75f,-.75f,0.f,1.f}}, {{0.f,.75f,0.f,1.f}}}};
        const std::array<std::array<float, 4>, 3> weights{{
            {{.5f,0,0,0}}, {{.5f,0,0,0}}, {{.5f,0,0,0}}}};
        const std::array<std::array<float, 2>, 3> uv{{{{.25f,.5f}},{{.25f,.5f}},{{.25f,.5f}}}};
        std::array<std::uint8_t, 144> bytes{};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data() + weight_offset, weights.data(), sizeof(weights));
        std::memcpy(bytes.data() + uv_offset, uv.data(), sizeof(uv));
        std::fill_n(bytes.data() + color_offset, 12, std::uint8_t{255});
        ALMetalBufferDescriptor bd; bd.length = bytes.size();
        auto vb = ALMetalBuffer::create(*device, bd, &error); REQUIRE(vb);
        REQUIRE(vb->write(0, bytes.data(), bytes.size(), &error)); vertices = std::move(vb);
        const std::array<std::uint16_t, 3> elements{0,1,2}; bd.length = sizeof(elements);
        auto ib = ALMetalBuffer::create(*device, bd, &error); REQUIRE(ib);
        REQUIRE(ib->write(0, elements.data(), sizeof(elements), &error)); indices = std::move(ib);
        ALMetalTextureDescriptor td;
        auto image = ALMetalTexture::create(*device, td, &error); REQUIRE(image);
        auto upload = device->makeCommandBuffer("Shadow task frozen texture", &error); REQUIRE(upload);
        const std::array<std::uint8_t, 4> white{255,255,255,255};
        REQUIRE(image->upload(*upload, 0, 0, {}, white.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error)); texture = std::move(image);
        for (unsigned view = 0; view < poses.size(); ++view)
        {
            std::array<float, 16> values{}; values[4] = values[9] = values[14] = 1.f;
            values[15] = float(view) / 32.f;
            poses[view] = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), 1, &error);
            REQUIRE(poses[view]);
        }
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::shared_ptr<ALMetalTexture> target(unsigned slices = 3)
    {
        ALMetalTextureDescriptor d; d.width = d.height = 32; d.array_length = slices;
        d.type = ALMetalTextureType::texture_2d_array; d.format = ALMetalPixelFormat::r32_float;
        d.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> result = ALMetalTexture::create(*device, d, &error);
        INFO(error); REQUIRE(result); return result;
    }
    Batch prototype(unsigned family = 0) const
    {
        Batch b; b.vertices = vertices.get(); b.indices = indices.get();
        b.vertex_count = b.index_count = 3; b.double_sided = true;
        std::memcpy(b.modelview_projection, alMetalShadowIdentity.data(), 64);
        if (family)
        {
            b.coverage = family == 1 ? ALMetalWorldSpotShadowCoverage::texture_mask : ALMetalWorldSpotShadowCoverage::pbr_mask;
            b.texcoord_offset = uv_offset; b.color_offset = color_offset; b.texture_count = 1;
            b.coverageData().textures[0] = texture.get(); b.coverageData().alpha_cutoffs[0] = .5f;
            b.coverageData().pbr_base_color_transforms[0][0][0] = 1.f;
            b.coverageData().pbr_base_color_transforms[0][0][1] = 1.f;
        }
        if (family == 2)
        {
            b.weight_offset = weight_offset; b.skin_palette_record = poses[0];
            b.skin_palette = poses[0]->data(); b.skin_palette_float_count = poses[0]->size(); b.skin_joint_count = 1;
        }
        return b;
    }
    std::unique_ptr<ALMetalShadowViewTask> task()
    {
        auto context = renderer->createShadowEncodingContext(&error); REQUIRE(context);
        return std::make_unique<ALMetalShadowViewTask>(std::move(context));
    }
    std::vector<float> read(ALMetalTexture& t, unsigned slice)
    {
        std::vector<float> pixels(32 * 32);
        REQUIRE(t.readback(*device, 0, slice, {}, pixels.data(), 32 * sizeof(float), &error)); return pixels;
    }
};
struct Owners
{
    std::vector<std::shared_ptr<const Record>> records;
    std::unordered_set<const Record*> identities;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
};
struct Capture
{
    Packet::Batches batches;
    Packet::Queries queries;
    Packet::BodyPalettes bodies;
    std::shared_ptr<const void> owners;
    std::size_t sources = 0;
    ALMetalWorldSpotShadowScene scene() const
    {
        ALMetalWorldSpotShadowScene s; s.width = s.height = 32;
        s.batches = batches.data(); s.batch_count = batches.size(); s.source_owner = owners;
        s.visibility_queries = queries.data(); s.visibility_query_count = queries.size();
        s.source_batch_count = sources; s.opaque_indirect_draws = true; return s;
    }
    std::unique_ptr<Packet> take(std::string& error)
    {
        auto result = Packet::take(scene(), batches, bodies, queries, &error);
        if (!result) throw std::runtime_error(error);
        owners.reset(); return result;
    }
};
// The timed producer is the retained-source, current scalar construction:
// acquire exact record/pose owners, copy scalar prototype, evaluate model*view
// then *projection with production LLMatrix4a, copy current UV and admit in order.
// This is not live LL traversal, asset publication or animation/pose solving.
struct Plan
{
    Fixture& f;
    std::vector<std::shared_ptr<const Record>> records;
    std::vector<LLMatrix4a> models;
    std::vector<std::array<float, 16>> uv;
    std::array<LLMatrix4a, 3> views;
    LLMatrix4a projection{alMetalShadowIdentity.data()};
    explicit Plan(Fixture& fixture, std::size_t count) : f(fixture)
    {
        for (unsigned v = 0; v < 3; ++v)
        { auto matrix = alMetalShadowIdentity; matrix[12] = float(v) / 64.f; views[v] = LLMatrix4a(matrix.data()); }
        for (std::size_t i = 0; i < count; ++i)
        {
            auto model = alMetalShadowIdentity, texture = alMetalShadowIdentity;
            model[12] = float(i % 13) / 256.f; model[14] = float(i % 5) / 256.f;
            texture[12] = float(i % 7) / 128.f;
            models.emplace_back(model.data()); uv.push_back(texture);
            auto b = f.prototype(std::min<std::size_t>(i * 3 / count, 2));
            std::vector<std::shared_ptr<const ALMetalTexture>> images;
            if (b.texture_count) images.push_back(f.texture);
            auto record = Record::create(i + 1, b, f.vertices, f.indices, std::move(images), &f.error);
            INFO(f.error); REQUIRE(record); records.push_back(std::move(record));
        }
    }
    void produce(Capture& capture, unsigned view, bool query) const
    {
        capture.batches.clear(); capture.queries.clear(); capture.bodies.clear();
        auto owner = std::make_shared<Owners>(); owner->pose = f.poses[view];
        ALMetalShadowCaptureAdmission admission;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            // Fixed family/query barriers; no aggregate small-family route.
            if (i == 0 || i * 3 / records.size() != (i - 1) * 3 / records.size()) admission.barrier();
            if (query && (i == 0 || i == records.size() / 2))
            {
                admission.barrier();
                ALMetalWorldVisibilityQuery q; q.batch_offset = capture.batches.size(); q.count_samples = 1;
                std::memcpy(q.modelview_projection, alMetalShadowIdentity.data(), 64);
                q.center[2] = .5f; q.half_extent[0] = q.half_extent[1] = q.half_extent[2] = .125f;
                capture.queries.push_back(q);
            }
            const auto& source = records[i];
            if (owner->identities.insert(source.get()).second) owner->records.push_back(source);
            const auto& prototype = source->prototype();
            Batch b; static_cast<ALMetalWorldSpotShadowValues&>(b) = prototype;
            const auto* pose = source->requiresPalette() ? owner->pose.get() : nullptr;
            b.borrowPreparedInputs(nullptr, pose, prototype.coverageDataIdentity());
            LLMatrix4a modelview, mvp; modelview.setMul(models[i], views[view]); mvp.setMul(modelview, projection);
            std::memcpy(b.modelview_projection, mvp.getF32ptr(), 64);
            if (b.coverage != ALMetalWorldSpotShadowCoverage::opaque)
                std::memcpy(b.texture_matrix, uv[i].data(), 64);
            if (pose) { b.skin_palette = pose->data(); b.skin_palette_float_count = pose->size(); b.skin_joint_count = pose->jointCount(); }
            if (!pose && b.coverage == ALMetalWorldSpotShadowCoverage::opaque)
            {
                ALMetalOpaqueShadowSource input;
                input.vertices = b.vertices; input.indices = b.indices; input.vertex_count = b.vertex_count;
                input.index_count = b.index_count; input.double_sided = b.double_sided;
                input.modelview_projection = b.modelview_projection;
                if (!admission.appendOpaque(capture.batches, input)) throw std::runtime_error("Opaque admission failed");
            }
            else { if (!admission.admitSeparate()) throw std::runtime_error("Separate admission failed");
                   capture.batches.push_back(std::move(b)); }
        }
        if (!admission.complete(records.size())) throw std::runtime_error("Capture reconciliation failed");
        capture.sources = records.size(); capture.owners = std::move(owner);
    }
};
std::vector<std::uint64_t> queryValues(const std::shared_ptr<ALMetalVisibilityResultBatch>& results)
{
    if (!results) return {};
    std::vector<std::uint64_t> values(results->resultCount());
    for (std::size_t i = 0; i < values.size(); ++i) REQUIRE(results->wait(i, values[i]));
    return values;
}
}

TEST_CASE("almetal: production shadow task adopts one command while the owner captures the next view",
          "[almetal][shadow-view-task]")
{
    @autoreleasepool
    {
        Fixture f; Plan plan(f, 1158); auto target = f.target(); auto expected_target = f.target();
        Capture capture, next; plan.produce(capture, 0, true);
        const auto serial_scene = capture.scene();
        std::shared_ptr<ALMetalVisibilityResultBatch> serial_queries;
        REQUIRE(f.renderer->renderSpotShadow(*expected_target, 0, serial_scene, &f.error, &serial_queries));
        const auto expected = f.read(*expected_target, 0); const auto expected_queries = queryValues(serial_queries);
        REQUIRE(expected_queries.size() == 2); REQUIRE(expected_queries[0] > 0); CHECK(expected_queries[1] == 0);
        auto task = f.task(); unsigned committed = 0; const auto owner_thread = std::this_thread::get_id();
        auto submission = std::make_shared<const int>(17); std::weak_ptr<const int> weak_submission = submission;
        { ALMetalSubmissionScope scope(submission);
          REQUIRE(f.renderer->beginShadowFrameBatch(100, "Original adopted view", &f.error)); }
        submission.reset();
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool value)
        { CHECK(std::this_thread::get_id() == owner_thread); CHECK(value); ++committed; }, &f.error));
        auto original = f.renderer->commandBatchCompletion();
        auto packet = capture.take(f.error); const auto* old_storage = packet->scene().batches;
        ALNativeFrameBudget budget; ALNativeFrameBudget::FrameInputs inputs; inputs.frame = 100; budget.beginFrame(inputs);
        GPUGate gate(*f.device);
        const auto before = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(task->start(*f.renderer, 100, packet, target, 0, &budget, &f.error));
        REQUIRE_FALSE(packet); CHECK(task->active()); CHECK_FALSE(f.renderer->commandBatchActive());
        CHECK(f.renderer->shadowEncodingActive()); CHECK(committed == 0);
        CHECK_FALSE(f.renderer->reloadShaders(&f.error)); // Detached work is still an active owner transaction.
        REQUIRE(f.renderer->beginShadowFrameBatch(100, "Next real producer transaction", &f.error));
        const auto producer_start = tick(); plan.produce(next, 1, true); const auto producer_end = tick();
        REQUIRE(next.batches.data() != old_storage); CHECK(next.queries[1].batch_offset == plan.records.size() / 2);
        auto outcome = task->finish(true, &f.error); INFO(f.error);
        REQUIRE(outcome.encoded); REQUIRE(outcome.submitted); REQUIRE(outcome.packet);
        CHECK_FALSE(outcome.worker_is_owner); CHECK(outcome.completion == original);
        CHECK(ALMetalDevice::committedCommandBufferCount() == before + 1);
        CHECK(committed == 1); CHECK(f.renderer->commandBatchActive()); CHECK_FALSE(f.renderer->shadowEncodingActive());
        CHECK(budget.snapshot().preparation_in_flight == 1);
        CHECK(budget.snapshot().preparation_bytes_in_flight == outcome.retained_cpu_bytes);
        CHECK_FALSE(original->isComplete());
        CHECK_FALSE(weak_submission.expired());
        CHECK(outcome.packet->scene().visibility_queries[1].batch_offset == plan.records.size() / 2);
        REQUIRE(outcome.packet->recycleBatches(capture.batches)); REQUIRE(outcome.packet->recycleQueries(capture.queries));
        CHECK(budget.snapshot().preparation_in_flight == 0);
        CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
        CHECK(capture.batches.data() == old_storage); outcome.packet.reset();
        const auto overlap = std::min(producer_end, outcome.encode_finished_ns) > std::max(producer_start, outcome.encode_started_ns)
            ? std::min(producer_end, outcome.encode_finished_ns) - std::max(producer_start, outcome.encode_started_ns) : 0;
        std::cout << "shadow_task_overlap_proof producer_ns=" << producer_end - producer_start
                  << " encode_ns=" << outcome.encode_ns << " intersection_ns=" << overlap << '\n';
        // Reuse this same private context while its first native command is
        // still pending: the original same-group six-view authority applies.
        packet = next.take(f.error);
        REQUIRE(task->start(*f.renderer, 100, packet, target, 1, &budget, &f.error));
        auto second = task->finish(true, &f.error); REQUIRE(second.submitted);
        CHECK(ALMetalDevice::committedCommandBufferCount() == before + 2);
        second.packet.reset(); gate.release(); settle(original); settle(second.completion);
        const auto retirement_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!weak_submission.expired() && std::chrono::steady_clock::now() < retirement_deadline) std::this_thread::yield();
        CHECK(weak_submission.expired()); CHECK(f.read(*target, 0) == expected);
        CHECK(queryValues(outcome.visibility_results) == expected_queries);
        auto next_reference = next.scene(); // Its CPU arrays were moved, so restore a fresh real producer.
        plan.produce(capture, 1, true); next_reference = capture.scene();
        REQUIRE(f.renderer->renderSpotShadow(*expected_target, 1, next_reference, &f.error));
        CHECK(f.read(*target, 1) == f.read(*expected_target, 1));
        CHECK_FALSE(task->finish().submitted);
    }
}

TEST_CASE("almetal: completed shadow packet keeps CPU debt through moves and retires before GPU completion",
          "[almetal][shadow-view-task]")
{
    @autoreleasepool
    {
        Fixture f; Plan plan(f, 867); Capture first, next;
        auto first_task = f.task(), next_task = f.task(); auto target = f.target();
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Retained completed CPU packet", &f.error));
        plan.produce(first, 0, true);
        const auto first_query_bytes = first.queries.capacity() * sizeof(ALMetalWorldVisibilityQuery);
        auto packet = first.take(f.error);
        const auto packet_capacity = packet->ownedCPUBytes();
        ALNativeFrameBudget::Limits limits; limits.preparation_jobs = 2;
        limits.preparation_bytes_in_flight = packet_capacity * 2;
        ALNativeFrameBudget budget(limits); budget.beginFrame(ALNativeFrameBudget::FrameInputs{});
        const auto original = f.renderer->commandBatchCompletion();
        GPUGate gate(*f.device);
        const auto committed = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(first_task->start(*f.renderer, 1, packet, target, 0, &budget, &f.error));
        // Observe the Dispatch completion fence, never mutable worker fields.
        // The owner has not called finish, submitted, or recovered the packet.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!first_task->encodingComplete() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        REQUIRE(first_task->encodingComplete()); CHECK(first_task->active());
        CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        const auto retained_bytes = budget.snapshot().preparation_bytes_in_flight;
        REQUIRE(retained_bytes > packet_capacity); CHECK(budget.snapshot().preparation_in_flight == 1);
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Next capture sees retained debt", &f.error));
        plan.produce(next, 1, true);
        const auto next_batch_bytes = next.batches.capacity() * sizeof(Batch);
        auto next_packet = next.take(f.error);
        const auto next_command = f.renderer->commandBatchCompletion();
        auto* next_identity = next_packet.get();
        CHECK_FALSE(next_task->start(*f.renderer, 1, next_packet, target, 1, &budget, &f.error));
        CHECK(next_task->lastDecline() == ALMetalShadowViewTask::Decline::budget);
        CHECK(next_packet.get() == next_identity); CHECK(f.renderer->commandBatchCompletion() == next_command);
        CHECK(budget.snapshot().preparation_bytes_in_flight == retained_bytes);
        f.renderer->cancelCommandBatch();
        auto outcome = first_task->finish(true, &f.error); REQUIRE(outcome.submitted);
        CHECK(outcome.retained_cpu_bytes == retained_bytes);
        auto held_packet = std::move(outcome.packet);
        outcome = {}; first_task.reset(); // Neither move nor task death releases CPU storage debt.
        CHECK(budget.snapshot().preparation_bytes_in_flight == retained_bytes);
        CHECK_FALSE(original->isComplete()); // GPU remains blocked independently.
        Packet::Batches batches; Packet::Queries queries;
        batches.reserve(4096); queries.reserve(64); // Empty may still own substantial capacity.
        REQUIRE(held_packet->recycleBatches(batches));
        CHECK(held_packet->ownedCPUBytes() == sizeof(Packet) + first_query_bytes);
        CHECK(budget.snapshot().preparation_bytes_in_flight == retained_bytes); // Queries still retained.
        REQUIRE(held_packet->recycleQueries(queries));
        CHECK(held_packet->ownedCPUBytes() == sizeof(Packet));
        CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
        CHECK(budget.snapshot().preparation_in_flight == 0);
        CHECK_FALSE(original->isComplete()); // CPU recycle never waits on GPU retirement.
        held_packet.reset(); CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
        // The preserved candidate can now admit. Reverse recycling order and
        // reserved-empty destinations must not leave hidden packet capacity.
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Capacity after CPU recycle", &f.error));
        REQUIRE(next_task->start(*f.renderer, 1, next_packet, target, 1, &budget, &f.error));
        auto canceled = next_task->finish(false, &f.error); REQUIRE(canceled.packet);
        CHECK(budget.snapshot().preparation_bytes_in_flight == canceled.retained_cpu_bytes);
        CHECK_FALSE(canceled.submitted);
        Packet::Queries reverse_queries; reverse_queries.reserve(64);
        Packet::Batches reverse_batches; reverse_batches.reserve(4096);
        REQUIRE(canceled.packet->recycleQueries(reverse_queries));
        CHECK(canceled.packet->ownedCPUBytes() == sizeof(Packet) + next_batch_bytes);
        CHECK(budget.snapshot().preparation_bytes_in_flight == canceled.retained_cpu_bytes);
        REQUIRE(canceled.packet->recycleBatches(reverse_batches));
        CHECK(canceled.packet->ownedCPUBytes() == sizeof(Packet));
        canceled.packet.reset();
        CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
        CHECK(budget.snapshot().preparation_in_flight == 0);
        for (const bool restore : {true, false})
        {
            Capture capture; plan.produce(capture, 2, true); auto candidate = capture.take(f.error);
            REQUIRE(f.renderer->beginShadowFrameBatch(1, "CPU restore or packet destruction", &f.error));
            REQUIRE(next_task->start(*f.renderer, 1, candidate, target, 2, &budget, &f.error));
            auto discarded = next_task->finish(false, &f.error); REQUIRE(discarded.packet);
            CHECK(budget.snapshot().preparation_bytes_in_flight == discarded.retained_cpu_bytes);
            if (restore)
            {
                const auto frozen = discarded.packet->scene();
                Packet::Batches restored_batches; restored_batches.reserve(4096);
                Packet::Queries restored_queries; restored_queries.reserve(64);
                REQUIRE(discarded.packet->restoreCPUStorage(restored_batches, restored_queries));
                CHECK(discarded.packet->ownedCPUBytes() == sizeof(Packet));
                CHECK(restored_batches.data() == frozen.batches);
                CHECK(restored_queries.data() == frozen.visibility_queries);
                CHECK(restored_batches.size() == frozen.batch_count);
                CHECK(restored_queries.size() == frozen.visibility_query_count);
                CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
            }
            discarded.packet.reset(); // Destruction without recycle also retires once.
            CHECK(budget.snapshot().preparation_bytes_in_flight == 0);
            CHECK(budget.snapshot().preparation_in_flight == 0);
        }
        gate.release(); settle(original);
    }
}

TEST_CASE("almetal: shadow task denial preserves serial fallback and failed prefixes never submit",
          "[almetal][shadow-view-task]")
{
    @autoreleasepool
    {
        Fixture f; Plan plan(f, 24); auto target = f.target(); auto task = f.task(); Capture c;
        plan.produce(c, 0, true);
        std::shared_ptr<ALMetalVisibilityResultBatch> baseline_results;
        REQUIRE(f.renderer->renderSpotShadow(*target, 0, c.scene(), &f.error, &baseline_results));
        const auto baseline = f.read(*target, 0);
        const auto baseline_queries = queryValues(baseline_results);
        REQUIRE(baseline_queries.size() == 2);
        ALNativeFrameBudget::Limits limits; limits.preparation_jobs = 1;
        ALNativeFrameBudget budget(limits); ALNativeFrameBudget::FrameInputs inputs; inputs.frame = 1; budget.beginFrame(inputs);
        auto held = budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit; REQUIRE(held);
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Budget denial keeps original", &f.error));
        auto original = f.renderer->commandBatchCompletion(); auto packet = c.take(f.error);
        CHECK_FALSE(task->start(*f.renderer, 1, packet, target, 1, &budget, &f.error));
        CHECK(task->lastDecline() == ALMetalShadowViewTask::Decline::budget); REQUIRE(packet);
        CHECK(f.renderer->commandBatchCompletion() == original); CHECK(f.renderer->commandBatchActive());
        const auto fallback = packet->scene(); REQUIRE(packet->restoreCPUStorage(c.batches, c.queries));
        REQUIRE(f.renderer->renderSpotShadow(*target, 1, fallback, &f.error));
        REQUIRE(f.renderer->commitCommandBatch(&f.error)); settle(original);
        CHECK(f.read(*target, 1) == baseline); held.reset(); packet.reset();

        // Packet capacity alone exceeds this real shared byte limit. Denial
        // must preserve the original native command and frozen query order so
        // the owner can execute the unchanged ordinary path immediately.
        {
            plan.produce(c, 0, true); packet = c.take(f.error);
            const auto packet_bytes = packet->ownedCPUBytes(); REQUIRE(packet_bytes > 0);
            const auto* original_packet = packet.get(); const auto frozen = packet->scene();
            ALNativeFrameBudget::Limits byte_limits;
            byte_limits.preparation_bytes_in_flight = packet_bytes - 1;
            ALNativeFrameBudget byte_budget(byte_limits); byte_budget.beginFrame(inputs);
            REQUIRE(f.renderer->beginShadowFrameBatch(1, "Byte denial keeps original query command", &f.error));
            original = f.renderer->commandBatchCompletion();
            unsigned byte_settlements = 0;
            REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool submitted)
                { CHECK(submitted); ++byte_settlements; }, &f.error));
            CHECK_FALSE(task->start(*f.renderer, 1, packet, target, 2, &byte_budget, &f.error));
            CHECK(task->lastDecline() == ALMetalShadowViewTask::Decline::budget);
            REQUIRE(packet); CHECK(packet.get() == original_packet);
            CHECK(packet->scene().batches == frozen.batches);
            CHECK(packet->scene().visibility_queries == frozen.visibility_queries);
            CHECK(f.renderer->commandBatchActive()); CHECK(f.renderer->commandBatchCompletion() == original);
            CHECK(byte_budget.snapshot().preparation_in_flight == 0);
            CHECK(byte_budget.snapshot().preparation_bytes_in_flight == 0);
            const auto byte_fallback = packet->scene(); REQUIRE(packet->restoreCPUStorage(c.batches, c.queries));
            std::shared_ptr<ALMetalVisibilityResultBatch> fallback_results;
            REQUIRE(f.renderer->renderSpotShadow(*target, 2, byte_fallback, &f.error, &fallback_results));
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); settle(original);
            CHECK(byte_settlements == 1); CHECK(f.read(*target, 2) == baseline);
            CHECK(queryValues(fallback_results) == baseline_queries);
            CHECK(byte_budget.snapshot().preparation_bytes_in_flight == 0); packet.reset();
        }

        // Borrowed ordinary command and stale executable snapshots decline
        // without stealing the command or CPU storage from serial encoding.
        for (const bool stale : {false, true})
        {
            plan.produce(c, 0, false); packet = c.take(f.error);
            if (stale) { REQUIRE(f.renderer->setAnisotropy(4, &f.error));
                         REQUIRE(f.renderer->beginShadowFrameBatch(2, "Stale context", &f.error)); }
            else REQUIRE(f.renderer->beginCommandBatch("Borrowed ordinary owner", &f.error));
            original = f.renderer->commandBatchCompletion();
            CHECK_FALSE(task->start(*f.renderer, 2, packet, target, 1, nullptr, &f.error));
            CHECK(task->lastDecline() == ALMetalShadowViewTask::Decline::adoption);
            CHECK(packet); CHECK(f.renderer->commandBatchCompletion() == original);
            f.renderer->cancelCommandBatch(); packet.reset();
        }
        task = f.task();
        for (const bool invalid : {false, true})
        {
            plan.produce(c, 2, true);
            if (invalid) c.batches.back().vertices = nullptr;
            packet = c.take(f.error);
            REQUIRE(f.renderer->beginShadowFrameBatch(3, "Canceled or invalid prefix", &f.error));
            unsigned settled = 0;
            REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool submitted)
                { CHECK_FALSE(submitted); ++settled; }, &f.error));
            // A genuine native prefix was encoded into the SAME adopted
            // command. Neither a later invalid view nor finish(false) may submit it.
            auto prefix = f.prototype(); prefix.modelview_projection[14] = .5f;
            ALMetalWorldSpotShadowScene s; s.width = s.height = 32; s.batches = &prefix; s.batch_count = 1;
            REQUIRE(f.renderer->renderSpotShadow(*target, 0, s, &f.error));
            const auto before = ALMetalDevice::committedCommandBufferCount();
            REQUIRE(task->start(*f.renderer, 3, packet, target, 1, &budget, &f.error));
            auto outcome = task->finish(invalid, &f.error);
            CHECK(outcome.encoded == !invalid); CHECK_FALSE(outcome.submitted); CHECK_FALSE(outcome.visibility_results);
            CHECK(settled == 1); CHECK(outcome.completion->isComplete());
            CHECK(ALMetalDevice::committedCommandBufferCount() == before);
            CHECK(f.read(*target, 0) == baseline); CHECK_FALSE(f.renderer->shadowEncodingActive());
        }
        // Destruction joins late encoding and cancels through the original owner.
        plan.produce(c, 0, true); packet = c.take(f.error);
        REQUIRE(f.renderer->beginShadowFrameBatch(4, "Destructor cancels late worker", &f.error));
        unsigned canceled = 0;
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool submitted)
            { CHECK_FALSE(submitted); ++canceled; }, &f.error));
        REQUIRE(task->start(*f.renderer, 4, packet, target, 2, &budget, &f.error));
        task.reset(); CHECK(canceled == 1); CHECK_FALSE(f.renderer->shadowEncodingActive());
        CHECK(budget.snapshot().preparation_in_flight == 0);
    }
}

TEST_CASE("almetal: serial and adopted shadow views share original group and invocation admission",
          "[almetal][shadow-view-task]")
{
    @autoreleasepool
    {
        Fixture f; Plan plan(f, 3); auto target = f.target(); auto first = f.task(), second = f.task(); Capture c;
        // Two still-unsubmitted adopted groups are the original global bound.
        REQUIRE(f.renderer->beginShadowFrameBatch(10, "First unsubmitted group", &f.error));
        plan.produce(c, 0, false); auto packet = c.take(f.error);
        REQUIRE(first->start(*f.renderer, 10, packet, target, 0, nullptr, &f.error));
        REQUIRE(f.renderer->beginShadowFrameBatch(11, "Second unsubmitted group", &f.error));
        plan.produce(c, 1, false); packet = c.take(f.error);
        REQUIRE(second->start(*f.renderer, 11, packet, target, 1, nullptr, &f.error));
        CHECK_FALSE(f.renderer->beginShadowFrameBatch(12, "Third group cannot pass detached reservations", &f.error));
        CHECK_FALSE(first->finish(false, &f.error).submitted); CHECK_FALSE(second->finish(false, &f.error).submitted);
        unsigned settlements = 0;
        const auto before = ALMetalDevice::committedCommandBufferCount();
        for (unsigned view = 0; view < 6; ++view)
        {
            REQUIRE(f.renderer->beginShadowFrameBatch(12, "Shared six-view invocation", &f.error));
            REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool submitted)
                { CHECK(submitted); ++settlements; }, &f.error));
            plan.produce(c, view % 3, false);
            if (view & 1)
            {
                packet = c.take(f.error);
                REQUIRE(first->start(*f.renderer, 12, packet, target, view % 3, nullptr, &f.error));
                auto outcome = first->finish(true, &f.error); REQUIRE(outcome.submitted);
                REQUIRE(outcome.packet->recycleBatches(c.batches));
            }
            else
            {
                REQUIRE(f.renderer->renderSpotShadow(*target, view % 3, c.scene(), &f.error));
                REQUIRE(f.renderer->commitCommandBatch(&f.error));
            }
        }
        CHECK(settlements == 6); CHECK(ALMetalDevice::committedCommandBufferCount() == before + 6);
        CHECK_FALSE(f.renderer->beginShadowFrameBatch(12, "Seventh serial/private view rejected", &f.error));
        CHECK(f.error.find("six-invocation") != std::string::npos);
        REQUIRE(f.renderer->beginShadowFrameBatch(13, "Next group still works", &f.error)); f.renderer->cancelCommandBatch();
    }
}

TEST_CASE("almetal: shadow task separates CPU packet reuse from body buffer and target generation retirement",
          "[almetal][shadow-view-task]")
{
    @autoreleasepool
    {
        Fixture f; auto task = f.task(); auto old_target = f.target(); auto new_target = f.target();
        std::weak_ptr<ALMetalTexture> weak_target = old_target;
        std::weak_ptr<const ALMetalBuffer> weak_vertices = f.vertices;
        std::array<float, 180> values{};
        for (unsigned joint = 0; joint < 15; ++joint)
        { values[joint * 4] = 1; values[(15 + joint) * 4 + 1] = 1; values[(30 + joint) * 4 + 2] = 1;
          values[(30 + joint) * 4 + 3] = .25f; }
        auto body = ALMetalBodyPaletteRecord::create(values, &f.error); REQUIRE(body);
        std::weak_ptr<const ALMetalBodyPaletteRecord> weak_body = body;
        struct Buffers { std::shared_ptr<const ALMetalBuffer> vertices, indices; };
        Capture c; auto b = f.prototype(); b.avatar = true; b.texcoord_offset = Fixture::uv_offset;
        b.weight_offset = Fixture::body_offset; b.body_palette_record = body.get(); b.skin_palette = body->data();
        b.skin_palette_float_count = body->size(); b.skin_joint_count = 15;
        c.batches.push_back(b); c.bodies.push_back(body); c.sources = 1;
        c.owners = std::make_shared<const Buffers>(Buffers{f.vertices, f.indices});
        auto packet = c.take(f.error); const auto* old_storage = packet->scene().batches;
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Retain original body generation", &f.error));
        GPUGate gate(*f.device);
        REQUIRE(task->start(*f.renderer, 1, packet, old_target, 0, nullptr, &f.error));
        // Replace producer and target generations while the worker owns the old
        // exact packet. These are actual native resources, not sentinel leases.
        std::array<std::uint8_t, 144> bytes{}; REQUIRE(f.vertices->read(0, bytes.data(), bytes.size()));
        ALMetalBufferDescriptor bd; bd.length = bytes.size();
        auto replacement = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(replacement);
        REQUIRE(replacement->write(0, bytes.data(), bytes.size(), &f.error)); f.vertices = std::move(replacement);
        old_target.reset(); body.reset();
        auto outcome = task->finish(true, &f.error); INFO(f.error); REQUIRE(outcome.encoded); REQUIRE(outcome.submitted);
        REQUIRE(outcome.packet->recycleBatches(c.batches)); outcome.packet.reset();
        CHECK(c.batches.data() == old_storage); CHECK_FALSE(outcome.completion->isComplete());
        CHECK_FALSE(weak_body.expired()); CHECK_FALSE(weak_vertices.expired()); CHECK_FALSE(weak_target.expired());
        b = f.prototype(); b.modelview_projection[14] = -.5f; c.batches.push_back(b);
        c.owners = std::make_shared<const Buffers>(Buffers{f.vertices, f.indices});
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Reused CPU storage and new native generation", &f.error));
        packet = c.take(f.error); REQUIRE(task->start(*f.renderer, 1, packet, new_target, 0, nullptr, &f.error));
        auto next = task->finish(true, &f.error); REQUIRE(next.submitted); next.packet.reset();
        task.reset(); // Retire private arena identities; committed commands keep their own leases.
        CHECK_FALSE(weak_body.expired()); CHECK_FALSE(weak_target.expired());
        auto retired_generation = weak_target.lock(); REQUIRE(retired_generation);
        gate.release(); settle(outcome.completion); settle(next.completion); f.device->waitUntilIdle();
        CHECK(std::abs(f.read(*retired_generation, 0)[16 * 32 + 16] - .625f) < 1e-6f);
        retired_generation.reset();
        CHECK(weak_body.expired()); CHECK(weak_vertices.expired()); CHECK(weak_target.expired());
        CHECK(std::abs(f.read(*new_target, 0)[16 * 32 + 16] - .25f) < 1e-6f);
        // The old target was intentionally released; completion owns its lifetime,
        // while the new generation's pixels establish that scratch reuse is real.
    }
}

namespace
{
struct CostArm
{
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalShadowViewTask> task;
    std::shared_ptr<ALMetalTexture> target;
    Capture capture;
    Packet::Batches recycled_batches;
    Packet::Queries recycled_queries;
    explicit CostArm(Fixture& f)
    {
        renderer = ALMetalWorldGBufferRenderer::create(*f.device, 1, f.renderer->shaderLibrary(), &f.error);
        INFO(f.error); REQUIRE(renderer);
        data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(data); renderer->setFrameData(data);
        target = f.target();
    }
};
struct CostResult
{
    double cpu_ms = 0, settled_ms = 0, gpu_sum_ms = 0, gpu_envelope_ms = 0;
    std::uint64_t overlap_ns = 0, producer_ns = 0, worker_ns = 0, commits = 0;
    std::uint64_t context_creation_ns = 0;
    unsigned workers = 0, settlements = 0, eligible_views = 0, below_work_views = 0;
    std::array<std::size_t, 3> native_batches{};
    std::array<std::vector<float>, 3> pixels;
    std::array<std::vector<std::uint64_t>, 3> queries;
};
CostResult measure(Fixture& f, const Plan& plan, CostArm& arm,
    bool overlap, bool query, std::uint64_t frame)
{
    CostResult result;
    std::array<Completion, 3> completions;
    std::array<std::weak_ptr<const void>, 3> producer_owners;
    std::array<std::shared_ptr<ALMetalVisibilityResultBatch>, 3> queries;
    ALNativeFrameBudget budget; ALNativeFrameBudget::FrameInputs inputs; inputs.frame = frame; budget.beginFrame(inputs);
    const auto owner_thread = std::this_thread::get_id();
    bool callbacks_valid = true;
    struct CancelOnFailure
    {
        CostArm& arm;
        ~CancelOnFailure() { if (arm.task) arm.task->finish(false); arm.renderer->cancelCommandBatch(); }
    } cleanup{arm};
    const auto must = [](bool condition, const char* message)
    { if (!condition) throw std::runtime_error(message); };
    const auto before = ALMetalDevice::committedCommandBufferCount();
    const auto started = tick();
    bool pending = false;
    auto finish_previous = [&](unsigned view, std::uint64_t producer_start, std::uint64_t producer_end)
    {
        auto outcome = arm.task->finish(true, &f.error);
        must(outcome.encoded && outcome.submitted && outcome.packet, "Private view did not submit");
        must(outcome.completion == completions[view] && !outcome.worker_is_owner, "Command identity or worker handoff failed");
        ++result.workers; result.worker_ns += outcome.encode_ns;
        const auto first = std::max(producer_start, outcome.encode_started_ns);
        const auto last = std::min(producer_end, outcome.encode_finished_ns);
        if (last > first) result.overlap_ns += last - first;
        queries[view] = std::move(outcome.visibility_results);
        must(outcome.packet->recycleBatches(arm.recycled_batches), "Batch recycling would overwrite storage");
        must(outcome.packet->recycleQueries(arm.recycled_queries), "Query recycling would overwrite storage");
        outcome.packet.reset();
    };
    for (unsigned view = 0; view < 3; ++view)
    {
        // The owner starts this original admitted command BEFORE capture,
        // exactly as production does. Prior private work is still encoding.
        arm.data->beginFrame(frame);
        must(arm.renderer->beginShadowFrameBatch(frame, "Complete owned shadow view", &f.error), "Original admission failed");
        must(arm.renderer->observeCommandBatchSettlement([&](bool submitted)
            { callbacks_valid &= std::this_thread::get_id() == owner_thread && submitted; ++result.settlements; }, &f.error),
            "Owner observer registration failed");
        completions[view] = arm.renderer->commandBatchCompletion();
        const auto producer_start = tick(); plan.produce(arm.capture, view, query); const auto producer_end = tick();
        producer_owners[view] = arm.capture.owners;
        result.producer_ns += producer_end - producer_start;
        result.native_batches[view] = arm.capture.batches.size();
        if (pending)
        {
            finish_previous(view - 1, producer_start, producer_end);
            pending = false;
        }
        const bool eligible = overlap && alMetalShadowViewWorkEligibility(arm.capture.batches.size()) ==
            ALMetalShadowViewWorkEligibility::eligible;
        if (eligible)
        {
            ++result.eligible_views;
            if (!arm.task)
            {
                const auto context_start = tick();
                auto context = arm.renderer->createShadowEncodingContext(&f.error); must(bool(context), "Context creation failed");
                arm.task = std::make_unique<ALMetalShadowViewTask>(std::move(context));
                result.context_creation_ns = tick() - context_start;
            }
            auto packet = arm.capture.take(f.error);
            must(arm.task->start(*arm.renderer, frame, packet, arm.target, view, &budget, &f.error), "Task adoption declined");
            must(!packet && !arm.renderer->commandBatchActive(), "Adoption left duplicate command or packet ownership");
            arm.capture.batches.swap(arm.recycled_batches); arm.capture.queries.swap(arm.recycled_queries);
            pending = true;
        }
        else
        {
            result.below_work_views += overlap;
            // Policy fallback uses the ordinary serial branch before creating
            // any packet, private context or job, including cold mode1 samples.
            must(arm.renderer->renderSpotShadow(*arm.target, view, arm.capture.scene(), &f.error, &queries[view]), "Serial encoding failed");
            must(arm.renderer->commitCommandBatch(&f.error), "Serial commitment failed");
            arm.capture.batches.clear(); arm.capture.queries.clear(); arm.capture.owners.reset();
        }
    }
    if (pending) finish_previous(2, 0, 0);
    const auto encoded = tick();
    for (unsigned view = 0; view < 3; ++view)
    {
        must(bool(completions[view]), "Original command lost its receipt");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!completions[view]->isComplete() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        must(completions[view]->isComplete(), "Native shadow command did not settle");
        // Actual native result consumption and owner-side index mapping remain
        // inside complete settlement, after the exact original command commits.
        if (queries[view])
        {
            result.queries[view].resize(queries[view]->resultCount());
            for (std::size_t index = 0; index < result.queries[view].size(); ++index)
                must(queries[view]->wait(index, result.queries[view][index], &f.error), "Native query settlement failed");
        }
        queries[view].reset();
    }
    const auto retirement_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (const auto& owner : producer_owners)
    {
        while (!owner.expired() && std::chrono::steady_clock::now() < retirement_deadline) std::this_thread::yield();
        must(owner.expired(), "Native producer owners did not retire");
    }
    const auto settled = tick();
    result.cpu_ms = double(encoded - started) / 1e6; result.settled_ms = double(settled - started) / 1e6;
    result.commits = ALMetalDevice::committedCommandBufferCount() - before;
    REQUIRE(callbacks_valid);
    REQUIRE(result.commits == 3); REQUIRE(result.settlements == 3); REQUIRE(budget.snapshot().preparation_in_flight == 0);
    // GPU timing/readback bookkeeping is outside CPU/settlement intervals.
    // All three exact native intervals are retained, including queue gaps.
    double gpu_begin = 0, gpu_end = 0;
    for (unsigned view = 0; view < 3; ++view)
    {
        REQUIRE(result.queries[view].size() == (query ? 2u : 0u));
        ALMetalCommandGPUTime time;
        REQUIRE(completions[view]); REQUIRE(completions[view]->gpuTime(time) == ALMetalCommandTimingStatus::ready);
        result.gpu_sum_ms += double(time.elapsed_nanoseconds) / 1e6;
        if (view == 0) gpu_begin = time.start_seconds;
        gpu_end = std::max(gpu_end, time.end_seconds);
        result.pixels[view] = f.read(*arm.target, view);
    }
    result.gpu_envelope_ms = (gpu_end - gpu_begin) * 1000.;
    return result;
}
double median(std::vector<double> values)
{ std::sort(values.begin(), values.end()); return (values[values.size()/2 - 1] + values[values.size()/2]) * .5; }
}

TEST_CASE("almetal: complete current serial and production shadow overlap cost frontier",
          "[.][almetal][shadow-view-task-cost]")
{
    const auto enabled = [](const char* name)
    { const char* value = std::getenv(name); return value && value[0] && value[0] != '0'; };
    if (!enabled("ALCHEMY_SHADOW_VIEW_TASK_COST")) SKIP("Set ALCHEMY_SHADOW_VIEW_TASK_COST=1 for complete native cost");
#ifndef NDEBUG
    SKIP("Shadow task timing requires optimized source");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    @autoreleasepool
    {
        Fixture f;
        std::cout << "shadow_task_cost_scope includes=source_pose_acquire_scalar_MVP_UV_ordered_owner_admission_packet_dispatch_join_native_encode_original_admission_commit_query_settlement_GPU_retirement"
            " excludes=live_LL_traversal_culling_asset_publication_initial_retained_targets_pose_solve_LL_query_ticket_pool"
            " views=3 families=opaque_mask_riggedPBR frozen_record_per_draw=1 current_raw_scalar_comparator=1 mode1=shared_native_batch_policy minimum_native_batches=1024 retained_context_and_frame_arenas=1 FPS_claim=0\n";
        for (const std::size_t count : {32u, 256u, 867u, 1024u, 1158u, 4096u})
        for (const bool query : {false, true})
        {
            Plan plan(f, count); std::array<std::unique_ptr<CostArm>, 2> arms;
            for (auto& arm : arms) arm = std::make_unique<CostArm>(f);
            std::array<std::vector<double>, 2> cpu, settled, gpu;
            constexpr unsigned warmups = 2, measured = 8;
            for (unsigned sample = 0; sample < warmups + measured; ++sample)
            {
                std::array<CostResult, 2> results;
                for (unsigned order = 0; order < 2; ++order)
                {
                    const unsigned mode = order ^ (sample & 1u);
                    results[mode] = measure(f, plan, *arms[mode], mode != 0, query, sample + 100);
                    const auto& value = results[mode];
                    if (sample >= warmups)
                    { cpu[mode].push_back(value.cpu_ms); settled[mode].push_back(value.settled_ms); gpu[mode].push_back(value.gpu_envelope_ms); }
                    std::cout << "shadow_task_cost draws=" << count << " query=" << query << " mode=" << mode
                        << " sample=" << sample << " phase=" << (sample == 0 ? "cold" : sample < warmups ? "warmup" : "warm")
                        << " cpu_produce_encode_commit_ms=" << value.cpu_ms << " settled_wall_ms=" << value.settled_ms
                        << " gpu_sum_ms=" << value.gpu_sum_ms << " gpu_envelope_ms=" << value.gpu_envelope_ms
                        << " producer_ns=" << value.producer_ns << " worker_ns=" << value.worker_ns
                        << " first_context_creation_ns=" << value.context_creation_ns
                        << " actual_producer_encode_intersection_ns=" << value.overlap_ns
                        << " private_workers=" << value.workers << " original_commands=" << value.commits
                        << " native_batches=" << value.native_batches[0] << '/' << value.native_batches[1] << '/' << value.native_batches[2]
                        << " policy_eligible_views=" << value.eligible_views << " policy_below_work_views=" << value.below_work_views
                        << " minimum_native_batches=" << alMetalShadowViewMinimumBatches << '\n';
                }
                CHECK(results[0].pixels == results[1].pixels); CHECK(results[0].queries == results[1].queries);
                const bool eligible = alMetalShadowViewWorkEligibility(count) == ALMetalShadowViewWorkEligibility::eligible;
                CHECK(results[0].workers == 0); CHECK(results[0].context_creation_ns == 0);
                CHECK(results[1].workers == (eligible ? 3u : 0u));
                CHECK(results[1].eligible_views == (eligible ? 3u : 0u));
                CHECK(results[1].below_work_views == (eligible ? 0u : 3u));
                for (const auto& value : results)
                    for (const auto actual : value.native_batches) CHECK(actual == count);
                if (!eligible) { CHECK(results[1].context_creation_ns == 0); CHECK_FALSE(arms[1]->task); }
            }
            std::cout << "shadow_task_cost_summary draws=" << count << " query=" << query << " samples=" << measured
                << " serial_cpu_median_ms=" << median(cpu[0]) << " overlap_cpu_median_ms=" << median(cpu[1])
                << " serial_settled_median_ms=" << median(settled[0]) << " overlap_settled_median_ms=" << median(settled[1])
                << " serial_gpu_envelope_median_ms=" << median(gpu[0]) << " overlap_gpu_envelope_median_ms=" << median(gpu[1])
                << " measured_settled_win=" << (median(settled[1]) < median(settled[0])) << '\n';
            // Every adverse sample/cell is retained. No speed or overlap
            // percentage is a correctness assertion or a viewer FPS claim.
        }
    }
}
