/** Temporal motion/coverage fixtures through the production G-buffer and motion replay. */
#include "almetal_world_inputs_fixture.h"
#include <cmath>

namespace
{
struct MotionPixels
{
    std::vector<std::array<std::uint16_t, 2>> motion;
    std::vector<std::uint8_t> reactive;
    std::vector<float> depth;
    float x(unsigned x, unsigned y) const
    { __fp16 value; const auto bits = motion[y * side + x][0]; std::memcpy(&value, &bits, 2); return float(value); }
    float y(unsigned x, unsigned y) const
    { __fp16 value; const auto bits = motion[y * side + x][1]; std::memcpy(&value, &bits, 2); return float(value); }
};
ALMetalWorldTemporalPreviousDraw previousDraw(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldTemporalPreviousDraw p;
    std::copy_n(batch.modelview_projection, 16, p.modelview_projection);
    std::copy_n(batch.texture_matrix, 16, p.texture_matrix);
    if (batch.skin_palette) p.skin_palette.assign(batch.skin_palette, batch.skin_palette + batch.skin_palette_float_count);
    p.skin_joint_count = batch.skin_joint_count;
    std::copy_n(batch.avatar_wind, 4, p.avatar_wind);
    std::copy_n(batch.avatar_sin_wave, 4, p.avatar_sin_wave);
    std::copy_n(batch.avatar_gravity, 4, p.avatar_gravity);
    std::copy_n(batch.avatar_cloth_depth, 4, p.avatar_cloth_depth);
    p.valid = true; return p;
}
MotionPixels motionPixels(Fixture& f, ALMetalWorldSimpleBatch& batch,
    const ALMetalWorldTemporalPreviousDraw& previous, float camera_shift = 0.f,
    float jitter_x = 0.f, float jitter_y = 0.f,
    const ALMetalWorldOpaquePostDeferredScene* forward = nullptr, float water_z = 0.f, bool reverse_z = false, float wave_margin = 0.f,
    const ALMetalWorldModernSkyScene* sky = nullptr, const float* clip_to_previous = nullptr, const float* inverse_projection = nullptr,
    bool reject_sky = false, bool reset = false)
{
    auto world = f.scene(); world.batches = &batch; world.batch_count = 1; world.reverse_z = reverse_z;
    world.temporal_jitter_pixels[0] = jitter_x; world.temporal_jitter_pixels[1] = jitter_y;
    auto gbuffer = f.renderer->render(world, &f.error); INFO(f.error); REQUIRE(gbuffer);
    ALMetalWorldTemporalMotionScene scene;
    scene.width = scene.height = side; scene.opaque_scene = &world;
    scene.depth = gbuffer->depthStencilTexture(); scene.reset = reset; scene.reverse_z = reverse_z;
    scene.previous_opaque = &previous; scene.previous_opaque_count = 1;
    scene.forward_scene = forward; scene.modern_sky = sky; scene.reject_sky_history = reject_sky;
    scene.jitter_pixels[0] = jitter_x; scene.jitter_pixels[1] = jitter_y;
    identity(scene.clip_to_previous_clip); scene.clip_to_previous_clip[12] = camera_shift;
    if (clip_to_previous) std::copy_n(clip_to_previous,16,scene.clip_to_previous_clip);
    identity(scene.inverse_projection);
    if (inverse_projection) std::copy_n(inverse_projection,16,scene.inverse_projection);
    scene.reject_water = water_z != 0.f;
    scene.eye_water_plane[2] = 1.f; scene.eye_water_plane[3] = -water_z;
    scene.water_rejection_margin = wave_margin;
    auto motion = f.renderer->renderTemporalMotion(scene, &f.error); INFO(f.error); REQUIRE(motion);
    MotionPixels result; result.motion.resize(side * side); result.reactive.resize(side * side); result.depth.resize(side * side);
    REQUIRE(motion->colorTexture(0)->readback(*f.device, 0, 0, {}, result.motion.data(), side * 4, &f.error));
    REQUIRE(motion->colorTexture(1)->readback(*f.device, 0, 0, {}, result.reactive.data(), side, &f.error));
    REQUIRE(motion->colorTexture(2)->readback(*f.device, 0, 0, {}, result.depth.data(), side * 4, &f.error));
    return result;
}
}

TEST_CASE("Temporal rigid object motion excludes camera-independent background and raster jitter", "[almetal][temporal][motion]")
{
    Fixture f;
    auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[12] = .25f;
    for (auto jitter : std::array<std::array<float, 2>, 3>{{{0, 0}, {.25f, -.375f}, {-.25f, .125f}}})
    {
        auto pixels = motionPixels(f, batch, previous, 0, jitter[0], jitter[1]);
        CHECK(std::abs(pixels.x(18, 16) + 4.f) < .02f);
        CHECK(std::abs(pixels.y(18, 16)) < .02f);
        CHECK(pixels.reactive[16 * side + 18] == 0);
        CHECK(std::abs(pixels.x(1, 30)) < .02f);
        CHECK(pixels.reactive[30 * side + 1] == 0); // uniform cleared background has stable history
    }
}

