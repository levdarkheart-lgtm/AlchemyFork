/**
 * @file almetal_rigged_depth_invariance_test.mm
 * @brief Physical production-VS coplanar depth contract (PPD-523).
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Standalone, no viewer/account/capture input. argv[1], or
 * ALMETAL_TEST_METALLIB, selects the EXACT production offline metallib.
 * Only the constant diagnostic fragment is compiled here. Shader positions,
 * clipping, raster coverage and depth all come from the production vertex
 * entry points. This does not qualify the owner's attachment or materials.
 */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../almetalworlddraws.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr unsigned side = 64, row_bytes = side * 4, pixels = side * side;
struct alignas(16) F4 { float x, y, z, w; };
struct alignas(16) U4 { std::uint32_t x, y, z, w; };
// Private production packing in almetalworld.cpp: WorldSimpleConstants,
// WorldShinyConstants, WorldPBRSlotConstants/WorldPBRConstants. Reuse the
// PUBLIC actual matrix prefix; the remaining ABI contains no shader math.
struct alignas(16) Simple
{
    ALMetalWorldViewMatrices view;
    F4 material{}; U4 config{1, 1, 0, 0};
    F4 wind{}, sine{}, gravity{}, cloth{}, mirror{0, 0, 0, 1};
};
struct alignas(16) Shiny
{
    ALMetalWorldViewMatrices view;
    U4 config{1, 1, 0, 0};
    F4 wind{}, sine{}, gravity{}, cloth{}, mirror{0, 0, 0, 1};
};
struct alignas(16) PBRSlot
{
    F4 transform[4][2]{}; F4 emissive{}, properties{.5f, 0, 0, 1};
};
struct alignas(16) PBR { PBRSlot slots[8]{}; U4 config{1, 0, 0, 0}; };
static_assert(sizeof(F4) == 16 && sizeof(U4) == 16);
static_assert(sizeof(Simple) == 400 && offsetof(Simple, config) == 304);
static_assert(sizeof(Shiny) == 384 && offsetof(Shiny, config) == 288);
static_assert(sizeof(PBRSlot) == 160 && sizeof(PBR) == 1296);

struct Family { const char* name; bool shiny; };
constexpr std::array<Family, 9> families{{
    {"almetal_world_simple_skinned_vertex", false},
    {"almetal_world_material_skinned_vertex", false},
    {"almetal_world_bump_skinned_vertex", false},
    {"almetal_world_pbr_skinned_vertex", false},
    {"almetal_world_fullbright_skinned_vertex", false},
    {"almetal_world_shiny_skinned_vertex", true},
    {"almetal_world_legacy_alpha_skinned_vertex", true},
    {"almetal_world_legacy_glow_skinned_vertex", false},
    {"almetal_world_pbr_glow_skinned_vertex", false},
}};
constexpr unsigned reference = 6;
using M4 = std::array<double, 16>;
using D4 = std::array<double, 4>;
M4 identity() { M4 m{}; m[0] = m[5] = m[10] = m[15] = 1; return m; }
M4 multiply(const M4& a, const M4& b)
{
    M4 result{};
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned r = 0; r < 4; ++r)
            for (unsigned k = 0; k < 4; ++k)
                result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    return result;
}
D4 transform(const float* m, D4 p)
{
    D4 result{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c) result[r] += double(m[c * 4 + r]) * p[c];
    return result;
}
struct Scene
{
    Simple simple; Shiny shiny;
    F4 origin;
    // Four valid joints, all identity: nontrivial packed four-influence
    // weights must preserve the authored triangle rather than skip skinning.
    std::array<F4, 13> palette{};
    std::array<F4, 3> vertices{{{-1.37f, -.93f, -.2021f, 0},
        {1.29f, -.81f, .0723f, 0}, {-.73f, 1.21f, .0117f, 0}}};
    std::array<float, 3> camera_toward{};

