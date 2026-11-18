/**
 * @file alpipeline_producer_drain_native.inl
 * @brief Actual owner drain over typed group membership and real publications.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace
{
namespace Inputs
{
#include "../../newview/pipeline_producer_source_drain_native.inl"
}

namespace ProducerRecipeFrontier
{
using LLDrawInfo = Inputs::LLDrawInfo;
namespace ALScenePublicationNative = Inputs::ALScenePublicationNative;
namespace ALSceneSourcePolicyNative = Inputs::ALSceneSourcePolicyNative;
// Typed draw-pool boundary only. The recipe-selection body is production code;
// real pool enumeration/header conformance is checked by the viewer build.
struct LLRenderPass
{
    enum : U32 { PASS_SIMPLE, PASS_SIMPLE_RIGGED, PASS_ALPHA_MASK, PASS_ALPHA_MASK_RIGGED,
        PASS_GRASS, PASS_MATERIAL=8, PASS_NORMSPEC=10,
        PASS_GLTF_PBR=20, PASS_GLTF_PBR_RIGGED, PASS_GLTF_PBR_ALPHA_MASK,
        PASS_GLTF_PBR_ALPHA_MASK_RIGGED };
};
struct LLDrawPoolMaterials
{
    struct Description
    {
        U32 render_type, mask;
        U32 alphaMode() const { return mask & 3u; }
        bool hasNormalMap() const { return (mask & 8u) != 0; }
        bool hasSpecularMap() const { return (mask & 4u) != 0; }
    };
    inline static constexpr std::array DEFERRED_PASSES{
        Description{LLRenderPass::PASS_MATERIAL,0}, Description{LLRenderPass::PASS_NORMSPEC,12}};
};
#include "../../newview/pipeline_producer_recipe_native.inl"
}

struct DrainGroup : std::enable_shared_from_this<DrainGroup>
{
    struct Row
    {
        U32 pass = ProducerRecipeFrontier::LLRenderPass::PASS_SIMPLE;
        Inputs::LLDrawInfo* source = nullptr;
        std::weak_ptr<ALNativePublication::State> producer;
        U64 identity = 0, layout_revision = 0;
    };
    struct Work
    {
        std::vector<Row> values;
        std::span<const Row> rows() const noexcept { return values; }
    };
    std::shared_ptr<Work> work = std::make_shared<Work>();
    std::shared_ptr<ALNativePublication::State> state = std::make_shared<ALNativePublication::State>();
    std::vector<std::shared_ptr<Inputs::LLDrawInfo>> draws;
    ALNativeCohortCompilationAdmission mMetalSourcePreparation;
    std::size_t mMetalSourcePreparationCursor = 0;
    U64 mMetalSourcePreparationIdentity = 0, mMetalSourcePreparationRevision = 0, mMetalSourcePreparationPolicy = 0;
    ALMetalWorldSourceCapabilitySummary mMetalSourceCapabilitySummary;
    ALNativeCohortCompilationAdmission mMetalSourceCapabilityPreparation;
    std::size_t mMetalSourceCapabilityCursor = 0;
    U64 mMetalSourceCapabilityIdentity = 0, mMetalSourceCapabilityRevision = 0;
    U64 mMetalSourceCapabilityPolicy = 0;
    bool mMetalSourceCapabilityComplete = false;
    bool dead = false;
    bool isDead() const { return dead; }
    const auto& metalSourceWork() const { return work; }
    const auto& metalSourceWorkState() const { return state; }
    bool sourceWorkCurrent(const Work& candidate) const { return !dead && work.get() == &candidate; }
    bool publishMetalSourceWork(bool) { rebuild(); return true; }
    void rebuild()
    {
        auto next = std::make_shared<Work>();
        for (const auto& draw : draws)
            next->values.push_back({ProducerRecipeFrontier::LLRenderPass::PASS_SIMPLE, draw.get(), draw->state,
                draw->state->identity(), draw->metalSourceLayoutRevision()});
        work = std::move(next);
    }
};

struct DrainFixture : Publication
{
    using Policy = ProducerRecipeFrontier::ALSceneProducerDrainNative::Policy;
    std::shared_ptr<DrainGroup> group = std::make_shared<DrainGroup>();
    ALNativeFrameBudget budget;
    ALNativeFrameBudget::FrameInputs inputs;
    Policy policy;
    U64 serial = 0;
    std::size_t selections = 0;
    std::function<void()> during_selection;
    DrainFixture(std::size_t count, ALNativeFrameBudget::Limits limits = {}) : budget(limits)
    {
        Inputs::ALSceneProducerDrainNative::statistics = {};
        Inputs::ALSceneProducerDrainNative::last_frame.reset();
        Inputs::ALSceneProducerDrainNative::scan_cursor = 0;
        auto geometry = buffer();
        for (std::size_t i = 0; i < count; ++i)
        {
            auto draw = std::make_shared<Inputs::LLDrawInfo>();
            draw->mVertexBuffer = geometry; draw->mTexture.value = &image;
            group->draws.push_back(std::move(draw));
        }
        group->rebuild();
        nextFrame();
    }
    void nextFrame() { ++inputs.frame; budget.beginFrame(inputs); }
    void drain(bool early = false)
    {
        const std::array groups{group.get()};
        Inputs::ALSceneProducerDrainNative::drain(groups.begin(), groups.end(), policy,
            Inputs::sNativeSceneSources, serial, U32(inputs.frame), budget,
            [](DrainGroup* owner) { return owner->shared_from_this(); },
            [this](Inputs::LLDrawInfo* source)
            {
                for (const auto& draw : group->draws) if (draw.get() == source) return draw;
                return std::shared_ptr<Inputs::LLDrawInfo>{};
            },
            [this](U32 pass, const Inputs::LLDrawInfo& source, const Policy& actual)
            {
                ++selections;
                const auto callback = during_selection;
                if (callback) callback();
                return ProducerRecipeFrontier::ALSceneProducerDrainNative::ordinaryFlavor(pass, source, actual);
            }, early);
    }
};
}

TEST_CASE("Actual producer drain bounds each frame and completes only the final source chunk",
          "[almetal][producer-drain][pipeline-buffer-publication]")
{
    DrainFixture f(258);
    f.drain();
    CHECK(f.group->mMetalSourcePreparationCursor == 256);
    CHECK(f.group->state->dirty());
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.completed == 0);
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.prepared == 256);
    const auto serial = f.serial;
    f.drain(); // Nested/repeated view cannot refill the frame's work allowance.
    CHECK(f.serial == serial);
    CHECK(f.selections == 256);
    f.nextFrame(); f.drain();
    CHECK(f.group->mMetalSourcePreparationCursor == 258);
    CHECK_FALSE(f.group->state->dirty());
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.completed == 1);
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.prepared == 258);
    f.nextFrame(); f.drain();
    CHECK(f.selections == 258); // Warm owner does no per-source rediscovery.
    CHECK(f.budget.snapshot().scene_publications_in_flight == 0);
    for (const auto& source : f.group->draws)
        CHECK(Inputs::ALScenePublicationNative::current(*source));
}

TEST_CASE("Actual producer drain preserves cold denial and reentrant membership replacement",
          "[almetal][producer-drain][pipeline-buffer-publication]")
{
    DrainFixture f(2);
    f.inputs.os_available_bytes = 0; f.nextFrame(); f.drain();
    CHECK(f.group->mMetalSourcePreparationCursor == 0);
    CHECK(f.group->state->dirty());
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.deferred == 1);
    // Required first capture is independent of the optional preparation ticket.
    auto required = Inputs::ALScenePublicationNative::world(*f.group->draws[0], {},
        Inputs::sNativeSceneSources, f.serial, U32(f.inputs.frame));
    REQUIRE(required);
    f.inputs.os_available_bytes = ALNativeFrameBudget::unknown_bytes;
    f.nextFrame();
    const auto work = f.group->work;
    std::weak_ptr<Inputs::LLDrawInfo> old_first = f.group->draws[0];
    f.during_selection = [&]
    {
        f.during_selection = {};
        f.group->draws.clear();
        f.group->state->invalidate();
        f.group->rebuild();
        CHECK_FALSE(old_first.expired()); // Active row remains pinned.
    };
    const auto selections = f.selections;
    f.drain();
    CHECK(f.group->work != work);
    CHECK(f.selections == selections + 1); // Stale next raw row is never read.
    CHECK(old_first.expired());
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.stale == 1);
    CHECK(f.group->state->dirty());
    f.nextFrame(); f.drain();
    CHECK_FALSE(f.group->state->dirty()); // Empty replacement safely completes.
}

TEST_CASE("Producer recipes preserve special maps and do not request unsupported mask cohorts",
          "[almetal][producer-drain][producer-policy][PPD-416]")
{
    using namespace ProducerRecipeFrontier;
    ALSceneProducerDrainNative::Policy policy; policy.native_groups = true;
    Inputs::LLDrawInfo info;
    auto simple = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_SIMPLE, info, policy);
    REQUIRE(simple); CHECK(simple->cohort_source); CHECK_FALSE(simple->alpha_mask);
    auto mask = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_ALPHA_MASK, info, policy);
    REQUIRE(mask); CHECK(mask->alpha_mask); CHECK_FALSE(mask->cohort_source);
    auto grass = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_GRASS, info, policy);
    REQUIRE(grass); CHECK(grass->alpha_mask); CHECK_FALSE(grass->indexed_textures); CHECK_FALSE(grass->cohort_source);
    auto legacy = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_NORMSPEC + 1, info, policy);
    REQUIRE(legacy); CHECK(legacy->normal_map); CHECK(legacy->specular_map);
    CHECK(legacy->rigged); CHECK_FALSE(legacy->cohort_source);
    policy.pass.animated_sources_active = policy.pass.animated_gpu_records_active = true;
    legacy = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_NORMSPEC + 1, info, policy);
    REQUIRE(legacy); CHECK_FALSE(legacy->animated_source); CHECK_FALSE(legacy->animated_gpu_source);
    auto capture_policy = policy.pass; capture_policy.rigged = true;
    const auto captured = ALSceneSourcePolicyNative::select(info, 1, false, false, capture_policy,
        policy.native_visibility, policy.native_resources, policy.native_groups);
    CHECK(captured.flavor.animated_source); CHECK(captured.flavor.animated_gpu_source);
    CHECK_FALSE(ALSceneProducerDrainNative::ordinaryFlavor(999, info, policy));
    LLMatrix4 animated_uv;
    info.mTextureMatrix = &animated_uv;
    simple = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_SIMPLE, info, policy);
    REQUIRE(simple); CHECK_FALSE(simple->cohort_source);
}

TEST_CASE("Early producer preparation waits for published stores without forcing an unmap",
          "[almetal][producer-drain][PPD-419]")
{
    DrainFixture f(1);
    auto& info = *f.group->draws.front();
    LLStrider<LLVector3> positions;
    REQUIRE(info.mVertexBuffer->getVertexStrider(positions));
    positions[0].set(-.9f, -.9f, 0.f);
    REQUIRE_FALSE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    f.drain(true);
    CHECK_FALSE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    CHECK(f.group->mMetalSourcePreparationCursor == 0);
    CHECK(f.group->state->dirty());
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.resources_pending == 1);
    CHECK(f.serial == 0);
    // Actual producer/flush completion retries in the same outer frame; the
    // failed readiness check did not consume the owner's compilation ticket.
    info.mVertexBuffer->unmapBuffer();
    REQUIRE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    f.drain(true);
    CHECK_FALSE(f.group->state->dirty());
    auto required = Inputs::ALScenePublicationNative::world(info, {}, Inputs::sNativeSceneSources,
        f.serial, U32(f.inputs.frame));
    REQUIRE(required); CHECK(required.reused);
    CHECK(f.budget.snapshot().scene_publications_in_flight == 0);
}

TEST_CASE("Early producers share a frame allowance while preserving later main preparation",
          "[almetal][producer-drain][PPD-419]")
{
    DrainFixture f(65);
    f.drain(true);
    CHECK(f.group->mMetalSourcePreparationCursor == 64);
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.rows == 64);
    const auto early_group = f.group;
    f.drain(true);
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.rows == 64);
    auto geometry = f.group->draws.front()->mVertexBuffer;
    f.group = std::make_shared<DrainGroup>();
    for (unsigned i = 0; i < 193; ++i)
    {
        auto draw = std::make_shared<Inputs::LLDrawInfo>();
        draw->mVertexBuffer = geometry; draw->mTexture.value = &f.image;
        f.group->draws.push_back(std::move(draw));
    }
    f.group->rebuild();
    f.drain();
    CHECK(Inputs::ALSceneProducerDrainNative::statistics.rows == 256);
    CHECK(f.group->mMetalSourcePreparationCursor == 192);
    CHECK(f.group->state->dirty());
    CHECK(early_group->mMetalSourcePreparationCursor == 64);
    f.nextFrame(); f.drain();
    CHECK_FALSE(f.group->state->dirty());
    CHECK(f.group->mMetalSourcePreparationCursor == 193);
}

TEST_CASE("Producer base preparation never nests animated-capability tickets in a one-credit frame",
          "[almetal][producer-drain][PPD-421]")
{
    using namespace Inputs::ALScenePublicationNative;
    MaterialGroupPublication f(1);
    auto& info = f.materialDraw();
    ALNativeFrameBudget::Limits limits; limits.scene_publications_per_frame = 1;
    ALNativeFrameBudget budget(limits);
    ALNativeFrameBudget::FrameInputs frame; frame.frame = 1; budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;
    auto animated = f.flavor; animated.rigged = animated.animated_source = true;
    const std::array combined{ProducerWorldRequest{producerSource(info), animated}};
    const auto denied = prepareProducerSources(combined, {}, Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial, 1, admission, budget, 4096,
        [] { return true; }, [](const auto&, auto&) { return false; });
    CHECK_FALSE(denied); CHECK(denied.status == ProducerPreparationStatus::unsupported);
    CHECK(budget.snapshot().scene_publications_this_frame == 0);
    const std::array base{ProducerWorldRequest{producerSource(info), f.flavor}};
    const auto prepared = prepareProducerSources(base, {}, Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial, 1, admission, budget, 4096,
        [] { return true; }, [](const auto&, auto&) { return false; });
    REQUIRE(prepared);
    CHECK(budget.snapshot().scene_publications_this_frame == 1);
    CHECK(budget.snapshot().scene_publications_in_flight == 0);
    auto required = world(info, f.flavor, Inputs::sNativeSceneSources, Inputs::sNativeSceneSerial, 1);
    REQUIRE(required); CHECK(required.reused);
}


TEST_CASE("Heavy capability summary never combines different actual material passes",
          "[almetal][producer-drain][producer-policy]")
{
    ALMetalWorldSourceCapabilitySummary summary;
    summary.add(8,100000);summary.add(8,100000);
    summary.add(10,120000);
    CHECK(summary.pass==8);CHECK(summary.indexed_elements==200000);CHECK(summary.rows==2);
    CHECK(alMetalWorldNeedsHeavySourceBounds(summary.input()));
    summary.add(10,100001);
    CHECK(summary.pass==10);CHECK(summary.indexed_elements==220001);CHECK(summary.rows==2);
    ALMetalWorldSourceCapabilitySummary small;
    small.add(8,100000);small.add(10,120000);
    CHECK_FALSE(alMetalWorldNeedsHeavySourceBounds(small.input()));
    summary.add(12,std::numeric_limits<U64>::max());summary.add(12,1);
    CHECK(summary.indexed_elements==std::numeric_limits<U64>::max());
    CHECK(summary.pass==12);CHECK(summary.rows==2);
}

TEST_CASE("Heavy source bounds have separate admission and warm preparation remains scan free",
          "[almetal][producer-drain][producer-capability]")
{
    using namespace Inputs::ALScenePublicationNative;
    ALNativeFrameBudget::Limits limits;limits.scene_publications_per_frame=1;
    DrainFixture f(1,limits);
    f.policy.heavy_bounds=true;
    auto& info=*f.group->draws.front();
    LLPointer<LLVertexBuffer> geometry=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
        LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
    constexpr U32 indices=196608;
    REQUIRE(geometry->allocateBuffer(3,indices));
    std::memset(geometry->getMappedData(),0,geometry->getSize());
    f.positions(*geometry,false);
    LLStrider<LLVector3> normal;REQUIRE(geometry->getNormalStrider(normal));
    LLStrider<LLColor4U> colors;REQUIRE(geometry->getColorStrider(colors));
    for(U32 i=0;i<3;++i){normal[i].set(0,0,1);colors[i].set(255,0,0,255);}
    std::vector<U16> elements(indices);for(U32 i=0;i<indices;++i)elements[i]=i%3;
    geometry->setIndexData(elements.data());geometry->unmapBuffer();
    info.mVertexBuffer=geometry;info.mCount=indices;
    f.group->rebuild();
    f.group->work->values[0].pass=ProducerRecipeFrontier::LLRenderPass::PASS_MATERIAL;
    f.drain();
    REQUIRE_FALSE(f.group->state->dirty());
    CHECK(f.group->mMetalSourceCapabilitySummary.indexed_elements==indices);
    CHECK_FALSE(f.group->mMetalSourceCapabilityComplete);
    CHECK(f.budget.snapshot().scene_publications_this_frame==1);
    CHECK(f.budget.snapshot().scene_publications_in_flight==0);
    f.nextFrame();f.drain();
    REQUIRE(f.group->mMetalSourceCapabilityComplete);
    CHECK(f.budget.snapshot().scene_publications_this_frame==1);
    CHECK(f.budget.snapshot().scene_publications_in_flight==0);
    auto flavor=ProducerRecipeFrontier::ALSceneProducerDrainNative::ordinaryFlavor(
        ProducerRecipeFrontier::LLRenderPass::PASS_MATERIAL,info,f.policy);
    REQUIRE(flavor);flavor->cohort_source=flavor->cohort_bounds=true;
    const auto required=world(info,*flavor,Inputs::sNativeSceneSources,f.serial,U32(f.inputs.frame));
    REQUIRE(required);CHECK(required.reused);
    const auto selections=f.selections;
    f.nextFrame();f.drain();
    CHECK(f.selections==selections);
    CHECK(f.budget.snapshot().scene_publications_this_frame==0);
}