TEST_CASE("Temporal camera projection has analytical current-to-previous input-pixel displacement", "[almetal][temporal][motion]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[12] = .125f;
    auto pixels = motionPixels(f, batch, previous, -.125f);
    CHECK(std::abs(pixels.x(16, 16) + 2.f) < .02f);
    CHECK(std::abs(pixels.x(1, 30)) < .02f); // uniform clear is not a finite camera-translated surface
    CHECK(std::abs(pixels.y(16, 16)) < .02f);
}

namespace
{
ALMetalWorldModernSkyScene motionSky(bool reverse)
{
    ALMetalWorldModernSkyScene sky; sky.width=sky.height=side;
    auto& c=sky.constants; identity(c.inverse_projection);
    for (unsigned axis=0;axis<3;++axis) c.eye_to_sky[axis][axis]=c.hdri_rotation[axis][axis]=1.f;
    c.config[1]=1; c.config[2]=reverse;
    c.dome.camera_local[3]=15000.f; c.dome.light_density[1]=1.f; c.dome.light_density[3]=.001f;
    c.dome.glow[0]=1.f; c.dome.glow[2]=-1.f; c.dome.glow[3]=1000.f;
    c.dome.blue_horizon[0]=.4f;c.dome.blue_horizon[1]=.3f;c.dome.blue_horizon[2]=.2f;c.dome.blue_horizon[3]=.1f;
    for (unsigned channel=0;channel<3;++channel)
    {
        c.dome.haze_extinction[channel]=.5f;c.dome.blue_weight[channel]=c.dome.haze_weight[channel]=1.f;
        c.dome.sky_light[channel]=.5f;c.dome.sky_ambient[channel]=.2f;
        c.dome.sky_light_atten[channel]=c.dome.sky_ambient_below[channel]=.1f;
    }
    c.dome.sky_ambient_below[3]=.2f;c.dome.optics[1]=10.f;
    return sky;
}
std::array<float,16> multiplyMotionMatrix(const std::array<float,16>& a,const std::array<float,16>& b)
{
    std::array<float,16> out{};
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)for(unsigned k=0;k<4;++k)
        out[col*4+row]+=a[k*4+row]*b[col*4+k];
    return out;
}
}

TEST_CASE("Temporal directional sky excludes translation and separates raster jitter from rotation", "[almetal][temporal][motion][sky]")
{
    // Two independently specified projections: GL finite-far and Metal reversed infinite.
    // P maps eye-space -Z forward. For a direction at infinity, translation cannot produce parallax.
    for(bool reverse:{false,true})
    {
        Fixture f; auto batch=f.batch(0);auto previous=previousDraw(batch);auto sky=motionSky(reverse);
        const float a=reverse?0.f:-101.f/99.f,b=reverse?1.f:-200.f/99.f;
        std::array<float,16> projection{},inverse{};
        projection[0]=projection[5]=1;projection[10]=a;projection[11]=-1;projection[14]=b;
        inverse[0]=inverse[5]=1;inverse[11]=1.f/b;inverse[14]=-1;inverse[15]=a/b;
        std::copy(inverse.begin(),inverse.end(),sky.constants.inverse_projection);
        std::array<float,16> translation{};identity(translation.data());translation[12]=3.f;translation[13]=-2.f;
        auto reproject=multiplyMotionMatrix(multiplyMotionMatrix(projection,translation),inverse);
        for(auto jitter:std::array<std::array<float,2>,3>{{{0,0},{.25f,-.375f},{-.25f,.125f}}})
        {
            auto pixels=motionPixels(f,batch,previous,0,jitter[0],jitter[1],nullptr,0,reverse,0,&sky,reproject.data(),inverse.data());
            CHECK(std::abs(pixels.x(1,30))<.025f);CHECK(std::abs(pixels.y(1,30))<.025f);
            CHECK(pixels.reactive[30*side+1]==0);
            CHECK(pixels.reactive[16*side+16]==0);
        }
        constexpr float theta=.03f;
        std::array<float,16> rotation{};identity(rotation.data());
        rotation[0]=rotation[10]=std::cos(theta);rotation[8]=std::sin(theta);rotation[2]=-std::sin(theta);
        reproject=multiplyMotionMatrix(multiplyMotionMatrix(projection,rotation),inverse);
        constexpr float jx=.25f,jy=-.375f;
        auto rotated=motionPixels(f,batch,previous,0,jx,jy,nullptr,0,reverse,0,&sky,reproject.data(),inverse.data());
        const float x=(1.5f-jx)/side*2.f-1.f,y=(30.5f-jy)/side*2.f-1.f;
        const float w=std::sin(theta)*x+std::cos(theta);
        const float expected_x=((std::cos(theta)*x-std::sin(theta))/w-x)*(.5f*side);
        const float expected_y=(y/w-y)*(.5f*side);
        CHECK(std::abs(rotated.x(1,30)-expected_x)<.025f);
        CHECK(std::abs(rotated.y(1,30)-expected_y)<.025f);
        CHECK(rotated.reactive[30*side+1]==0);
    }
}

