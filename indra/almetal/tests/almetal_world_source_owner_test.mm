/**
 * @file almetal_world_source_owner_test.mm
 * @brief Producer-generation ownership across direct and forward commands.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalpipeline.h"

namespace
{
// A real earlier command on the same queue keeps subsequent renderer commands
// pending. No renderer hook or simulated completion decides lease retirement.
struct QueueGate
{
    id<MTLSharedEvent> event;
    std::unique_ptr<ALMetalCommandBuffer> blocker;

    explicit QueueGate(Fixture& fixture)
    {
        auto native_device = (__bridge id<MTLDevice>)fixture.device->nativeHandle();
        event = [native_device newSharedEvent];
        REQUIRE(event);
        blocker = fixture.device->makeCommandBuffer("World source lease queue gate", &fixture.error);
        INFO(fixture.error); REQUIRE(blocker);
        auto native = (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
        [native encodeWaitForEvent:event value:1];
        REQUIRE(blocker->commit(&fixture.error));
    }
    ~QueueGate() { event.signaledValue = 1; }
    void release(Fixture& fixture)
    {
        event.signaledValue = 1;
        REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
    }
};

void requireRetired(const std::weak_ptr<Resources>& source,
                    const std::weak_ptr<int>& credit)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!source.expired() || !credit.expired()) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(source.expired());
    CHECK(credit.expired());
}

std::vector<std::uint8_t> colorBytes(Fixture& fixture, const ALMetalRenderTarget& target)
{
    const auto* texture = target.colorTexture(0);
    REQUIRE(texture);
    const std::size_t bytes = texture->descriptor().format == ALMetalPixelFormat::rgba16_float ? 8 : 4;
    std::vector<std::uint8_t> result(side * side * bytes);
    REQUIRE(texture->readback(*fixture.device, 0, 0, {}, result.data(), side * bytes, &fixture.error));
    return result;
}

std::unique_ptr<ALMetalRenderTarget> lighting(Fixture& fixture)
{
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = descriptor.height = side;
    descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    auto target = ALMetalRenderTarget::create(*fixture.device, descriptor, &fixture.error);
    INFO(fixture.error); REQUIRE(target);
    ALMetalRenderPassConfig config;
    config.colors[0].clear = {.03125, .0625, .125, .5};
    config.depth_store = config.stencil_store = ALMetalStoreAction::store;
    auto pass = target->makeRenderPass(config, &fixture.error);
    REQUIRE(pass);
    auto command = fixture.device->makeCommandBuffer("World source lease lighting clear", &fixture.error);
    REQUIRE(command);
    auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "World source lighting", &fixture.error);
    INFO(fixture.error); REQUIRE(encoder);
    REQUIRE(encoder->end(&fixture.error));
    REQUIRE(command->commitAndWait(&fixture.error));
    return target;
}

ALMetalWorldOpaquePostDeferredScene forwardScene(
    const ALMetalRenderTarget& gbuffer, const ALMetalRenderTarget& composed,
    const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldOpaquePostDeferredScene scene;
    scene.gbuffer = &gbuffer;
    scene.composed_lighting = &composed;
    scene.batches = &batch;
    scene.batch_count = 1;
    scene.width = scene.height = side;
    return scene;
}

ALMetalWorldSimpleBatch fullbright(Fixture& fixture)
{
    auto batch = fixture.batch(0);
    batch.impostor_fullbright = true;
    return batch;
}
}

TEST_CASE("almetal: direct and forward abandonment retire source and scope owners independently",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        for (bool forward : {false, true})
        {
            CAPTURE(forward);
            Fixture fixture;
            auto empty = fixture.scene();
            auto gbuffer = fixture.renderer->render(empty, &fixture.error);
            INFO(fixture.error); REQUIRE(gbuffer);
            auto composed = lighting(fixture);
            auto batch = forward ? fullbright(fixture) : fixture.batch(0);
            std::weak_ptr<Resources> source(fixture.resources);
            auto credit = std::make_shared<int>(23);
            std::weak_ptr<int> weak_credit(credit);
            std::unique_ptr<ALMetalRenderTarget> output;
            {
                ALMetalSubmissionScope scope(credit);
                REQUIRE(fixture.renderer->beginCommandBatch("Abandoned world source generation", &fixture.error));
                if (forward)
                {
                    auto scene = forwardScene(*gbuffer, *composed, batch);
                    scene.source_owner = fixture.resources;
                    output = fixture.renderer->renderOpaquePostDeferred(scene, &fixture.error);
                }
                else
                {
                    auto scene = fixture.scene();
                    scene.batches = &batch;
                    scene.batch_count = 1;
                    scene.source_owner = fixture.resources;
                    output = fixture.renderer->render(scene, &fixture.error);
                }
                INFO(fixture.error); REQUIRE(output);
            }
            fixture.resources.reset();
            credit.reset();
            CHECK_FALSE(source.expired());
            CHECK_FALSE(weak_credit.expired());
            auto completion = fixture.renderer->commandBatchCompletion();
            REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            fixture.renderer->cancelCommandBatch();
            CHECK(source.expired());
            CHECK(weak_credit.expired());
        }
    }
}

TEST_CASE("almetal: both forward overloads pin producer bytes through delayed native completion",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        for (bool continuation : {false, true})
        {
            CAPTURE(continuation);
            Fixture fixture;
            auto gbuffer = fixture.renderer->render(fixture.scene(), &fixture.error);
            INFO(fixture.error); REQUIRE(gbuffer);
            auto reference_light = lighting(fixture);
            auto batch = fullbright(fixture);
            auto reference_scene = forwardScene(*gbuffer, *reference_light, batch);
            auto reference = fixture.renderer->renderOpaquePostDeferred(reference_scene, &fixture.error);
            INFO(fixture.error); REQUIRE(reference);
            const auto expected = colorBytes(fixture, *reference);
            // Require a real forward draw, not merely a successful clear/copy.
            const auto center = (side / 2 * side + side / 2) * 8;
            CHECK_FALSE(std::equal(expected.begin(), expected.begin() + 8, expected.begin() + center));
            auto candidate_light = lighting(fixture);
            auto scene = forwardScene(*gbuffer, *candidate_light, batch);
            scene.source_owner = fixture.resources;
            std::weak_ptr<Resources> source(fixture.resources);
            auto credit = std::make_shared<int>(31);
            std::weak_ptr<int> weak_credit(credit);
            QueueGate gate(fixture);
            std::unique_ptr<ALMetalRenderTarget> candidate;
            {
                ALMetalSubmissionScope scope(credit);
                candidate = continuation
                    ? fixture.renderer->renderOpaquePostDeferred(scene, std::move(candidate_light), &fixture.error)
                    : fixture.renderer->renderOpaquePostDeferred(scene, &fixture.error);
                INFO(fixture.error); REQUIRE(candidate);
            }
            scene.source_owner.reset();
            fixture.resources.reset();
            credit.reset();
            fixture.renderer.reset();
            CHECK_FALSE(source.expired());
            CHECK_FALSE(weak_credit.expired());
            gate.release(fixture);
            requireRetired(source, weak_credit);
            CHECK(colorBytes(fixture, *candidate) == expected);
        }
    }
}

TEST_CASE("almetal: impostor forward commands pin producer bytes after caller and renderer destruction",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto reference = fixture.renderer->render(fixture.scene(), &fixture.error);
        auto candidate = fixture.renderer->render(fixture.scene(), &fixture.error);
        INFO(fixture.error); REQUIRE(reference); REQUIRE(candidate);
        auto batch = fullbright(fixture);
        auto scene = fixture.scene();
        scene.batches = &batch;
        scene.batch_count = 1;
        const auto clear = colorBytes(fixture, *reference);
        REQUIRE(fixture.renderer->compositeImpostorPostDeferred(*reference, scene, &fixture.error));
        const auto expected = colorBytes(fixture, *reference);
        CHECK(expected != clear);
        scene.source_owner = fixture.resources;
        std::weak_ptr<Resources> source(fixture.resources);
        auto credit = std::make_shared<int>(47);
        std::weak_ptr<int> weak_credit(credit);
        QueueGate gate(fixture);
        {
            ALMetalSubmissionScope scope(credit);
            REQUIRE(fixture.renderer->compositeImpostorPostDeferred(*candidate, scene, &fixture.error));
        }
        scene.source_owner.reset();
        fixture.resources.reset();
        credit.reset();
        fixture.renderer.reset();
        CHECK_FALSE(source.expired());
        CHECK_FALSE(weak_credit.expired());
        gate.release(fixture);
        requireRetired(source, weak_credit);
        CHECK(colorBytes(fixture, *candidate) == expected);
    }
}

TEST_CASE("almetal: failed G-buffer transactions reject a careless outer commit and recover",
          "[almetal][world-source-owner][world-transaction]")
{
    @autoreleasepool
    {
        // The real raw/owned input checks fail deterministically. In each case an
        // earlier valid pass has already encoded into the enclosing command;
        // rejecting only the failed pass must not publish that valid prefix.
        for (unsigned failure : {0u, 1u, 2u})
        {
            CAPTURE(failure);
            Fixture fixture;
            auto batch = fixture.batch(0);
            auto scene = fixture.scene();
            scene.batches = &batch;
            scene.batch_count = 1;
            auto reference = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(reference);
            const auto expected = colorBytes(fixture, *reference);
            auto owner = std::make_shared<int>(59);
            std::weak_ptr<int> weak_owner(owner);
            scene.source_owner = owner;
            REQUIRE(fixture.renderer->beginCommandBatch("Failed world pass must discard encoded prefix", &fixture.error));
            auto prefix = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(prefix);
            scene.source_owner.reset();
            owner.reset();
            CHECK_FALSE(weak_owner.expired());
            auto completion = fixture.renderer->commandBatchCompletion();
            REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());

            if (failure < 2)
            {
                auto invalid = batch;
                invalid.diffuse_textures[0] = nullptr;
                auto invalid_scene = scene;
                invalid_scene.batches = &invalid;
                CHECK_FALSE(fixture.renderer->render(invalid_scene, failure ? nullptr : &fixture.error));
                if (!failure) CHECK_FALSE(fixture.error.empty());
            }
            else
            {
                auto invalid = batch;
                invalid.diffuse_textures[0] = nullptr;
                auto inputs = ALMetalWorldViewInputs::adopt(scene, {invalid}, {}, fixture.resources, 17, &fixture.error);
                INFO(fixture.error); REQUIRE(inputs);
                CHECK_FALSE(fixture.renderer->renderOwnedInputs(*inputs, &fixture.error));
                CHECK_FALSE(fixture.error.empty());
            }

            const auto submitted_before = ALMetalDevice::committedCommandBufferCount();
            CHECK_FALSE(fixture.renderer->commitCommandBatch(&fixture.error));
            CHECK(ALMetalDevice::committedCommandBufferCount() == submitted_before);
            CHECK_FALSE(fixture.renderer->commandBatchActive());
            CHECK(completion->isComplete()); // Unsubmitted abandonment only.
            CHECK(weak_owner.expired());

            fixture.error.clear();
            REQUIRE(fixture.renderer->beginCommandBatch("Fresh world transaction after failure", &fixture.error));
            scene.source_owner = fixture.resources;
            auto recovered = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(recovered);
            REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
            REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
            CHECK(colorBytes(fixture, *recovered) == expected);
        }
    }
}

TEST_CASE("almetal: quiescent CPU packet storage is reusable while direct and owned_input GPU work remains pending",
          "[almetal][world-source-owner][world-cpu-retirement]")
{
    @autoreleasepool
    {
        for(bool owned_path:{false,true})
        {
            CAPTURE(owned_path);
            Fixture fixture;
            std::vector<ALMetalWorldSimpleBatch> batches;
            std::vector<ALMetalWorldVisibilityQuery> queries;
            batches.reserve(64);queries.reserve(16);
            batches.push_back(fixture.batch(0));batches.push_back(fixture.batch(1));batches.push_back(fixture.batch(4));
            for(std::size_t offset:{std::size_t{0},batches.size()})
            {
                ALMetalWorldVisibilityQuery query;
                identity(query.modelview_projection);query.center[2]=.75f;
                query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.05f;
                query.batch_offset=offset;query.fan=7;query.count_samples=1;
                queries.push_back(query);
            }
            auto scene=fixture.scene();scene.batches=batches.data();scene.batch_count=batches.size();
            scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> reference_queries;
            auto reference=fixture.renderer->render(scene,&fixture.error,&reference_queries);
            INFO(fixture.error);REQUIRE(reference);REQUIRE(reference_queries);
            const auto expected=fixture.read(*reference);
            std::array<std::uint64_t,2> expected_queries{};
            for(unsigned q=0;q<expected_queries.size();++q)REQUIRE(reference_queries->wait(q,expected_queries[q],&fixture.error));
            const auto batch_capacity=batches.capacity(),query_capacity=queries.capacity();
            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(batches),std::move(queries),fixture.resources,91,&fixture.error);
            INFO(fixture.error);REQUIRE(inputs);
            scene={}; // No borrowed CPU array addresses escape the joined owner.
            CHECK(inputs->resourceLease().get()==fixture.resources.get());
            std::weak_ptr<const ALMetalWorldViewInputs> weak_inputs(inputs);
            auto reader=inputs; // A real strong CPU reader, never a raced weak lock.
            const auto* copied_batches=reader->scene().batches;
            const auto* copied_queries=reader->scene().visibility_queries;
            REQUIRE(batches.empty());REQUIRE(queries.empty());
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,batches,queries));
            CHECK(inputs.get()==reader.get());CHECK(reader->scene().batches==copied_batches);
            CHECK(reader->scene().visibility_queries==copied_queries);
            CHECK(reader->scene().batch_count==3);CHECK(reader->scene().visibility_query_count==2);
            CHECK(reader->scene().batches[2].skin_palette[4]==1.f);
            CHECK(batches.empty());CHECK(queries.empty());
            reader.reset();
            REQUIRE(inputs.use_count()==1);
            batches.emplace_back();batches.back().index_count=777;
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,batches,queries));
            CHECK(inputs);CHECK(batches.size()==1);CHECK(batches.front().index_count==777);
            CHECK(inputs->scene().batches==copied_batches);
            batches.clear();
            std::shared_ptr<const ALMetalWorldViewInputs> owned_input;
            if(owned_path)
            {

                owned_input=inputs;INFO(fixture.error);REQUIRE(owned_input);
            }
            // The original mutable pose is no longer a CPU or GPU input.
            fixture.attachment_pose.fill(std::numeric_limits<float>::quiet_NaN());
            auto credit=std::make_shared<int>(71);
            std::weak_ptr<int> weak_credit(credit);
            std::weak_ptr<Resources> weak_resources(fixture.resources);
            QueueGate gate(fixture);
            std::shared_ptr<ALMetalVisibilityResultBatch> candidate_queries;
            std::unique_ptr<ALMetalRenderTarget> candidate;
            {
                ALMetalSubmissionScope scope(credit);
                if(owned_path)candidate=fixture.renderer->renderOwnedInputs(*owned_input,&fixture.error,&candidate_queries);
                else
                {
                    auto owned_scene=inputs->scene();
                    owned_scene.source_owner=inputs->resourceLease();
                    candidate=fixture.renderer->render(owned_scene,&fixture.error,&candidate_queries);
                }
                INFO(fixture.error);REQUIRE(candidate);REQUIRE(candidate_queries);
            }
            // All CPU jobs/readers have joined and encoding has returned.
            // GPU commands own only exact native generations, not these arrays.
            owned_input.reset();
            REQUIRE(inputs.use_count()==1);
            REQUIRE_FALSE(gate.blocker->completionToken()->isComplete());
            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,batches,queries));
            CHECK_FALSE(inputs);CHECK(weak_inputs.expired());
            CHECK(batches.empty());CHECK(queries.empty());
            CHECK(batches.capacity()>=batch_capacity);CHECK(queries.capacity()>=query_capacity);
            batches.resize(32);queries.resize(8);
            for(auto& batch:batches)batch.modelview_projection[0]=std::numeric_limits<float>::quiet_NaN();
            for(auto& query:queries)query.center[0]=std::numeric_limits<float>::quiet_NaN();
            fixture.resources.reset();credit.reset();fixture.renderer.reset();
            CHECK_FALSE(weak_resources.expired());CHECK_FALSE(weak_credit.expired());
            REQUIRE_FALSE(gate.blocker->completionToken()->isComplete());
            gate.release(fixture);
            requireRetired(weak_resources,weak_credit);
            CHECK(fixture.read(*candidate)==expected);
            for(unsigned q=0;q<expected_queries.size();++q)
            {
                std::uint64_t value=0;REQUIRE(candidate_queries->wait(q,value,&fixture.error));CHECK(value==expected_queries[q]);
            }
        }
    }
}

TEST_CASE("almetal: an empty owned_input view renders with an exact null native resource lease",
          "[almetal][world-source-owner][world-cpu-retirement]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto scene=fixture.scene();
        auto reference=fixture.renderer->render(scene,&fixture.error);INFO(fixture.error);REQUIRE(reference);
        const auto expected=fixture.read(*reference);
        auto inputs=ALMetalWorldViewInputs::adopt(scene,{}, {},{},103,&fixture.error);REQUIRE(inputs);
        CHECK_FALSE(inputs->resourceLease());

        auto owned_input=inputs;REQUIRE(owned_input);
        auto candidate=fixture.renderer->renderOwnedInputs(*owned_input,&fixture.error);INFO(fixture.error);REQUIRE(candidate);
        CHECK(fixture.read(*candidate)==expected);
    }
}
