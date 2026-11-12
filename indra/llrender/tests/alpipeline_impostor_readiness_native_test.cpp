/**
 * @file alpipeline_impostor_readiness_native_test.cpp
 * @brief Actual impostor admission preserves the rest of a current world frame.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "llgl.h"
#include "llrender.h"
#include "llmatrix4a.h"
#include "llmatrix3a.h"
#include "lluuid.h"
#include "almetalavatarresources.h"
#include "almetalpipeline.h"
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace
{
namespace Inputs
{
// Only viewer identity, camera, pass flags and queue inputs are named facades.
// The included production method, cache, textures, commands and renderer are real.
struct LLVOAvatar
{
    LLUUID id;
    bool mNeedsImpostorUpdate = false;
    const LLUUID& getID() const { return id; }
};
struct LLViewerCamera
{
    LLMatrix4a view = LLMatrix4a::identity();
    LLMatrix4a projection = LLMatrix4a::identity();
    const LLMatrix4a& getModelview() const { return view; }
    const LLMatrix4a& getProjection() const { return projection; }
    static LLViewerCamera& getCurrent() { static LLViewerCamera camera; return camera; }
};
struct Capture
{
    bool frame_active = true, pass_active = true, impostor = true, complete = true;
    bool hdr = false, normal_hdr = false;
    ALMetalDevice* device = nullptr;
    std::shared_ptr<ALMetalAvatarResources> avatar_resources =
        std::make_shared<ALMetalAvatarResources>();
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<std::unique_ptr<std::array<F32, 45 * 4>>> avatar_palettes;
    void markIncomplete(U32) { complete = false; }
};
struct LLPipeline
{
    using MetalWorldSimpleCapture = Capture;
    enum { RENDER_DEBUG_IMPOSTORS = 1 };
    bool debug = false;
    std::unique_ptr<Capture> mMetalWorldSimpleCapture = std::make_unique<Capture>();
    bool hasRenderDebugMask(U32) const { return debug; }
    void captureMetalWorldImpostorBatch(LLVOAvatar&, const LLVector3&,
        const LLVector3&, const LLVector3&, const LLMatrix3a&, F32 = 0.f);
};
#include "../../newview/pipeline_impostor_readiness_native.inl"
}

constexpr U32 side = 32;
const LLVector3 left(.18f, 0.f, 0.f), up(0.f, .20f, 0.f);
const LLVector3 ready_position(-.45f, .35f, .2f), late_position(.45f, .35f, .2f);

ALMetalAvatarResources::Key key(const Inputs::LLVOAvatar& avatar)
{
    ALMetalAvatarResources::Key result{};
    std::copy_n(avatar.getID().mData, result.size(), result.begin());
    return result;
}

LLMatrix4a currentMVP()
{
    const auto& camera = Inputs::LLViewerCamera::getCurrent();
    LLMatrix4a result;
    result.setMul(camera.getModelview(), camera.getProjection());
    return result;
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalBuffer> vertices, indices;
    std::unique_ptr<ALMetalTexture> green;
    const Inputs::LLViewerCamera previous_camera = Inputs::LLViewerCamera::getCurrent();

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto frame = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frame);
        frame->beginFrame(1); renderer->setFrameData(std::move(frame));
        struct VertexData
        {
            F32 positions[4][4]{{-.8f,-.8f,.2f,1.f}, {-.2f,-.8f,.2f,1.f},
                                  {-.8f,-.3f,.2f,1.f}, {-.2f,-.3f,.2f,1.f}};
            F32 normals[4][4]{{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}};
            F32 uv[4][2]{};
            U8 colors[4][4]{{255,255,255,255},{255,255,255,255},
                           {255,255,255,255},{255,255,255,255}};
        } data;
        ALMetalBufferDescriptor desc; desc.storage = ALMetalStorageMode::shared;
        desc.length = sizeof(data);
        vertices = ALMetalBuffer::create(*device, desc, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, &data, sizeof(data), &error));
        const U16 values[]{0,1,2,1,3,2}; desc.length = sizeof(values);
        indices = ALMetalBuffer::create(*device, desc, &error); REQUIRE(indices);
        REQUIRE(indices->write(0, values, sizeof(values), &error));
        green = texture(ALMetalPixelFormat::rgba8_unorm_srgb);
        const U8 pixel[]{0,255,0,255};
        auto command = device->makeCommandBuffer("World input", &error); REQUIRE(command);
        REQUIRE(green->upload(*command, 0, 0, {}, pixel, 4, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    ~Fixture()
    {
        if (device) CHECK(device->waitUntilIdle(&error));
        Inputs::LLViewerCamera::getCurrent() = previous_camera;
    }
    std::unique_ptr<ALMetalTexture> texture(ALMetalPixelFormat format)
    {
        ALMetalTextureDescriptor d; d.format = format;
        d.storage = ALMetalStorageMode::private_gpu;
        d.usage = ALMetalTextureUsage::shader_read;
        auto result = ALMetalTexture::create(*device, d, &error);
        INFO(error); REQUIRE(result); return result;
    }
    ALMetalAvatarResources::Record quartet(bool hdr, bool normal_hdr, bool blue)
    {
        ALMetalAvatarResources::Record r;
        r.device = device.get(); r.width = r.height = 1;
        r.hdr = hdr; r.normal_hdr = normal_hdr;
        r.textures[0] = texture(ALMetalPixelFormat::rgba8_unorm_srgb);
        r.textures[1] = texture(ALMetalPixelFormat::rgba8_unorm);
        r.textures[2] = texture(normal_hdr ? ALMetalPixelFormat::rgba16_unorm
                                         : ALMetalPixelFormat::rgb10a2_unorm);
        r.textures[3] = texture(hdr ? ALMetalPixelFormat::rg11b10_float
                                  : ALMetalPixelFormat::rgba8_unorm);
        const U8 albedo[]{U8(blue ? 0 : 255),0,U8(blue ? 255 : 0),255};
        const U8 black[4]{};
        const U16 normal16[]{32768,32768,0,0};
        const U32 normal10 = 512u | (512u << 10);
        auto command = device->makeCommandBuffer("Committed native impostor producer", &error);
        REQUIRE(command);
        REQUIRE(r.textures[0]->upload(*command,0,0,{},albedo,4,&error));
        REQUIRE(r.textures[1]->upload(*command,0,0,{},black,4,&error));
        REQUIRE(r.textures[2]->upload(*command,0,0,{},
            normal_hdr ? static_cast<const void*>(normal16) : &normal10,
            normal_hdr ? 8 : 4,&error));
        REQUIRE(r.textures[3]->upload(*command,0,0,{},black,4,&error));
        REQUIRE(command->commitAndWait(&error));
        return r;
    }
    ALMetalWorldSimpleBatch world() const
    {
        ALMetalWorldSimpleBatch b;
        b.vertices = vertices.get(); b.normal_offset = 4*4*sizeof(F32);
        b.texcoord_offset = b.normal_offset + 4*4*sizeof(F32);
        b.color_offset = b.texcoord_offset + 4*2*sizeof(F32);
        b.vertex_count = 4; b.indices = indices.get(); b.index_count = 6;
        b.texture_count = 1; b.diffuse_textures[0] = green.get();
        b.double_sided = true;
        const auto mvp = currentMVP();
        std::memcpy(b.modelview_projection,mvp.getF32ptr(),sizeof(b.modelview_projection));
        const auto& view = Inputs::LLViewerCamera::getCurrent().getModelview();
        std::memcpy(b.modelview,view.getF32ptr(),sizeof(b.modelview));
        for(U32 axis=0;axis<3;++axis) b.normal_matrix[axis][axis]=1.f;
        return b;
    }
    std::vector<U8> pixels(const std::vector<ALMetalWorldSimpleBatch>& batches,
                           bool hdr, bool normal_hdr, bool reverse)
    {
        ALMetalWorldGBufferScene scene;
        scene.batches=batches.data(); scene.batch_count=batches.size();
        scene.width=scene.height=side; scene.hdr=hdr;
        scene.normal_hdr=normal_hdr; scene.reverse_z=reverse;
        std::fill_n(scene.clear_color,4,0.f);
        auto target=renderer->render(scene,&error); INFO(error); REQUIRE(target);
        REQUIRE(target->colorTexture(0));
        REQUIRE(target->colorTexture(0)->width()==side);
        REQUIRE(target->colorTexture(0)->height()==side);
        std::vector<U8> result(side*side*4);
        REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},result.data(),side*4,&error));
        return result;
    }
};

void capture(Inputs::LLPipeline& pipeline, Inputs::LLVOAvatar& avatar,
             const LLVector3& position)
{
    const LLMatrix3a rotation = LLMatrix3a::getIdentity();
    pipeline.captureMetalWorldImpostorBatch(avatar,position,left,up,rotation);
}

U32 positive(const std::vector<U8>& pixels,U32 channel)
{
    U32 result=0;
    for(std::size_t pixel=0;pixel<pixels.size()/4;++pixel)
        result += pixels[4*pixel+channel]>0;
    return result;
}
}

TEST_CASE("Actual missing impostor preserves other current-camera world batches and retries after publication",
          "[almetal][pipeline-impostor-readiness]")
{
    Fixture f;
    for(bool hdr:{false,true}) for(bool normal_hdr:{false,true}) for(bool reverse:{false,true})
    {
        CAPTURE(hdr,normal_hdr,reverse);
        Inputs::LLPipeline pipeline; auto& c=*pipeline.mMetalWorldSimpleCapture;
        c.device=f.device.get(); c.hdr=hdr; c.normal_hdr=normal_hdr;
        Inputs::LLVOAvatar ready,late; ready.id.mData[0]=1; late.id.mData[0]=2;
        REQUIRE(c.avatar_resources->publish(key(ready),f.quartet(hdr,normal_hdr,false),&f.error));
        auto& camera=Inputs::LLViewerCamera::getCurrent();
        camera.view=LLMatrix4a::identity(); camera.projection=LLMatrix4a::identity();
        c.batches.push_back(f.world()); capture(pipeline,ready,ready_position);
        REQUIRE(c.batches.size()==2);
        c.avatar_palettes.push_back(std::make_unique<std::array<F32,45*4>>());
        c.avatar_palettes[0]->fill(.375f); const auto* palette=c.avatar_palettes[0].get();
        const auto frame_a=c.batches;
        const auto pixels_a=f.pixels(frame_a,hdr,normal_hdr,reverse);
        CHECK(positive(pixels_a,0)>0); CHECK(positive(pixels_a,1)>0);
        capture(pipeline,late,late_position);
        CHECK(c.complete); CHECK(c.batches.size()==2); CHECK(late.mNeedsImpostorUpdate);
        CHECK(c.avatar_palettes.size()==1);
        if(!c.avatar_palettes.empty())
        {
            CHECK(c.avatar_palettes[0].get()==palette);
            CHECK(std::all_of(c.avatar_palettes[0]->begin(),c.avatar_palettes[0]->end(),
                              [](F32 x){return x==.375f;}));
        }
        if(c.complete && c.batches.size()==2)
            CHECK(f.pixels(c.batches,hdr,normal_hdr,reverse)==pixels_a);

        // Named next-frame inputs model the existing scheduler, not its execution.
        // A failed bake is still pending: the rest of a changed-camera frame advances.
        c.complete=true; c.batches.clear(); c.avatar_palettes.clear();
        alignas(16) F32 moved[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, -.25f,.15f,0,1};
        camera.view=LLMatrix4a(moved);
        c.batches.push_back(f.world()); capture(pipeline,ready,ready_position);
        const auto frame_b=c.batches;
        const auto pixels_b=f.pixels(frame_b,hdr,normal_hdr,reverse);
        CHECK(pixels_b!=pixels_a); CHECK(positive(pixels_b,0)>0); CHECK(positive(pixels_b,1)>0);
        capture(pipeline,late,late_position);
        CHECK(c.complete); CHECK(c.batches.size()==2); CHECK(late.mNeedsImpostorUpdate);
        if(c.complete && c.batches.size()==2)
            CHECK(f.pixels(c.batches,hdr,normal_hdr,reverse)==pixels_b);

        // Real committed texture publication admits the pending avatar without
        // altering its authored geometry or substituting a synthetic texture.
        REQUIRE(c.avatar_resources->publish(key(late),f.quartet(hdr,normal_hdr,true),&f.error));
        c.complete=true; c.batches=frame_b;
        capture(pipeline,late,late_position);
        CHECK(c.complete); REQUIRE(c.batches.size()==3);
        const auto& admitted=c.batches.back(); const auto mvp=currentMVP();
        CHECK(std::memcmp(admitted.modelview_projection,mvp.getF32ptr(),sizeof(admitted.modelview_projection))==0);
        CHECK(std::memcmp(admitted.modelview,camera.view.getF32ptr(),sizeof(admitted.modelview))==0);
        CHECK(std::memcmp(admitted.impostor_center,late_position.mV,sizeof(late_position.mV))==0);
        CHECK(admitted.impostor_texture_owner==c.avatar_resources->find(key(late),*f.device,hdr,normal_hdr));
        const auto published=f.pixels(c.batches,hdr,normal_hdr,reverse);
        CHECK(positive(published,2)>0); CHECK(published!=pixels_b);
        const auto green_before=positive(pixels_b,1),red_before=positive(pixels_b,0);
        CHECK(positive(published,1)==green_before); CHECK(positive(published,0)==red_before);
        CHECK(late.mNeedsImpostorUpdate); // Capture does not forge the scheduler's success receipt.
    }
}

TEST_CASE("Actual impostor capture retains a valid quartet on failed replacement and retries incompatible inputs",
          "[almetal][pipeline-impostor-readiness]")
{
    Fixture f; Inputs::LLPipeline pipeline; auto& c=*pipeline.mMetalWorldSimpleCapture;
    c.device=f.device.get(); Inputs::LLVOAvatar avatar; avatar.id.mData[0]=7;
    REQUIRE(c.avatar_resources->publish(key(avatar),f.quartet(false,false,false),&f.error));
    const auto old=c.avatar_resources->find(key(avatar),*f.device,false,false); REQUIRE(old);
    const auto* old_texture=old->textures[0].get();
    capture(pipeline,avatar,ready_position); REQUIRE(c.batches.size()==1);
    const auto before=f.pixels(c.batches,false,false,false);
    avatar.mNeedsImpostorUpdate=true; // A replacement bake is still pending.
    auto rejected=f.quartet(false,false,true); rejected.textures[2].reset();
    CHECK_FALSE(c.avatar_resources->publish(key(avatar),std::move(rejected),&f.error));
    CHECK(c.avatar_resources->find(key(avatar),*f.device,false,false)==old);
    CHECK(old->textures[0].get()==old_texture);
    c.batches.clear(); capture(pipeline,avatar,ready_position);
    CHECK(c.complete); REQUIRE(c.batches.size()==1);
    CHECK(f.pixels(c.batches,false,false,false)==before);
    CHECK(avatar.mNeedsImpostorUpdate);
    c.hdr=true; c.avatar_palettes.push_back(std::make_unique<std::array<F32,45*4>>());
    capture(pipeline,avatar,ready_position);
    CHECK(c.complete); CHECK(c.batches.size()==1); CHECK(c.avatar_palettes.size()==1);
    CHECK(avatar.mNeedsImpostorUpdate);
    CHECK(c.avatar_resources->find(key(avatar),*f.device,false,false)==old);
    CHECK_FALSE(c.avatar_resources->find(key(avatar),*f.device,true,false));
}

TEST_CASE("Actual impostor inactive and malformed-debug guards remain fail closed",
          "[pipeline-impostor-readiness]")
{
    Inputs::LLVOAvatar avatar;
    for(U32 inactive=0;inactive<3;++inactive)
    {
        Inputs::LLPipeline p; auto& c=*p.mMetalWorldSimpleCapture;
        c.frame_active=inactive!=0; c.pass_active=inactive!=1; c.impostor=inactive!=2;
        c.batches.emplace_back(); capture(p,avatar,ready_position);
        CHECK(c.complete); CHECK(c.batches.size()==1); CHECK_FALSE(avatar.mNeedsImpostorUpdate);
    }
    for(F32 malformed:{0.f,-1.f,std::numeric_limits<F32>::quiet_NaN()})
    {
        Inputs::LLPipeline p; auto& c=*p.mMetalWorldSimpleCapture; p.debug=true;
        c.batches.emplace_back(); c.avatar_palettes.push_back(std::make_unique<std::array<F32,45*4>>());
        const LLMatrix3a rotation = LLMatrix3a::getIdentity();
        p.captureMetalWorldImpostorBatch(avatar,ready_position,left,up,rotation,malformed);
        CHECK_FALSE(c.complete); CHECK(c.batches.empty()); CHECK(c.avatar_palettes.empty());
        CHECK_FALSE(avatar.mNeedsImpostorUpdate);
    }
    Inputs::LLPipeline p; auto& c=*p.mMetalWorldSimpleCapture;
    c.complete=false; c.batches.emplace_back();
    c.avatar_palettes.push_back(std::make_unique<std::array<F32,45*4>>());
    capture(p,avatar,ready_position);
    CHECK_FALSE(c.complete); CHECK(c.batches.empty()); CHECK(c.avatar_palettes.empty());
    CHECK_FALSE(avatar.mNeedsImpostorUpdate);
}