TEST_CASE("Temporal sky rejection follows visible dynamic layers and leaves opaque foreground alone", "[almetal][temporal][motion][sky][reactive]")
{
    Fixture f;auto batch=f.batch(0);auto previous=previousDraw(batch);auto sky=motionSky(true);
    const float inverse[16]{1,0,0,0, 0,1,0,0, 0,0,0,1, 0,0,-1,0};
    std::copy_n(inverse,16,sky.constants.inverse_projection);
    const std::uint8_t black[4]{0,0,0,255},white[4]{255,255,255,255};
    const auto* dark=f.texture(ALMetalPixelFormat::rgba8_unorm,1,black,4);
    const auto* star=f.texture(ALMetalPixelFormat::rgba8_unorm,1,white,4);
    sky.constants.config[0]=1; // stars: only actual nonzero radiance is reactive.
    sky.stars_texture=dark;
    auto clear=motionPixels(f,batch,previous,0,.25f,-.375f,nullptr,0,true,0,&sky,nullptr,inverse);
    CHECK(clear.reactive[30*side+1]==0);
    sky.stars_texture=star;
    auto lit=motionPixels(f,batch,previous,0,.25f,-.375f,nullptr,0,true,0,&sky,nullptr,inverse);
    CHECK(lit.reactive[30*side+1]==255);
    CHECK(lit.reactive[16*side+16]==0); // a dynamic sky pixel cannot invalidate its opaque occluder.
    sky.stars_texture=dark;
    auto changed=motionPixels(f,batch,previous,0,0,0,nullptr,0,true,0,&sky,nullptr,inverse,true);
    CHECK(changed.reactive[30*side+1]==255);
    CHECK(changed.reactive[16*side+16]==0);
    auto reset=motionPixels(f,batch,previous,0,0,0,nullptr,0,true,0,&sky,nullptr,inverse,false,true);
    CHECK(reset.reactive[30*side+1]==255);CHECK(reset.reactive[16*side+16]==255);
    float unsupported[16];identity(unsupported);
    auto unknown=motionPixels(f,batch,previous,0,0,0,nullptr,0,true,0,&sky,nullptr,unsupported);
    CHECK(unknown.reactive[30*side+1]==255);
    // The body uses the exact sampled opacity, not a whole-sky boolean.
    const std::uint8_t translucent[4]{255,255,255,128};
    sky.constants.config[0]=4;sky.sun_texture=f.texture(ALMetalPixelFormat::rgba8_unorm,1,translucent,4);
    sky.constants.sun_quad[0][2]=-1.f;sky.constants.sun_quad[1][0]=.25f;sky.constants.sun_quad[2][1]=.25f;
    auto body=motionPixels(f,batch,previous,0,.25f,-.375f,nullptr,0,true,0,&sky,nullptr,inverse);
    CHECK(std::abs(int(body.reactive[30*side+1])-128)<=1);
    CHECK(body.reactive[16*side+16]==0);
    sky.constants.config[0]=1;
    auto water=motionPixels(f,batch,previous,0,0,0,nullptr,.1f,true,.2f,&sky,nullptr,inverse);
    CHECK(water.reactive[30*side+1]==255); // reset/water safeguards remain independent.
}

TEST_CASE("Temporal attachment and avatar palettes produce independent deformation motion", "[almetal][temporal][motion][avatar]")
{
    for (unsigned family : {4u, 6u})
    {
        CAPTURE(family);
        Fixture f; auto batch = f.batch(family); auto previous = previousDraw(batch);
        if (family == 4) f.attachment_pose[7] = .125f; // joint column0.w
        else for (unsigned joint = 0; joint < 15; ++joint) f.avatar_pose[joint * 4 + 3] = .125f;
        auto pixels = motionPixels(f, batch, previous);
        CHECK(std::abs(pixels.x(17, 16) + 2.f) < .02f);
        CHECK(std::abs(pixels.y(17, 16)) < .02f);
        CHECK(pixels.reactive[16 * side + 17] == 0);
        previous.valid = false;
        auto invalid = motionPixels(f, batch, previous);
        CHECK(invalid.reactive[16 * side + 17] == 255);
    }
}

TEST_CASE("Temporal alpha-tested legacy and PBR coverage agrees with production depth", "[almetal][temporal][motion][alpha]")
{
    for (unsigned family : {0u, 1u, 2u})
    {
        CAPTURE(family);
        Fixture f;
        const std::uint8_t texture[16]{255,255,255,255, 255,255,255,0, 255,255,255,0, 255,255,255,255};
        const auto* coverage = f.texture(ALMetalPixelFormat::rgba8_unorm, 2, texture, 8);
        f.geometry.uv[0][0] = 0; f.geometry.uv[0][1] = 0;
        f.geometry.uv[1][0] = 1; f.geometry.uv[1][1] = 0;
        f.geometry.uv[2][0] = .5f; f.geometry.uv[2][1] = 1;
        f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
        auto batch = f.batch(family); batch.diffuse_textures[0] = coverage;
        // Legacy material masks are selected by material_alpha_mode; the generic alpha_mask bit is illegal
        // for that family. Keep the production validator and the per-family coverage assertions unchanged.
        batch.alpha_mask = family != 1; batch.alpha_cutoff = .5f;
        if (family == 1) { batch.material_alpha_mode = 2; batch.materialSlots()[0].alpha_cutoff = .5f; }
        if (family == 2) batch.pbrSlots()[0].alpha_cutoff = .5f;
        auto previous = previousDraw(batch);
        auto pixels = motionPixels(f, batch, previous, 0, .25f, -.125f);
        unsigned covered = 0, uncovered = 0;
        for (unsigned i = 0; i < side * side; ++i)
        {
            if (pixels.depth[i] < .99f)
            {
                ++covered;
                CHECK(pixels.reactive[i] == 0);
                const unsigned x = i % side, y = i / side;
                CHECK(std::abs(pixels.x(x, y)) < .02f);
                CHECK(std::abs(pixels.y(x, y)) < .02f);
            }
            else ++uncovered;
        }
        CHECK(covered > 20); CHECK(uncovered > 100);
    }
}

