/**
 * @file almetal_probe_workload_test.mm
 * @brief Opt-in sparse probe records, same-command ordering and private output.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalprobeworkload.h"
#include "../almetalworld.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    struct Environment
    {
        std::optional<std::string> previous;
        bool timing = ALMetalDevice::commandTimingEnabled();
        Environment()
        {
            if (const char* value = std::getenv(ALMetalProbeWorkloadCapture::directory_environment)) previous = value;
            unsetenv(ALMetalProbeWorkloadCapture::directory_environment);
            ALMetalDevice::setCommandTimingEnabled(false);
        }
        ~Environment()
        {
            if (previous) setenv(ALMetalProbeWorkloadCapture::directory_environment, previous->c_str(), 1);
            else unsetenv(ALMetalProbeWorkloadCapture::directory_environment);
            ALMetalDevice::setCommandTimingEnabled(timing);
        }
    };
    struct Directory
    {
        fs::path path;
        Directory()
        {
            char pattern[] = "/private/tmp/almetal-probe-workload-test.XXXXXX";
            const char* created = mkdtemp(pattern);
            REQUIRE(created);
            path = created;
            REQUIRE(chmod(path.c_str(), 0700) == 0);
            REQUIRE(setenv(ALMetalProbeWorkloadCapture::directory_environment, path.c_str(), 1) == 0);
        }
        ~Directory() { std::error_code ignored; fs::remove_all(path, ignored); }
    };
    std::vector<char> readBytes(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        REQUIRE(input.good());
        return {std::istreambuf_iterator<char>(input), {}};
    }
    NSDictionary* readJSON(const fs::path& path)
    {
        NSData* data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]];
        REQUIRE(data);
        NSError* error = nil;
        id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
        REQUIRE(error == nil);
        REQUIRE([object isKindOfClass:NSDictionary.class]);
        return object;
    }
    template<typename Predicate> bool eventually(Predicate predicate)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < end)
        {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return predicate();
    }
    struct Fixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
        std::unique_ptr<ALMetalTexture> depth, normal, specular;
        ALMetalWorldBaseLightingConstants lighting{};
        ALMetalWorldReflectionProbeConstants probes{};
        ALMetalWorldHeroProbeConstants hero{};
        double specular_green = .25, specular_alpha = .5, normal_blue = .4;
        ALMetalProbeWorkloadView view{};
        Fixture()
        {
            INFO(error);
            REQUIRE(device);
            for (unsigned i = 0; i < 4; ++i) view.projection[i * 5] = lighting.inverse_projection[i * 5] = 1.f;
            view.view_kind = 1;
            view.frame_index = 100;
            lighting.config[0] = 1;
            probes.config[0] = 2;
            probes.ref_sphere[1][0] = 3.25f;
        }
        void targets(ALMetalPixelFormat format)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.width = 130; descriptor.height = 82;
            descriptor.format = ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
            descriptor.label = "Probe workload test depth";
            depth = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(depth);
            descriptor.format = format;
            descriptor.label = "Probe workload test normal";
            normal = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(normal);
            descriptor.format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.label = "Probe workload test specular";
            specular = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(specular);
        }
        void clear(ALMetalCommandBuffer& command, double depth_value, double alpha)
        {
            @autoreleasepool
            {
                auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = (__bridge id<MTLTexture>)normal->nativeHandle();
                pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                pass.colorAttachments[0].clearColor = MTLClearColorMake(.2,.3,normal_blue,alpha);
                pass.colorAttachments[1].texture = (__bridge id<MTLTexture>)specular->nativeHandle();
                pass.colorAttachments[1].loadAction = MTLLoadActionClear;
                pass.colorAttachments[1].storeAction = MTLStoreActionStore;
                pass.colorAttachments[1].clearColor = MTLClearColorMake(.2,specular_green,.3,specular_alpha);
                pass.depthAttachment.texture = (__bridge id<MTLTexture>)depth->nativeHandle();
                pass.depthAttachment.loadAction = MTLLoadActionClear;
                pass.depthAttachment.storeAction = MTLStoreActionStore;
                pass.depthAttachment.clearDepth = depth_value;
                pass.stencilAttachment.texture = pass.depthAttachment.texture;
                pass.stencilAttachment.loadAction = MTLLoadActionClear;
                pass.stencilAttachment.storeAction = MTLStoreActionStore;
                auto native = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
                auto encoder = [native renderCommandEncoderWithDescriptor:pass];
                REQUIRE(encoder);
                [encoder endEncoding];
            }
        }
        void arm(ALMetalProbeWorkloadCapture& capture)
        {
            const auto buffers = alMetalResourceAllocationStats().buffers_created;
            auto command = device->makeCommandBuffer("Probe workload arming", &error);
            REQUIRE(command);
            ALMetalDevice::setCommandTimingEnabled(true);
            REQUIRE(capture.captureIfReady(*command, *depth, *normal, *specular, lighting, &probes, &hero, view, &error));
            REQUIRE(capture.stats().scheduled == 0);
            CHECK(alMetalResourceAllocationStats().buffers_created == buffers);
            ALMetalDevice::setCommandTimingEnabled(false);
        }
        fs::path record(const Directory& directory, const char* extension) const
        {
            return directory.path / ("probe-workload-" + std::to_string(getpid()) + '-' +
                std::to_string(view.frame_index) + '-' + std::to_string(view.view_kind) + '-' +
                std::to_string(view.face_index) + extension);
        }
    };
}

TEST_CASE("almetal: probe workload is absent by default and rejects nonprivate directories", "[almetal][probe-workload]")
{
    Environment environment;
    Fixture f;
    const auto before = alMetalResourceAllocationStats();
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    CHECK(alMetalResourceAllocationStats().buffers_created == before.buffers_created);
    REQUIRE(setenv(ALMetalProbeWorkloadCapture::directory_environment, "relative", 1) == 0);
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    CHECK_FALSE(f.error.empty());
    Directory directory;
    REQUIRE(chmod(directory.path.c_str(), 0755) == 0);
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    REQUIRE(chmod(directory.path.c_str(), 0700) == 0);
    CHECK(fs::is_empty(directory.path));
}

TEST_CASE("almetal: sparse probe workload samples only after timing and actual GPU completion", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    INFO(f.error);
    REQUIRE(capture);
    for (unsigned iteration = 0; iteration < 3; ++iteration)
    {
        f.targets(iteration == 1 ? ALMetalPixelFormat::rgb10a2_unorm : ALMetalPixelFormat::rgba16_unorm);
        f.view.view_kind = iteration == 0 ? 1 : iteration == 1 ? 2 : 3;
        f.view.face_index = iteration;
        f.view.frame_index = 100 + iteration;
        f.lighting.surface[1] = iteration != 1;
        if (iteration == 0)
        {
            f.arm(*capture);
            CHECK(fs::is_empty(directory.path));
        }
        auto command = f.device->makeCommandBuffer("Probe workload same-command test", &f.error);
        REQUIRE(command);
        f.clear(*command, iteration == 2 ? 0.0 : .375, iteration == 0 ? .67 : 0.0);
        const auto expected_probes = f.probes;
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, *f.specular, f.lighting, &f.probes, &f.hero, f.view, &f.error));
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, *f.specular, f.lighting, &f.probes, &f.hero, f.view, &f.error));
        CHECK(capture->stats().scheduled == iteration + 1);
        CHECK_FALSE(fs::exists(f.record(directory, ".json")));
        f.probes.ref_sphere[1][0] += 1.f;
        f.depth.reset(); f.normal.reset(); f.specular.reset();
        // Last capture must survive destruction of its CPU helper and both
        // source texture wrappers before submission/completion.
        if (iteration == 2) capture.reset();
        REQUIRE(command->commitAndWait(&f.error));
        if (capture)
        {
            REQUIRE(eventually([&] { return capture->stats().written == iteration + 1; }));
            CHECK(capture->stats().failed == 0);
        }
        else REQUIRE(eventually([&]
        {
            NSData* data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:f.record(directory, ".json").c_str()]];
            return data && [NSJSONSerialization JSONObjectWithData:data options:0 error:nullptr] != nil;
        }));
        const auto binary_path = f.record(directory, ".bin");
        const auto json_path = f.record(directory, ".json");
        const auto data = readBytes(binary_path);
        REQUIRE(data.size() == 49728 + 2560 * 64);
        CHECK(std::memcmp(data.data(), "ALPRWK2", 7) == 0);
        CHECK(std::memcmp(data.data() + 400, &expected_probes, sizeof(expected_probes)) == 0);
        NSDictionary* json = readJSON(json_path);
        CHECK([json[@"schema_version"] unsignedIntValue] == 2);
        CHECK([json[@"binary"][@"hero_offset"] unsignedIntValue] == 49616);
        CHECK([json[@"binary"][@"samples_offset"] unsignedIntValue] == 49728);
        CHECK(std::memcmp(data.data() + 49616, &f.hero, sizeof(f.hero)) == 0);
        CHECK([json[@"status"] isEqualToString:@"complete"]);
        CHECK([json[@"gpu_completed"] boolValue]);
        CHECK([json[@"width"] unsignedIntValue] == 130);
        CHECK([json[@"height"] unsignedIntValue] == 82);
        struct Sample { std::uint32_t x,y; float depth,alpha; std::uint32_t kind,geometry,reserved[2]; };
        for (unsigned y = 0; y < 40; ++y) for (unsigned x = 0; x < 64; ++x)
        {
            Sample sample{};
            std::memcpy(&sample, data.data() + 49728 + (y * 64 + x) * 64, 32);
            CHECK(sample.x == (2*x+1)*130/128);
            CHECK(sample.y == (2*y+1)*82/80);
            CHECK(sample.depth == (iteration == 2 ? 0.f : .375f));
            CHECK(sample.kind == (iteration == 0 ? 1u : iteration == 1 ? 0u : 2u));
            CHECK(sample.geometry == (iteration != 2));
            CHECK(sample.reserved[0] == 0);
            CHECK(sample.reserved[1] == 0);
        }
        for (const auto& path : {binary_path, json_path})
        {
            struct stat status{};
            REQUIRE(stat(path.c_str(), &status) == 0);
            CHECK((status.st_mode & 0777) == 0600);
            CHECK(status.st_uid == geteuid());
        }
    }
}

TEST_CASE("almetal: abandoned probe workload commands emit no GPU sample bytes", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    f.targets(ALMetalPixelFormat::rgba16_unorm);
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    REQUIRE(capture);
    f.arm(*capture);
    {
        auto command = f.device->makeCommandBuffer("Abandoned probe workload", &f.error);
        REQUIRE(command);
        f.clear(*command, .375, .67);
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, *f.specular, f.lighting, &f.probes, &f.hero, f.view, &f.error));
    }
    auto poll = f.device->makeCommandBuffer("Probe workload completion polling", &f.error);
    REQUIRE(poll);
    ALMetalProbeWorkloadView ignored{};
    REQUIRE(capture->captureIfReady(*poll, *f.depth, *f.normal, *f.specular, f.lighting, &f.probes, &f.hero, ignored, &f.error));
    REQUIRE(eventually([&] { return capture->stats().cancelled == 1; }));
    CHECK_FALSE(fs::exists(f.record(directory, ".bin")));
    NSDictionary* json = readJSON(f.record(directory, ".json"));
    CHECK([json[@"status"] isEqualToString:@"cancelled"]);
    CHECK_FALSE([json[@"gpu_completed"] boolValue]);
}

TEST_CASE("almetal: probe workload output never replaces existing files", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    f.targets(ALMetalPixelFormat::rgba16_unorm);
    const auto json_path = f.record(directory, ".json");
    { std::ofstream sentinel(json_path); sentinel << "existing private record"; }
    REQUIRE(chmod(json_path.c_str(), 0600) == 0);
    const auto original = readBytes(json_path);
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    REQUIRE(capture);
    f.arm(*capture);
    auto command = f.device->makeCommandBuffer("Probe workload no-overwrite test", &f.error);
    REQUIRE(command);
    f.clear(*command, .375, .67);
    REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, *f.specular, f.lighting, &f.probes, &f.hero, f.view, &f.error));
    REQUIRE(command->commitAndWait(&f.error));
    REQUIRE(eventually([&] { return capture->stats().failed == 1; }));
    CHECK(readBytes(json_path) == original);
    CHECK_FALSE(fs::exists(f.record(directory, ".bin")));
}

TEST_CASE("almetal: probe packet layouts preserve archived v1 records", "[almetal][probe-workload]")
{
    const auto old = ALMetalProbeWorkloadPacketLayout::forVersion(1);
    const auto current = ALMetalProbeWorkloadPacketLayout::forVersion(2);
    REQUIRE(old); REQUIRE(current);
    CHECK(old->samples_offset == 49616); CHECK(old->sample_stride == 32);
    CHECK(old->hero_bytes == 0); CHECK(old->hero_offset == 0);
    CHECK(current->samples_offset == 49728); CHECK(current->sample_stride == 64);
    CHECK(current->hero_offset == old->samples_offset); CHECK(current->hero_bytes == 112);
    CHECK(current->probe_offset == old->probe_offset);
    CHECK_FALSE(ALMetalProbeWorkloadPacketLayout::forVersion(0));
    CHECK_FALSE(ALMetalProbeWorkloadPacketLayout::forVersion(3));
    // A version-dispatched offline reader can still decode the original
    // frozen32B sample and can read exactly that prefix from the v2 record.
    ALMetalProbeWorkloadSampleV1 frozen{7,9,.375f,.67f,1,1,{0,0}};
    std::vector<char> archived(old->samples_offset + old->sample_stride);
    std::memcpy(archived.data(), "ALPRWK1", 7);
    const std::uint32_t version = 1; std::memcpy(archived.data()+8, &version, 4);
    std::memcpy(archived.data()+old->samples_offset, &frozen, sizeof(frozen));
    std::uint32_t decoded_version; std::memcpy(&decoded_version, archived.data()+8, 4);
    const auto layout = ALMetalProbeWorkloadPacketLayout::forVersion(decoded_version);
    REQUIRE(layout);
    ALMetalProbeWorkloadSampleV1 decoded{};
    std::memcpy(&decoded, archived.data()+layout->samples_offset, sizeof(decoded));
    CHECK(std::memcmp(&decoded, &frozen, sizeof(decoded)) == 0);
    ALMetalProbeWorkloadSampleV2 v2{}; v2.prefix = frozen;
    CHECK(std::memcmp(&v2, &frozen, sizeof(frozen)) == 0);
}

TEST_CASE("almetal: sparse workload decodes base channels and exact frozen hero weights", "[almetal][probe-workload]")
{
    Environment environment;
    Fixture f;
    // Frozen weight contract from1eb44bb reflectionProbeF::tapHeroProbe and
    // the unmodified native finish, deliberately independent of sampler code.
    const std::string reference = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Hero { float4x4 box; float4 sphere; float4 clip; uint4 config; };
struct ReferenceOutput { float4 values; packed_float3 position; uint flags; };
static float original_weight(float3 pos,float gloss,constant Hero& h) {
    if(h.config.y<1u || gloss<=0.75) return 0.0;
    float w;
    if(h.config.x<1u) {
        const float3 p=(h.box*float4(pos,1.0)).xyz;
        w=max(1.0-max(max(abs(p.x),abs(p.y)),abs(p.z)),0.0);
    } else {
        const float r=h.sphere.w;
        const float inner=r*0.5;
        const float d=max(length(pos-h.sphere.xyz),0.001);
        const float a=1.0-max(d-inner,0.0)/max(r-inner,0.001);
        w=a/d;
    }
    float c=dot(pos,h.clip.xyz)+h.clip.w;
    c=clamp((c*0.95+0.05)*10.0,0.0,1.0);
    w=clamp(w*10.0*c,0.0,1.0);
    w*=clamp(gloss-0.75,0.0,1.0)*4.0;
    if(w<=0.0) return 0.0;
    return w;
}
kernel void reference_weights(texture2d<float> depth[[texture(0)]],
    texture2d<float> normal[[texture(1)]], texture2d<float> specular[[texture(2)]],
    device ReferenceOutput* output[[buffer(0)]],constant float4x4& inverse[[buffer(1)]],
    constant uint2& config[[buffer(2)]],constant Hero& hero[[buffer(3)]],
    uint2 cell[[thread_position_in_grid]]) {
    uint2 dim=uint2(depth.get_width(),depth.get_height());
    uint2 pixel=min(((cell*2u+1u)*dim)/uint2(128u,80u),dim-1u);
    float d=depth.read(pixel).r;
    float4 n=normal.read(pixel),s=specular.read(pixel);
    bool pbr=abs(n.a-0.67)<0.1;
    float g=pbr?1.0-(s.g+s.a/255.0):s.a;
    float env=pbr?0.0:n.z;
    float ndc=config.x!=0u?d:d*2.0-1.0;
    float2 uv=(float2(pixel)+0.5)/float2(dim);
    float4 p=inverse*float4(uv*2.0-1.0,ndc,1.0);
    p.w=max(p.w,0.000001);p/=p.w;
    bool geometry=config.x!=0u?d>0.0:d<1.0;
    bool active=geometry&&config.y==0u;
    float wg=active?original_weight(p.xyz,g,hero):0.0;
    float wl=active?original_weight(p.xyz,1.0,hero):0.0;
    uint flags=uint(active&&(pbr||g>0.0))|
       (uint(active&&!pbr&&env>0.0)<<1u);
    flags|=uint((flags&1u)!=0u&&wg==1.0)<<2u;
    flags|=uint((flags&2u)!=0u&&wl==1.0)<<3u;
    output[cell.y*64u+cell.x]={float4(g,env,wg,wl),packed_float3(p.xyz),flags};
})metal";
    auto library = ALMetalShaderLibrary::compileSource(*f.device, reference, "Frozen base-channel weight reference", &f.error);
    INFO(f.error); REQUIRE(library);
    ALMetalComputePipelineDescriptor pd;
    pd.library = library.get(); pd.function = "reference_weights"; pd.label = "Frozen base-channel reference";
    auto pipeline = ALMetalComputePipeline::create(*f.device, pd, &f.error);
    REQUIRE(pipeline);
    ALMetalBufferDescriptor bd; bd.length = 2560 * 32; bd.label = "Frozen base-channel reference output";
    auto expected = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(expected);
    struct Case { bool pbr; unsigned alpha, green, shape; bool active, partial, clipped, sky, reverse, large; };
    const std::array<Case, 14> cases{{
        {true,0,0,1,true,false,false,false,true,false},
        {false,255,0,1,true,false,false,false,true,false},
        {false,230,0,0,true,false,false,false,true,false},
        {false,191,0,1,true,false,false,false,true,false},
        {false,192,0,1,true,false,false,false,true,false},
        {false,254,0,1,true,false,false,false,true,false},
        {false,0,0,1,true,false,false,false,true,false},
        {true,255,255,1,true,false,false,false,true,false}, // PBR still uses gloss at nonpositive g.
        {true,0,0,1,false,false,false,false,true,false},
        {false,255,0,1,true,true,false,false,true,false},
        {false,255,0,0,true,false,true,false,true,false},
        {false,255,0,1,true,false,false,true,true,false},
        {true,0,0,1,true,false,false,false,false,false},
        {true,0,0,1,true,false,false,false,true,true},
    }};
    for (std::size_t ci = 0; ci < cases.size(); ++ci)
    {
        INFO(ci);
        const auto c = cases[ci];
        Directory directory;
        f.targets(ALMetalPixelFormat::rgba16_unorm);
        f.view.frame_index = 300 + ci;
        f.specular_alpha = c.alpha / 255.; f.specular_green = c.green / 255.;
        f.lighting.config[0] = c.reverse;
        f.lighting.environment_config[0] = c.sky;
        f.lighting.inverse_projection[14] = c.large ? 4000.f : 0.f;
        f.hero = {};
        for (unsigned i = 0; i < 4; ++i) f.hero.hero_box[i*5] = 1;
        f.hero.hero_box[0] = f.hero.hero_box[5] = f.hero.hero_box[10] = .2f;
        f.hero.hero_box[14] = -.075f;
        f.hero.hero_sphere[2] = c.large ? 4000.375f : .375f;
        f.hero.hero_sphere[3] = 8;
        f.hero.clip_plane[0] = c.clipped ? .2f : 0;
        f.hero.clip_plane[3] = c.clipped ? -.05f : 1;
        f.hero.config[0] = c.shape; f.hero.config[1] = c.active; f.hero.config[2] = 3;
        if (c.partial) { f.hero.hero_sphere[0] = 100; f.hero.hero_sphere[3] = 400; }
        const auto frozen_hero = c.active ? f.hero : ALMetalWorldHeroProbeConstants{};
        auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error); REQUIRE(capture);
        f.arm(*capture);
        auto command = f.device->makeCommandBuffer("Sparse exact-channel test", &f.error); REQUIRE(command);
        f.clear(*command, .375, c.pbr ? .67 : .34);
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, *f.specular,
            f.lighting, &f.probes, c.active ? &f.hero : nullptr, f.view, &f.error));
        auto encoder = ALMetalComputeEncoder::begin(*command, "Frozen exact-channel reference", &f.error);
        REQUIRE(encoder);
        const std::uint32_t config[2]{std::uint32_t(c.reverse),std::uint32_t(c.sky)};
        REQUIRE(encoder->setPipeline(*pipeline, &f.error));
        REQUIRE(encoder->setTexture(0, f.depth.get(), &f.error));
        REQUIRE(encoder->setTexture(1, f.normal.get(), &f.error));
        REQUIRE(encoder->setTexture(2, f.specular.get(), &f.error));
        REQUIRE(encoder->setBuffer(0, *expected, 0, &f.error));
        REQUIRE(encoder->setBytes(1, f.lighting.inverse_projection, 64, &f.error));
        REQUIRE(encoder->setBytes(2, config, sizeof(config), &f.error));
        REQUIRE(encoder->setBytes(3, &f.hero, sizeof(f.hero), &f.error));
        REQUIRE(encoder->dispatch(64,40,1,8,8,1,&f.error)); REQUIRE(encoder->end(&f.error));
        // Snapshot constants and native textures survive producer mutation.
        f.hero.config[1] = 0;
        REQUIRE(command->commitAndWait(&f.error));
        REQUIRE(eventually([&] { return capture->stats().written == 1; }));
        const auto bytes = readBytes(f.record(directory, ".bin"));
        REQUIRE(bytes.size() == 49728 + 2560 * 64);
        CHECK(std::memcmp(bytes.data()+49616, &frozen_hero, sizeof(frozen_hero)) == 0);
        const auto reference_bytes = static_cast<const char*>(expected->mappedBytes());
        REQUIRE(reference_bytes);
        std::size_t differences = 0, full_gloss = 0, full_legacy = 0;
        for (std::size_t i = 0; i < 2560; ++i)
        {
            ALMetalProbeWorkloadSampleV2 sample{};
            std::memcpy(&sample, bytes.data()+49728+i*64, sizeof(sample));
            differences += std::memcmp(&sample.glossiness, reference_bytes+i*32, 32) != 0;
            full_gloss += (sample.channel_flags & sample.full_hero_gloss) != 0;
            full_legacy += (sample.channel_flags & sample.full_hero_legacy) != 0;
        }
        CHECK(differences == 0);
        if (!c.active || c.sky || c.partial) CHECK(full_gloss+full_legacy == 0);
        if (ci == 0) CHECK(full_gloss == 2560);
        if (ci == 2) { CHECK(full_gloss == 0); CHECK(full_legacy == 2560); }
    }
}
