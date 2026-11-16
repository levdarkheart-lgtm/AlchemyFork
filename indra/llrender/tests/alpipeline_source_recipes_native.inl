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
inline ALMetalWorldSpotShadowBatch covered(const Inputs::LLDrawInfo& info,const ALMetalTexture* texture)
{
    auto value = shadow(info);value.coverage = ALMetalWorldSpotShadowCoverage::texture_mask;
    value.texcoord_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
    value.color_offset = info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_COLOR);
    value.texture_count = std::max<std::size_t>(1,std::max(info.mTextureList.size(),
        std::max(info.mMaterialSlotList.size(),info.mGLTFMaterialList.size())));
    for (U32 slot = 0; slot < value.texture_count; ++slot)
    { value.coverageData().textures[slot] = texture;value.coverageData().alpha_cutoffs[slot] = info.mAlphaMaskCutoff; }
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

TEST_CASE("Covered source CPU recipes coexist with main probe and world publications",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);Scene::WorldFlavor flavor;
    flavor.family=Scene::WorldFamily::legacy;U32 builds=0;
    const auto build=[&](auto& value){++builds;value=covered(info,f.white->getMetalTexture());return true;};
    auto first=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(first);
    CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);
    const auto pixels=f.render(view(*first.prototype));
    CHECK(std::any_of(pixels.begin(),pixels.end(),[](const auto& pixel){return pixel[0]!=0;}));
    auto source=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(source);CHECK(builds==1);
    REQUIRE(info.mMetalShadowSource->native_scene_recipe);
    std::weak_ptr<const Scene::Payload> retired(source.owner);source={};first={};CHECK(retired.expired());
    auto main=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(main);
    CHECK(f.render(view(*main.prototype))==pixels);
    const auto restores=Scene::recipe_statistics.shadow_restores;
    auto probe=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,
        denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(probe);CHECK(builds==1);
    CHECK(probe.reused);CHECK(Scene::recipe_statistics.shadow_restores==restores+1);
    CHECK(Scene::findWorld(*probe.owner,flavor.key()));CHECK_FALSE(info.state->publication());
    CHECK(Group::cpuLayoutStats().total_bytes<=Group::cpu_layouts.limits().maximum_bytes);
    main={};probe={};
    // Cheap opaque stays on its existing source factory even under denial.
    U32 opaque_builds=0;
    const auto opaque=[&](auto& value){++opaque_builds;value=shadow(info);return true;};
    {auto reader=Scene::shadow(info,9,ALMetalWorldSpotShadowCoverage::opaque,denied,Inputs::sNativeSceneSerial,1,opaque);REQUIRE(reader);}
    {auto reader=Scene::shadow(info,9,ALMetalWorldSpotShadowCoverage::opaque,denied,Inputs::sNativeSceneSerial,1,opaque);REQUIRE(reader);}
    CHECK(opaque_builds==2);
}

TEST_CASE("Resident source variant extension allocates no covered recipe before actual expiry",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet resident(16,8*1024*1024,100);Scene::WorldFlavor flavor;
    {auto reader=Scene::world(info,flavor,resident,Inputs::sNativeSceneSerial,1);REQUIRE(reader);}
    auto masked=flavor;masked.alpha_mask=true;
    {auto reader=Scene::world(info,masked,resident,Inputs::sNativeSceneSerial,1);REQUIRE(reader);}
    U32 builds=0;const auto build=[&](auto& value){++builds;value=covered(info,f.white->getMetalTexture());return true;};
    auto source=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,
        resident,Inputs::sNativeSceneSerial,1,build);REQUIRE(source);
    CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);
    REQUIRE(resident.erase(info.state->identity(),source.owner->serial));source={};
    auto rebuilt=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,
        resident,Inputs::sNativeSceneSerial,2,build);REQUIRE(rebuilt);CHECK(builds==2);
    REQUIRE(info.mMetalShadowSource->native_scene_recipe);
    REQUIRE(resident.erase(info.state->identity(),rebuilt.owner->serial));rebuilt={};
    auto restored=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,
        resident,Inputs::sNativeSceneSerial,3,build);REQUIRE(restored);CHECK(builds==2);
}