TEST_CASE("Temporal animated UV and blended content conservatively reject history", "[almetal][temporal][motion][reactive]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.texture_matrix[12] = .25f;
    auto animated = motionPixels(f, batch, previous);
    CHECK(animated.reactive[16 * side + 16] == 255);
    batch.texture_matrix[12] = 0.f;
    auto blended = batch; blended.pre_water_alpha = true; blended.pre_water_minimum_alpha = .004f;
    ALMetalWorldOpaquePostDeferredScene forward; forward.batches = &blended; forward.batch_count = 1;
    auto pixels = motionPixels(f, batch, previous, 0, 0, 0, &forward);
    CHECK(pixels.reactive[16 * side + 16] == 255);
    CHECK(std::abs(pixels.x(16, 16)) < .02f); // transparent rejection does not replace opaque motion/depth
}

namespace
{
void transparentHistoryCase(unsigned family, bool moving_underneath, bool animated_uv, float texture_alpha)
{
    CAPTURE(family, moving_underneath, animated_uv, texture_alpha);
    Fixture f;
    if (family == 2)
    {
        for (auto& color : f.geometry.colors) color[3] = 191; // LLFace's baked .749 base-alpha factor.
        f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
    }
    auto opaque=f.batch(0);auto previous_opaque=previousDraw(opaque);
    if(moving_underneath) opaque.modelview_projection[12]=.25f;
    auto world=f.scene();world.batches=&opaque;world.batch_count=1;
    auto gbuffer=f.renderer->render(world,&f.error);INFO(f.error);REQUIRE(gbuffer);
    auto snapshot=f.renderer->prepareTemporalOpaqueDepth(side,side,&f.error);REQUIRE(snapshot);
    REQUIRE(f.renderer->beginCommandBatch("Preserve original opaque depth before forward alpha-depth",&f.error));
    REQUIRE(f.renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(),*snapshot,&f.error));
    REQUIRE(f.renderer->commitCommandBatch(&f.error));INFO(f.error);
    std::vector<float> original_depth(side*side);
    REQUIRE(snapshot->colorTexture(0)->readback(*f.device,0,0,{},original_depth.data(),side*4,&f.error));
    CHECK(std::abs(original_depth[16*side+16]-.625f)<1.e-6f);
    ALMetalRenderPassConfig clear;
    for(auto& c:clear.colors){c.load=ALMetalLoadAction::load;c.store=ALMetalStoreAction::store;}
    clear.depth_load=ALMetalLoadAction::clear;clear.depth_store=ALMetalStoreAction::store;clear.clear_depth=.5;
    clear.stencil_load=ALMetalLoadAction::load;clear.stencil_store=ALMetalStoreAction::store;
    auto pass=gbuffer->makeRenderPass(clear,&f.error);REQUIRE(pass);
    auto command=f.device->makeCommandBuffer("Fixture forward depth replacement",&f.error);REQUIRE(command);
    auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Fixture transparent front depth",&f.error);REQUIRE(encoder);
    REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));
    auto glass=f.batch(family);glass.post_water_alpha=true;
    glass.pre_water_fullbright=family==0;glass.pre_water_pbr=family==2;
    glass.pre_water_minimum_alpha=.004f;glass.modelview_projection[14]=-.25f;
    if (family == 2)
    {
        glass.pbrSlots()[0].alpha_cutoff = -1.f;
        glass.pbrSlots()[0].base_color_factor[3] = 191.f / 255.f;
        const __fp16 texel[4]{.25f,.625f,.125f,__fp16(texture_alpha)};
        glass.diffuse_textures[0]=f.texture(ALMetalPixelFormat::rgba16_float,1,texel,8);
    }
    else
    {
        const std::uint8_t translucent[4]{64,160,32,190};
        glass.diffuse_textures[0]=f.texture(ALMetalPixelFormat::rgba8_unorm,1,translucent,4);
    }
    // These expected values are defined independently of the shader. At
    // .006 texel alpha, one baked factor remains above .004; applying it
    // twice falls below. At .004 texel alpha, omitting it wrongly survives.
    const float coverage = family == 2 ? float(__fp16(texture_alpha)) * (191.f / 255.f) : 190.f / 255.f;
    const bool visible = coverage >= .004f;
    auto previous_glass=previousDraw(glass);
    if(animated_uv) glass.texture_matrix[12]=.125f;
    ALMetalWorldOpaquePostDeferredScene forward;forward.batches=&glass;forward.batch_count=1;
    ALMetalWorldTemporalMotionScene scene;
    scene.width=scene.height=side;scene.opaque_scene=&world;scene.forward_scene=&forward;
    scene.depth=gbuffer->depthStencilTexture();scene.opaque_depth=snapshot->colorTexture(0);
    scene.previous_opaque=&previous_opaque;scene.previous_opaque_count=1;
    scene.previous_forward=&previous_glass;scene.previous_forward_count=1;
    scene.reset=false;scene.reverse_z=false;identity(scene.clip_to_previous_clip);identity(scene.inverse_projection);
    auto motion=f.renderer->renderTemporalMotion(scene,&f.error);INFO(f.error);REQUIRE(motion);
    std::vector<std::array<__fp16,2>> vectors(side*side);std::vector<std::uint8_t> rejection(side*side);std::vector<float> depth(side*side);
    REQUIRE(motion->colorTexture(0)->readback(*f.device,0,0,{},vectors.data(),side*4,&f.error));
    REQUIRE(motion->colorTexture(1)->readback(*f.device,0,0,{},rejection.data(),side,&f.error));
    REQUIRE(motion->colorTexture(2)->readback(*f.device,0,0,{},depth.data(),side*4,&f.error));
    const bool represented_foreground = visible;
    const float expected_motion = represented_foreground ? 0.f : moving_underneath ? -4.f : 0.f;
    CHECK(std::abs(float(vectors[16*side+16][0])-expected_motion)<.025f);
    CHECK(rejection[16*side+16]==(visible && (moving_underneath||animated_uv)?255:0));
    // Qualified alpha supplies its own geometric surface, independently of
    // history acceptance. Animated UV still supplies current geometry/depth
    // while rejecting history; neither choice mutates real G-buffer depth.
    CHECK(std::abs(depth[16*side+16]-(represented_foreground?.5f:.625f))<1.e-6f);
    std::vector<float> unchanged_depth(side*side);
    REQUIRE(gbuffer->depthStencilTexture()->readback(*f.device,0,0,{},unchanged_depth.data(),side*4,&f.error));
    CHECK(std::abs(unchanged_depth[16*side+16]-.5f)<1.e-6f);
    scene.opaque_depth=nullptr;
    auto missing=f.renderer->renderTemporalMotion(scene,&f.error);REQUIRE(missing);
    REQUIRE(missing->colorTexture(1)->readback(*f.device,0,0,{},rejection.data(),side,&f.error));
    CHECK(rejection[16*side+16]==(visible?255:0)); // Missing provenance remains conservative on represented color.
}
}

