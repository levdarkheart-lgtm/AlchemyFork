/**
 * @file almetal_world_body_palette_native.inl
 * @brief View-specific body palettes preserve scalar world/shadow ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after almetal_world_prepare_fixture.h in attachment-palette tests.
 */
namespace BodyPaletteNative
{
using Host = Fixture;
using Record = ALMetalBodyPaletteRecord;
std::array<float,180> values(float translation=0.f)
{
    std::array<float,180> result{};
    for(unsigned joint=0;joint<15;++joint)
    {result[joint*4]=1;result[(15+joint)*4+1]=1;result[(30+joint)*4+2]=1;
     result[joint*4+3]=translation;}
    return result;
}
void install(ALMetalWorldSimpleBatch& batch,const Record& record)
{batch.body_palette_record=&record;batch.skin_palette=record.data();batch.skin_palette_float_count=record.size();batch.skin_joint_count=15;}
void cloth(ALMetalWorldSimpleBatch& batch)
{
    batch.avatar_cloth=true;batch.cloth_offset=offsetof(Host::Geometry,cloth);
    batch.avatar_wind[0]=.25f;batch.avatar_wind[2]=.5f;batch.avatar_wind[3]=.375f;
    batch.avatar_sin_wave[0]=1;batch.avatar_sin_wave[1]=2;batch.avatar_sin_wave[2]=.5f;
    batch.avatar_gravity[2]=-1;batch.avatar_cloth_depth[2]=1;
}
std::unique_ptr<ALMetalTexture> shadowTarget(Host& f)
{
    ALMetalTextureDescriptor descriptor;descriptor.width=descriptor.height=side;
    descriptor.type=ALMetalTextureType::texture_2d_array;descriptor.array_length=2;
    descriptor.format=ALMetalPixelFormat::r32_float;
    descriptor.usage=ALMetalTextureUsage::shader_read|ALMetalTextureUsage::render_target;
    auto result=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(result);return result;
}
ALMetalWorldSpotShadowBatch shadow(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldSpotShadowBatch result;result.avatar=true;result.vertices=batch.vertices;result.indices=batch.indices;
    result.position_offset=batch.position_offset;result.normal_offset=batch.normal_offset;
    result.texcoord_offset=batch.texcoord_offset;result.weight_offset=batch.weight_offset;result.cloth_offset=batch.cloth_offset;
    result.vertex_count=batch.vertex_count;result.index_count=batch.index_count;result.index_offset=batch.index_offset;
    result.skin_palette=batch.skin_palette;result.skin_palette_float_count=batch.skin_palette_float_count;result.skin_joint_count=15;
    result.body_palette_record=batch.body_palette_record;result.avatar_cloth=batch.avatar_cloth;
    std::memcpy(result.modelview_projection,batch.modelview_projection,sizeof(result.modelview_projection));
    std::memcpy(result.avatar_wind,batch.avatar_wind,sizeof(result.avatar_wind));
    std::memcpy(result.avatar_sin_wave,batch.avatar_sin_wave,sizeof(result.avatar_sin_wave));
    std::memcpy(result.avatar_gravity,batch.avatar_gravity,sizeof(result.avatar_gravity));
    std::memcpy(result.avatar_cloth_depth,batch.avatar_cloth_depth,sizeof(result.avatar_cloth_depth));
    result.coverage=ALMetalWorldSpotShadowCoverage::avatar_mask;result.texture_count=1;
    result.coverageData().textures[0]=batch.diffuse_textures[0];result.coverageData().alpha_cutoffs[0]=.2f;
    return result;
}
}

TEST_CASE("almetal: body records seal exactly180 finite view-specific floats",
          "[almetal][body-palette][PPD-282]")
{
    using namespace BodyPaletteNative;
    std::string error;auto source=values(.125f);auto record=Record::create(source,&error);INFO(error);REQUIRE(record);
    CHECK(record->size()==180);CHECK(record->jointCount()==15);CHECK(record->data()!=source.data());
    CHECK(std::equal(source.begin(),source.end(),record->data()));CHECK(record->ownedBytes()>=180*sizeof(float));
    CHECK(record->owner()==record);source[3]=.5f;CHECK(record->data()[3]==.125f);
    for(auto at:{0u,179u})for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
    {auto malformed=source;malformed[at]=bad;CHECK_FALSE(Record::create(malformed,&error));}
    Host f;auto batch=f.batch(6);install(batch,*record);CHECK(batch.matchesBodyPaletteRecord());
    const auto attachment_values=attachmentValues(1);
    auto attachment=ALMetalAttachmentPaletteRecord::create(attachment_values.data(),attachment_values.size(),1,&error);REQUIRE(attachment);
    for(unsigned fault=0;fault<7;++fault)
    {
        auto wrong=batch;
        if(fault==0)wrong.skin_palette=source.data();if(fault==1)--wrong.skin_palette_float_count;
        if(fault==2)--wrong.skin_joint_count;if(fault==3)wrong.avatar_skinning=false;
        if(fault==4)wrong.avatar_rigid=true;if(fault==5)wrong.terrain=true;
        if(fault==6)wrong.skin_palette_record=attachment.get();
        // The ordinary geometry and raw/body layout gates remain mandatory.
        CHECK_FALSE(wrong.matchesBodyPaletteRecord());
        auto scene=f.scene();scene.batches=&wrong;scene.batch_count=1;
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(scene,{wrong},{},f.resources,1,&f.error));
    }
}

