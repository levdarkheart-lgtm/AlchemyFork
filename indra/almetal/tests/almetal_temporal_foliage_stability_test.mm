/**
 * @file almetal_temporal_foliage_stability_test.mm
 * @brief Stationary cutout/grass detail over complete production Temporal jitter cycles.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetaltemporalworldhistory.h"
#include "almetal_temporal_direct_fixture.h"
#include "../../alrender/alrenderresourceinputs.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string_view>

namespace
{
constexpr unsigned InputWidth = 256, InputHeight = 128;
constexpr unsigned OutputWidth = 384, OutputHeight = 192;
constexpr unsigned Phases = 18, WarmFrames = 2 * Phases, StableFrames = 3 * Phases;
using Color = std::array<float, 4>;
using Texel = std::array<std::uint8_t, 4>;
struct Region { const char* name; unsigned x, y; bool foliage, identifiable_coverage; };
constexpr std::array<Region, 8> Regions{{
    {"generic_mask_sky",0,0,true,true}, {"legacy_mask_opaque",96,0,true,true},
    {"pbr_mask_sky",192,0,true,true}, {"blended_leaf_sky",288,0,true,true},
    {"grass_sky",0,96,true,true}, {"dense_mask_opaque",96,96,true,false},
    {"opaque_texture_control",192,96,false,false}, {"straight_edge_control",288,96,false,false}}};

bool leaf(const Color& p) { return p[1] > p[0] * 1.4f && p[1] > p[2] * 1.4f; }
float luminance(const Color& p) { return .2126f*p[0] + .7152f*p[1] + .0722f*p[2]; }

struct FramePixels
{
    std::vector<Color> input, output, native_output;
    std::vector<float> depth, motion_depth, opaque_depth;
    std::vector<std::array<__fp16,2>> motion;
    std::vector<std::uint8_t> reactive;
    std::array<float,2> jitter{};
    unsigned phase = 0;
    unsigned input_width = InputWidth, input_height = InputHeight;
    std::size_t previous_valid = 0;
    std::uint64_t resets = 0;
};
struct CoverageReference
{
    std::vector<Color> color,foreground;
    std::vector<float> coverage;
    std::array<Color,Regions.size()> backgrounds{};
};
struct Stability
{
    std::uint64_t comparisons = 0, pixels = 0, coverage_changes = 0;
    double absolute_change = 0, maximum_change = 0, edge_energy = 0, leaf_coverage = 0;
    double reference_error = 0;
    std::uint64_t reactive_leaf = 0, leaf_pixels = 0, reactive_background = 0, background_pixels = 0;
    double edge_band_change=0; std::uint64_t edge_band_pixels=0;
    std::vector<double> frame_edge_changes;
    double minimum_recall=1,minimum_precision=1,minimum_area=1.e9,maximum_area=0;
    float maximum_stationary_motion = 0;
    unsigned coverage_mismatches = 0;
    std::uint64_t proxy_foreground=0,proxy_background=0,proxy_ambiguous=0;
};

std::vector<Color> readColors(const ALMetalTexture& texture, const ALMetalDevice& device, std::string& error)
{
    const auto width=texture.width(), height=texture.height();
    std::vector<__fp16> half(width*height*4);
    REQUIRE(texture.readback(device,0,0,{},half.data(),width*8,&error)); INFO(error);
    std::vector<Color> result(width*height);
    for(std::size_t i=0;i<result.size();++i)
        for(unsigned c=0;c<4;++c) result[i][c]=float(half[4*i+c]);
    return result;
}
void dumpRaw(const std::filesystem::path& path, const void* bytes, std::size_t size)
{
    std::ofstream file(path,std::ios::binary); REQUIRE(file.good());
    file.write(static_cast<const char*>(bytes),static_cast<std::streamsize>(size)); REQUIRE(file.good());
}
void dumpImage(const std::filesystem::path& path,const std::vector<Color>& pixels,unsigned width,unsigned height)
{
    std::ofstream file(path,std::ios::binary); REQUIRE(file.good());
    file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(const auto& pixel:pixels)
    {
        std::array<std::uint8_t,3> rgb;
        for(unsigned c=0;c<3;++c)
            rgb[c]=static_cast<std::uint8_t>(std::lround(std::pow(std::clamp(pixel[c],0.f,1.f),1.f/2.2f)*255.f));
        file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
    }
    REQUIRE(file.good());
}

template<ALRenderResourceInputs::MainResolutionMode Mode>
struct ScaledFoliageFixture : Fixture
{
    static constexpr auto Dimensions=[]{
        ALRenderResourceInputs input;input.mWidth=OutputWidth;input.mHeight=OutputHeight;
        input.mMainResolutionMode=Mode;return input.mainViewDimensions();
    }();
    static constexpr unsigned InputWidth=Dimensions.mSceneWidth,InputHeight=Dimensions.mSceneHeight;
    ALMetalTemporalWorldHistory history;
    std::unique_ptr<ALMetalTemporalScaler> scaler;
    ALMetalTemporalScalerDescriptor descriptor{InputWidth,InputHeight,OutputWidth,OutputHeight};
    std::unique_ptr<ALMetalRenderTarget> hdr, raw, resolved, current_gbuffer, opaque_depth;
    unsigned opaque_depth_captures=0;
    std::vector<ALMetalWorldSimpleBatch> opaque, forward;
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting alpha_lighting;
    const ALMetalTexture* black_environment = nullptr;
    std::filesystem::path output_dir;
    const float mip_bias = ALRenderResourceInputs::materialMipBias(Mode,InputWidth,OutputWidth,true);
    bool reverse_z = false;
    bool observe_native_output = false;

    explicit ScaledFoliageFixture(bool reversed = false) : reverse_z(reversed)
    {
        if(const char* path=std::getenv("ALCHEMY_TEMPORAL_FOLIAGE_LIBRARY"))
        { REQUIRE(renderer->reloadShaders(std::string(path),&error)); INFO(error); }
        if(const char* path=std::getenv("ALCHEMY_TEMPORAL_FOLIAGE_OUTPUT"))
        { output_dir=path; std::filesystem::create_directories(output_dir); }
        scaler=ALMetalTemporalScaler::create(*device,&error); REQUIRE(scaler); INFO(error);
        ALMetalTemporalScalerRequirements requirements;
        REQUIRE(scaler->prepare(descriptor,requirements,&error)); INFO(error);
        REQUIRE(renderer->prepareTemporalMotion(&error));
        opaque_depth=renderer->prepareTemporalOpaqueDepth(InputWidth,InputHeight,&error);
        REQUIRE(opaque_depth); INFO(error);
        hdr=renderer->prepareTemporalTarget(InputWidth,InputHeight,false,&error); REQUIRE(hdr);
        raw=renderer->prepareTemporalTarget(OutputWidth,OutputHeight,true,&error); REQUIRE(raw);
        resolved=renderer->prepareTemporalTarget(OutputWidth,OutputHeight,false,&error); REQUIRE(resolved);
        const auto* palm=foliageTexture(false,false,false);
        const auto* grass=foliageTexture(true,false,false);
        const auto* dense_palm=foliageTexture(false,false,true);
        const auto* blended=foliageTexture(false,true,false);
        const auto* detailed=detailTexture();
        const std::uint8_t gray[4]{90,90,90,255}, edge[4]{180,180,180,255};
        const auto* gray_texture=texture(ALMetalPixelFormat::rgba8_unorm,1,gray,4);
        const auto* edge_texture=texture(ALMetalPixelFormat::rgba8_unorm,1,edge,4);
        opaque.push_back(quad(1,100,4,188,92,.6f,gray_texture,0,false));
        opaque.push_back(quad(2,100,100,188,188,.6f,gray_texture,0,false));
        opaque.push_back(quad(10,4,4,92,92,-.35f,palm,0,true));
        // Matches the classic-tree consumer's published alpha policy. This
        // does not claim to exercise the separate LLFace CPU tree producer.
        opaque.back().use_vertex_color=false; opaque.back().alpha_cutoff_bias=false;
        opaque.push_back(quad(11,100,4,188,92,-.35f,palm,1,true));
        opaque.push_back(quad(12,196,4,284,92,-.35f,grass,2,true));
        opaque.push_back(quad(13,4,100,92,188,-.35f,grass,0,true));
        opaque.push_back(quad(14,100,100,188,188,-.35f,dense_palm,0,true));
        opaque.push_back(quad(15,196,100,284,188,-.35f,detailed,0,false));
        opaque.push_back(quad(16,292,100,334.4f,188,-.35f,edge_texture,0,false));
        auto color=quad(20,292,4,380,92,-.35f,blended,0,false);
        color.post_water_alpha=true; color.pre_water_fullbright=true; color.pre_water_minimum_alpha=.004f;
        color.double_sided=false; // Classic blended family has a front-facing production contract.

        auto depth=color; depth.post_water_depth_only=true; depth.pre_water_minimum_alpha=.33f;
        forward={color,depth};
        environment.water_plane[2]=1.f; environment.water_plane[3]=-100.f;
        environment.water_fog[0]=environment.water_fog[2]=1.f;
        environment.glow_max_y[0]=1.f; environment.glow_max_y[2]=-1.f; environment.glow_max_y[3]=1.f;
        environment.light_direction[2]=1.f;
        environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1.f;
        environment.atmos_sunlit[3]=environment.atmos_sunlit_linear[3]=1.f;
        environment.environment[0]=1.f;
        for(unsigned i=0;i<3;++i) environment.eye_to_sky[i][i]=1.f;
        alpha_lighting.sun_direction[2]=1.f;
        alpha_lighting.parameters[0]=128.f; alpha_lighting.parameters[1]=.004f; alpha_lighting.parameters[2]=1.f;
        ALMetalTextureDescriptor cube; cube.type=ALMetalTextureType::texture_cube;
        cube.format=ALMetalPixelFormat::rgba8_unorm; cube.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> black=ALMetalTexture::create(*device,cube,&error); REQUIRE(black);
        auto command=device->makeCommandBuffer("Foliage authored environment",&error); REQUIRE(command);
        const Texel black_pixel{0,0,0,255};
        for(unsigned face=0;face<6;++face) REQUIRE(black->upload(*command,0,face,{},black_pixel.data(),4,&error));
        REQUIRE(command->commitAndWait(&error)); black_environment=black.get(); resources->textures.push_back(black);
    }
    ~ScaledFoliageFixture()
    { renderer->cancelCommandBatch(); device->waitUntilIdle(); }

    const ALMetalTexture* uploadMips(std::vector<Texel> pixels)
    {
        ALMetalTextureDescriptor desc; desc.width=desc.height=256; desc.mipmap_level_count=9;
        desc.format=ALMetalPixelFormat::rgba8_unorm; desc.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> result=ALMetalTexture::create(*device,desc,&error); REQUIRE(result);
        auto command=device->makeCommandBuffer("Independently authored foliage mips",&error); REQUIRE(command);
        unsigned extent=256;
        for(unsigned level=0;level<9;++level)
        {
            REQUIRE(result->upload(*command,level,0,{},pixels.data(),extent*4,&error));
            if(extent==1)break;
            std::vector<Texel> next(extent*extent/4);
            for(unsigned y=0;y<extent/2;++y) for(unsigned x=0;x<extent/2;++x)
                for(unsigned c=0;c<4;++c)
                {
                    unsigned total=0;
                    for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)
                        total+=pixels[(2*y+dy)*extent+2*x+dx][c];
                    next[y*(extent/2)+x][c]=static_cast<std::uint8_t>((total+2)/4);
                }
            pixels=std::move(next); extent/=2;
        }
        REQUIRE(command->commitAndWait(&error)); resources->textures.push_back(result); return result.get();
    }
    const ALMetalTexture* foliageTexture(bool grass,bool blended,bool textured)
    {
        std::vector<Texel> pixels(256*256);
        for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x)
        {
            const float u=float(x)+.5f,v=float(y)+.5f;
            bool covered=false;
            if(grass)
            {
                for(unsigned stem=0;stem<19;++stem)
                {
                    const float root=6.f+stem*13.1f;
                    const float curve=root+(.14f+.012f*float(stem%4))*(255.f-v)+4.f*std::sin(v*.012f+stem);
                    const float thickness=.7f+2.5f*v/256.f;
                    covered|=v>12.f+float(stem%4)*15.f && std::abs(u-curve)<thickness;
                }
            }
            else
            {
                const float stem=128.f+8.f*std::sin(v*.018f);
                covered=std::abs(u-stem)<1.6f;
                for(unsigned frond=0;frond<24;++frond)
                {
                    const float base=10.f+float(frond)*9.8f;
                    const float dx=std::abs(u-stem),line=base+.29f*dx;
                    covered|=dx<115.f-2.f*std::abs(float(frond)-11.f) && std::abs(v-line)<1.7f;
                }
            }
            // Coverage ROIs have constant RGB on both sides of each alpha
            // boundary, so color unmixing can identify reconstructed coverage.
            // A separate dense masked ROI retains high-frequency RGB detail.
            const float detail=textured?.72f+.18f*std::sin(u*.7f)*std::cos(v*.59f):1.f;
            pixels[y*256+x]={static_cast<std::uint8_t>(20*detail),static_cast<std::uint8_t>(210*detail),
                static_cast<std::uint8_t>(35*detail),static_cast<std::uint8_t>(covered?(blended?190:255):0)};
        }
        return uploadMips(std::move(pixels));
    }
    const ALMetalTexture* detailTexture()
    {
        std::vector<Texel> pixels(256*256);
        for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x)
        {
            const unsigned value=80+unsigned(100.f*(.5f+.5f*std::sin((float(x)+.5f)*.69f)*std::cos((float(y)+.5f)*.53f)));
            pixels[y*256+x]={static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value),255};
        }
        return uploadMips(std::move(pixels));
    }
    ALMetalWorldSimpleBatch quad(std::uint64_t id,float left,float top,float right,float bottom,
        float z,const ALMetalTexture* image,unsigned family,bool mask)
    {
        auto b=batch(family); Geometry g=geometry;
        const float positions[4][4]{{2*left/OutputWidth-1,2*top/OutputHeight-1,z,1},
            {2*right/OutputWidth-1,2*top/OutputHeight-1,z,1},
            {2*right/OutputWidth-1,2*bottom/OutputHeight-1,z,1},
            {2*left/OutputWidth-1,2*bottom/OutputHeight-1,z,1}};
        const float uv[4][2]{{0,0},{1,0},{1,1},{0,1}};
        std::memcpy(g.positions,positions,sizeof(positions));std::memcpy(g.uv,uv,sizeof(uv));
        std::memset(g.colors,255,sizeof(g.colors));
        const std::array<std::uint16_t,6> order{0,1,2,0,2,3};
        b.vertices=buffer(&g,sizeof(g));b.indices=buffer(order.data(),sizeof(order));
        b.index_count=6;b.vertex_count=4;b.diffuse_textures[0]=image;
        b.temporal_instance_id=id;b.temporal_generation=1;b.double_sided=true;
        b.alpha_mask=mask && family!=1;b.alpha_cutoff=.5f;b.alpha_cutoff_bias=true;
        identity(b.texture_matrix);
        if(family==1){b.material_alpha_mode=2;b.materialSlots()[0].alpha_cutoff=.5f;b.materialSlots()[0].env_intensity=0;}
        if(family==2)
        {
            b.pbrSlots()[0].alpha_cutoff=.5f;
            b.pbrSlots()[0].emissive_texture=Fixture::color;
            std::fill_n(b.pbrSlots()[0].emissive_color,3,0.f);
        }
        return b;
    }
    struct World
    {
        std::vector<ALMetalWorldSimpleBatch> opaque,forward;
        ALMetalWorldGBufferScene gbuffer;
        ALMetalWorldOpaquePostDeferredScene forward_scene;
    };
    void cameraMatrix(float* matrix,float camera_shift) const
    {
        identity(matrix); matrix[12]=camera_shift;
        // The normal path converts GL clip depth in its vertex shader. The
        // reverse-Z path accepts Metal depth directly, so supply the equivalent
        // reversed projection explicitly: z_metal = (w_gl - z_gl) / 2.
        if(reverse_z){matrix[10]=-.5f;matrix[14]=.5f;}
    }
    void usePBRPalm(bool finite_background,float background_gl_depth=.6f)
    {
        const auto palm=std::find_if(opaque.begin(),opaque.end(),[](const auto& b){return b.temporal_instance_id==10;});
        const auto pbr=std::find_if(opaque.begin(),opaque.end(),[](const auto& b){return b.temporal_instance_id==12;});
        REQUIRE(palm!=opaque.end()); REQUIRE(pbr!=opaque.end());
        REQUIRE(pbr->pbr_slot_count==1); REQUIRE(pbr->alpha_mask);
        pbr->diffuse_textures[0]=palm->diffuse_textures[0];
        if(finite_background)
        {
            // Copy the actual linear-HDR clear color, rather than choosing a
            // nominally similar blue or reinterpreting encoded bytes as linear.
            // The paired arm requires exact MetalFX-input RGBA equality.
            const auto clear=native(0.f).front();std::array<__fp16,4> linear;
            for(unsigned c=0;c<4;++c)linear[c]=clear[c];
            const auto* blue=texture(ALMetalPixelFormat::rgba16_float,1,linear.data(),8);
            auto background=quad(90,196,4,284,92,background_gl_depth,blue,1,false);
            background.material_alpha_mode=0;background.materialSlots()[0].fullbright=1.f;
            opaque.insert(opaque.begin(),background);
        }
    }
    World world(unsigned width,unsigned height,float camera_shift,float bias,std::array<float,2> jitter)
    {
        World w;w.opaque=opaque;w.forward=forward;
        for(auto* list:{&w.opaque,&w.forward})for(auto& b:*list)cameraMatrix(b.modelview_projection,camera_shift);
        w.gbuffer=scene();w.gbuffer.width=width;w.gbuffer.height=height;
        w.gbuffer.reverse_z=reverse_z;
        w.gbuffer.hdr=true;w.gbuffer.material_mip_bias=bias;w.gbuffer.clear_color[0]=.035f;
        w.gbuffer.clear_color[1]=.075f;w.gbuffer.clear_color[2]=.21f;
        std::copy(jitter.begin(),jitter.end(),w.gbuffer.temporal_jitter_pixels);
        w.gbuffer.source_owner=resources;
        w.forward_scene.width=width;w.forward_scene.height=height;w.forward_scene.hdr=true;
        w.forward_scene.reverse_z=reverse_z;
        w.forward_scene.material_mip_bias=bias;w.forward_scene.source_owner=resources;
        w.forward_scene.alpha_environment=&environment;w.forward_scene.alpha_lighting=&alpha_lighting;
        w.forward_scene.legacy_environment=black_environment;
        // Raw pointers are rebound after the movable World reaches its final storage.
        return w;
    }
    static void bind(World& w)
    {
        w.gbuffer.batches=w.opaque.data();w.gbuffer.batch_count=w.opaque.size();
        w.forward_scene.batches=w.forward.data();w.forward_scene.batch_count=w.forward.size();
    }
    std::unique_ptr<ALMetalRenderTarget> color(World& w,ALMetalRenderTarget& composed,
        std::array<float,2> jitter)
    {
        bind(w);
        auto gbuffer=renderer->render(w.gbuffer,&error); INFO(error); REQUIRE(gbuffer);
        if(w.gbuffer.width==InputWidth && w.gbuffer.height==InputHeight)
        {
            // The main production owner snapshots opaque depth before its
            // borrowed-depth continuation can write alpha-depth coverage.
            REQUIRE(renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(),*opaque_depth,&error));
            INFO(error); ++opaque_depth_captures;
        }
        // Unlit material fixture: production G-buffer color/depth/coverage is copied to HDR.
        // This isolates reconstruction from lighting noise while retaining real masked material shaders.
        REQUIRE(renderer->resolveTemporalColor(*gbuffer,*gbuffer,composed,&error)); INFO(error);
        auto color_batches=w.forward;
        for(auto& b:color_batches) alMetalApplyTemporalJitter(b.modelview_projection,jitter[0],jitter[1],
            w.gbuffer.width,w.gbuffer.height);
        // Production BASE carries borrowed G-buffer depth into the owned
        // forward continuation. Reproduce that exact contract while keeping
        // this fixture's color unlit: alpha-depth writes must be visible to
        // the subsequent Temporal camera/motion pass, not a private depth copy.
        auto continuation_descriptor=composed.descriptor();
        continuation_descriptor.colors[0].texture=composed.colorTexture(0);
        continuation_descriptor.depth_stencil_format=gbuffer->descriptor().depth_stencil_format;
        continuation_descriptor.depth_stencil_shader_read=true;
        continuation_descriptor.depth_stencil_texture=gbuffer->depthStencilTexture();
        continuation_descriptor.label="Foliage production borrowed-depth continuation";
        auto continuation=ALMetalRenderTarget::create(*device,continuation_descriptor,&error);
        REQUIRE(continuation); INFO(error);
        auto forward_color=w.forward_scene;forward_color.batches=color_batches.data();
        forward_color.gbuffer=gbuffer.get();forward_color.composed_lighting=continuation.get();
        auto result=renderer->renderOpaquePostDeferred(forward_color,std::move(continuation),&error);
        INFO(error); REQUIRE(result);
        REQUIRE(result->depthStencilTexture()==gbuffer->depthStencilTexture());
        current_gbuffer=std::move(gbuffer);
        return result;
    }
    std::vector<Color> native(float camera_shift)
    {
        data->beginFrame(10000);
        auto composed=renderer->prepareTemporalTarget(OutputWidth,OutputHeight,false,&error); REQUIRE(composed);
        REQUIRE(renderer->beginCommandBatch("Foliage independent native-detail reference",&error));
        auto w=world(OutputWidth,OutputHeight,camera_shift,0.f,{0,0});
        auto result=color(w,*composed,{0,0});
        REQUIRE(renderer->commitCommandBatch(&error)); INFO(error);
        return readColors(*result->colorTexture(0),*device,error);
    }
    CoverageReference coverageReference(float camera_shift)
    {
        // A single 4x-output render would need a positive material bias, which
        // the production policy correctly rejects. Four 2x-output renders at
        // quarter-pixel offsets produce the identical regular 4x sample grid
        // with bias0. One-mip-coarser diagnostic sampling uses sixteen 1x
        // renders; no production sampling validation is bypassed.
        unsigned scale=2;
        float reference_bias=mip_bias+std::log2(float(OutputWidth*scale)/float(InputWidth));
        if(reference_bias>2.e-6f)
        {scale=1;reference_bias=mip_bias+std::log2(float(OutputWidth)/float(InputWidth));}
        REQUIRE(reference_bias<=2.e-6f);
        reference_bias=std::min(reference_bias,0.f); // Theoretical zero may round positive by one float ULP.
        const unsigned width=OutputWidth*scale,height=OutputHeight*scale,phases=4/scale;
        CoverageReference truth;truth.color.resize(OutputWidth*OutputHeight);
        truth.foreground.resize(OutputWidth*OutputHeight);truth.coverage.resize(OutputWidth*OutputHeight);
        std::vector<unsigned> covered_samples(OutputWidth*OutputHeight);
        std::array<Color,Regions.size()> foreground_sum{},background_sum{};
        std::array<unsigned,Regions.size()> foreground_count{},background_count{};
        Color blend_max{};
        for(unsigned py=0;py<phases;++py)for(unsigned px=0;px<phases;++px)
        {
            const std::array<float,2> jitter{(float(px)+.5f)/float(phases)-.5f,
                (float(py)+.5f)/float(phases)-.5f};
            data->beginFrame(10001+py*phases+px);
            auto composed=renderer->prepareTemporalTarget(width,height,false,&error); REQUIRE(composed);
            REQUIRE(renderer->beginCommandBatch("Independent matched-LOD coverage supersamples",&error));
            auto w=world(width,height,camera_shift,reference_bias,jitter);
            auto target=color(w,*composed,jitter);
            REQUIRE(renderer->commitCommandBatch(&error)); INFO(error);
            const auto high=readColors(*target->colorTexture(0),*device,error);
            std::vector<float> depth(width*height);
            REQUIRE(current_gbuffer->depthStencilTexture()->readback(*device,0,0,{},depth.data(),width*4,&error));
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
            {
                const auto sample=y*width+x,ox=x/scale,oy=y/scale,i=oy*OutputWidth+ox;
                const auto region=(oy/96)*4+ox/96;const auto& r=Regions[region];
                for(unsigned c=0;c<4;++c)truth.color[i][c]+=high[sample][c]/16.f;
                if(reverse_z?depth[sample]>.5f:depth[sample]<.5f)
                {
                    ++covered_samples[i];truth.coverage[i]+=1.f/16.f;
                    for(unsigned c=0;c<4;++c)truth.foreground[i][c]+=high[sample][c];
                }
                if(ox>=r.x+6 && ox<r.x+90 && oy>=r.y+6 && oy<r.y+90)
                {
                    if(reverse_z?depth[sample]>.5f:depth[sample]<.5f)
                    {for(unsigned c=0;c<4;++c)foreground_sum[region][c]+=high[sample][c];++foreground_count[region];}
                    else
                    {for(unsigned c=0;c<4;++c)background_sum[region][c]+=high[sample][c];++background_count[region];}
                }
                if(region==3 && high[sample][1]>blend_max[1])blend_max=high[sample];
            }
        }
        std::array<Color,Regions.size()> foreground{};
        for(std::size_t region=0;region<Regions.size();++region)
        {
            if(foreground_count[region])for(unsigned c=0;c<4;++c)
                foreground[region][c]=foreground_sum[region][c]/foreground_count[region];
            if(background_count[region])for(unsigned c=0;c<4;++c)
                truth.backgrounds[region][c]=background_sum[region][c]/background_count[region];
        }
        // This authored constant-color .745-alpha card has a separate RGB-linear
        // coverage reference. The viewer's .33 alpha-depth auxiliary and the
        // Temporal .004 contributing-surface proxy have distinct contracts.
        truth.backgrounds[3]=truth.backgrounds[0];
        const auto& sky=truth.backgrounds[3];
        for(unsigned c=0;c<3;++c)foreground[3][c]=(blend_max[c]-sky[c]*(1.f-190.f/255.f))/(190.f/255.f);
        for(unsigned y=0;y<OutputHeight;++y)for(unsigned x=0;x<OutputWidth;++x)
        {
            const auto i=y*OutputWidth+x,region=(y/96)*4+x/96;
            if(covered_samples[i])for(float& value:truth.foreground[i])value/=covered_samples[i];
            else truth.foreground[i]=foreground[region];
            if(region==3)
            {
                truth.foreground[i]=foreground[region];float numerator=0,denominator=0;
                for(unsigned c=0;c<3;++c)
                {const float d=foreground[region][c]-sky[c];numerator+=(truth.color[i][c]-sky[c])*d;denominator+=d*d;}
                truth.coverage[i]=denominator>1.e-6f?std::clamp(numerator/denominator,0.f,1.f):0.f;
            }
        }
        return truth;
    }
    FramePixels render(unsigned frame_index,float camera_shift)
    {
        data->beginFrame(frame_index+1);
        auto w=world(InputWidth,InputHeight,camera_shift,mip_bias,{0,0});bind(w);
        ALMetalTemporalWorldHistory::FrameInput input;
        input.world_id=frame_index+1;input.input_width=InputWidth;input.input_height=InputHeight;
        input.output_width=OutputWidth;input.output_height=OutputHeight;input.mode=static_cast<int>(Mode);
        input.reverse_z=reverse_z;cameraMatrix(input.camera_mvp,camera_shift);
        REQUIRE(history.beginFrame(input,&error)); INFO(error);
        REQUIRE(history.stage(w.gbuffer,w.forward_scene,&error)); INFO(error);
        FramePixels pixels;pixels.input_width=InputWidth;pixels.input_height=InputHeight;pixels.jitter=history.jitterPixels();
        pixels.phase=history.stats().jitter_phase;pixels.previous_valid=history.stats().valid_previous_draws;
        REQUIRE(history.jitterPhaseCount()==ALMetalTemporalWorldHistory::recommendedJitterPhaseCount(InputWidth,InputHeight,OutputWidth,OutputHeight));
        if(frame_index) REQUIRE(pixels.previous_valid==opaque.size()+forward.size());
        std::copy(pixels.jitter.begin(),pixels.jitter.end(),w.gbuffer.temporal_jitter_pixels);
        REQUIRE(renderer->beginMainFrameBatch(frame_index+1,"Production stationary foliage Temporal",&error));
        auto color_target=color(w,*hdr,pixels.jitter);
        ALMetalWorldTemporalMotionScene motion_scene;
        motion_scene.width=InputWidth;motion_scene.height=InputHeight;
        motion_scene.opaque_scene=&w.gbuffer;motion_scene.forward_scene=&w.forward_scene;
        // Final viewer G-buffer depth remains authoritative for its own
        // consumers; the Temporal output builds a separate foreground proxy.
        motion_scene.depth=current_gbuffer->depthStencilTexture();motion_scene.reverse_z=reverse_z;
        motion_scene.opaque_depth=opaque_depth->colorTexture(0);
        motion_scene.reset=history.resetRequired();
        std::copy(history.clipToPrevious().begin(),history.clipToPrevious().end(),motion_scene.clip_to_previous_clip);
        std::copy(pixels.jitter.begin(),pixels.jitter.end(),motion_scene.jitter_pixels);
        identity(motion_scene.inverse_projection);
        if(reverse_z){motion_scene.inverse_projection[10]=-2.f;motion_scene.inverse_projection[14]=1.f;}
        const auto& previous_opaque=history.previousOpaque();const auto& previous_forward=history.previousForward();
        motion_scene.previous_opaque=previous_opaque.data();motion_scene.previous_opaque_count=previous_opaque.size();
        motion_scene.previous_forward=previous_forward.data();motion_scene.previous_forward_count=previous_forward.size();
        auto motion_target=renderer->renderTemporalMotion(motion_scene,&error); INFO(error); REQUIRE(motion_target);
        ALMetalTemporalScalerFrame frame;frame.depth_reversed=reverse_z;frame.reset=history.resetRequired();
        frame.jitter_x=pixels.jitter[0];frame.jitter_y=pixels.jitter[1];ALMetalTemporalScaler::Ticket ticket=0;
        REQUIRE(renderer->encodeTemporalUpscale(*scaler,descriptor,*color_target->colorTexture(0),*motion_target,*raw,frame,ticket,&error)); INFO(error);
        REQUIRE(renderer->resolveTemporalColor(*color_target,*raw,*resolved,&error,pixels.jitter.data())); INFO(error);
        REQUIRE(renderer->commitCommandBatch(&error)); INFO(error);
        REQUIRE(scaler->submitted(ticket,&error));REQUIRE(history.commit(frame_index+1,&error));
        pixels.resets=history.stats().resets;
        pixels.input=readColors(*color_target->colorTexture(0),*device,error);
        pixels.output=readColors(*resolved->colorTexture(0),*device,error);
        // Read the distinct vendor-written texture before another world can
        // reuse it. Resolve samples it read-only; this captures its original
        // RGB/alpha independently of Alchemy's resolve destination.
        if(observe_native_output)
        {
            REQUIRE(raw->colorTexture(0)->nativeHandle()!=resolved->colorTexture(0)->nativeHandle());
            pixels.native_output=readColors(*raw->colorTexture(0),*device,error);
        }
        pixels.depth.resize(InputWidth*InputHeight);pixels.motion_depth.resize(InputWidth*InputHeight);
        pixels.opaque_depth.resize(InputWidth*InputHeight);
        pixels.motion.resize(InputWidth*InputHeight);pixels.reactive.resize(InputWidth*InputHeight);
        REQUIRE(current_gbuffer->depthStencilTexture()->readback(*device,0,0,{},pixels.depth.data(),InputWidth*4,&error));
        REQUIRE(opaque_depth->colorTexture(0)->readback(*device,0,0,{},pixels.opaque_depth.data(),InputWidth*4,&error));
        REQUIRE(motion_target->colorTexture(0)->readback(*device,0,0,{},pixels.motion.data(),InputWidth*4,&error));
        REQUIRE(motion_target->colorTexture(1)->readback(*device,0,0,{},pixels.reactive.data(),InputWidth,&error));
        REQUIRE(motion_target->colorTexture(2)->readback(*device,0,0,{},pixels.motion_depth.data(),InputWidth*4,&error));
        return pixels;
    }
};

using FoliageFixture=ScaledFoliageFixture<ALRenderResourceInputs::MainResolutionMode::Temporal67>;
using FoliageFixture75=ScaledFoliageFixture<ALRenderResourceInputs::MainResolutionMode::Temporal75>;
static_assert(FoliageFixture::InputWidth==InputWidth && FoliageFixture::InputHeight==InputHeight);
static_assert(FoliageFixture75::InputWidth==288 && FoliageFixture75::InputHeight==144);

double referenceEnergy(const std::vector<Color>& image,const Region& r)
{
    double energy=0;
    for(unsigned y=r.y+6;y<r.y+90;++y)for(unsigned x=r.x+6;x<r.x+90;++x)
    {
        const auto i=y*OutputWidth+x;
        energy+=std::abs(luminance(image[i])-luminance(image[i+1]));
        energy+=std::abs(luminance(image[i])-luminance(image[i+OutputWidth]));
    }
    return energy/(84.*84.);
}
bool nearLeaf(const std::vector<Color>& image,unsigned x,unsigned y,unsigned radius=1)
{
    for(int dy=-int(radius);dy<=int(radius);++dy)for(int dx=-int(radius);dx<=int(radius);++dx)
        if(leaf(image[(y+dy)*OutputWidth+x+dx]))return true;
    return false;
}
struct MaskQuality {double recall=1,precision=1,area=1;};
MaskQuality maskQuality(const std::vector<Color>& reference,const std::vector<Color>& current,const Region& r)
{
    std::uint64_t expected=0,actual=0,recalled=0,precise=0;
    for(unsigned y=r.y+6;y<r.y+90;++y)for(unsigned x=r.x+6;x<r.x+90;++x)
    {
        const bool a=leaf(reference[y*OutputWidth+x]),b=leaf(current[y*OutputWidth+x]);
        expected+=a; actual+=b;
        recalled+=a && nearLeaf(current,x,y); precise+=b && nearLeaf(reference,x,y);
    }
    return {expected?double(recalled)/expected:1.,actual?double(precise)/actual:1.,
        expected?double(actual)/expected:1.};
}
std::vector<float> recoveredCoverage(const CoverageReference& truth,const std::vector<Color>& image)
{
    std::vector<float> result(image.size());
    for(unsigned y=0;y<OutputHeight;++y)for(unsigned x=0;x<OutputWidth;++x)
    {
        const auto i=y*OutputWidth+x,region=(y/96)*4+x/96;
        const auto& foreground=truth.foreground[i];const auto& background=truth.backgrounds[region];
        float numerator=0,denominator=0;
        for(unsigned c=0;c<3;++c)
        {const float d=foreground[c]-background[c];numerator+=(image[i][c]-background[c])*d;denominator+=d*d;}
        result[i]=denominator>1.e-6f?std::clamp(numerator/denominator,0.f,1.f):0.f;
    }
    return result;
}
float nearCoverage(const std::vector<float>& mask,unsigned x,unsigned y,unsigned radius=1)
{
    float value=0;
    for(int dy=-int(radius);dy<=int(radius);++dy)for(int dx=-int(radius);dx<=int(radius);++dx)
        value=std::max(value,mask[(y+dy)*OutputWidth+x+dx]);
    return value;
}
MaskQuality maskQuality(const CoverageReference& truth,const std::vector<float>& actual,const Region& r)
{
    double expected=0,covered=0,recalled=0,precise=0;
    for(unsigned y=r.y+6;y<r.y+90;++y)for(unsigned x=r.x+6;x<r.x+90;++x)
    {
        const auto i=y*OutputWidth+x;expected+=truth.coverage[i];covered+=actual[i];
        recalled+=std::min(truth.coverage[i],nearCoverage(actual,x,y));
        precise+=std::min(actual[i],nearCoverage(truth.coverage,x,y));
    }
    return {expected?recalled/expected:1.,covered?precise/covered:1.,expected?covered/expected:1.};
}
bool nativeEdge(const std::vector<Color>& image,const Region& r,unsigned x,unsigned y)
{
    const auto& p=image[y*OutputWidth+x];
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
    {
        const auto& q=image[(y+dy)*OutputWidth+x+dx];
        if(r.foliage ? leaf(p)!=leaf(q) :
            std::abs(p[0]-q[0])+std::abs(p[1]-q[1])+std::abs(p[2]-q[2])>.045f)return true;
    }
    return false;
}
std::vector<Color> blurredReference(const std::vector<Color>& image)
{
    auto result=image;
    for(unsigned y=2;y+2<OutputHeight;++y)for(unsigned x=2;x+2<OutputWidth;++x)
    {
        Color sum{};
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
            for(unsigned c=0;c<4;++c)sum[c]+=image[(y+dy)*OutputWidth+x+dx][c]/25.f;
        result[y*OutputWidth+x]=sum;
    }
    return result;
}
struct StopQuality
{
    double native_quality_error=0, phase_matched_error=0, old_history_fraction=0;
};
struct MaskMotionContract
{
    unsigned foreground_samples=0,coverage_mismatches=0,rejected_foreground=0;
    float maximum_vector_error=0;
};
MaskMotionContract maskMotionContract(const FramePixels& frame,float expected_x,bool reverse_z)
{
    MaskMotionContract result;
    const auto& r=Regions[2];
    for(unsigned y=(r.y+6)*frame.input_height/OutputHeight;y<(r.y+90)*frame.input_height/OutputHeight;++y)
        for(unsigned x=(r.x+6)*frame.input_width/OutputWidth;x<(r.x+90)*frame.input_width/OutputWidth;++x)
        {
            const auto i=y*frame.input_width+x;
            const bool foreground=leaf(frame.input[i]);
            const float authored_depth=reverse_z?.675f:.325f;
            const bool foreground_depth=std::abs(frame.depth[i]-authored_depth)<2.e-6f;
            result.coverage_mismatches+=foreground!=foreground_depth ||
                std::abs(frame.depth[i]-frame.motion_depth[i])>2.e-6f;
            if(!foreground)continue;
            ++result.foreground_samples;result.rejected_foreground+=frame.reactive[i]!=0;
            result.maximum_vector_error=std::max(result.maximum_vector_error,
                std::max(std::abs(float(frame.motion[i][0])-expected_x),std::abs(float(frame.motion[i][1]))));
        }
    return result;
}
unsigned straightEdgeRasterMismatches(const FramePixels& frame,float camera_shift,bool reverse_z)
{
    const float offset=.5f*frame.input_width*camera_shift+frame.jitter[0];
    const float left=292.f*frame.input_width/OutputWidth+offset;
    const float right=334.4f*frame.input_width/OutputWidth+offset;
    unsigned mismatches=0;
    // Track this control's authored moving bounds. A fixed screen window
    // would include the neighboring opaque-texture tile after camera motion.
    const auto begin=static_cast<unsigned>(std::floor(left-2.f));
    const auto end=static_cast<unsigned>(std::ceil(right+2.f));
    for(unsigned y=80;y<110;++y)for(unsigned x=begin;x<end;++x)
    {
        const float center=float(x)+.5f;
        if(std::min(std::abs(center-left),std::abs(center-right))<1.e-4f)continue;
        const bool expected=center>=left && center<right;
        const float depth=frame.depth[y*frame.input_width+x];
        const bool actual=reverse_z?depth>.5f:depth<.5f;
        mismatches+=expected!=actual;
    }
    return mismatches;
}
void dumpFrame(const std::filesystem::path& directory,const std::string& name,const FramePixels& frame)
{
    if(directory.empty())return;
    dumpImage(directory/(name+"-output.ppm"),frame.output,OutputWidth,OutputHeight);
    dumpRaw(directory/(name+"-input.rgba32f"),frame.input.data(),frame.input.size()*sizeof(Color));
    dumpRaw(directory/(name+"-output.rgba32f"),frame.output.data(),frame.output.size()*sizeof(Color));
    if(!frame.native_output.empty())
        dumpRaw(directory/(name+"-native-output.rgba32f"),frame.native_output.data(),frame.native_output.size()*sizeof(Color));
    dumpRaw(directory/(name+"-depth.r32f"),frame.depth.data(),frame.depth.size()*sizeof(float));
    dumpRaw(directory/(name+"-proxy-depth.r32f"),frame.motion_depth.data(),frame.motion_depth.size()*sizeof(float));
    dumpRaw(directory/(name+"-motion.rg16f"),frame.motion.data(),frame.motion.size()*sizeof(frame.motion[0]));
    dumpRaw(directory/(name+"-reactive.r8"),frame.reactive.data(),frame.reactive.size());
}
StopQuality stopQuality(const std::vector<Color>& initial_native,const std::vector<Color>& shifted_native,
    const std::vector<Color>& initial_temporal,const std::vector<Color>& stationary_final,
    const std::vector<Color>& current,const Region& r)
{
    double native_error=0,phase_error=0,old_history=0;
    unsigned changed=0,disoccluded=0;
    for(unsigned y=r.y+6;y<r.y+90;++y)for(unsigned x=r.x+6;x<r.x+90;++x)
    {
        const auto i=y*OutputWidth+x;float delta=0;
        for(unsigned c=0;c<3;++c)delta+=std::abs(initial_native[i][c]-shifted_native[i][c])/3.f;
        if(delta>.05f)
        {
            for(unsigned c=0;c<3;++c)
            {
                native_error+=std::abs(current[i][c]-shifted_native[i][c])/3.f;
                phase_error+=std::abs(current[i][c]-stationary_final[i][c])/3.f;
            }
            ++changed;
        }
        // Old-only pixels use an independent, stationary Temporal reference at
        // the final camera and the same jitter phase. Project residual color
        // toward the original Temporal image; Native filtering differences
        // therefore cannot be mistaken for retained old-world history.
        if(leaf(initial_native[i]) && !nearLeaf(shifted_native,x,y))
        {
            float numerator=0,denominator=0;
            for(unsigned c=0;c<3;++c)
            {
                const float old=initial_temporal[i][c]-stationary_final[i][c];
                numerator+=(current[i][c]-stationary_final[i][c])*old;
                denominator+=old*old;
            }
            if(denominator>.0004f)
            {old_history+=std::clamp(numerator/denominator,0.f,1.f);++disoccluded;}
        }
    }
    return {changed?native_error/changed:0.,changed?phase_error/changed:0.,
        disoccluded?old_history/disoccluded:0.};
}

void measure(Stability& m,const FramePixels& previous,const FramePixels& current,
    const std::vector<Color>& reference,const CoverageReference& coverage_reference,const Region& r,bool reverse_z=false)
{
    ++m.comparisons;
    double frame_edge_change=0;unsigned frame_edge_pixels=0;
    const auto current_coverage=recoveredCoverage(coverage_reference,current.output);
    const auto quality=maskQuality(coverage_reference,current_coverage,r);
    m.minimum_recall=std::min(m.minimum_recall,quality.recall);
    m.minimum_precision=std::min(m.minimum_precision,quality.precision);
    m.minimum_area=std::min(m.minimum_area,quality.area);
    m.maximum_area=std::max(m.maximum_area,quality.area);
    for(unsigned y=r.y+6;y<r.y+90;++y)for(unsigned x=r.x+6;x<r.x+90;++x)
    {
        const auto i=y*OutputWidth+x;const auto& a=previous.output[i];const auto& b=current.output[i];
        float delta=0;for(unsigned c=0;c<3;++c)
        {const float d=std::abs(a[c]-b[c]);delta+=d;m.reference_error+=std::abs(b[c]-reference[i][c]);}
        delta/=3.f;m.absolute_change+=delta;m.maximum_change=std::max(m.maximum_change,double(delta));
        if(nativeEdge(reference,r,x,y)){frame_edge_change+=delta;++frame_edge_pixels;}
        m.coverage_changes+=leaf(a)!=leaf(b) && std::abs(a[1]-b[1])>.05f;
        m.leaf_coverage+=leaf(b);++m.pixels;
    }
    m.edge_energy+=referenceEnergy(current.output,r);
    m.edge_band_change+=frame_edge_change;m.edge_band_pixels+=frame_edge_pixels;
    m.frame_edge_changes.push_back(frame_edge_pixels?frame_edge_change/frame_edge_pixels:0.);
    const unsigned left=(r.x+6)*current.input_width/OutputWidth,right=(r.x+90)*current.input_width/OutputWidth;
    const unsigned top=(r.y+6)*current.input_height/OutputHeight,bottom=(r.y+90)*current.input_height/OutputHeight;
    for(unsigned y=top;y<bottom;++y)for(unsigned x=left;x<right;++x)
    {
        const auto i=y*current.input_width+x;
        if(leaf(current.input[i])){++m.leaf_pixels;m.reactive_leaf+=current.reactive[i];}
        else {++m.background_pixels;m.reactive_background+=current.reactive[i];}
        m.maximum_stationary_motion=std::max(m.maximum_stationary_motion,std::max(std::abs(float(current.motion[i][0])),std::abs(float(current.motion[i][1]))));
        const bool blended=std::string_view(r.name)=="blended_leaf_sky";
        if(!blended)
        {
            m.coverage_mismatches+=std::abs(current.depth[i]-current.motion_depth[i])>2.e-6f;
            // Authored masked cutouts are green at device depth .325;
            // blue/gray backgrounds are clear or farther opaque surfaces.
            if(r.foliage)m.coverage_mismatches+=leaf(current.input[i])!=(reverse_z?current.depth[i]>.5f:current.depth[i]<.5f);
        }
        else
        {
            // One known constant-RGB standard-alpha layer over a uniform sky:
            // recover its actual current coverage from C = aF + (1-a)B.
            // F/B are independently rendered known colors, not Temporal output.
            // A 2/1024 alpha band accounts for the RGBA16F blend/readback and
            // foreground-reference rounding; interior/background expectations
            // remain exact on opposite sides of the color discard threshold.
            const auto& foreground=coverage_reference.foreground[(r.y+10)*OutputWidth+r.x+10];
            const auto& background=coverage_reference.backgrounds[3];
            float numerator=0,denominator=0;
            for(unsigned c=0;c<3;++c)
            {
                const float d=foreground[c]-background[c];
                numerator+=(current.input[i][c]-background[c])*d;denominator+=d*d;
            }
            const float alpha=denominator>1.e-6f?numerator/denominator:0.f;
            constexpr float cutoff=.004f,rounding=2.f/1024.f,foreground_depth=.325f;
            const float opaque_depth=current.opaque_depth[i],proxy=current.motion_depth[i];
            if(alpha>cutoff+rounding)
            {++m.proxy_foreground;m.coverage_mismatches+=std::abs(proxy-foreground_depth)>2.e-6f;}
            else if(alpha<cutoff-rounding)
            {++m.proxy_background;m.coverage_mismatches+=std::abs(proxy-opaque_depth)>2.e-6f;}
            else
            {
                ++m.proxy_ambiguous;
                m.coverage_mismatches+=std::min(std::abs(proxy-foreground_depth),std::abs(proxy-opaque_depth))>2.e-6f;
            }
        }
    }
}
}

TEST_CASE("Temporal67 stationary foliage retains stable edge coverage and useful detail across jitter cycles",
          "[metal][temporal][foliage-stability]")
{
    @autoreleasepool
    {
        FoliageFixture f;
        const auto reference=f.native(0.f);
        const auto shifted_reference=f.native(.05f);
        const auto coverage_reference=f.coverageReference(0.f);
        std::array<Stability,Regions.size()> metrics{};
        std::array<double,3> stop_errors{};
        std::array<std::array<StopQuality,Regions.size()>,3> stopped{};
        std::array<std::vector<Color>,Phases> initial_temporal,stationary_final;
        {
            // This separate history is never exposed to the moved main stream.
            // A warm stationary final-camera world provides an AA/mip/phase-
            // matched reference, rather than an unattainable Native pixel match.
            FoliageFixture independent;
            for(unsigned i=0;i<WarmFrames+Phases;++i)
            {
                auto current=independent.render(i,.05f);
                if(i>=WarmFrames)
                {
                    if(!f.output_dir.empty())
                    {
                        std::ostringstream phase;phase<<std::setw(2)<<std::setfill('0')<<current.phase;
                        dumpImage(f.output_dir/("stationary-final-phase-"+phase.str()+".ppm"),
                            current.output,OutputWidth,OutputHeight);
                        dumpRaw(f.output_dir/("stationary-final-phase-"+phase.str()+".rgba32f"),
                            current.output.data(),current.output.size()*sizeof(Color));
                    }
                    stationary_final[current.phase]=std::move(current.output);
                }
            }
            REQUIRE(independent.scaler->stats().submitted_calls==WarmFrames+Phases);
            REQUIRE(independent.history.stats().resets==1);
            REQUIRE(independent.opaque_depth_captures==WarmFrames+Phases);
        }
        const auto blur=blurredReference(reference);
        unsigned rejected_blurred_foliage=0;
        for(const auto& r:Regions)if(r.foliage)
        {
            const auto q=maskQuality(reference,blur,r);
            rejected_blurred_foliage+=q.recall<.9 || q.precision<.9 || q.area<.85 || q.area>1.15 ||
                referenceEnergy(blur,r)<.5*referenceEnergy(reference,r);
        }
        REQUIRE(rejected_blurred_foliage>=3); // Independently reject an obvious blur, not only a bad candidate.
        const auto blurred_coverage_reference=blurredReference(coverage_reference.color);
        const auto blurred_coverage=recoveredCoverage(coverage_reference,blurred_coverage_reference);
        unsigned rejected_coverage_blur=0;
        for(const auto& r:Regions)if(r.identifiable_coverage)
        {
            const auto q=maskQuality(coverage_reference,blurred_coverage,r);
            rejected_coverage_blur+=q.recall<.9 || q.precision<.9 || q.area<.85 || q.area>1.15 ||
                referenceEnergy(blurred_coverage_reference,r)<.5*referenceEnergy(reference,r);
        }
        REQUIRE(rejected_coverage_blur>=3);

        FramePixels previous;
        std::ofstream trace;
        if(!f.output_dir.empty())
        {
            dumpImage(f.output_dir/"native-reference.ppm",reference,OutputWidth,OutputHeight);
            dumpImage(f.output_dir/"blur-negative-reference.ppm",blur,OutputWidth,OutputHeight);
            dumpImage(f.output_dir/"coverage-matched-reference.ppm",coverage_reference.color,OutputWidth,OutputHeight);
            dumpRaw(f.output_dir/"coverage-ground-truth.r32f",coverage_reference.coverage.data(),coverage_reference.coverage.size()*sizeof(float));
            trace.open(f.output_dir/"frames.csv");
            trace<<std::setprecision(17);
            trace<<"frame,phase,jitter_x,jitter_y,history_resets,valid_previous,camera_shift\n";
        }
        constexpr unsigned MovingFrames=18, StoppedFrames=2*Phases;
        const unsigned total=WarmFrames+StableFrames+MovingFrames+StoppedFrames;
        float previous_shift=0.f,maximum_mask_vector_error=0.f;
        unsigned mask_motion_coverage_mismatches=0;
        for(unsigned i=0;i<total;++i)
        {
            const unsigned moving=i>=WarmFrames+StableFrames?i-(WarmFrames+StableFrames)+1:0;
            const float shift=.05f*float(std::min(moving,MovingFrames))/float(MovingFrames);
            auto current=f.render(i,shift);
            REQUIRE(current.resets==1);
            REQUIRE(current.phase==i%Phases);
            if(i)
            {
                const auto contract=maskMotionContract(current,-(shift-previous_shift)*(.5f*InputWidth),false);
                CHECK(contract.foreground_samples>100);
                CHECK(contract.maximum_vector_error<.002f);
                CHECK(contract.coverage_mismatches==0);
                CHECK(contract.rejected_foreground==0);
                CHECK(straightEdgeRasterMismatches(current,shift,false)==0);
                maximum_mask_vector_error=std::max(maximum_mask_vector_error,contract.maximum_vector_error);
                mask_motion_coverage_mismatches+=contract.coverage_mismatches;
            }
            previous_shift=shift;
            if(i>=WarmFrames && i<WarmFrames+StableFrames)
            {
                if(i<WarmFrames+Phases)initial_temporal[current.phase]=current.output;
                for(std::size_t region=0;region<Regions.size();++region)
                    measure(metrics[region],previous,current,reference,coverage_reference,Regions[region]);
            }
            if(i==WarmFrames+StableFrames+MovingFrames ||
                i==WarmFrames+StableFrames+MovingFrames+Phases-1 || i==total-1)
            {
                const unsigned sample=i==total-1?2:i==WarmFrames+StableFrames+MovingFrames?0:1;
                for(std::size_t pixel=0;pixel<current.output.size();++pixel)for(unsigned c=0;c<3;++c)
                    stop_errors[sample]+=std::abs(current.output[pixel][c]-shifted_reference[pixel][c]);
                stop_errors[sample]/=double(current.output.size()*3);
                for(std::size_t region=0;region<Regions.size();++region)
                    stopped[sample][region]=stopQuality(reference,shifted_reference,
                        initial_temporal[current.phase],stationary_final[current.phase],current.output,Regions[region]);
            }
            if(trace)trace<<i<<','<<current.phase<<','<<current.jitter[0]<<','<<current.jitter[1]<<','<<current.resets<<','<<current.previous_valid<<','<<shift<<'\n';
            if(!f.output_dir.empty())
            {
                std::ostringstream filename;filename<<"output-"<<std::setw(4)<<std::setfill('0')<<i<<".ppm";
                dumpImage(f.output_dir/filename.str(),current.output,OutputWidth,OutputHeight);
                const unsigned first_move=WarmFrames+StableFrames,first_stop=first_move+MovingFrames;
                if(i==first_move)dumpFrame(f.output_dir,"first-moving",current);
                if(i==first_stop)dumpFrame(f.output_dir,"stop-01",current);
                if(i==first_stop+Phases-1)dumpFrame(f.output_dir,"stop-18",current);
                if(i==total-1)dumpFrame(f.output_dir,"stop-36",current);
                if(i>=WarmFrames && i<WarmFrames+Phases)
                {
                    std::ostringstream phase;phase<<std::setw(2)<<std::setfill('0')<<current.phase;
                    dumpImage(f.output_dir/("input-phase-"+phase.str()+".ppm"),current.input,InputWidth,InputHeight);
                    dumpRaw(f.output_dir/("input-phase-"+phase.str()+".rgba32f"),current.input.data(),current.input.size()*sizeof(Color));
                    dumpRaw(f.output_dir/("motion-phase-"+phase.str()+".rg16f"),current.motion.data(),current.motion.size()*sizeof(current.motion[0]));
                    dumpRaw(f.output_dir/("reactive-phase-"+phase.str()+".r8"),current.reactive.data(),current.reactive.size());
                    dumpRaw(f.output_dir/("depth-phase-"+phase.str()+".r32f"),current.depth.data(),current.depth.size()*sizeof(float));
                    dumpRaw(f.output_dir/("proxy-depth-phase-"+phase.str()+".r32f"),current.motion_depth.data(),current.motion_depth.size()*sizeof(float));
                    dumpRaw(f.output_dir/("opaque-depth-phase-"+phase.str()+".r32f"),current.opaque_depth.data(),current.opaque_depth.size()*sizeof(float));
                    dumpRaw(f.output_dir/("output-phase-"+phase.str()+".rgba32f"),current.output.data(),current.output.size()*sizeof(Color));
                }
            }
            previous=std::move(current);
        }
        if(!f.output_dir.empty())dumpImage(f.output_dir/"native-move-stop-reference.ppm",shifted_reference,OutputWidth,OutputHeight);
        double settled_error=0;
        for(std::size_t i=0;i<shifted_reference.size();++i)for(unsigned c=0;c<3;++c)
            settled_error+=std::abs(previous.output[i][c]-shifted_reference[i][c]);
        settled_error/=double(shifted_reference.size()*3);
        std::cout<<std::setprecision(17)<<"Temporal foliage67 input="<<InputWidth<<'x'<<InputHeight<<", output="<<OutputWidth<<'x'<<OutputHeight
            <<", bias="<<f.mip_bias<<", phases="<<Phases<<", measured_cycles=3, move_stop_mean_reference_error="<<settled_error
            <<", stop_error_first="<<stop_errors[0]<<", stop_error_phase18="<<stop_errors[1]<<", stop_error_phase36="<<stop_errors[2]
            <<", max_mask_vector_error="<<maximum_mask_vector_error
            <<", mask_motion_coverage_mismatches="<<mask_motion_coverage_mismatches<<'\n';
        std::ofstream summary;
        if(!f.output_dir.empty())
        {
            summary.open(f.output_dir/"regions.csv");
            summary<<std::setprecision(17);
            summary<<"region,mean_rgb_change,large_coverage_change,edge_energy,reference_edge_energy,mean_reference_error,leaf_reactive,background_reactive,max_stationary_motion,depth_mismatches,edge_band_change,edge_band_frame_p95,minimum_recall,minimum_precision,minimum_area,maximum_area,stop18_native_quality_error,stop18_phase_matched_error,stop18_old_history_fraction,stop36_native_quality_error,stop36_phase_matched_error,stop36_old_history_fraction,proxy_foreground_samples,proxy_background_samples,proxy_threshold_ambiguous_samples\n";
        }
        for(std::size_t i=0;i<Regions.size();++i)
        {
            const auto& r=Regions[i];const auto& m=metrics[i];CAPTURE(r.name);
            REQUIRE(m.pixels>0);const double change=m.absolute_change/m.pixels;
            const double coverage=double(m.coverage_changes)/m.pixels;
            const double energy=m.edge_energy/m.comparisons, reference_energy=referenceEnergy(reference,r);
            const double reference_error=m.reference_error/(3.*m.pixels);
            const double edge_change=m.edge_band_pixels?m.edge_band_change/m.edge_band_pixels:0.;
            auto sorted=m.frame_edge_changes;std::sort(sorted.begin(),sorted.end());
            const double edge_p95=sorted[std::min(sorted.size()-1,std::size_t(std::ceil(sorted.size()*.95)-1))];
            std::cout<<"Temporal foliage ROI="<<r.name<<", mean_rgb_change="<<change<<", large_coverage_change="<<coverage
                <<", edge_energy="<<energy<<", reference_edge_energy="<<reference_energy<<", mean_reference_error="<<reference_error
                <<", leaf_reactive="<<(m.leaf_pixels?double(m.reactive_leaf)/(255.*m.leaf_pixels):0.)
                <<", background_reactive="<<(m.background_pixels?double(m.reactive_background)/(255.*m.background_pixels):0.)
                <<", max_stationary_motion="<<m.maximum_stationary_motion<<", depth_mismatches="<<m.coverage_mismatches
                <<", edge_band_change="<<edge_change<<", edge_band_frame_p95="<<edge_p95
                <<", minimum_recall="<<m.minimum_recall<<", minimum_precision="<<m.minimum_precision
                <<", area_min="<<m.minimum_area<<", area_max="<<m.maximum_area
                <<", hue_leaf_fraction="<<m.leaf_coverage/m.pixels
                <<", stop18_native_quality_error="<<stopped[1][i].native_quality_error
                <<", stop18_phase_matched_error="<<stopped[1][i].phase_matched_error
                <<", stop18_old_history_fraction="<<stopped[1][i].old_history_fraction
                <<", stop36_native_quality_error="<<stopped[2][i].native_quality_error
                <<", stop36_phase_matched_error="<<stopped[2][i].phase_matched_error
                <<", stop36_old_history_fraction="<<stopped[2][i].old_history_fraction
                <<", proxy_foreground_samples="<<m.proxy_foreground<<", proxy_background_samples="<<m.proxy_background
                <<", proxy_threshold_ambiguous_samples="<<m.proxy_ambiguous<<'\n';
            if(summary)summary<<r.name<<','<<change<<','<<coverage<<','<<energy<<','<<reference_energy<<','<<reference_error<<','
                <<(m.leaf_pixels?double(m.reactive_leaf)/(255.*m.leaf_pixels):0.)<<','
                <<(m.background_pixels?double(m.reactive_background)/(255.*m.background_pixels):0.)<<','
                <<m.maximum_stationary_motion<<','<<m.coverage_mismatches<<','<<edge_change<<','<<edge_p95<<','
                <<m.minimum_recall<<','<<m.minimum_precision<<','<<m.minimum_area<<','<<m.maximum_area<<','
                <<stopped[1][i].native_quality_error<<','<<stopped[1][i].phase_matched_error<<','
                <<stopped[1][i].old_history_fraction<<','<<stopped[2][i].native_quality_error<<','
                <<stopped[2][i].phase_matched_error<<','<<stopped[2][i].old_history_fraction<<','
                <<m.proxy_foreground<<','<<m.proxy_background<<','<<m.proxy_ambiguous<<'\n';
            // Authored image-quality bounds: less than one 8-bit linear level
            // of average phase-to-phase change; no large binary edge toggles
            // in >0.5% of ROI pixels. Detail must not be blurred away to pass.
            CHECK(change<=1./255.);
            CHECK(edge_change<=1./255.);
            CHECK(edge_p95<=2./255.);
            if(r.foliage)
            {
                CHECK(coverage<=.005);
                if(r.identifiable_coverage)
                {
                    CHECK(m.minimum_recall>=.90);CHECK(m.minimum_precision>=.90);
                    CHECK(m.minimum_area>=.85);CHECK(m.maximum_area<=1.15);
                }
                CHECK(stopped[1][i].old_history_fraction<=.02);
                CHECK(stopped[2][i].old_history_fraction<=.02);
            }
            // Compare like reconstruction to like reconstruction for settling.
            // Native0 absolute error remains separately reported as quality.
            CHECK(stopped[1][i].phase_matched_error<=1./255.);
            CHECK(stopped[2][i].phase_matched_error<=1./255.);
            CHECK((reference_energy<1.e-5 || energy>=.5*reference_energy));
            CHECK(reference_error<=.12);
            CHECK(m.maximum_stationary_motion<=.025f);
            CHECK(m.coverage_mismatches==0);
            if(std::string_view(r.name)=="blended_leaf_sky")
            {
                // This is a coverage oracle, not merely a finite/member-depth
                // test: both authored leaf and background must be exercised.
                const auto samples=m.proxy_foreground+m.proxy_background+m.proxy_ambiguous;
                REQUIRE(samples>0);
                CHECK(m.proxy_foreground>samples/20);
                CHECK(m.proxy_background>samples/20);
            }
        }
        CHECK(settled_error<=.10);
        CHECK(stop_errors[1]<=.10); // Broad Native0 quality guard; localized history is checked above.
        CHECK(f.scaler->stats().submitted_calls==total);
        CHECK(f.history.stats().submitted_worlds==total);
        CHECK(f.opaque_depth_captures==total);
        CHECK(f.history.stats().resets==1);
    }
}

TEST_CASE("Temporal67 PBR MASK palm separates background depth from material coverage",
          "[metal][temporal][foliage-mask-contract]")
{
    @autoreleasepool
    {
        // A diagnostic, not a proposal to add background geometry to the
        // viewer. Keep the same PBR palm, RGB/alpha input and reconstruction
        // policy while distinguishing clear depth from a finite blue plane.
        const Region palm{"pbr_palm",192,0,true,true};
        for(bool reverse:{false,true})
        {
            std::array<std::vector<Color>,Phases> clear_inputs;
            for(bool finite:{false,true})
            {
                const std::string arm=std::string(reverse?"reverse-":"normal-")+(finite?"finite":"clear");
                CAPTURE(arm);
                FoliageFixture f(reverse);f.usePBRPalm(finite);
                if(!f.output_dir.empty()){f.output_dir/=arm;std::filesystem::create_directories(f.output_dir);}
                const auto reference=f.native(0.f),shifted_reference=f.native(.05f);
                const auto truth=f.coverageReference(0.f);
                std::array<std::vector<Color>,Phases> initial,stationary_final;
                {
                    FoliageFixture independent(reverse);independent.usePBRPalm(finite);
                    for(unsigned i=0;i<WarmFrames+Phases;++i)
                    {
                        auto current=independent.render(i,.05f);
                        if(i>=WarmFrames)stationary_final[current.phase]=std::move(current.output);
                    }
                }
                Stability metrics;std::array<StopQuality,3> stopped{};FramePixels previous;
                unsigned coverage_mismatches=0,raster_mismatches=0,rejected_foreground=0,input_mismatches=0;
                float max_vector_error=0,previous_shift=0;
                constexpr unsigned moving_frames=18,total=WarmFrames+StableFrames+moving_frames+2*Phases;
                std::ofstream frames;
                if(!f.output_dir.empty())
                {
                    frames.open(f.output_dir/"frames.csv");
                    frames<<std::setprecision(17);
                    frames<<"frame,phase,jitter_x,jitter_y,history_resets,valid_previous,camera_shift,expected_motion_x,maximum_motion_error\n";
                    dumpImage(f.output_dir/"native-reference.ppm",reference,OutputWidth,OutputHeight);
                    dumpImage(f.output_dir/"coverage-reference.ppm",truth.color,OutputWidth,OutputHeight);
                    dumpRaw(f.output_dir/"coverage-reference.r32f",truth.coverage.data(),truth.coverage.size()*sizeof(float));
                }
                for(unsigned i=0;i<total;++i)
                {
                    const unsigned moving=i>=WarmFrames+StableFrames?i-(WarmFrames+StableFrames)+1:0;
                    const float shift=.05f*float(std::min(moving,moving_frames))/float(moving_frames);
                    auto current=f.render(i,shift);
                    const float expected_motion=-(shift-previous_shift)*(.5f*InputWidth);
                    const auto contract=maskMotionContract(current,expected_motion,reverse);
                    REQUIRE(current.resets==1);REQUIRE(current.phase==i%Phases);
                    if(i)
                    {
                        CHECK(contract.foreground_samples>100);
                        coverage_mismatches+=contract.coverage_mismatches;
                        rejected_foreground+=contract.rejected_foreground;
                        max_vector_error=std::max(max_vector_error,contract.maximum_vector_error);
                    }
                    raster_mismatches+=straightEdgeRasterMismatches(current,shift,reverse);
                    previous_shift=shift;
                    if(i>=WarmFrames && i<WarmFrames+StableFrames)
                    {
                        measure(metrics,previous,current,reference,truth,palm,reverse);
                        if(i<WarmFrames+Phases)
                        {
                            initial[current.phase]=current.output;
                            if(!finite)clear_inputs[current.phase]=current.input;
                            else
                            {
                                // Same material color and alpha, including the
                                // clear's glow alpha: depth is the only intended
                                // MetalFX input difference in the stationary arm.
                                const auto& control=clear_inputs[current.phase];REQUIRE(control.size()==current.input.size());
                                for(unsigned y=4;y<60;++y)for(unsigned x=132;x<188;++x)
                                    for(unsigned c=0;c<4;++c)input_mismatches+=current.input[y*InputWidth+x][c]!=control[y*InputWidth+x][c];
                            }
                            std::ostringstream label;label<<"phase-"<<std::setw(2)<<std::setfill('0')<<current.phase;
                            dumpFrame(f.output_dir,label.str(),current);
                        }
                    }
                    const unsigned first_move=WarmFrames+StableFrames,first_stop=first_move+moving_frames;
                    if(i==first_move)dumpFrame(f.output_dir,"first-moving",current);
                    if(i==first_stop || i==first_stop+Phases-1 || i==total-1)
                    {
                        const unsigned sample=i==total-1?2:i==first_stop?0:1;
                        stopped[sample]=stopQuality(reference,shifted_reference,initial[current.phase],
                            stationary_final[current.phase],current.output,palm);
                        dumpFrame(f.output_dir,sample==0?"stop-01":sample==1?"stop-18":"stop-36",current);
                    }
                    if(frames)frames<<i<<','<<current.phase<<','<<current.jitter[0]<<','<<current.jitter[1]<<','
                        <<current.resets<<','<<current.previous_valid<<','<<shift<<','<<expected_motion<<','<<contract.maximum_vector_error<<'\n';
                    previous=std::move(current);
                }
                const double change=metrics.absolute_change/metrics.pixels;
                const double edge_change=metrics.edge_band_change/metrics.edge_band_pixels;
                auto sorted=metrics.frame_edge_changes;std::sort(sorted.begin(),sorted.end());
                const double edge_p95=sorted[std::min(sorted.size()-1,std::size_t(std::ceil(sorted.size()*.95)-1))];
                const double coverage_change=double(metrics.coverage_changes)/metrics.pixels;
                const double reference_error=metrics.reference_error/(3.*metrics.pixels);
                const double energy=metrics.edge_energy/metrics.comparisons,reference_energy=referenceEnergy(reference,palm);
                std::cout<<std::setprecision(17)<<"PBR MASK palm arm="<<arm<<", bias="<<f.mip_bias
                    <<", mean_rgb_change="<<change<<", edge_band_change="<<edge_change<<", edge_band_p95="<<edge_p95
                    <<", coverage_change="<<coverage_change<<", mean_reference_error="<<reference_error<<", minimum_recall="<<metrics.minimum_recall
                    <<", minimum_precision="<<metrics.minimum_precision<<", minimum_area="<<metrics.minimum_area
                    <<", maximum_area="<<metrics.maximum_area<<", edge_energy="<<energy<<", reference_edge_energy="<<reference_energy
                    <<", stop18_phase_error="<<stopped[1].phase_matched_error<<", stop36_phase_error="<<stopped[2].phase_matched_error
                    <<", stop18_old_fraction="<<stopped[1].old_history_fraction<<", stop36_old_fraction="<<stopped[2].old_history_fraction
                    <<", max_motion_error="<<max_vector_error<<", coverage_mismatches="<<coverage_mismatches
                    <<", raster_mismatches="<<raster_mismatches<<", rejected_foreground="<<rejected_foreground
                    <<", paired_input_mismatches="<<input_mismatches<<'\n';
                CHECK(max_vector_error<.002f);CHECK(coverage_mismatches==0);CHECK(raster_mismatches==0);
                CHECK(rejected_foreground==0);CHECK(input_mismatches==0);
                CHECK(change<=1./255.);CHECK(edge_change<=1./255.);
                CHECK(edge_p95<=2./255.);CHECK(coverage_change<=.005);CHECK(reference_error<=.12);
                CHECK(metrics.minimum_recall>=.90);CHECK(metrics.minimum_precision>=.90);
                CHECK(metrics.minimum_area>=.85);CHECK(metrics.maximum_area<=1.15);
                CHECK(energy>=.5*reference_energy);
                CHECK(stopped[1].phase_matched_error<=1./255.);CHECK(stopped[2].phase_matched_error<=1./255.);
                CHECK(stopped[1].old_history_fraction<=.02);CHECK(stopped[2].old_history_fraction<=.02);
            }
        }
    }
}

namespace
{
std::vector<Color> directPalmFrame(almetal_test::DirectTemporalFixture& direct,
    const FramePixels& current,unsigned index,std::string& error)
{
    // Readback floats represent the original16F color exactly; prove that
    // packing into the independently owned native input loses no source bits.
    std::vector<almetal_test::DirectTemporalFixture::HalfPixel> color(current.input.size());
    unsigned mismatches=0;
    for(std::size_t i=0;i<color.size();++i)for(unsigned c=0;c<4;++c)
    {color[i][c]=current.input[i][c];mismatches+=float(color[i][c])!=current.input[i][c];}
    REQUIRE(mismatches==0);
    const almetal_test::DirectTemporalFixture::PackedInputs inputs{
        std::as_bytes(std::span(color)),std::as_bytes(std::span(current.motion_depth)),
        std::as_bytes(std::span(current.motion)),std::as_bytes(std::span(current.reactive))};
    almetal_test::DirectTemporalFixture::Frame frame;
    frame.world_id=index+1;frame.jitter_x=current.jitter[0];frame.jitter_y=current.jitter[1];
    frame.pre_exposure=1.f;frame.depth_reversed=false;frame.reset=index==0;
    almetal_test::DirectTemporalFixture::Result result;
    REQUIRE(direct.render(inputs,frame,result,error));INFO(error);REQUIRE(result.world_id==index+1);
    std::vector<Color> pixels(result.rgba16.size());
    for(std::size_t i=0;i<pixels.size();++i)for(unsigned c=0;c<4;++c)pixels[i][c]=result.rgba16[i][c];
    return pixels;
}
}

TEST_CASE("Temporal67 failing PBR palm separates native reconstruction from Alchemy resolve",
          "[metal][temporal][foliage-native-isolation]")
{
    @autoreleasepool
    {
        const Region palm{"pbr_palm",192,0,true,true};
        FoliageFixture f;f.usePBRPalm(false);f.observe_native_output=true;
        if(!f.output_dir.empty()){f.output_dir/="native-isolation";std::filesystem::create_directories(f.output_dir);}
        const auto reference=f.native(0.f),shifted_reference=f.native(.05f);
        const auto truth=f.coverageReference(0.f);
        using Direct=almetal_test::DirectTemporalFixture;
        auto direct=Direct::create((__bridge id<MTLDevice>)f.device->nativeHandle(),
            {InputWidth,InputHeight,OutputWidth,OutputHeight,true},f.error);
        REQUIRE(direct);INFO(f.error);
        constexpr unsigned Stages=3;
        const std::array<const char*,Stages> names{"native","resolved","direct"};
        std::array<std::array<std::vector<Color>,Phases>,Stages> initial,stationary_final;
        if(!f.output_dir.empty())
        {
            dumpRaw(f.output_dir/"native-detail-reference.rgba32f",reference.data(),reference.size()*sizeof(Color));
            dumpRaw(f.output_dir/"shifted-native-detail-reference.rgba32f",shifted_reference.data(),shifted_reference.size()*sizeof(Color));
            dumpRaw(f.output_dir/"coverage-reference.r32f",truth.coverage.data(),truth.coverage.size()*sizeof(float));
            dumpImage(f.output_dir/"native-detail-reference.ppm",reference,OutputWidth,OutputHeight);
        }
        // Each stage is compared with its OWN full-history, final-camera
        // reference, including all warm-up worlds from the initial reset.
        {
            FoliageFixture independent;independent.usePBRPalm(false);independent.observe_native_output=true;
            auto direct_reference=Direct::create((__bridge id<MTLDevice>)independent.device->nativeHandle(),
                {InputWidth,InputHeight,OutputWidth,OutputHeight,true},independent.error);
            REQUIRE(direct_reference);INFO(independent.error);
            std::ofstream reference_frames;
            if(!f.output_dir.empty())
            {
                reference_frames.open(f.output_dir/"reference-sequence.csv");reference_frames<<std::setprecision(17);
                reference_frames<<"frame,world_id,phase,jitter_x,jitter_y,camera_shift,reset,history_resets,valid_previous,input_width,input_height,output_width,output_height,depth_reversed,pre_exposure\n";
            }
            for(unsigned i=0;i<WarmFrames+Phases;++i)
            {
                auto current=independent.render(i,.05f);
                auto direct_output=directPalmFrame(*direct_reference,current,i,independent.error);
                std::uint64_t reference_rgb_differences=0;
                for(std::size_t p=0;p<direct_output.size();++p)for(unsigned c=0;c<3;++c)
                    reference_rgb_differences+=direct_output[p][c]!=current.native_output[p][c];
                CHECK(reference_rgb_differences==0);
                if(reference_frames)reference_frames<<i<<','<<i+1<<','<<current.phase<<','<<current.jitter[0]<<','<<current.jitter[1]<<','<<.05f<<','<<(i==0)<<','<<current.resets<<','<<current.previous_valid<<','<<InputWidth<<','<<InputHeight<<','<<OutputWidth<<','<<OutputHeight<<",0,1\n";
                if(!f.output_dir.empty())
                {std::ostringstream label;label<<"reference-world-"<<std::setw(3)<<std::setfill('0')<<i;dumpFrame(f.output_dir,label.str(),current);dumpRaw(f.output_dir/(label.str()+"-direct-output.rgba32f"),direct_output.data(),direct_output.size()*sizeof(Color));}
                if(i>=WarmFrames)
                {
                    stationary_final[0][current.phase]=std::move(current.native_output);
                    stationary_final[1][current.phase]=std::move(current.output);
                    stationary_final[2][current.phase]=std::move(direct_output);
                }
            }
        }
        std::array<Stability,Stages> metrics;
        std::array<std::array<StopQuality,3>,Stages> stopped;
        FramePixels previous;std::vector<Color> previous_direct;
        std::ofstream frames;
        if(!f.output_dir.empty())
        {
            frames.open(f.output_dir/"sequence.csv");frames<<std::setprecision(17);
            frames<<"frame,world_id,phase,jitter_x,jitter_y,camera_shift,reset,history_resets,valid_previous,input_width,input_height,output_width,output_height,depth_reversed,pre_exposure,rgb_differences,rgb_max_error,alpha_differences,alpha_max_error,resolve_alpha_max_error,direct_rgb_differences,direct_rgb_max_error,direct_alpha_differences,direct_alpha_max_error\n";
        }
        std::uint64_t rgb_differences=0,alpha_differences=0;double rgb_max=0,alpha_max=0,alpha_expected_max=0;
        std::uint64_t direct_rgb=0,direct_alpha=0;double direct_rgb_max=0,direct_alpha_max=0;
        constexpr unsigned MovingFrames=18,total=WarmFrames+StableFrames+MovingFrames+2*Phases;
        for(unsigned i=0;i<total;++i)
        {
            const unsigned moving=i>=WarmFrames+StableFrames?i-(WarmFrames+StableFrames)+1:0;
            const float shift=.05f*float(std::min(moving,MovingFrames))/float(MovingFrames);
            auto current=f.render(i,shift);REQUIRE(current.native_output.size()==current.output.size());
            auto direct_output=directPalmFrame(*direct,current,i,f.error);
            REQUIRE(direct_output.size()==current.native_output.size());
            REQUIRE(current.resets==1);REQUIRE(current.phase==i%Phases);
            std::uint64_t frame_rgb=0,frame_alpha=0,frame_direct_rgb=0,frame_direct_alpha=0;double frame_direct_rgb_max=0,frame_direct_alpha_max=0;unsigned nonfinite=0;double frame_rgb_max=0,frame_alpha_max=0,frame_expected_max=0;
            for(unsigned y=0;y<OutputHeight;++y)for(unsigned x=0;x<OutputWidth;++x)
            {
                const auto p=y*OutputWidth+x;
                for(unsigned c=0;c<3;++c)
                {
                    nonfinite+=!std::isfinite(current.native_output[p][c]) || !std::isfinite(current.output[p][c]);
                    const double error=std::abs(current.output[p][c]-current.native_output[p][c]);
                    frame_rgb+=error!=0;frame_rgb_max=std::max(frame_rgb_max,error);
                    const double direct_error=std::abs(direct_output[p][c]-current.native_output[p][c]);
                    frame_direct_rgb+=direct_error!=0;frame_direct_rgb_max=std::max(frame_direct_rgb_max,direct_error);
                }
                const double alpha_error=std::abs(current.output[p][3]-current.native_output[p][3]);
                frame_alpha+=alpha_error!=0;frame_alpha_max=std::max(frame_alpha_max,alpha_error);
                const double direct_alpha_error=std::abs(direct_output[p][3]-current.native_output[p][3]);
                frame_direct_alpha+=direct_alpha_error!=0;frame_direct_alpha_max=std::max(frame_direct_alpha_max,direct_alpha_error);
                // Independently evaluate only the documented alpha resolve:
                // linear/clamp original input at output center + input jitter.
                const float sx=(float(x)+.5f)*InputWidth/OutputWidth+current.jitter[0]-.5f;
                const float sy=(float(y)+.5f)*InputHeight/OutputHeight+current.jitter[1]-.5f;
                const int x0=std::floor(sx),y0=std::floor(sy);const float tx=sx-x0,ty=sy-y0;
                const auto alpha=[&](int xx,int yy){return current.input[std::clamp(yy,0,int(InputHeight)-1)*InputWidth+std::clamp(xx,0,int(InputWidth)-1)][3];};
                const float expected=(alpha(x0,y0)*(1-tx)+alpha(x0+1,y0)*tx)*(1-ty)+
                    (alpha(x0,y0+1)*(1-tx)+alpha(x0+1,y0+1)*tx)*ty;
                frame_expected_max=std::max(frame_expected_max,double(std::abs(current.output[p][3]-expected)));
            }
            REQUIRE(nonfinite==0);
            rgb_differences+=frame_rgb;rgb_max=std::max(rgb_max,frame_rgb_max);
            alpha_differences+=frame_alpha;alpha_max=std::max(alpha_max,frame_alpha_max);
            alpha_expected_max=std::max(alpha_expected_max,frame_expected_max);
            direct_rgb+=frame_direct_rgb;direct_alpha+=frame_direct_alpha;
            direct_rgb_max=std::max(direct_rgb_max,frame_direct_rgb_max);direct_alpha_max=std::max(direct_alpha_max,frame_direct_alpha_max);
            if(i>=WarmFrames && i<WarmFrames+StableFrames)
            {
                FramePixels raw_previous=previous,raw_current=current;
                raw_previous.output=previous.native_output;raw_current.output=current.native_output;
                measure(metrics[0],raw_previous,raw_current,reference,truth,palm);
                measure(metrics[1],previous,current,reference,truth,palm);
                raw_previous.output=previous_direct;raw_current.output=direct_output;
                measure(metrics[2],raw_previous,raw_current,reference,truth,palm);
                if(i<WarmFrames+Phases)
                {initial[0][current.phase]=current.native_output;initial[1][current.phase]=current.output;initial[2][current.phase]=direct_output;}
            }
            const unsigned first_stop=WarmFrames+StableFrames+MovingFrames;
            if(i==first_stop || i==first_stop+Phases-1 || i==total-1)
            {
                const unsigned sample=i==total-1?2:i==first_stop?0:1;
                stopped[0][sample]=stopQuality(reference,shifted_reference,initial[0][current.phase],stationary_final[0][current.phase],current.native_output,palm);
                stopped[1][sample]=stopQuality(reference,shifted_reference,initial[1][current.phase],stationary_final[1][current.phase],current.output,palm);
                stopped[2][sample]=stopQuality(reference,shifted_reference,initial[2][current.phase],stationary_final[2][current.phase],direct_output,palm);
            }
            if(!f.output_dir.empty())
            {std::ostringstream label;label<<"world-"<<std::setw(3)<<std::setfill('0')<<i;dumpFrame(f.output_dir,label.str(),current);dumpRaw(f.output_dir/(label.str()+"-direct-output.rgba32f"),direct_output.data(),direct_output.size()*sizeof(Color));}
            if(frames)frames<<i<<','<<i+1<<','<<current.phase<<','<<current.jitter[0]<<','<<current.jitter[1]<<','<<shift<<','<<(i==0)<<','<<current.resets<<','<<current.previous_valid<<','<<InputWidth<<','<<InputHeight<<','<<OutputWidth<<','<<OutputHeight<<",0,1,"<<frame_rgb<<','<<frame_rgb_max<<','<<frame_alpha<<','<<frame_alpha_max<<','<<frame_expected_max<<','<<frame_direct_rgb<<','<<frame_direct_rgb_max<<','<<frame_direct_alpha<<','<<frame_direct_alpha_max<<'\n';
            previous=std::move(current);previous_direct=std::move(direct_output);
        }
        std::cout<<std::setprecision(17)<<"PBR palm native/resolve worlds="<<total<<", rgb_differences="<<rgb_differences
            <<", rgb_max_error="<<rgb_max<<", alpha_differences="<<alpha_differences<<", alpha_max_error="<<alpha_max
            <<", resolve_alpha_max_error="<<alpha_expected_max<<'\n';
        std::cout<<"PBR palm direct worlds="<<direct->submittedFrames()<<", resets="<<direct->resetFrames()
            <<", rgb_differences="<<direct_rgb<<", rgb_max_error="<<direct_rgb_max
            <<", alpha_differences="<<direct_alpha<<", alpha_max_error="<<direct_alpha_max<<'\n';
        CHECK(direct->submittedFrames()==total);CHECK(direct->resetFrames()==1);
        CHECK(direct_rgb==0); // Vendor alpha is observed, not assigned a viewer glow contract.
        CHECK(rgb_differences==0); // Intentionally independent of alpha's different contract.
        // Ideal real-valued interpolation is an observation, not a valid
        // sharp-alpha oracle: the native linear sampler has finite filter
        // weight precision. The existing analytical glow-alpha fixtures retain
        // their original bounds and separately check the intended alpha rule.
        for(unsigned stage=0;stage<Stages;++stage)
        {
            CAPTURE(names[stage]);const auto& m=metrics[stage];const auto& stop=stopped[stage];
            const double change=m.absolute_change/m.pixels,edge_change=m.edge_band_change/m.edge_band_pixels;
            auto sorted=m.frame_edge_changes;std::sort(sorted.begin(),sorted.end());
            const double p95=sorted[std::min(sorted.size()-1,std::size_t(std::ceil(sorted.size()*.95)-1))];
            std::cout<<std::setprecision(17)<<"PBR palm stage="<<names[stage]<<", mean_rgb_change="<<change<<", edge_band_change="<<edge_change
                <<", edge_band_p95="<<p95<<", minimum_recall="<<m.minimum_recall<<", minimum_area="<<m.minimum_area
                <<", edge_energy="<<m.edge_energy/m.comparisons<<", stop18_phase_error="<<stop[1].phase_matched_error
                <<", stop36_phase_error="<<stop[2].phase_matched_error<<", stop18_old_fraction="<<stop[1].old_history_fraction
                <<", stop36_old_fraction="<<stop[2].old_history_fraction<<'\n';
            // The same failing palm gates; no weakened thresholds or new blur oracle.
            CHECK(change<=1./255.);CHECK(edge_change<=1./255.);CHECK(p95<=2./255.);
            CHECK(double(m.coverage_changes)/m.pixels<=.005);CHECK(m.reference_error/(3.*m.pixels)<=.12);
            CHECK(m.minimum_recall>=.90);CHECK(m.minimum_precision>=.90);CHECK(m.minimum_area>=.85);CHECK(m.maximum_area<=1.15);
            CHECK(m.edge_energy/m.comparisons>=.5*referenceEnergy(reference,palm));
            CHECK(stop[1].phase_matched_error<=1./255.);CHECK(stop[2].phase_matched_error<=1./255.);
            CHECK(stop[1].old_history_fraction<=.02);CHECK(stop[2].old_history_fraction<=.02);
        }
    }
}

TEST_CASE("Temporal75 freshly rasterized PBR palm meets the existing foliage quality bounds",
          "[metal][temporal][foliage-75-quality]")
{
    @autoreleasepool
    {
        const Region palm{"pbr_palm",192,0,true,true};
        constexpr unsigned Width=FoliageFixture75::InputWidth,Height=FoliageFixture75::InputHeight;
        const unsigned phases=ALMetalTemporalWorldHistory::recommendedJitterPhaseCount(Width,Height,OutputWidth,OutputHeight);
        REQUIRE(phases==15);
        // Three full warm-up cycles, followed by three measured cycles. Both
        // modes begin the SAME18-world motion at index90 and stop at108.
        const unsigned warm=3*phases,stable=3*phases;
        constexpr unsigned moving_frames=18,stopped_frames=36;
        const unsigned first_move=warm+stable,first_stop=first_move+moving_frames,total=first_stop+stopped_frames;
        REQUIRE(first_move==90);REQUIRE(total==144);
        FoliageFixture75 f;f.usePBRPalm(false);
        if(!f.output_dir.empty()){f.output_dir/="temporal75";std::filesystem::create_directories(f.output_dir);}
        const auto reference=f.native(0.f),shifted_reference=f.native(.05f);
        const auto truth=f.coverageReference(0.f);
        std::vector<std::vector<Color>> initial(phases),stationary_final(phases);
        std::ofstream reference_frames;
        if(!f.output_dir.empty())
        {
            dumpRaw(f.output_dir/"native-detail-reference.rgba32f",reference.data(),reference.size()*sizeof(Color));
            dumpRaw(f.output_dir/"shifted-native-detail-reference.rgba32f",shifted_reference.data(),shifted_reference.size()*sizeof(Color));
            dumpRaw(f.output_dir/"coverage-reference.r32f",truth.coverage.data(),truth.coverage.size()*sizeof(float));
            dumpImage(f.output_dir/"native-detail-reference.ppm",reference,OutputWidth,OutputHeight);
            reference_frames.open(f.output_dir/"reference-sequence.csv");reference_frames<<std::setprecision(17);
            reference_frames<<"frame,world_id,phase,phase_count,jitter_x,jitter_y,camera_shift,reset,history_resets,valid_previous,input_width,input_height,output_width,output_height,mode,mip_bias\n";
        }
        {
            FoliageFixture75 independent;independent.usePBRPalm(false);
            for(unsigned i=0;i<warm+phases;++i)
            {
                auto current=independent.render(i,.05f);
                REQUIRE(current.phase==i%phases);REQUIRE(current.resets==1);
                if(!f.output_dir.empty())
                {std::ostringstream label;label<<"reference-world-"<<std::setw(3)<<std::setfill('0')<<i;dumpFrame(f.output_dir,label.str(),current);}
                if(reference_frames)reference_frames<<i<<','<<i+1<<','<<current.phase<<','<<phases<<','<<current.jitter[0]<<','<<current.jitter[1]<<','<<.05f<<','<<(i==0)<<','<<current.resets<<','<<current.previous_valid<<','<<Width<<','<<Height<<','<<OutputWidth<<','<<OutputHeight<<",7,"<<f.mip_bias<<'\n';
                if(i>=warm)stationary_final[current.phase]=std::move(current.output);
            }
        }
        FramePixels previous;Stability metrics;std::array<StopQuality,3> stopped{};
        float previous_shift=0,max_vector_error=0;unsigned coverage_mismatches=0,rejected_foreground=0,raster_mismatches=0;
        std::ofstream frames;
        if(!f.output_dir.empty())
        {
            frames.open(f.output_dir/"sequence.csv");frames<<std::setprecision(17);
            frames<<"frame,world_id,phase,phase_count,jitter_x,jitter_y,camera_shift,reset,history_resets,valid_previous,input_width,input_height,output_width,output_height,mode,mip_bias,expected_motion_x,maximum_motion_error\n";
        }
        for(unsigned i=0;i<total;++i)
        {
            const unsigned moving=i>=first_move?i-first_move+1:0;
            const float shift=.05f*float(std::min(moving,moving_frames))/float(moving_frames);
            auto current=f.render(i,shift);REQUIRE(current.phase==i%phases);REQUIRE(current.resets==1);
            REQUIRE(f.current_gbuffer->descriptor().width==Width);REQUIRE(f.current_gbuffer->descriptor().height==Height);
            REQUIRE(current.input.size()==Width*Height);REQUIRE(current.motion.size()==Width*Height);
            REQUIRE(current.motion_depth.size()==Width*Height);REQUIRE(current.reactive.size()==Width*Height);
            const float expected_motion=-(shift-previous_shift)*(.5f*Width);previous_shift=shift;
            const auto contract=maskMotionContract(current,expected_motion,false);
            if(i){REQUIRE(contract.foreground_samples>100);coverage_mismatches+=contract.coverage_mismatches;
                rejected_foreground+=contract.rejected_foreground;max_vector_error=std::max(max_vector_error,contract.maximum_vector_error);}
            raster_mismatches+=straightEdgeRasterMismatches(current,shift,false);
            if(i>=warm && i<first_move)
            {
                measure(metrics,previous,current,reference,truth,palm);
                if(i<warm+phases)initial[current.phase]=current.output;
            }
            if(i==first_stop || i==first_stop+17 || i==first_stop+35)
            {
                const unsigned sample=i==first_stop?0:i==first_stop+17?1:2;
                REQUIRE(!initial[current.phase].empty());REQUIRE(!stationary_final[current.phase].empty());
                stopped[sample]=stopQuality(reference,shifted_reference,initial[current.phase],stationary_final[current.phase],current.output,palm);
            }
            if(!f.output_dir.empty())
            {std::ostringstream label;label<<"world-"<<std::setw(3)<<std::setfill('0')<<i;dumpFrame(f.output_dir,label.str(),current);}
            if(frames)frames<<i<<','<<i+1<<','<<current.phase<<','<<phases<<','<<current.jitter[0]<<','<<current.jitter[1]<<','<<shift<<','<<(i==0)<<','<<current.resets<<','<<current.previous_valid<<','<<Width<<','<<Height<<','<<OutputWidth<<','<<OutputHeight<<",7,"<<f.mip_bias<<','<<expected_motion<<','<<contract.maximum_vector_error<<'\n';
            previous=std::move(current);
        }
        CHECK(f.scaler->stats().creation_successes==1);CHECK(f.scaler->stats().submitted_calls==total);
        CHECK(f.scaler->stats().reset_calls==1);
        const double change=metrics.absolute_change/metrics.pixels,edge_change=metrics.edge_band_change/metrics.edge_band_pixels;
        auto sorted=metrics.frame_edge_changes;std::sort(sorted.begin(),sorted.end());
        const double p95=sorted[std::min(sorted.size()-1,std::size_t(std::ceil(sorted.size()*.95)-1))];
        const double energy=metrics.edge_energy/metrics.comparisons,reference_energy=referenceEnergy(reference,palm);
        std::cout<<std::setprecision(17)<<"PBR palm75 input="<<Width<<'x'<<Height<<", output="<<OutputWidth<<'x'<<OutputHeight
            <<", mode=7, bias="<<f.mip_bias<<", phases="<<phases<<", warm="<<warm<<", stable="<<stable
            <<", mean_rgb_change="<<change<<", edge_band_change="<<edge_change<<", edge_band_p95="<<p95
            <<", coverage_change="<<double(metrics.coverage_changes)/metrics.pixels<<", mean_reference_error="<<metrics.reference_error/(3.*metrics.pixels)
            <<", minimum_recall="<<metrics.minimum_recall<<", minimum_precision="<<metrics.minimum_precision
            <<", minimum_area="<<metrics.minimum_area<<", maximum_area="<<metrics.maximum_area
            <<", edge_energy="<<energy<<", reference_edge_energy="<<reference_energy
            <<", stop18_phase_error="<<stopped[1].phase_matched_error<<", stop36_phase_error="<<stopped[2].phase_matched_error
            <<", stop18_old_fraction="<<stopped[1].old_history_fraction<<", stop36_old_fraction="<<stopped[2].old_history_fraction
            <<", max_motion_error="<<max_vector_error<<", coverage_mismatches="<<coverage_mismatches
            <<", raster_mismatches="<<raster_mismatches<<", rejected_foreground="<<rejected_foreground<<'\n';
        CHECK(max_vector_error<.002f);CHECK(coverage_mismatches==0);CHECK(raster_mismatches==0);CHECK(rejected_foreground==0);
        // Exactly the existing quality bounds; changing resolution does not
        // change the output-space oracle or turn the18/36world horizons into15/30.
        CHECK(change<=1./255.);CHECK(edge_change<=1./255.);CHECK(p95<=2./255.);
        CHECK(double(metrics.coverage_changes)/metrics.pixels<=.005);CHECK(metrics.reference_error/(3.*metrics.pixels)<=.12);
        CHECK(metrics.minimum_recall>=.90);CHECK(metrics.minimum_precision>=.90);CHECK(metrics.minimum_area>=.85);CHECK(metrics.maximum_area<=1.15);
        CHECK(energy>=.5*reference_energy);
        CHECK(stopped[1].phase_matched_error<=1./255.);CHECK(stopped[2].phase_matched_error<=1./255.);
        CHECK(stopped[1].old_history_fraction<=.02);CHECK(stopped[2].old_history_fraction<=.02);
    }
}