TEST_CASE("Temporal transparent history requires pristine opaque visibility and matching layer motion", "[almetal][temporal][motion][reactive][transparency]")
{
    // A moving object behind stationary glass must still reject; camera-only
    // motion from overwritten front depth would falsely agree.
    for(bool moving_underneath:{false,true}) for(bool animated_uv:{false,true})
        transparentHistoryCase(0,moving_underneath,animated_uv,190.f/255.f);
}

TEST_CASE("Temporal static PBR alpha preserves baked coverage and rejects unmatched background motion", "[almetal][temporal][motion][reactive][transparency][pbr-alpha]")
{
    for(float alpha:{.004f,.006f}) for(bool moving_underneath:{false,true}) for(bool animated_uv:{false,true})
        transparentHistoryCase(2,moving_underneath,animated_uv,alpha);
}

TEST_CASE("Temporal alpha proxy selects nearest represented motion and depth independently of draw order", "[almetal][temporal][motion][transparency][pbr-alpha][depth]")
{
    for(bool reverse_order:{false,true})
    {
        CAPTURE(reverse_order);
        Fixture f;
        for (auto& color:f.geometry.colors) color[3]=191;
        f.vertices=f.buffer(&f.geometry,sizeof(f.geometry));
        auto opaque=f.batch(0);auto previous_opaque=previousDraw(opaque);
        auto world=f.scene();world.batches=&opaque;world.batch_count=1;
        auto gbuffer=f.renderer->render(world,&f.error);REQUIRE(gbuffer);
        auto snapshot=f.renderer->prepareTemporalOpaqueDepth(side,side,&f.error);REQUIRE(snapshot);
        REQUIRE(f.renderer->beginCommandBatch("Nearest alpha fixture opaque snapshot",&f.error));
        REQUIRE(f.renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(),*snapshot,&f.error));
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        auto far=f.batch(0);far.post_water_alpha=true;far.pre_water_fullbright=true;
        far.modelview_projection[14]=-.25f; // Original GL .25-.25 -> .5 device depth.
        auto near=f.batch(2);near.post_water_alpha=true;near.pre_water_pbr=true;
        near.modelview_projection[14]=-.5f;near.pbrSlots()[0].alpha_cutoff=-1.f;
        near.pbrSlots()[0].base_color_factor[3]=191.f/255.f;
        auto previous_far=previousDraw(far),previous_near=previousDraw(near);
        previous_near.modelview_projection[12]=-.125f; // Nearest foreground moved two input pixels.
        std::array<ALMetalWorldSimpleBatch,2> layers{far,near};
        std::array<ALMetalWorldTemporalPreviousDraw,2> prior{previous_far,previous_near};
        if(reverse_order){std::reverse(layers.begin(),layers.end());std::reverse(prior.begin(),prior.end());}
        ALMetalWorldOpaquePostDeferredScene forward;forward.batches=layers.data();forward.batch_count=layers.size();
        ALMetalWorldTemporalMotionScene scene;
        scene.width=scene.height=side;scene.opaque_scene=&world;scene.forward_scene=&forward;
        scene.depth=gbuffer->depthStencilTexture();scene.opaque_depth=snapshot->colorTexture(0);
        scene.previous_opaque=&previous_opaque;scene.previous_opaque_count=1;
        scene.previous_forward=prior.data();scene.previous_forward_count=prior.size();
        scene.reset=false;scene.reverse_z=false;identity(scene.clip_to_previous_clip);identity(scene.inverse_projection);
        auto result=f.renderer->renderTemporalMotion(scene,&f.error);INFO(f.error);REQUIRE(result);
        std::vector<std::array<__fp16,2>> motion(side*side);std::vector<float> depth(side*side);
        std::vector<std::uint8_t> rejection(side*side);
        REQUIRE(result->colorTexture(0)->readback(*f.device,0,0,{},motion.data(),side*4,&f.error));
        REQUIRE(result->colorTexture(2)->readback(*f.device,0,0,{},depth.data(),side*4,&f.error));
        REQUIRE(result->colorTexture(1)->readback(*f.device,0,0,{},rejection.data(),side,&f.error));
        CHECK(std::abs(float(motion[16*side+16][0])+2.f)<.025f);
        CHECK(std::abs(depth[16*side+16]-.375f)<1.e-6f);
        CHECK(rejection[16*side+16]==255); // Differently moving layers still reject history in either order.
        REQUIRE(gbuffer->depthStencilTexture()->readback(*f.device,0,0,{},depth.data(),side*4,&f.error));
        CHECK(std::abs(depth[16*side+16]-.625f)<1.e-6f);
        for(unsigned invalid:{0u,1u})
        {
            CAPTURE(invalid);
            if(invalid==0){for(auto& previous:prior)previous.valid=false;}
            else {for(auto& previous:prior)previous.valid=true;scene.reset=true;}
            auto rejected=f.renderer->renderTemporalMotion(scene,&f.error);REQUIRE(rejected);
            REQUIRE(rejected->colorTexture(1)->readback(*f.device,0,0,{},rejection.data(),side,&f.error));
            REQUIRE(rejected->colorTexture(2)->readback(*f.device,0,0,{},depth.data(),side*4,&f.error));
            CHECK(rejection[16*side+16]==255);
            CHECK(std::abs(depth[16*side+16]-.375f)<1.e-6f); // Current depth remains valid while previous history is rejected.
        }
    }
}