TEST_CASE("Covered recipes preserve indexed world materials and same-frame resource changes",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;
    for(U32 family:{0u,1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);auto& info=f.materialDraw(true);
        auto flavor=f.flavor;flavor.native_resources=false;flavor.indexed_textures=family==0;flavor.indexed_materials=family!=0;
        ALNativeSourceWorkingSet denied(0,8*1024*1024,100);U32 builds=0;
        const auto build=[&](auto& value){++builds;value=covered(info,f.white->getMetalTexture());return true;};
        auto original=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(original);
        const auto pixels=f.render(view(*original.prototype));
        CHECK(std::any_of(pixels.begin(),pixels.end(),[](const auto& pixel){return pixel[0]!=0;}));
        {auto source=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(source);}
        original={};
        auto current=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(current);
        CHECK(current.prototype->texture_count==2);CHECK(f.render(view(*current.prototype))==pixels);
        auto before=info.state->revision();f.positions(*info.mVertexBuffer,true);info.mVertexBuffer->unmapBuffer();
        REQUIRE(info.state->revision()!=before);
        auto changed=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(changed);
        CHECK(changed.owner->producer_revision==info.state->revision());
        {auto source=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(source);}
        CHECK(builds==2);
        if(family==2)
        {
            info.mGLTFMaterial.get()->mRoughnessFactor=.625f;info.mGLTFMaterial.get()->signal.didPublish();
            auto material=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(material);
            CHECK(material.prototype->pbrSlots()[0].roughness_factor==.625f);
            LLPointer<LLImageRaw> raw=new LLImageRaw(1,1,4);std::memset(raw->getData(),127,4);
            LLPointer<LLImageGL> native=new LLImageGL(raw,false);Inputs::LLViewerFetchedTexture fallback;fallback.image=native;
            Inputs::LLViewerFetchedTexture::sWhiteImagep.value=&fallback;info.state->invalidate(); // Typed fixture binding event.
            auto replaced=Scene::world(info,flavor,denied,Inputs::sNativeSceneSerial,1);REQUIRE(replaced);
            CHECK(replaced.prototype->pbrSlots()[0].orm_texture==native->getMetalTexture());
            Inputs::LLViewerFetchedTexture::sWhiteImagep.value=&f.image;
        }
    }
}

TEST_CASE("Rigged covered recipes retain no old pose and reject unchanged-wrapper writes",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer(true,LLVertexBuffer::MAP_WEIGHT4);info.mTexture.value=&f.image;
    std::array<float,16> values{};values[4]=values[9]=values[14]=1.f;
    auto pose=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),1);REQUIRE(pose);
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);U32 builds=0;
    const auto build=[&](auto& value)
    {++builds;value=covered(info,f.white->getMetalTexture());value.weight_offset=info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4);
     value.skin_palette_record=pose;value.skin_palette=pose->data();value.skin_palette_float_count=pose->size();value.skin_joint_count=pose->jointCount();return true;};
    {auto source=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(source);}
    std::weak_ptr<const ALMetalAttachmentPaletteRecord> old_pose(pose);pose.reset();CHECK(old_pose.expired());
    auto restored=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(restored);CHECK(builds==1);
    values[7]=.25f;pose=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),1);REQUIRE(pose);
    ALMetalShadowPrepareInput input;input.source=*restored.record;input.source_generation=input.source->generation();input.epoch=1;input.palette=pose;
    std::array<ALMetalWorldSpotShadowBatch,1> output;REQUIRE(alMetalPrepareShadowBatches(std::span(&input,1),output,1));
    CHECK(output[0].skin_palette==pose->data());restored={};input={};output={};
    auto native=info.mVertexBuffer->getMetalVertexBufferLease();const auto revision=info.state->revision();
    info.mVertexBuffer->nativePublication().willWrite();const std::array<float,4> point{.125f,-1.f,0.f,0.f};
    REQUIRE(const_cast<ALMetalBuffer*>(native.get())->write(info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_VERTEX),point.data(),sizeof(point),&f.error));
    info.mVertexBuffer->nativePublication().didPublish();CHECK(info.mVertexBuffer->getMetalVertexBuffer()==native.get());REQUIRE(info.state->revision()!=revision);
    auto changed=Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,build);REQUIRE(changed);CHECK(builds==2);
    CHECK(changed.owner->producer_revision==info.state->revision());
}

TEST_CASE("Optional covered recipe failures preserve current required source publication",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;ALNativeSourceWorkingSet denied(0,8*1024*1024,100);
    const auto get=[&](){return Scene::shadow(info,7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,1,
        [&](auto& value){value=covered(info,f.white->getMetalTexture());return true;});};
    Scene::recipeAllocationForTesting=[](ALNativePublication::State&){throw std::bad_alloc();};
    {auto required=get();REQUIRE(required);CHECK_FALSE(info.mMetalShadowSource->native_scene_recipe);}
    Scene::recipeAllocationForTesting=nullptr;{auto source=get();REQUIRE(source);}REQUIRE(info.mMetalShadowSource->native_scene_recipe);
    Scene::recipeAllocationForTesting=[](ALNativePublication::State&){throw std::bad_alloc();};{auto fallback=get();REQUIRE(fallback);}
    static bool mutate=false;mutate=true;Scene::recipeAllocationForTesting=[](ALNativePublication::State& state){if(std::exchange(mutate,false))state.invalidate();};
    const auto revision=info.state->revision();{auto current=get();REQUIRE(current);CHECK(current.owner->producer_revision==info.state->revision());}
    CHECK(info.state->revision()==revision+1);Scene::recipeAllocationForTesting=nullptr;
    info.state->invalidate();info.mVertexBuffer=nullptr;CHECK_FALSE(get());
}

