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
    DrainFixture(std::size_t count)
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
    void drain()
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
            });
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
    CHECK_FALSE(ALSceneProducerDrainNative::ordinaryFlavor(999, info, policy));
    LLMatrix4 animated_uv;
    info.mTextureMatrix = &animated_uv;
    simple = ALSceneProducerDrainNative::ordinaryFlavor(LLRenderPass::PASS_SIMPLE, info, policy);
    REQUIRE(simple); CHECK_FALSE(simple->cohort_source);
}