TEST_CASE("Temporal partial alpha rejects motion chaining and selects strict nearest contributing depth", "[almetal][temporal][motion][transparency][depth][fixed-anchor]")
{
    for (bool reverse_z : {false, true}) for (bool reverse_order : {false, true})
        for (bool close_depths : {false, true})
    {
        CAPTURE(reverse_z, reverse_order, close_depths);
        Fixture f;
        // These are genuinely contributing transparent layers. Alpha1 would
        // replace preceding color in a read-only-depth alpha pass; requiring
        // hidden layers to keep rejecting that opaque result was a bad oracle.
        for (auto& color : f.geometry.colors) color[3] = 191;
        f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
        auto opaque = f.batch(0); auto previous_opaque = previousDraw(opaque);
        auto world = f.scene(); world.batches = &opaque; world.batch_count = 1; world.reverse_z = reverse_z;
        auto gbuffer = f.renderer->render(world, &f.error); INFO(f.error); REQUIRE(gbuffer);
        auto snapshot = f.renderer->prepareTemporalOpaqueDepth(side, side, &f.error); REQUIRE(snapshot);
        REQUIRE(f.renderer->beginCommandBatch("Fixed-anchor alpha fixture snapshot", &f.error));
        REQUIRE(f.renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(), *snapshot, &f.error));
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        std::array<ALMetalWorldSimpleBatch, 3> layers;
        std::array<ALMetalWorldTemporalPreviousDraw, 3> prior;
        for (unsigned i = 0; i < layers.size(); ++i)
        {
            auto& layer = layers[i]; layer = f.batch(i == 1 ? 2 : 0);
            layer.post_water_alpha = true; layer.pre_water_pbr = i == 1;
            layer.pre_water_fullbright = i != 1; layer.pre_water_minimum_alpha = .004f;
            if (i == 1) layer.pbrSlots()[0].alpha_cutoff = -1.f;
            // The close-depth arm catches a tolerant nearest test that drifts
            // through successively farther layers. All motions agree with the
            // opaque anchor there, so it must remain fully history-compatible.
            const float depth = close_depths ? (reverse_z ? .8f - i * .0000015f : .2f + i * .0000015f)
                : (reverse_z ? .5f + i * .1f : .5f - i * .1f);
            layer.modelview_projection[14] = (reverse_z ? depth : depth * 2.f - 1.f) - .25f;
            prior[i] = previousDraw(layer);
            const float motion = close_depths ? 0.f : i * .020f;
            prior[i].modelview_projection[12] = motion / (side * .5f);
        }
        if (reverse_order) { std::reverse(layers.begin(), layers.end()); std::reverse(prior.begin(), prior.end()); }
        ALMetalWorldOpaquePostDeferredScene forward; forward.batches = layers.data(); forward.batch_count = layers.size();
        ALMetalWorldTemporalMotionScene scene;
        scene.width = scene.height = side; scene.opaque_scene = &world; scene.forward_scene = &forward;
        scene.depth = gbuffer->depthStencilTexture(); scene.opaque_depth = snapshot->colorTexture(0);
        scene.previous_opaque = &previous_opaque; scene.previous_opaque_count = 1;
        scene.previous_forward = prior.data(); scene.previous_forward_count = prior.size();
        scene.reset = false; scene.reverse_z = reverse_z; identity(scene.clip_to_previous_clip); identity(scene.inverse_projection);
        auto result = f.renderer->renderTemporalMotion(scene, &f.error); INFO(f.error); REQUIRE(result);
        std::vector<std::array<__fp16, 2>> motion(side * side); std::vector<float> depth(side * side);
        std::vector<std::uint8_t> rejection(side * side);
        REQUIRE(result->colorTexture(0)->readback(*f.device, 0, 0, {}, motion.data(), side * 4, &f.error));
        REQUIRE(result->colorTexture(2)->readback(*f.device, 0, 0, {}, depth.data(), side * 4, &f.error));
        REQUIRE(result->colorTexture(1)->readback(*f.device, 0, 0, {}, rejection.data(), side, &f.error));
        constexpr auto center = 16 * side + 16;
        CHECK(std::abs(float(motion[center][0]) - (close_depths ? 0.f : .040f)) < .0001f);
        CHECK(std::abs(depth[center] - (close_depths ? (reverse_z ? .8f : .2f) : (reverse_z ? .7f : .3f))) < .0000001f);
        CHECK(rejection[center] == (close_depths ? 0 : 255));
        // .000 -> .020 -> .040 cannot chain through a rolling tolerance.
        // The close-depth arm independently tests strict nearest selection
        // with exactly equal motion, rather than conflating the two policies.
    }
}

