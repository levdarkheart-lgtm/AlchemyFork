/**
 * @file alpipeline_source_recipes_native.inl
 * @brief Actual shared producer recipe/restoration and CPU residency checks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace SourceRecipes
{
namespace Scene = Inputs::ALScenePublicationNative;
namespace Group = Inputs::ALSceneGroupPublicationNative;
struct Policy
{
    Group::CpuLayoutLimits saved = Group::cpu_layouts.limits();
    void (*hook)(ALNativePublication::State&) = Scene::recipeAllocationForTesting;
    Policy()
    {
        auto zero = saved;zero.maximum_layouts = 0;Group::cpu_layouts.configure(zero);
        Group::cpu_layouts.configure(saved);Scene::recipeAllocationForTesting = nullptr;
    }
    ~Policy()
    {
        Scene::recipeAllocationForTesting = hook;
        auto zero = saved;zero.maximum_layouts = 0;Group::cpu_layouts.configure(zero);
        Group::cpu_layouts.configure(saved);
    }
};
inline ALMetalWorldSpotShadowBatch shadow(const Inputs::LLDrawInfo& info)
{
    const auto& buffer = *info.mVertexBuffer;
    ALMetalWorldSpotShadowBatch value;
    value.vertices = buffer.getMetalVertexBuffer();value.indices = buffer.getMetalIndexBuffer();
    value.vertex_count = buffer.getNumVerts();value.index_count = info.mCount;
    value.position_offset = buffer.getOffset(LLVertexBuffer::TYPE_VERTEX);
    return value;
}
inline ALMetalWorldSimpleBatch view(const ALMetalWorldSimpleBatch& source)
{
    auto value = source;
    std::copy(alMetalShadowIdentity.begin(),alMetalShadowIdentity.end(),value.modelview_projection);
    std::copy(alMetalShadowIdentity.begin(),alMetalShadowIdentity.end(),value.modelview);
    for (U32 axis = 0; axis < 3; ++axis) value.normal_matrix[axis][axis] = 1.f;
    return value;
}
}

TEST_CASE("Shared producer CPU recipes survive native withdrawal across world and shadow views",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer = f.buffer();info.mTexture.value = &f.image;
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);
    Scene::WorldFlavor flavor;flavor.family = Scene::WorldFamily::legacy;U32 builds = 0;
    const auto build = [&](ALMetalWorldSpotShadowBatch& value)
    { ++builds;value = shadow(info);return true; };
    auto first = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(first);
    REQUIRE(info.mMetalShadowSource->native_scene_recipe);
    CHECK_FALSE(info.state->publication());
    const auto expected = first.prototype->vertices;
    const auto pixels = f.render(view(*first.prototype));
    CHECK(std::any_of(pixels.begin(),pixels.end(),[](const auto& pixel) { return pixel[0] != 0; }));
    std::weak_ptr<const Scene::Payload> retired(first.owner);first = {};CHECK(retired.expired());
    const auto before = Scene::recipe_statistics;
    auto probe = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(probe);
    CHECK(probe.reused);CHECK(probe.prototype->vertices == expected);
    auto original_shadow = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(original_shadow);CHECK(builds == 1);
    probe = {};original_shadow = {};
    auto main = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(main);
    CHECK(f.render(view(*main.prototype)) == pixels);
    auto next_shadow = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(next_shadow);
    CHECK(builds == 1);CHECK(next_shadow.reused);
    CHECK((*next_shadow.record)->vertexLease().get() == expected);
    CHECK(Scene::recipe_statistics.world_restores - before.world_restores == 2);
    CHECK(Scene::recipe_statistics.shadow_restores - before.shadow_restores == 1);
    CHECK_FALSE(info.state->publication());
    CHECK(Group::cpuLayoutStats().total_bytes <= Group::cpu_layouts.limits().maximum_bytes);
}

TEST_CASE("First resident source publication avoids CPU recipe allocation until actual retirement",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer = f.buffer();info.mTexture.value = &f.image;
    ALNativeSourceWorkingSet resident(16,8*1024*1024,100);Scene::WorldFlavor flavor;
    flavor.family = Scene::WorldFamily::legacy;
    auto reader = Scene::world(info,flavor,resident,Inputs::sNativeSceneSerial,1);REQUIRE(reader);
    CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);
    reader = {};
    // Normal native-resident variant replacement releases the old State
    // Payload. That post-replacement expiry is not prior residency churn.
    auto masked = flavor;masked.alpha_mask = true;
    auto extended = Scene::world(info,masked,resident,Inputs::sNativeSceneSerial,1);REQUIRE(extended);
    CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);extended = {};
    auto shadow_variant = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        resident,Inputs::sNativeSceneSerial,1,[&](auto& value) { value = shadow(info);return true; });
    REQUIRE(shadow_variant);CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);
    REQUIRE(resident.erase(info.state->identity(),shadow_variant.owner->serial));shadow_variant = {};
    auto rebuilt = Scene::world(info,flavor,resident,Inputs::sNativeSceneSerial,2);REQUIRE(rebuilt);
    CHECK(info.mMetalShadowSource->native_scene_recipe);
    REQUIRE(resident.erase(info.state->identity(),rebuilt.owner->serial));rebuilt = {};
    const auto restores = Scene::recipe_statistics.world_restores;
    auto restored = Scene::world(info,flavor,resident,Inputs::sNativeSceneSerial,3);REQUIRE(restored);
    CHECK(Scene::recipe_statistics.world_restores == restores + 1);
}

TEST_CASE("Shared recipes preserve indexed materials fallback slots and same-frame producer changes",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;
    for (U32 family : {0u,1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);
        auto& info = f.materialDraw(true);
        auto flavor = f.flavor;flavor.native_resources = false;
        flavor.indexed_textures = family == 0;flavor.indexed_materials = family != 0;
        ALNativeSourceWorkingSet denied(0,8*1024*1024,100);
        auto original = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(original);
        const auto original_pixels = f.render(view(*original.prototype));
        CHECK(std::any_of(original_pixels.begin(),original_pixels.end(),[](const auto& pixel) { return pixel[0] != 0; }));
        const auto count = original.prototype->texture_count;
        CHECK(count == 2);auto texture = original.prototype->diffuse_textures[1];
        original = {};
        const auto restores = Scene::recipe_statistics.world_restores;
        auto restored = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(restored);
        CHECK(Scene::recipe_statistics.world_restores == restores + (family != 0));
        CHECK(f.render(view(*restored.prototype)) == original_pixels);
        CHECK(restored.prototype->texture_count == count);CHECK(restored.prototype->diffuse_textures[1] == texture);
        if (family == 1) CHECK(restored.prototype->material_slot_count == 2);
        if (family == 2) CHECK(restored.prototype->pbr_slot_count == 2);
        const auto revision = info.state->revision();auto old = restored.owner;
        // Real LLVB publication signals invalidate the source even within the
        // same outer frame. A weak native wrapper alone is never freshness.
        f.positions(*info.mVertexBuffer,true);info.mVertexBuffer->unmapBuffer();
        REQUIRE(info.state->revision() != revision);
        const auto before_changed = Scene::recipe_statistics.world_restores;
        auto changed = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(changed);
        CHECK(changed.owner->producer_revision == info.state->revision());
        CHECK(changed.owner->producer_revision != old->producer_revision);
        CHECK(Scene::recipe_statistics.world_restores == before_changed);
        if (family == 2)
        {
            info.mGLTFMaterial.get()->mRoughnessFactor = .625f;
            info.mGLTFMaterial.get()->signal.didPublish();
            auto material = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(material);
            CHECK(material.prototype->pbrSlots()[0].roughness_factor == .625f);
            CHECK(material.owner->producer_revision == info.state->revision());
            LLPointer<LLImageRaw> raw = new LLImageRaw(1,1,4);
            std::memset(raw->getData(),127,4);
            LLPointer<LLImageGL> native = new LLImageGL(raw,false);
            Inputs::LLViewerFetchedTexture fallback;fallback.image = native;
            Inputs::LLViewerFetchedTexture::sWhiteImagep.value = &fallback;
            // Explicit fixture binding event; the production dependency
            // visitor's fallback identity wiring is covered independently.
            info.state->invalidate();
            auto replaced = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(replaced);
            CHECK(replaced.prototype->pbrSlots()[0].orm_texture == native->getMetalTexture());
            replaced = {};
            auto reused = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(reused);
            CHECK(reused.prototype->pbrSlots()[0].orm_texture == native->getMetalTexture());
            Inputs::LLViewerFetchedTexture::sWhiteImagep.value = &f.image;
        }
    }
}

TEST_CASE("Rigged shared shadow recipes retain no old pose and reject same-wrapper writes",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer = f.buffer(true,LLVertexBuffer::MAP_WEIGHT4);info.mTexture.value = &f.image;
    std::array<float,16> values{};values[4] = values[9] = values[14] = 1.f;
    auto pose = ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),1);REQUIRE(pose);
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);U32 builds = 0;
    const auto build = [&](ALMetalWorldSpotShadowBatch& value)
    {
        ++builds;value = shadow(info);value.weight_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4);
        value.skin_palette_record = pose;value.skin_palette = pose->data();
        value.skin_palette_float_count = pose->size();value.skin_joint_count = pose->jointCount();return true;
    };
    auto original = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(original);CHECK(builds == 1);
    original = {};std::weak_ptr<const ALMetalAttachmentPaletteRecord> old_pose(pose);pose.reset();CHECK(old_pose.expired());
    auto restored = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(restored);CHECK(builds == 1);
    REQUIRE((*restored.record)->requiresPalette());CHECK_FALSE((*restored.record)->prototype().skin_palette);
    values[7] = .25f;pose = ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),1);REQUIRE(pose);
    ALMetalShadowPrepareInput input;input.source = *restored.record;input.source_generation = input.source->generation();
    input.epoch = 1;input.palette = pose;std::array<ALMetalWorldSpotShadowBatch,1> output;
    REQUIRE(alMetalPrepareShadowBatches(std::span(&input,1),output,1));
    CHECK(output[0].skin_palette == pose->data());CHECK(output[0].skinPaletteRecord() == pose.get());
    // Remove every reader before writing, so this exercises an unchanged
    // wrapper as well as the COW case protected by the previous test.
    restored = {};input = {};output = {};
    const auto wrapper = info.mVertexBuffer->getMetalVertexBuffer();
    const auto revision = info.state->revision();
    // Exercise the actual producer signal/write/publication protocol without
    // asking LLVB's allocator to choose in-place storage over a pooled orphan.
    auto native = info.mVertexBuffer->getMetalVertexBufferLease();
    info.mVertexBuffer->nativePublication().willWrite();
    const std::array<float,4> point{.125f,-1.f,0.f,0.f};
    REQUIRE(const_cast<ALMetalBuffer*>(native.get())->write(
        info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_VERTEX),point.data(),sizeof(point),&f.error));
    info.mVertexBuffer->nativePublication().didPublish();
    CHECK(info.mVertexBuffer->getMetalVertexBuffer() == wrapper);
    REQUIRE(info.state->revision() != revision);
    auto changed = Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(changed);CHECK(builds == 2);
    CHECK(changed.owner->producer_revision == info.state->revision());
}

TEST_CASE("Optional recipe failures and reentrant epochs preserve required current publication",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer = f.buffer();info.mTexture.value = &f.image;
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);Scene::WorldFlavor flavor;
    flavor.family = Scene::WorldFamily::legacy;
    Scene::recipeAllocationForTesting = [](ALNativePublication::State&) { throw std::bad_alloc(); };
    auto required = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(required);
    CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);required = {};
    Scene::recipeAllocationForTesting = nullptr;
    required = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(required);required = {};
    REQUIRE(info.mMetalShadowSource->native_scene_recipe);
    Scene::recipeAllocationForTesting = [](ALNativePublication::State&) { throw std::bad_alloc(); };
    auto fallback = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(fallback);fallback = {};
    static bool mutate = false;mutate = true;
    Scene::recipeAllocationForTesting = [](ALNativePublication::State& state)
    { if (std::exchange(mutate,false)) state.invalidate(); };
    const auto revision = info.state->revision();
    auto reentrant = Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(reentrant);
    CHECK(info.state->revision() == revision + 1);CHECK(reentrant.owner->producer_revision == info.state->revision());
    Scene::recipeAllocationForTesting = nullptr;reentrant = {};
    // The production layout owner invalidates before removing/replacing its
    // geometry binding. CPU facts cannot resurrect that absent producer.
    info.state->invalidate();info.mVertexBuffer = nullptr;
    CHECK_FALSE(Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1));
}

TEST_CASE("Source recipes share existing CPU bounds and yield to group publication",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;GroupPublication f;
    auto limits = Group::cpu_layouts.limits();limits.maximum_rows = 2;limits.maximum_bytes = 1024*1024;
    Group::cpu_layouts.configure(limits);
    std::array<Inputs::LLDrawInfo,3> infos;
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);Scene::WorldFlavor flavor;
    flavor.family = Scene::WorldFamily::legacy;
    for (auto& info : infos) { info.mVertexBuffer = f.buffer();info.mTexture.value = &f.image; }
    for (U32 at = 0; at < 2; ++at)
    { auto reader = Scene::world(infos[at],flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(reader); }
    CHECK(Group::cpuLayoutStats().recipe_rows == 2);
    const auto publications = Scene::recipe_statistics.publications;
    auto reader = Scene::world(infos[2],flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(reader);
    CHECK_FALSE(infos[2].mMetalShadowSource->native_scene_recipe);reader = {};
    // Repeated denial at an already protected current-frame row cap must
    // stop before optional CPU recipe construction, not allocate then reject.
    Scene::recipeAllocationForTesting = [](ALNativePublication::State&) { FAIL("Recipe preflight must precede allocation"); };
    for (U32 attempt = 0; attempt < 3; ++attempt)
        REQUIRE(Scene::world(infos[2],flavor,denied,Inputs::sNativeSceneSerial,1));
    CHECK(Scene::recipe_statistics.publications == publications);
    Scene::recipeAllocationForTesting = nullptr;
    auto later = Scene::world(infos[2],flavor,denied,Inputs::sNativeSceneSerial,2);REQUIRE(later);
    CHECK_FALSE(infos[0].mMetalShadowSource->native_scene_recipe);
    CHECK(Group::cpuLayoutStats().recipe_rows == 2);
    f.draw();auto group = f.seal(Inputs::sNativeSceneSources,2);REQUIRE(group);
    const auto stats = Group::cpuLayoutStats();CHECK(stats.rows == 1);CHECK(stats.recipe_rows <= 1);
    CHECK(stats.rows + stats.recipe_rows <= limits.maximum_rows);CHECK(stats.total_bytes <= limits.maximum_bytes);
    CHECK(stats.total_bytes == stats.estimated_bytes + stats.recipe_bytes);
    auto zero = limits;zero.maximum_bytes = 0;Group::cpu_layouts.configure(zero);
    CHECK(Group::cpuLayoutStats().total_bytes == 0);CHECK_FALSE(infos[2].mMetalShadowSource->native_scene_recipe);
    // A real already-admitted reader owns its exact generations separately.
    CHECK(later.prototype->vertices);CHECK(group->sources[0].prototype->vertices);
}

TEST_CASE("Group eviction rechecks shared recipe capacity after a reentrant publisher callback",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;
    Policy policy;GroupPublication f;
    auto limits = Group::cpu_layouts.limits();limits.maximum_layouts = 1;limits.maximum_rows = 2;
    limits.maximum_bytes = 1024*1024;Group::cpu_layouts.configure(limits);
    f.draw();auto first = f.seal(Inputs::sNativeSceneSources,1);REQUIRE(first);
    std::array<Inputs::LLDrawInfo,3> infos;
    for (auto& info : infos)
    { info.mVertexBuffer = f.buffer();info.mTexture.value = &f.image;info.mModelMatrix = &f.model; }
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);Scene::WorldFlavor flavor;bool called = false;
    flavor.family = Scene::WorldFamily::legacy;
    struct Observer final : ALNativePublication::Observer
    {
        std::function<void(ALNativePublication::Phase)> call;
        void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override { call(phase); }
    };
    auto observer = std::make_shared<Observer>();
    observer->call = [&](ALNativePublication::Phase phase)
    {
        if (phase != ALNativePublication::Phase::withdrawn || std::exchange(called,true)) return;
        for (U32 at = 0; at < 2; ++at)
        { auto reader = Scene::world(infos[at],flavor,denied,Inputs::sNativeSceneSerial,2);CHECK(reader); }
    };
    f.group->observe(observer);
    auto next_state = std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> dependencies;
    std::shared_ptr<Group::Layout> layout;
    auto next = Group::seal(next_state,dependencies,layout,f.model,f.region.snapshot(),
        std::vector<Inputs::LLDrawInfo*>{&infos[2]},Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,2);
    REQUIRE(called);REQUIRE(next);CHECK(next_state->publication().get() == next.get());
    const auto stats = Group::cpuLayoutStats();CHECK(stats.rows == 1);CHECK(stats.recipe_rows == 1);
    CHECK(stats.rows + stats.recipe_rows == limits.maximum_rows);CHECK(stats.total_bytes <= limits.maximum_bytes);
    CHECK(first->sources[0].prototype->vertices);CHECK_FALSE(f.group->publication());
}

TEST_CASE("Shared source recipe complete cold warm churn and protected-denial cost",
          "[.][almetal][source-recipes-cost]")
{
    using namespace SourceRecipes;
    // Small triangle draws make publication cost visible; they do not model
    // the unknown per-draw geometry distribution of a live SL scene. Every
    // timed source is consumed by the actual native G-buffer and read back to
    // actual completion. Synchronous owner source-preparation wall time and
    // settled completion wall time stay separate; neither is sampled CPU time.
    std::cout.precision(17);
    for (U32 cell = 0; cell < 10; ++cell)
    {
        const U32 family = cell == 9 ? 1 : cell >= 7 ? 2 : cell >= 5 ? 1 : cell == 3 ? 2 : 0;
        CAPTURE(cell);Policy policy;MaterialGroupPublication f(family);
        const U32 count = cell == 0 ? 1 : cell == 2 || cell == 4 || cell == 9 ? 4 : 2;
        const U32 views = cell == 0 || cell == 5 || cell == 7 ? 1 : 3;
        for (U32 at = 0; at < count; ++at) f.materialDraw();
        auto infos = f.draws();auto flavor = f.flavor;flavor.native_resources = false;
        auto renderer = ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
        std::array<std::vector<double>,2> source_times,complete_times;
        for (U32 block = 0; block < 8; ++block)
        {
            const std::array<U32,2> order = block % 2 ? std::array<U32,2>{1,0} : std::array<U32,2>{0,1};
            for (U32 arm_at = 0; arm_at < order.size(); ++arm_at)
            {
                const U32 enabled = order[arm_at];
                auto zero = policy.saved;zero.maximum_layouts = 0;Group::cpu_layouts.configure(zero);
                auto limits = policy.saved;
                if (!enabled) limits.maximum_rows = 0;
                else if (cell == 4 || cell == 9) limits.maximum_rows = 1;
                Group::cpu_layouts.configure(limits);
                ALNativeSourceWorkingSet residency(cell >= 2 ? 0 : 8192,64*1024*1024,100);
                Inputs::LLDrawInfo keeper;keeper.mVertexBuffer = f.buffer();keeper.mTexture.value = &f.image;
                if (enabled && (cell == 4 || cell == 9))
                { auto keep_flavor = Scene::WorldFlavor{};keep_flavor.family = Scene::WorldFamily::legacy;
                  auto keep = Scene::world(keeper,keep_flavor,residency,Inputs::sNativeSceneSerial,1);REQUIRE(keep); }
                for (auto* info : infos) info->state->invalidate();
                double cpu_seconds = 0,wall_seconds = 0;
                for (U32 iteration = 0; iteration < 12; ++iteration)
                {
                    const U32 frame = iteration + 2;
                    if (enabled && (cell == 4 || cell == 9)) REQUIRE(Scene::recipeFor(keeper,frame));
                    if (cell == 0) { residency.clear();for (auto* info : infos) info->state->invalidate(); }
                    if (cell == 3)
                        for (auto* info : infos)
                        {
                            info->mGLTFMaterial.get()->mRoughnessFactor = .25f + .01f*iteration;
                            info->mGLTFMaterial.get()->signal.didPublish();
                        }
                    const auto start = std::chrono::steady_clock::now();
                    double iteration_owner_s = 0;
                    for (U32 which = 0; which < views; ++which)
                    {
                        std::vector<Scene::WorldLease> readers;readers.reserve(count);
                        std::vector<ALMetalWorldSimpleBatch> batches;batches.reserve(count);
                        const auto begin_source = std::chrono::steady_clock::now();
                        for (auto* info : infos)
                        {
                            auto source = Scene::world(*info,flavor,residency,Inputs::sNativeSceneSerial,frame);REQUIRE(source);
                            batches.push_back(view(*source.prototype));readers.push_back(std::move(source));
                        }
                        iteration_owner_s += std::chrono::duration<double>(std::chrono::steady_clock::now()-begin_source).count();
                        ALMetalWorldGBufferScene scene;scene.batches = batches.data();scene.batch_count = batches.size();
                        scene.width = scene.height = Publication::side;
                        auto target = renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);
                        std::array<U8,Publication::side*Publication::side*4> pixels;
                        REQUIRE(target->colorTexture(0)->readback(*f.device,0,0,{},pixels.data(),Publication::side*4,&f.error));
                    }
                    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                    if (iteration >= 2) { wall_seconds += seconds;cpu_seconds += iteration_owner_s; }
                    std::cout << "SOURCE_RECIPE_SAMPLE cell=" << cell << " block=" << block << " order=" << arm_at
                        << " arm=" << enabled << " iteration=" << iteration << " warmup=" << (iteration < 2)
                        << " source_owner_us=" << iteration_owner_s*1e6 << " complete_us=" << seconds*1e6 << '\n';
                }
                source_times[enabled].push_back(cpu_seconds/10);
                complete_times[enabled].push_back(wall_seconds/10);
                const auto stats = Group::cpuLayoutStats();CHECK(stats.total_bytes <= limits.maximum_bytes);
            }
        }
        const auto median = [](auto values) { std::sort(values.begin(),values.end());return (values[3]+values[4])*.5; };
        std::cout << "SOURCE_RECIPE_COST cell=" << cell << " sources=" << count << " views=" << views
            << " baseline_source_us=" << median(source_times[0])*1e6 << " candidate_source_us=" << median(source_times[1])*1e6
            << " baseline_complete_us=" << median(complete_times[0])*1e6 << " candidate_complete_us=" << median(complete_times[1])*1e6 << '\n';
    }
}

TEST_CASE("Certified shadow recipe consumed opaque covered and rigged small-run cost",
          "[.][almetal][source-recipes-shadow-cost]")
{
    using namespace SourceRecipes;std::cout.precision(17);
    for (U32 cell = 0; cell < 4; ++cell)
    {
        if (std::getenv("AL_SOURCE_RECIPE_PRESSURE_ONLY") && cell != 3) continue;
        Policy policy;Publication f;std::array<Inputs::LLDrawInfo,2> infos;
        for (auto& info : infos)
        { info.mVertexBuffer = f.buffer(true,cell >= 2 ? LLVertexBuffer::MAP_WEIGHT4 : 0);info.mTexture.value = &f.image; }
        std::array<float,16> values{};values[4] = values[9] = values[14] = 1.f;
        auto pose = ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),1);REQUIRE(pose);
        auto renderer = ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
        ALMetalTextureDescriptor descriptor;descriptor.width = descriptor.height = Publication::side;
        descriptor.type = ALMetalTextureType::texture_2d_array;descriptor.array_length = 1;
        descriptor.format = ALMetalPixelFormat::r32_float;
        descriptor.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        auto target = ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(target);
        const auto coverage = cell ? ALMetalWorldSpotShadowCoverage::texture_mask : ALMetalWorldSpotShadowCoverage::opaque;
        std::array<std::vector<double>,2> owner_times,complete_times;
        for (U32 block = 0; block < 8; ++block)
        {
            const std::array<U32,2> order = block % 2 ? std::array<U32,2>{1,0} : std::array<U32,2>{0,1};
            for (U32 arm_at = 0; arm_at < order.size(); ++arm_at)
            {
                const auto enabled = order[arm_at];
                auto zero = policy.saved;zero.maximum_layouts = 0;Group::cpu_layouts.configure(zero);
                auto limits = policy.saved;if (!enabled) limits.maximum_rows = 0;Group::cpu_layouts.configure(limits);
                constexpr U32 pressure_count = 64, pressure_bytes = 4096;
                ALNativeSourceWorkingSet denied(cell == 3 ? 256 : 0,
                    cell == 3 ? pressure_count*pressure_bytes : 64*1024*1024,100);
                std::vector<std::shared_ptr<ALNativePublication::State>> pinned;
                std::vector<U64> pin_serials;
                if (cell == 3)
                    for (U32 at = 0; at < pressure_count; ++at)
                    {
                        ALMetalBufferDescriptor buffer_descriptor;buffer_descriptor.length = pressure_bytes;
                        std::shared_ptr<const ALMetalBuffer> buffer(ALMetalBuffer::create(*f.device,buffer_descriptor,&f.error));REQUIRE(buffer);
                        auto state = std::make_shared<ALNativePublication::State>();
                        REQUIRE(state->publish(state->revision(),buffer));
                        const std::array<ALNativeSourceWorkingSet::Resource,1> resources{{{buffer,buffer->length()}}};
                        const auto serial = ++Inputs::sNativeSceneSerial;
                        REQUIRE(denied.retain(state,serial,state->publication(),resources,1));
                        pinned.push_back(std::move(state));pin_serials.push_back(serial);
                    }
                for (auto& info : infos) info.state->invalidate();
                double owner_sum = 0,complete_sum = 0;
                for (U32 iteration = 0; iteration < 12; ++iteration)
                {
                    if (cell == 3)
                        for (U32 at = 0; at < pinned.size(); ++at)
                            REQUIRE(denied.touch(pinned[at]->identity(),pin_serials[at],iteration+1));
                    const auto start = std::chrono::steady_clock::now();double owner_s = 0;
                    for (U32 which = 0; which < 3; ++which)
                    {
                        struct Packet
                        {
                            std::array<Scene::ShadowLease,2> readers;
                            std::array<ALMetalShadowPrepareInput,2> inputs;
                        };
                        auto packet = std::make_shared<Packet>();
                        auto& readers = packet->readers;auto& inputs = packet->inputs;
                        std::array<ALMetalWorldSpotShadowBatch,2> batches;
                        const auto begin_owner = std::chrono::steady_clock::now();
                        for (U32 at = 0; at < infos.size(); ++at)
                        {
                            auto& info = infos[at];
                            readers[at] = Scene::shadow(info,7,coverage,denied,Inputs::sNativeSceneSerial,iteration+1,
                                [&](auto& prototype)
                                {
                                    // Typed live-input frontier only: no
                                    // invented CPU work. Sealing, validation,
                                    // restoration and preparation are actual
                                    // production source/record implementations.
                                    prototype = shadow(info);prototype.coverage = coverage;
                                    if (cell)
                                    {
                                        prototype.texcoord_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
                                        prototype.color_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_COLOR);
                                        prototype.texture_count = 1;prototype.coverageData().textures[0] = f.white->getMetalTexture();
                                        prototype.coverageData().alpha_cutoffs[0] = info.mAlphaMaskCutoff;
                                    }
                                    if (cell >= 2)
                                    {
                                        prototype.weight_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4);
                                        prototype.skin_palette_record = pose;prototype.skin_palette = pose->data();
                                        prototype.skin_palette_float_count = pose->size();prototype.skin_joint_count = pose->jointCount();
                                    }
                                    return true;
                                });
                            REQUIRE(readers[at]);inputs[at].source = *readers[at].record;
                            inputs[at].source_generation = inputs[at].source->generation();inputs[at].epoch = iteration+1;
                            if (cell >= 2) inputs[at].palette = pose;
                            if (cell == 3)
                            {
                                U64 bytes = 0;for (const auto& resource : readers[at].owner->resources) bytes += resource.bytes;
                                REQUIRE(bytes <= pressure_count*pressure_bytes);
                            }
                        }
                        REQUIRE(alMetalPrepareShadowBatches(inputs,batches,iteration+1));
                        owner_s += std::chrono::duration<double>(std::chrono::steady_clock::now()-begin_owner).count();
                        ALMetalWorldSpotShadowScene scene;scene.width = scene.height = Publication::side;
                        scene.source_owner = packet;
                        scene.batches = batches.data();scene.batch_count = batches.size();scene.bulk_draws = false;
                        const bool encoded = renderer->renderSpotShadow(*target,0,scene,&f.error);INFO(f.error);REQUIRE(encoded);
                        std::array<float,Publication::side*Publication::side> pixels;
                        REQUIRE(target->readback(*f.device,0,0,{},pixels.data(),Publication::side*sizeof(float),&f.error));
                    }
                    const auto complete_s = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                    if (iteration >= 2) { owner_sum += owner_s;complete_sum += complete_s; }
                    std::cout << "SHADOW_RECIPE_SAMPLE cell=" << cell << " block=" << block << " order=" << arm_at
                        << " arm=" << enabled << " iteration=" << iteration << " warmup=" << (iteration < 2)
                        << " source_owner_us=" << owner_s*1e6 << " complete_us=" << complete_s*1e6 << '\n';
                }
                owner_times[enabled].push_back(owner_sum/10);complete_times[enabled].push_back(complete_sum/10);
                if (cell == 3)
                {
                    const auto pressure = denied.stats();
                    REQUIRE(pressure.capacity_rejections[2] > 0);CHECK(pressure.capacity_rejections[1] == 0);
                    CHECK(pressure.oversized_sources == 0);CHECK(pressure.capacity_evictions == 0);
                    CHECK(pressure.estimated_bytes == pressure_count*pressure_bytes);
                    std::cout << "RECIPE_BYTE_PRESSURE block=" << block << " order=" << arm_at << " arm=" << enabled
                        << " byte_rejections=" << pressure.capacity_rejections[2] << " entries=" << pressure.entries
                        << " estimated_bytes=" << pressure.estimated_bytes << '\n';
                }
            }
        }
        const auto median = [](auto times) { std::sort(times.begin(),times.end());return .5*(times[3]+times[4]); };
        std::cout << "SHADOW_RECIPE_COST cell=" << cell << " sources=2 views=3 baseline_source_us=" << median(owner_times[0])*1e6
            << " candidate_source_us=" << median(owner_times[1])*1e6 << " baseline_complete_us=" << median(complete_times[0])*1e6
            << " candidate_complete_us=" << median(complete_times[1])*1e6 << '\n';
    }
}