    Scene(F4 where, unsigned camera) : origin(where)
    {
        const double yaw[] = {.0, .317, -.481};
        const double pitch[] = {.0, -.193, .271};
        const double cy = std::cos(yaw[camera]), sy = std::sin(yaw[camera]);
        const double cp = std::cos(pitch[camera]), sp = std::sin(pitch[camera]);
        M4 ry = identity(), rx = identity();
        ry[0] = cy; ry[8] = sy; ry[2] = -sy; ry[10] = cy;
        rx[5] = cp; rx[9] = -sp; rx[6] = sp; rx[10] = cp;
        M4 rotation = multiply(rx, ry), translation = identity();
        // Camera is a fixed distance along its own +Z, around the avatar.
        // Only synthetic origins/cameras are used; none came from a capture.
        const double distance = 4.731;
        const double o[] = {where.x, where.y, where.z};
        for (unsigned r = 0; r < 3; ++r)
        {
            translation[12 + r] = -o[r] - rotation[r * 4 + 2] * distance;
            camera_toward[r] = float(rotation[r * 4 + 2]);
        }
        M4 mv = multiply(rotation, translation), projection{};
        const double near = .173, far = 173.;
        projection[0] = 1.0 / std::tan(.617);
        projection[5] = 1.0 / std::tan(.583);
        projection[10] = near / (far - near);
        projection[11] = -1;
        projection[14] = near * far / (far - near);
        const M4 mvp = multiply(projection, mv);
        for (unsigned i = 0; i < 16; ++i)
        {
            simple.view.modelview[i] = float(mv[i]);
            simple.view.modelview_projection[i] = float(mvp[i]);
        }
        for (unsigned c = 0; c < 3; ++c)
            for (unsigned r = 0; r < 3; ++r)
            {
                simple.view.normal_matrix[c][r] = float(rotation[c * 4 + r]);
                simple.view.rigged_normal_matrix[c][r] = float(rotation[c * 4 + r]);
            }
        shiny.view = simple.view;
        palette[0] = origin;
        for (unsigned j = 0; j < 4; ++j)
        {
            palette[1 + 3 * j] = {1, 0, 0, 0};
            palette[2 + 3 * j] = {0, 1, 0, 0};
            palette[3 + 3 * j] = {0, 0, 1, 0};
        }
    }
};
struct Image
{
    std::array<std::uint32_t, pixels> color{}, depth{};
    unsigned coverage() const
    { unsigned n = 0; for (auto c : color) n += c != 0; return n; }
};
struct Results
{
    unsigned checks = 0, failures = 0;
    void check(bool ok, const std::string& description)
    {
        ++checks;
        if (!ok) { ++failures; std::cout << "FAIL " << description << '\n'; }
    }
};
void require(bool ok, const char* what, NSError* error = nil)
{
    if (!ok) throw std::runtime_error(std::string(what) +
        (error ? std::string(": ") + error.localizedDescription.UTF8String : ""));
}
struct GPU
{
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLLibrary> production, diagnostic;
    std::array<id<MTLRenderPipelineState>, families.size()> pipeline;
    id<MTLDepthStencilState> write_depth, test_depth;
    id<MTLTexture> color, depth;
    id<MTLBuffer> read_color, read_depth;