TEST_CASE("Temporal cloth motion uses prior submitted procedural deformation parameters", "[almetal][temporal][motion][avatar]")
{
    Fixture f;
    for (auto& clothing : f.geometry.cloth) { clothing[0] = .2f; clothing[3] = 1.f; }
    f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
    auto batch = f.batch(6); batch.avatar_cloth = true; batch.cloth_offset = offsetof(Fixture::Geometry, cloth);
    auto previous = previousDraw(batch);
    // Unit +Z source normal, gravity .1 gives .15 cloth displacement factor; .2*.15=.03 NDC, .48 input pixels.
    batch.avatar_gravity[2] = .1f;
    auto pixels = motionPixels(f, batch, previous);
    CHECK(std::abs(pixels.x(16, 16) + .48f) < .025f);
    CHECK(std::abs(pixels.y(16, 16)) < .025f);
    CHECK(pixels.reactive[16 * side + 16] == 0);
}

TEST_CASE("Temporal water rejection is bounded by the opaque eye-ray intersection", "[almetal][temporal][motion][reactive]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    auto behind = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .5f);
    auto front = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .1f);
    CHECK(behind.reactive[16 * side + 16] == 0);
    CHECK(front.reactive[16 * side + 16] == 255);
    CHECK(std::abs(front.x(16, 16)) < .02f);
    CHECK(std::abs(front.depth[16 * side + 16] - .625f) < 1.e-6f);
    auto crest = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .5f, false, .4f);
    CHECK(crest.reactive[16 * side + 16] == 255); // nearer wave bound intersects before the opaque surface
    auto near_water = motionPixels(f, batch, previous, 0, 0, 0, nullptr, .1f, false, .2f);
    CHECK(std::all_of(near_water.reactive.begin(), near_water.reactive.end(), [](auto value) { return value == 255; }));
}

TEST_CASE("Temporal reversed depth retains motion sign and device depth", "[almetal][temporal][motion][depth]")
{
    Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
    batch.modelview_projection[13] = .125f;
    auto pixels = motionPixels(f, batch, previous, 0, .25f, -.25f, nullptr, 0, true);
    CHECK(std::abs(pixels.y(16, 17) + 2.f) < .02f);
    CHECK(std::abs(pixels.x(16, 17)) < .02f);
    CHECK(std::abs(pixels.depth[17 * side + 16] - .25f) < 1.e-6f);
    CHECK(pixels.reactive[17 * side + 16] == 0);
    CHECK(pixels.depth[31 * side + 1] == 0.f);
}