TEST_CASE("almetal: raw and body records preserve mixed queries formats cloth and shadow output",
          "[almetal][body-palette][world-inputs]")
{
    using namespace BodyPaletteNative;
    Host f;
    for(auto& row:f.geometry.cloth){row[0]=.015625f;row[1]=.03125f;row[2]=.0078125f;row[3]=.5f;}
    f.vertices=f.buffer(&f.geometry,sizeof(f.geometry));
    auto library=f.data->builtinLibrary(&f.error);REQUIRE(library);
    auto probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);REQUIRE(probe);probe->setFrameData(f.data);
    auto depth=shadowTarget(f);std::uint64_t epoch=2;
    for(unsigned format=0;format<4;++format)for(bool cloth_enabled:{false,true})
    {
        CAPTURE(format,cloth_enabled);auto source=values(.03125f);auto record=Record::create(source,&f.error);REQUIRE(record);
        auto body=f.batch(6);body.skin_palette=source.data();body.avatar_cull_mode=ALMetalCullMode::none;
        if(cloth_enabled)cloth(body);
        auto prefix=f.batch(0);prefix.modelview_projection[12]=4.f;
        auto scene=f.scene();scene.hdr=(format&1)!=0;scene.normal_hdr=(format&2)!=0;scene.reverse_z=(format&1)!=0;
        std::vector<ALMetalWorldVisibilityQuery> queries(2);
        for(unsigned at=0;at<2;++at)
        {auto& q=queries[at];identity(q.modelview_projection);q.center[2]=scene.reverse_z?.75f:-.75f;
         q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.125f;q.batch_offset=at+1;q.fan=7;q.count_samples=1;}
        std::array<Pixels,2> images;std::array<std::array<std::uint64_t,2>,2> samples;
        for(unsigned mode=0;mode<2;++mode)
        {
            f.data->beginFrame(epoch++);if(mode)install(body,*record);
            const auto* original=body.skin_palette;const std::array batches{prefix,body};scene.batches=batches.data();scene.batch_count=batches.size();
            auto inputs=ALMetalWorldViewInputs::adopt(scene,{prefix,body},queries,f.resources,epoch,&f.error);INFO(f.error);REQUIRE(inputs);
            CHECK((inputs->scene().batches[1].skin_palette==original)==(mode!=0));
            std::shared_ptr<ALMetalVisibilityResultBatch> results;
            auto target=f.renderer->renderOwnedInputs(*inputs,&f.error,&results);INFO(f.error);REQUIRE(target);REQUIRE(results);
            images[mode]=f.read(*target);
            for(unsigned at=0;at<2;++at){REQUIRE(results->wait(at,samples[mode][at],&f.error));CHECK(samples[mode][at]>0);}
            if(mode)
            {
                auto second=probe->renderOwnedInputs(*inputs,&f.error);REQUIRE(second);CHECK(f.read(*second)==images[mode]);
                auto owned_input=f.adopt(scene,{prefix,body});auto third=probe->renderOwnedInputs(*owned_input,&f.error);REQUIRE(third);
                CHECK(f.read(*third)==images[mode]);CHECK(f.data->stats().body_record_reuses>=2);
            }
            auto caster=BodyPaletteNative::shadow(body);ALMetalWorldSpotShadowScene shadows;shadows.batches=&caster;shadows.batch_count=1;
            shadows.width=shadows.height=side;shadows.reverse_z=scene.reverse_z;shadows.depth_clamp=(format&2)!=0;
            const bool shadow_rendered=f.renderer->renderSpotShadow(*depth,mode,shadows,&f.error);
            INFO(f.error);REQUIRE(shadow_rendered);
        }
        CHECK(images[0]==images[1]);CHECK(samples[0]==samples[1]);
        std::array<std::vector<float>,2> shadow_pixels;
        for(unsigned mode=0;mode<2;++mode)
        {shadow_pixels[mode].resize(side*side);REQUIRE(depth->readback(*f.device,0,mode,{},shadow_pixels[mode].data(),side*sizeof(float),&f.error));}
        CHECK(shadow_pixels[0]==shadow_pixels[1]);
        CHECK(std::any_of(images[1].depth.begin(),images[1].depth.end(),[&](auto z){return z!=(scene.reverse_z?0u:0x3f800000u);}));
    }
}