    explicit GPU(const char* path)
    {
        device = MTLCreateSystemDefaultDevice(); require(device != nil, "Metal device");
        queue = [device newCommandQueue]; require(queue != nil, "Metal queue");
        NSError* error = nil;
        production = [device newLibraryWithURL:[NSURL fileURLWithPath:
            [NSString stringWithUTF8String:path]] error:&error];
        require(production != nil, "production metallib", error);
        // Constant fragment deliberately consumes no interpolated varying.
        // It cannot implement, repair or provide expected vertex positions.
        NSString* source = @"#include <metal_stdlib>\nusing namespace metal;\n"
            "fragment float4 rigged_depth_color(constant float4& c [[buffer(0)]]) { return c; }\n";
        diagnostic = [device newLibraryWithSource:source options:nil error:&error];
        require(diagnostic != nil, "diagnostic constant fragment", error);
        for (unsigned i = 0; i < families.size(); ++i)
        {
            MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
            descriptor.vertexFunction = [production newFunctionWithName:
                [NSString stringWithUTF8String:families[i].name]];
            require(descriptor.vertexFunction != nil, families[i].name);
            descriptor.fragmentFunction = [diagnostic newFunctionWithName:@"rigged_depth_color"];
            descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
            descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
            pipeline[i] = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
            require(pipeline[i] != nil, families[i].name, error);
        }
        MTLDepthStencilDescriptor* ds = [MTLDepthStencilDescriptor new];
        ds.depthCompareFunction = MTLCompareFunctionGreaterEqual;
        ds.depthWriteEnabled = YES; write_depth = [device newDepthStencilStateWithDescriptor:ds];
        ds.depthWriteEnabled = NO; test_depth = [device newDepthStencilStateWithDescriptor:ds];
        require(write_depth && test_depth, "inclusive reverse-Z depth states");
        MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            MTLPixelFormatRGBA8Unorm width:side height:side mipmapped:NO];
        td.storageMode = MTLStorageModePrivate; td.usage = MTLTextureUsageRenderTarget;
        color = [device newTextureWithDescriptor:td];
        td.pixelFormat = MTLPixelFormatDepth32Float; depth = [device newTextureWithDescriptor:td];
        require(color && depth, "private color/depth targets");
        read_color = [device newBufferWithLength:pixels * 4 options:MTLResourceStorageModeShared];
        read_depth = [device newBufferWithLength:pixels * 4 options:MTLResourceStorageModeShared];
        require(read_color && read_depth, "shared readback buffers");
        std::cout << "device=" << device.name.UTF8String << " families=" << families.size()
            << " side=" << side << " reverse_z=1 compare=greater_equal\n";
    }
    void draw(id<MTLRenderCommandEncoder> e, const Scene& scene, unsigned family,
        F4 paint, float toward, bool write)
    {
        auto vertices = scene.vertices;
        for (auto& v : vertices)
        {
            v.x += toward * scene.camera_toward[0];
            v.y += toward * scene.camera_toward[1];
            v.z += toward * scene.camera_toward[2];
        }
        const std::array<F4, 3> normals{{{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}}};
        const std::array<F4, 3> tangents{{{1, 0, 0, 1}, {1, 0, 0, 1}, {1, 0, 0, 1}}};
        const std::array<F4, 3> weights{{{.125f, 1.25f, 2.375f, 3.25f},
            {.125f, 1.25f, 2.375f, 3.25f}, {.125f, 1.25f, 2.375f, 3.25f}}};
        const std::array<float, 6> uv{{0, 0, 1, 0, 0, 1}};
        const std::array<std::uint32_t, 3> colors{{~0u, ~0u, ~0u}};
        const std::uint32_t joint_count = 4;
        PBR pbr;
        for (auto& slot : pbr.slots)
            for (auto& t : slot.transform) { t[0] = {1, 0, 0, 0}; t[1] = {0, 1, 0, 0}; }
        [e setRenderPipelineState:pipeline[family]];
        [e setDepthStencilState:write ? write_depth : test_depth];
        [e setCullMode:MTLCullModeNone];
        [e setViewport:MTLViewport{0, 0, side, side, 0, 1}];
        [e setVertexBytes:vertices.data() length:sizeof(vertices) atIndex:0];
        [e setVertexBytes:normals.data() length:sizeof(normals) atIndex:1];
        [e setVertexBytes:uv.data() length:sizeof(uv) atIndex:2];
        [e setVertexBytes:colors.data() length:sizeof(colors) atIndex:3];
        if (families[family].shiny)
            [e setVertexBytes:&scene.shiny length:sizeof(scene.shiny) atIndex:4];
        else [e setVertexBytes:&scene.simple length:sizeof(scene.simple) atIndex:4];
        [e setVertexBytes:scene.palette.data() length:sizeof(scene.palette) atIndex:5];
        [e setVertexBytes:weights.data() length:sizeof(weights) atIndex:6];
        [e setVertexBytes:&joint_count length:sizeof(joint_count) atIndex:7];
        [e setVertexBytes:tangents.data() length:sizeof(tangents) atIndex:8];
        [e setVertexBytes:uv.data() length:sizeof(uv) atIndex:9];
        [e setVertexBytes:uv.data() length:sizeof(uv) atIndex:10];
        [e setVertexBytes:&pbr length:sizeof(pbr) atIndex:11];
        [e setFragmentBytes:&paint length:sizeof(paint) atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    }
    Image render(const Scene& scene, unsigned first, int second = -1, float toward = 0)
    {
        id<MTLCommandBuffer> cb = [queue commandBuffer]; require(cb != nil, "command buffer");
        MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = color;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
        rp.depthAttachment.texture = depth; rp.depthAttachment.clearDepth = 0;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> e = [cb renderCommandEncoderWithDescriptor:rp];
        require(e != nil, "render encoder");
        draw(e, scene, first, {1, 0, 0, 1}, 0, true);
        if (second >= 0) draw(e, scene, unsigned(second), {0, 1, 0, 1}, toward, false);
        [e endEncoding];
        id<MTLBlitCommandEncoder> b = [cb blitCommandEncoder]; require(b != nil, "readback encoder");
        for (unsigned i = 0; i < 2; ++i)
            [b copyFromTexture:i ? depth : color sourceSlice:0 sourceLevel:0
                sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(side, side, 1)
                toBuffer:i ? read_depth : read_color destinationOffset:0
                destinationBytesPerRow:row_bytes destinationBytesPerImage:row_bytes * side];
        [b endEncoding]; [cb commit]; [cb waitUntilCompleted];
        require(cb.status == MTLCommandBufferStatusCompleted, "GPU command completion", cb.error);
        Image image;
        std::memcpy(image.color.data(), read_color.contents, sizeof(image.color));
        std::memcpy(image.depth.data(), read_depth.contents, sizeof(image.depth));
        return image;
    }
};