TEST_CASE("Temporal reconstruction consumes production rigid and avatar motion with jittered color", "[almetal][temporal][motion][integration]")
{
    for (unsigned family : {0u, 6u})
    {
        CAPTURE(family);
        Fixture f;
        auto scaler = ALMetalTemporalScaler::create(*f.device, &f.error); INFO(f.error); REQUIRE(scaler);
        ALMetalTemporalScalerDescriptor descriptor;
        descriptor.input_width = descriptor.input_height = side;
        descriptor.output_width = descriptor.output_height = side * 2;
        ALMetalTemporalScalerRequirements requirements;
        REQUIRE(scaler->prepare(descriptor, requirements, &f.error)); INFO(f.error);
        REQUIRE(f.renderer->prepareTemporalMotion(&f.error));
        auto hdr = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
        auto reconstructed = f.renderer->prepareTemporalTarget(side * 2, side * 2, true, &f.error);
        auto resolved = f.renderer->prepareTemporalTarget(side * 2, side * 2, false, &f.error);
        REQUIRE(hdr); REQUIRE(reconstructed); REQUIRE(resolved);
        auto batch = f.batch(family);
        auto previous = previousDraw(batch);
        std::array<float, 2> centroids{};
        for (unsigned frame_index = 0; frame_index != 2; ++frame_index)
        {
            if (frame_index)
            {
                if (family == 6) for (unsigned joint = 0; joint != 15; ++joint) f.avatar_pose[joint * 4 + 3] = .125f;
                else batch.modelview_projection[12] = .125f;
            }
            const float jitter_x = frame_index ? -.25f : 0.f;
            const float jitter_y = frame_index ? .16666667f : -.16666667f;
            auto world = f.scene(); world.batches = &batch; world.batch_count = 1;
            world.temporal_jitter_pixels[0] = jitter_x; world.temporal_jitter_pixels[1] = jitter_y;
            REQUIRE(f.renderer->beginCommandBatch("Production avatar Temporal fixture", &f.error));
            auto gbuffer = f.renderer->render(world, &f.error); INFO(f.error); REQUIRE(gbuffer);
            // This independently authored fixture uses the production G-buffer's decoded material color as linear
            // HDR scene input. Actual world lighting/post orchestration has its separate presentation fixture.
            REQUIRE(f.renderer->resolveTemporalColor(*gbuffer, *gbuffer, *hdr, &f.error));
            ALMetalWorldTemporalMotionScene motion_scene;
            motion_scene.width = motion_scene.height = side; motion_scene.opaque_scene = &world;
            motion_scene.depth = gbuffer->depthStencilTexture(); motion_scene.reverse_z = false;
            motion_scene.reset = frame_index == 0; identity(motion_scene.clip_to_previous_clip);
            motion_scene.jitter_pixels[0] = jitter_x; motion_scene.jitter_pixels[1] = jitter_y;
            motion_scene.previous_opaque = &previous; motion_scene.previous_opaque_count = 1;
            auto motion = f.renderer->renderTemporalMotion(motion_scene, &f.error); INFO(f.error); REQUIRE(motion);
            ALMetalTemporalScalerFrame frame; frame.depth_reversed = false;
            frame.jitter_x = jitter_x; frame.jitter_y = jitter_y; frame.reset = frame_index == 0;
            ALMetalTemporalScaler::Ticket ticket = 0;
            REQUIRE(f.renderer->encodeTemporalUpscale(*scaler, descriptor, *hdr->colorTexture(0), *motion,
                *reconstructed, frame, ticket, &f.error)); INFO(f.error);
            REQUIRE(f.renderer->resolveTemporalColor(*hdr, *reconstructed, *resolved, &f.error));
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); INFO(f.error);
            REQUIRE(scaler->submitted(ticket, &f.error));
            std::vector<__fp16> pixels(side * 2 * side * 2 * 4);
            REQUIRE(resolved->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 2 * 8, &f.error));
            float sum = 0.f; unsigned count = 0;
            for (unsigned y = 0; y < side * 2; ++y) for (unsigned x = 0; x < side * 2; ++x)
            {
                const auto* pixel = pixels.data() + (y * side * 2 + x) * 4;
                CHECK(std::isfinite(float(pixel[0]))); CHECK(std::isfinite(float(pixel[1]))); CHECK(std::isfinite(float(pixel[2])));
                if (float(pixel[0]) > .2f && float(pixel[1]) < .15f && float(pixel[2]) < .1f)
                { sum += float(x); ++count; }
            }
            REQUIRE(count > 300); centroids[frame_index] = sum / float(count);
            previous = previousDraw(batch);
        }
        // Known .125 clip-space translation is 2 input / 4 output pixels. Reconstruction may soften the boundary;
        // the centroid must follow the independently moving skin rather than leave its prior silhouette behind.
        CHECK(std::abs((centroids[1] - centroids[0]) - 4.f) < 1.25f);
        CHECK(scaler->stats().encoded_calls == 2);
        CHECK(scaler->stats().submitted_calls == 2);
    }
}

TEST_CASE("Temporal rejects behind-camera and half-float-overflow previous projections", "[almetal][temporal][motion][disocclusion]")
{
    for (unsigned invalid_kind : {0u, 1u})
    {
        CAPTURE(invalid_kind);
        Fixture f; auto batch = f.batch(0); auto previous = previousDraw(batch);
        if (invalid_kind == 0) previous.modelview_projection[15] = -1.f;
        else previous.modelview_projection[12] = 1.e9f;
        auto pixels = motionPixels(f, batch, previous);
        CHECK(pixels.reactive[16 * side + 16] == 255);
        CHECK(std::isfinite(pixels.x(16, 16)));
        CHECK(std::isfinite(pixels.y(16, 16)));
        CHECK(pixels.x(16, 16) == 0.f); CHECK(pixels.y(16, 16) == 0.f);
    }
}