TEST_CASE("almetal: body records preserve forward avatar alpha and reject mismatched proofs",
          "[almetal][body-palette]")
{
    using namespace BodyPaletteNative;
    Host f;auto source=values();auto record=Record::create(source,&f.error);REQUIRE(record);
    auto batch=f.batch(6);batch.skin_palette=source.data();batch.alpha_mask=false;batch.alpha_cutoff=0;
    batch.avatar_alpha=true;batch.pre_water_simple=true;batch.pre_water_minimum_alpha=.2f;
    batch.avatar_cull_mode=ALMetalCullMode::none;
    auto offscreen=f.batch(0);offscreen.modelview_projection[12]=4;auto background=f.scene();background.batches=&offscreen;background.batch_count=1;
    auto gbuffer=f.renderer->render(background,&f.error);REQUIRE(gbuffer);auto lighting=paletteLighting(f);
    ALMetalTextureDescriptor descriptor;descriptor.width=descriptor.height=1;descriptor.type=ALMetalTextureType::texture_cube;
    descriptor.format=ALMetalPixelFormat::rgba8_unorm;descriptor.usage=ALMetalTextureUsage::shader_read;
    auto environment_cube=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(environment_cube);
    auto upload=f.device->makeCommandBuffer("Body alpha environment",&f.error);REQUIRE(upload);const std::array<std::uint8_t,4> black{};
    for(unsigned face=0;face<6;++face)REQUIRE(environment_cube->upload(*upload,0,face,{},black.data(),4,&f.error));
    REQUIRE(upload->commitAndWait(&f.error));
    ALMetalWorldAlphaEnvironment environment;environment.water_plane[2]=1;environment.water_plane[3]=-1;
    environment.light_direction[2]=1;environment.water_fog[0]=1;environment.water_fog[2]=-1;
    environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1;
    environment.atmos_sunlit[3]=environment.atmos_sunlit_linear[3]=1;
    for(unsigned axis=0;axis<3;++axis){environment.eye_to_sky[axis][axis]=1;environment.atmos_amblit[axis]=.25f;}
    ALMetalWorldLegacyAlphaLighting lights;lights.sun_direction[2]=1;lights.sun_color[0]=lights.sun_color[1]=lights.sun_color[2]=.5f;
    lights.parameters[0]=128;lights.parameters[1]=.004f;lights.parameters[2]=1;
    ALMetalWorldOpaquePostDeferredScene scene;scene.width=scene.height=side;scene.gbuffer=gbuffer.get();scene.composed_lighting=lighting.get();
    scene.batches=&batch;scene.batch_count=1;scene.alpha_environment=&environment;scene.alpha_lighting=&lights;scene.legacy_environment=environment_cube.get();
    auto raw=f.renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(raw);const auto expected=paletteColor(f,*raw);
    CHECK(expected!=paletteColor(f,*lighting));install(batch,*record);
    auto typed=f.renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(typed);CHECK(paletteColor(f,*typed)==expected);
    CHECK(f.data->stats().body_record_publications==1);
    auto wrong=batch;wrong.skin_palette=source.data();scene.batches=&wrong;CHECK_FALSE(f.renderer->renderOpaquePostDeferred(scene,&f.error));
}

TEST_CASE("almetal: pending body palette bytes outlive CPU record and cancellation retires readers",
          "[almetal][body-palette][world-inputs]")
{
    using namespace BodyPaletteNative;
    Host f;auto source=values();auto record=Record::create(source,&f.error);REQUIRE(record);
    auto batch=f.batch(6);install(batch,*record);auto scene=f.scene();scene.batches=&batch;scene.batch_count=1;
    auto input=ALMetalWorldViewInputs::adopt(scene,{batch},{},f.resources,1,&f.error);REQUIRE(input);
    const auto* borrowed=input->scene().batches[0].skin_palette;CHECK(borrowed==record->data());
    auto library=f.data->builtinLibrary(&f.error);REQUIRE(library);
    auto probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);REQUIRE(probe);probe->setFrameData(f.data);
    auto reference=f.renderer->renderOwnedInputs(*input,&f.error);REQUIRE(reference);const auto expected=f.read(*reference);
    PaletteQueueGate gate(f);REQUIRE(f.renderer->beginCommandBatch("Body palette pending native reader",&f.error));
    auto previous=f.renderer->renderOwnedInputs(*input,&f.error);REQUIRE(previous);
    auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);REQUIRE(f.renderer->commitCommandBatch(&f.error));
    CHECK_FALSE(completion->isComplete());std::weak_ptr<const Record> weak=record;
    const auto native_only=input->resourceLease();REQUIRE(native_only);
    input.reset();record.reset();f.data->beginFrame(2);CHECK(weak.expired());
    source=values(.5f);auto changed=Record::create(source,&f.error);REQUIRE(changed);install(batch,*changed);
    auto current=probe->render(scene,&f.error);REQUIRE(current);CHECK_FALSE(completion->isComplete());gate.release(f);
    CHECK(f.read(*previous)==expected);CHECK_FALSE(f.read(*current)==expected);CHECK(completion->isComplete());
    REQUIRE(f.renderer->beginCommandBatch("Body palette abandon",&f.error));auto canceled=f.renderer->render(scene,&f.error);REQUIRE(canceled);
    completion=f.renderer->commandBatchCompletion();f.renderer->cancelCommandBatch();CHECK(completion->isComplete());
    weak=changed;changed.reset();f.data->beginFrame(3);CHECK(weak.expired());
}