// Independent mathematical scene oracle: double evaluation of the PROVIDED
// float matrix and unreduced world geometry. No emulation of the shader's
// blend, matrix split, rounding, FMA, or candidate helper. Avoid subpixel edge
// ambiguity by requiring only samples >1 pixel from all authored edges.
void coverageOracle(const Scene& scene, const Image& image, Results& results, const std::string& label)
{
    std::array<std::array<double, 2>, 3> screen{};
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& p = scene.vertices[i];
        const D4 clip = transform(scene.simple.view.modelview_projection,
            {double(p.x) + scene.origin.x, double(p.y) + scene.origin.y,
             double(p.z) + scene.origin.z, 1});
        require(clip[3] > 0, "authored triangle lies in front of camera");
        screen[i] = {(clip[0] / clip[3] + 1) * side / 2,
                     (clip[1] / clip[3] + 1) * side / 2};
    }
    auto edge = [](auto a, auto b, double x, double y) {
        return ((b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])) /
            std::hypot(b[0] - a[0], b[1] - a[1]); };
    const double sign = edge(screen[0], screen[1], screen[2][0], screen[2][1]) > 0 ? 1 : -1;
    unsigned interior = 0, exterior = 0, wrong = 0, invalid_depth = 0;
    float minimum = 1, maximum = 0;
    for (unsigned y = 0; y < side; ++y)
        for (unsigned x = 0; x < side; ++x)
        {
            double smallest = std::numeric_limits<double>::infinity();
            for (unsigned i = 0; i < 3; ++i)
                smallest = std::min(smallest, sign * edge(screen[i], screen[(i + 1) % 3], x + .5, y + .5));
            const unsigned index = y * side + x;
            const bool covered = image.color[index] != 0;
            if (smallest > 1) { ++interior; wrong += !covered; }
            if (smallest < -1) { ++exterior; wrong += covered; }
            float z; std::memcpy(&z, &image.depth[index], sizeof(z));
            if (covered)
            {
                invalid_depth += !(std::isfinite(z) && z > 0 && z < 1);
                minimum = std::min(minimum, z); maximum = std::max(maximum, z);
            }
            else invalid_depth += image.depth[index] != 0;
        }
    results.check(interior > 50 && exterior > 500, label + " nontrivial independent coverage oracle");
    results.check(wrong == 0, label + " independent interior/exterior coverage wrong=" + std::to_string(wrong));
    results.check(invalid_depth == 0 && maximum - minimum > .0001f,
        label + " finite sloped non-clear depth");
}
void run(GPU& gpu, Results& results)
{
    constexpr std::array<F4, 3> origins{{{.125f, -.25f, .375f, 0},
        {182.317f, 30.193f, 28.271f, 0}, {182.317f, 30.193f, 4096.271f, 0}}};
    unsigned pairs = 0;
    for (unsigned o = 0; o < origins.size(); ++o)
        for (unsigned camera = 0; camera < 3; ++camera)
        {
            @autoreleasepool
            {
                const Scene scene(origins[o], camera);
                const std::string context = "origin=" + std::to_string(o) + " camera=" + std::to_string(camera);
                std::array<Image, families.size()> single;
                for (unsigned f = 0; f < families.size(); ++f)
                {
                    single[f] = gpu.render(scene, f);
                    coverageOracle(scene, single[f], results, context + " " + families[f].name);
                }
                for (unsigned f = 0; f < families.size(); ++f)
                {
                    unsigned coverage_delta = 0, depth_delta = 0;
                    for (unsigned i = 0; i < pixels; ++i)
                    {
                        coverage_delta += single[f].color[i] != single[reference].color[i];
                        depth_delta += single[f].depth[i] != single[reference].depth[i];
                    }
                    const auto label = context + " " + families[f].name;
                    results.check(coverage_delta == 0, label + " coverage bits differ=" + std::to_string(coverage_delta));
                    results.check(depth_delta == 0, label + " depth bits differ=" + std::to_string(depth_delta));
                    // Both writer/overlay directions. Equality must pass an
                    // inclusive depth test without bias or disabled depth.
                    for (unsigned direction = 0; direction < 2; ++direction)
                    {
                        const unsigned a = direction ? reference : f;
                        const unsigned b = direction ? f : reference;
                        const Image overlay = gpu.render(scene, a, int(b));
                        unsigned missed = 0, unexpected = 0, overwritten_depth = 0;
                        for (unsigned i = 0; i < pixels; ++i)
                        {
                            if (single[a].color[i]) missed += overlay.color[i] != 0xff00ff00u;
                            else unexpected += overlay.color[i] != 0;
                            overwritten_depth += overlay.depth[i] != single[a].depth[i];
                        }
                        results.check(missed == 0 && unexpected == 0,
                            label + " order=" + std::to_string(direction) + " coplanar lost=" +
                            std::to_string(missed) + " outside=" + std::to_string(unexpected));
                        results.check(overwritten_depth == 0, label + " overlay preserves writer depth");
                        ++pairs;
                    }
                }
                for (float offset : {-.2f, .2f})
                {
                    const Image control = gpu.render(scene, reference, int(reference), offset);
                    unsigned expected = 0, wrong = 0;
                    for (unsigned i = 0; i < pixels; ++i)
                        if (single[reference].color[i])
                        {
                            ++expected;
                            wrong += control.color[i] != (offset > 0 ? 0xff00ff00u : 0xff0000ffu);
                        }
                    // The front triangle grows and contains the original;
                    // behind may shrink, but must never overwrite its red.
                    results.check(expected > 50 && wrong == 0, context + (offset > 0
                        ? " distinct front accepted" : " distinct behind rejected") +
                        " wrong=" + std::to_string(wrong));
                    results.check(control.depth == single[reference].depth,
                        context + " front/behind read-only depth unchanged");
                }
                std::cout << "scene " << context << " reference_coverage=" << single[reference].coverage() << '\n';
            }
        }
    std::cout << "ordered_coplanar_pairs=" << pairs << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        const char* path = argc == 2 ? argv[1] : std::getenv("ALMETAL_TEST_METALLIB");
        if (!path || !*path || argc > 2)
        {
            std::cerr << "usage: almetal_rigged_depth_invariance_test <production.metallib>\n";
            return 2;
        }
        try
        {
            GPU gpu(path); Results results; run(gpu, results);
            std::cout << "checks=" << results.checks << " failures=" << results.failures << '\n';
            return results.failures ? 1 : 0;
        }
        catch (const std::exception& error)
        { std::cerr << "ERROR " << error.what() << '\n'; return 2; }
    }
}