TEST_CASE("Covered CPU recipes share existing row byte bounds and yield to group publication",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;GroupPublication f;
    auto limits=Group::cpu_layouts.limits();limits.maximum_rows=2;limits.maximum_bytes=1024*1024;Group::cpu_layouts.configure(limits);
    std::array<Inputs::LLDrawInfo,3> infos;for(auto& info:infos){info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;}
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);
    const auto get=[&](U32 at,U32 frame){return Scene::shadow(infos[at],7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,frame,
        [&](auto& value){value=covered(infos[at],f.white->getMetalTexture());return true;});};
    for(U32 at=0;at<2;++at){auto source=get(at,1);REQUIRE(source);}CHECK(Group::cpuLayoutStats().recipe_rows==2);
    {auto required=get(2,1);REQUIRE(required);}CHECK_FALSE(infos[2].mMetalShadowSource->native_scene_recipe);
    Scene::recipeAllocationForTesting=[](ALNativePublication::State&){FAIL("Recipe capacity preflight must precede allocation");};
    for(U32 attempt=0;attempt<3;++attempt){auto source=get(2,1);REQUIRE(source);}Scene::recipeAllocationForTesting=nullptr;
    auto later=get(2,2);REQUIRE(later);CHECK_FALSE(infos[0].mMetalShadowSource->native_scene_recipe);
    f.draw();auto group=f.seal(Inputs::sNativeSceneSources,2);REQUIRE(group);const auto stats=Group::cpuLayoutStats();
    CHECK(stats.rows==1);CHECK(stats.recipe_rows<=1);CHECK(stats.rows+stats.recipe_rows<=limits.maximum_rows);
    CHECK(stats.total_bytes==stats.estimated_bytes+stats.recipe_bytes);CHECK(stats.total_bytes<=limits.maximum_bytes);
    auto zero=limits;zero.maximum_bytes=0;Group::cpu_layouts.configure(zero);CHECK(Group::cpuLayoutStats().total_bytes==0);
    CHECK_FALSE(infos[2].mMetalShadowSource->native_scene_recipe);CHECK((*later.record)->vertexLease());CHECK(group->sources[0].prototype->vertices);
}

TEST_CASE("Group eviction rechecks covered recipe capacity after reentrant publication",
          "[almetal][pipeline-buffer-publication][source-recipes]")
{
    using namespace SourceRecipes;Policy policy;GroupPublication f;
    auto limits=Group::cpu_layouts.limits();limits.maximum_layouts=1;limits.maximum_rows=2;limits.maximum_bytes=1024*1024;Group::cpu_layouts.configure(limits);
    f.draw();auto first=f.seal(Inputs::sNativeSceneSources,1);REQUIRE(first);
    std::array<Inputs::LLDrawInfo,3> infos;for(auto& info:infos){info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;info.mModelMatrix=&f.model;}
    ALNativeSourceWorkingSet denied(0,8*1024*1024,100);bool called=false;
    struct Observer final:ALNativePublication::Observer{std::function<void(ALNativePublication::Phase)> call;
        void nativePublicationChanged(ALNativePublication::Phase phase)noexcept override{call(phase);}};
    auto observer=std::make_shared<Observer>();observer->call=[&](ALNativePublication::Phase phase)
    {if(phase!=ALNativePublication::Phase::withdrawn||std::exchange(called,true))return;
     for(U32 at=0;at<2;++at){auto source=Scene::shadow(infos[at],7,ALMetalWorldSpotShadowCoverage::texture_mask,denied,Inputs::sNativeSceneSerial,2,
        [&](auto& value){value=covered(infos[at],f.white->getMetalTexture());return true;});CHECK(source);}};
    f.group->observe(observer);auto next_state=std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> dependencies;std::shared_ptr<Group::Layout> layout;
    auto next=Group::seal(next_state,dependencies,layout,f.model,f.region.snapshot(),std::vector<Inputs::LLDrawInfo*>{&infos[2]},
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,2);REQUIRE(called);REQUIRE(next);
    const auto stats=Group::cpuLayoutStats();CHECK(stats.rows==1);CHECK(stats.recipe_rows==1);
    CHECK(stats.total_bytes<=limits.maximum_bytes);CHECK(first->sources[0].prototype->vertices);CHECK_FALSE(f.group->publication());
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
