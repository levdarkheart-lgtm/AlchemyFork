/** Production-component replay and cost qualification; not a viewer render path.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * The only synchronous waits/readbacks here are explicit offline qualification.
 */
#include "almetalaurora.h"
#include "almetalworld.h"
#include "../aurora-evaluation/workload/input_workload.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
namespace fs    = std::filesystem;
using Pixel     = std::array<float, 4>;
using HalfPixel = std::array<__fp16, 4>;
using Motion    = std::array<__fp16, 2>;
using Clock     = std::chrono::steady_clock;
void require(bool ok, const std::string& message)
{
    if (!ok)
        throw std::runtime_error(message);
}
double ms(Clock::time_point a, Clock::time_point b)
{ return std::chrono::duration<double, std::milli>(b - a).count(); }
NSString* str(const fs::path& path)
{ return [NSString stringWithUTF8String:path.c_str()]; }
void json(const fs::path& path, id object)
{
    NSError* error = nil;
    auto     data  = [NSJSONSerialization dataWithJSONObject:object options:NSJSONWritingPrettyPrinted error:&error];
    require(data && [data writeToFile:str(path) options:NSDataWritingAtomic error:&error],
            error ? error.localizedDescription.UTF8String : "JSON write failed");
}
template<class T>
std::vector<T> read(const fs::path& path, std::size_t count)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(file.good() && file.tellg() == std::streamoff(count * sizeof(T)), "Wrong-sized input " + path.string());
    file.seekg(0);
    std::vector<T> values(count);
    file.read(reinterpret_cast<char*>(values.data()), count * sizeof(T));
    require(file.good(), "Read failed " + path.string());
    return values;
}
template<class T>
void write(const fs::path& path, const std::vector<T>& values)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
    require(file.good(), "Write failed " + path.string());
}
std::string idx(unsigned frame)
{
    std::ostringstream s;
    s << std::setw(3) << std::setfill('0') << frame;
    return s.str();
}
std::vector<std::string> columns(std::string line)
{
    require(line.find('"') == std::string::npos, "Expected unquoted fixture CSV");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    std::vector<std::string> out;
    std::stringstream        s(line);
    std::string              x;
    while (std::getline(s, x, ','))
        out.push_back(x);
    return out;
}
struct Row
{
    unsigned           frame = 0, world = 0, phase = 0, iw = 0, ih = 0, ow = 0, oh = 0;
    bool               reset = false;
    ALMetalAuroraFrame metadata{};
};
std::vector<Row> sequence(const fs::path& path)
{
    std::ifstream file(path);
    std::string   line;
    require(bool(std::getline(file, line)), "Missing CSV");
    const auto                         names = columns(line);
    std::map<std::string, std::size_t> keys;
    for (std::size_t i = 0; i < names.size(); ++i)
        require(keys.emplace(names[i], i).second, "Duplicate CSV column");
    std::vector<Row> rows;
    while (std::getline(file, line))
    {
        auto fields = columns(line);
        require(fields.size() == names.size(), "Missing CSV fields");
        auto f = [&](const std::string& key)
        {
            return std::stof(fields.at(keys.at(key)));
        };
        Row r;
        r.frame = f("frame");
        r.world = f("world_id");
        r.phase = f("phase");
        r.iw    = f("input_width");
        r.ih    = f("input_height");
        r.ow    = f("output_width");
        r.oh    = f("output_height");
        r.reset = f("reset") != 0;
        require(r.frame == rows.size() && r.world == r.frame + 1 && r.phase == r.frame % 18 && r.reset == (r.frame == 0),
                "Unexpected world, phase or reset sequence");
        require(f("depth_reversed") == 1 && f("pre_exposure") == 1 && f("phase_count") == 18, "Unexpected depth/exposure/phase convention");
        r.metadata.jitter_x = f("jitter_x");
        r.metadata.jitter_y = f("jitter_y");
        r.metadata.reset    = r.reset;
        for (unsigned i = 0; i < 16; ++i)
        {
            r.metadata.projection[i]          = f("projection_" + std::to_string(i));
            r.metadata.motion_reprojection[i] = f("reprojection_" + std::to_string(i));
        }
        rows.push_back(r);
    }
    require(rows.size() == 468 || rows.size() == 414, "Expected complete 468-world main or 414-world mature-reference history");
    return rows;
}
struct Fixture
{
    std::string                                 error;
    std::unique_ptr<ALMetalDevice>              device;
    std::unique_ptr<ALMetalAuroraReconstructor> aurora;
    std::shared_ptr<ALMetalWorldFrameData>      frame_data;
    ALMetalAuroraDescriptor                     d;
    std::unique_ptr<ALMetalTexture>             color, opaque, depth, motion, output;
    Fixture(unsigned iw, unsigned ih, unsigned ow, unsigned oh)
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled);
        require(bool(device), error);
        require(device->info().name.find("M1 Max") != std::string::npos, "M1 Max required for this retained comparison");
        aurora = ALMetalAuroraReconstructor::create(*device, &error);
        require(bool(aurora), error);
        frame_data = ALMetalWorldFrameData::create(*device, &error);
        require(bool(frame_data), error);
        aurora->setFrameData(frame_data);
        d = { iw, ih, ow, oh };
        require(aurora->prepare(d, &error), error);
        color  = texture(iw, ih, ALMetalPixelFormat::rgba16_float);
        opaque = texture(iw, ih, ALMetalPixelFormat::rgba16_float);
        depth  = texture(iw, ih, ALMetalPixelFormat::r32_float);
        motion = texture(iw, ih, ALMetalPixelFormat::rg16_float);
        output = texture(ow, oh, ALMetalPixelFormat::rgba16_float);
    }
    std::unique_ptr<ALMetalTexture> texture(unsigned w, unsigned h, ALMetalPixelFormat format)
    {
        ALMetalTextureDescriptor t;
        t.width  = w;
        t.height = h;
        t.format = format;
        t.usage  = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
        auto p   = ALMetalTexture::create(*device, t, &error);
        require(bool(p), error);
        return p;
    }
    ALMetalAuroraInputs inputs(bool same_opaque = false)
    {
        ALMetalAuroraInputs i;
        i.color  = color.get();
        i.opaque = same_opaque ? color.get() : opaque.get();
        i.depth  = depth.get();
        i.motion = motion.get();
        i.output = output.get();
        return i;
    }
    std::vector<Pixel> pixels(const ALMetalTexture& t)
    {
        std::vector<HalfPixel> half(std::size_t(t.width()) * t.height());
        require(t.readback(*device, 0, 0, {}, half.data(), t.width() * sizeof(HalfPixel), &error), error);
        std::vector<Pixel> out(half.size());
        for (std::size_t p = 0; p < out.size(); ++p)
            for (unsigned c = 0; c < 4; ++c)
                out[p][c] = half[p][c];
        return out;
    }
    void upload(ALMetalCommandBuffer& cmd, const std::vector<HalfPixel>& c, const std::vector<float>& z, const std::vector<Motion>& m,
                const std::vector<HalfPixel>* o = nullptr)
    {
        require(color->upload(cmd, 0, 0, {}, c.data(), d.input_width * 8, &error), error);
        require(depth->upload(cmd, 0, 0, {}, z.data(), d.input_width * 4, &error), error);
        require(motion->upload(cmd, 0, 0, {}, m.data(), d.input_width * 4, &error), error);
        if (o)
            require(opaque->upload(cmd, 0, 0, {}, o->data(), d.input_width * 8, &error), error);
    }
    void submit(ALMetalCommandBuffer& cmd, ALMetalAuroraReconstructor::Ticket ticket)
    {
        require(cmd.commit(&error), error);
        require(aurora->submitted(ticket, &error), error);
        require(cmd.waitUntilCompleted(&error), error);
    }
};

void replay(const fs::path& csv, const std::string& prefix, const fs::path& out, const fs::path& expected)
{
    require(!fs::exists(out), "Preserve earlier evidence; output must be fresh");
    fs::create_directories(out);
    const auto      rows = sequence(csv);
    const auto&     r    = rows.front();
    Fixture         f(r.iw, r.ih, r.ow, r.oh);
    double          sum = 0, max = 0;
    std::uint64_t   count = 0, differences = 0;
    NSMutableArray* comparisons = [NSMutableArray array];
    for (const auto& row : rows)
        @autoreleasepool
        {
            const auto             base = csv.parent_path() / (prefix + idx(row.frame));
            const auto             n    = std::size_t(row.iw) * row.ih;
            const auto             full = read<Pixel>(base.string() + "-input.rgba32f", n);
            std::vector<HalfPixel> color(n);
            for (std::size_t p = 0; p < n; ++p)
                for (unsigned c = 0; c < 4; ++c)
                {
                    color[p][c] = full[p][c];
                    require(float(color[p][c]) == full[p][c], "Retained input is not exact half expansion");
                }
            auto depth    = read<float>(base.string() + "-proxy-depth.r32f", n);
            auto motion   = read<Motion>(base.string() + "-motion.rg16f", n);
            auto producer = f.device->makeCommandBuffer("Aurora replay: untimed retained input producer", &f.error);
            require(bool(producer), f.error);
            f.upload(*producer, color, depth, motion);
            require(producer->commitAndWait(&f.error), f.error);
            auto cmd = f.device->makeCommandBuffer("Aurora replay: actual production reconstruction", &f.error);
            require(bool(cmd), f.error);
            ALMetalAuroraReconstructor::Ticket ticket = 0;
            f.frame_data->beginFrame(row.world);
            require(f.aurora->encode(*cmd, f.d, f.inputs(true), row.metadata, ticket, &f.error), f.error);
            f.submit(*cmd, ticket);
            const auto actual = f.pixels(*f.output);
            const auto raw    = f.pixels(*f.aurora->rawOutput());
            write(out / ("world-" + idx(row.frame) + "-output.rgba32f"), actual);
            write(out / ("world-" + idx(row.frame) + "-raw-output.rgba32f"), raw);
            const auto    ref       = read<Pixel>(expected / ("world-" + idx(row.frame) + "-output.rgba32f"), actual.size());
            double        frame_sum = 0, frame_max = 0;
            std::uint64_t frame_count = 0;
            for (std::size_t p = 0; p < actual.size(); ++p)
                for (unsigned c = 0; c < 3; ++c)
                {
                    require(std::isfinite(raw[p][c]) && std::isfinite(ref[p][c]), "Nonfinite native/reference RGB");
                    const auto e = double(std::abs(raw[p][c] - ref[p][c]));
                    sum += e;
                    frame_sum += e;
                    max       = std::max(max, e);
                    frame_max = std::max(frame_max, e);
                    ++count;
                    ++frame_count;
                    differences += e != 0;
                }
            [comparisons addObject:@{
                @"frame": @(row.frame),
                @"world": @(row.world),
                @"phase": @(row.phase),
                @"rgb_mean_absolute_error": @(frame_sum / frame_count),
                @"rgb_max_error": @(frame_max)
            }];
        }
    // Port parity, separate from the frozen image-quality bounds: source RGB
    // is [0,1], so0.001 bounds two half-float ULPs near1. The much tighter
    // whole-stream MAE prevents a sparse tolerance from hiding a broad drift.
    const bool parity_passed = max <= .001 && sum / count <= .00001;
    json(
        out / "parity.json", @{
            @"complete": @YES,
            @"frames": @(comparisons.count),
            @"components": @(count),
            @"rgb_differences": @(differences),
            @"rgb_max_error": @(max),
            @"rgb_mean_absolute_error": @(sum / count),
            @"parity_passed": @(parity_passed),
            @"rgb_max_bound": @.001,
            @"rgb_mae_bound": @.00001,
            @"frames_detail": comparisons,
            @"comparison": @"Native component immediate RGB versus retained unchanged Aurora WGSL OFF; alpha assessed separately because "
                           @"Alchemy glow resolve is intentional"
        });
    std::cout << "Native replay " << rows.size() << " worlds; RGB MAE=" << sum / count << ", max=" << max << '\n';
    require(parity_passed, "Native port differs materially from the pinned WGSL output");
}

NSDictionary* allocation(const ALMetalDevice& d)
{
    const auto a = alMetalResourceAllocationStats();
    return @{
        @"metal_allocated_bytes": @(d.currentAllocatedSize()),
        @"buffers_created": @(a.buffers_created),
        @"buffers_destroyed": @(a.buffers_destroyed),
        @"buffer_bytes_live": @(a.buffer_bytes_live),
        @"textures_created": @(a.textures_created),
        @"textures_destroyed": @(a.textures_destroyed),
        @"texture_bytes_live": @(a.texture_bytes_live)
    };
}
NSDictionary* ownedMemory(const Fixture& f)
{
    const auto a     = f.aurora->stats();
    const auto pages = f.frame_data->stats();
    return @{
        @"component_texture_payload_bytes": @(a.texture_bytes),
        @"component_buffer_payload_bytes": @(a.buffer_bytes),
        @"opaque_before_transparency_preservation_bytes": @(f.opaque->estimatedAllocationBytes()),
        @"resolved_output_bytes": @(f.output->estimatedAllocationBytes()),
        @"input_color_bytes": @(f.color->estimatedAllocationBytes()),
        @"input_depth_bytes": @(f.depth->estimatedAllocationBytes()),
        @"input_motion_bytes": @(f.motion->estimatedAllocationBytes()),
        @"shared_constant_arena_pages_created": @(pages.pages_created),
        @"shared_constant_arena_pages_reused": @(pages.pages_reused),
        @"note": @"Descriptor-derived component payload and explicit integration surfaces; device physical footprint is separate. Existing "
                 @"live viewer color/depth/motion and shared arena already serve other render stages."
    };
}
void checks(const fs::path& out)
{
    require(!fs::exists(out), "Output must be fresh");
    fs::create_directories(out);
    Fixture                f(64, 32, 96, 48);
    const auto             n = std::size_t(64) * 32;
    std::vector<HalfPixel> color(n), opaque(n);
    std::vector<float>     depth(n);
    std::vector<Motion>    motion(n);
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 64; ++x)
        {
            const auto p  = y * 64 + x;
            const bool fg = x > 10 && x < 48 && y > 5 && y < 26;
            opaque[p]     = { .07f, .14f, .4f, 0.f };
            color[p]      = fg ? HalfPixel{ .2f, .75f, .1f, .6f } : opaque[p];
            depth[p]      = fg ? .05f : 0.f;
        }
    ALMetalAuroraFrame frame;
    frame.projection = { .5f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, .1f, 0 };
    frame.reset      = true;
    auto render      = [&](unsigned world, bool same)
    {
        f.frame_data->beginFrame(world);
        auto c = f.device->makeCommandBuffer("Native Aurora focused integration fixture", &f.error);
        require(bool(c), f.error);
        f.upload(*c, color, depth, motion, &opaque);
        ALMetalAuroraReconstructor::Ticket t = 0;
        require(f.aurora->encode(*c, f.d, f.inputs(same), frame, t, &f.error), f.error);
        f.submit(*c, t);
        frame.reset = false;
    };
    render(1, true);
    const auto static_output = f.pixels(*f.output);
    require(f.aurora->stats().submitted_calls == 1, "History did not advance at submission");
    auto static_mask = f.aurora->generatedReactive();
    require(static_mask, "Missing upstream generated reactive output");
    std::vector<std::uint8_t> mask(64 * 32);
    require(static_mask->readback(*f.device, 0, 0, {}, mask.data(), 64, &f.error), f.error);
    require(*std::max_element(mask.begin(), mask.end()) == 0, "MASK-only identical opaque/final inputs generated reactivity");
    render(2, false);
    std::vector<std::uint8_t> changed_mask(64 * 32);
    require(f.aurora->generatedReactive()->readback(*f.device, 0, 0, {}, changed_mask.data(), 64, &f.error), f.error);
    require(*std::max_element(changed_mask.begin(), changed_mask.end()) > 0, "Real opaque/final difference did not generate reactive mask");
    const auto resolved = f.pixels(*f.output), raw = f.pixels(*f.aurora->rawOutput());
    double     rgb_resolve_max = 0, glow_error_max = 0;
    for (unsigned y = 0; y < 48; ++y)
        for (unsigned x = 0; x < 96; ++x)
        {
            const auto p = y * 96 + x;
            for (unsigned c = 0; c < 3; ++c)
            {
                require(std::isfinite(resolved[p][c]), "Nonfinite focused output");
                rgb_resolve_max = std::max(rgb_resolve_max, double(std::abs(resolved[p][c] - raw[p][c])));
            }
            const unsigned sx = std::min(63u, unsigned((x + .5f) * 64 / 96)), sy = std::min(31u, unsigned((y + .5f) * 32 / 48));
            if (x > 20 && x < 68 && y > 10 && y < 35)
                glow_error_max = std::max(glow_error_max, double(std::abs(resolved[p][3] - float(color[sy * 64 + sx][3]))));
        }
    require(rgb_resolve_max < .001, "Glow resolve altered native RGB");
    require(glow_error_max < .001, "Glow alpha was not retained in foreground interior");
    std::vector<float> display_depth(96 * 48);
    require(f.aurora->displayDepth()->readback(*f.device, 0, 0, {}, display_depth.data(), 96 * 4, &f.error), f.error);
    for (float z : display_depth)
        require(std::isfinite(z) && z >= 0 && z <= 1, "Invalid output depth");
    require(display_depth[24 * 96 + 40] > .04f, "Foreground output depth missing");
    require(display_depth[2 * 96 + 2] == 0.f, "Clear sky output depth changed");
    for (unsigned world = 3; world <= 10; ++world)
        render(world, true);
    const auto warm = allocation(*f.device);
    for (unsigned world = 11; world <= 30; ++world)
        render(world, true);
    const auto end = allocation(*f.device);
    require([warm[@"textures_created"] unsignedLongLongValue] == [end[@"textures_created"] unsignedLongLongValue],
            "Fixed descriptor reconstruction keeps allocating textures after warm-up");
    require([warm[@"buffers_created"] unsignedLongLongValue] == [end[@"buffers_created"] unsignedLongLongValue],
            "Constant arena keeps allocating buffers after warm-up");
    const auto before        = f.aurora->stats();
    const auto output_before = f.pixels(*f.output);
    const auto committed     = ALMetalDevice::committedCommandBufferCount();
    {
        f.frame_data->beginFrame(31);
        auto c = f.device->makeCommandBuffer("Intentionally abandoned Aurora world", &f.error);
        require(bool(c), f.error);
        ALMetalAuroraReconstructor::Ticket t = 0;
        require(f.aurora->encode(*c, f.d, f.inputs(true), frame, t, &f.error), f.error);
        f.aurora->abandon(t);
        f.aurora->abandon(t);
    }
    require(ALMetalDevice::committedCommandBufferCount() == committed, "Abandoned reconstruction submitted GPU work");
    const auto abandoned = f.aurora->stats();
    require(abandoned.parity == before.parity && abandoned.submitted_calls == before.submitted_calls,
            "Abandon advanced authoritative parity/history");
    require(!abandoned.history_valid, "Abandon did not invalidate interrupted history");
    require(f.pixels(*f.output) == output_before, "Abandon altered previous valid output");
    render(32, true);
    require(f.aurora->stats().reset_calls == before.reset_calls + 1, "Recovery did not reset interrupted history");
    const auto valid   = f.aurora->stats();
    auto       invalid = f.inputs(true);
    invalid.output     = nullptr;
    {
        auto                               c = f.device->makeCommandBuffer("Rejected Aurora input", &f.error);
        ALMetalAuroraReconstructor::Ticket t = 0;
        require(!f.aurora->encode(*c, f.d, invalid, frame, t, &f.error), "Null output accepted");
        require(t == 0, "Failed preflight minted a transaction");
    }
    require(f.aurora->stats().submitted_calls == valid.submitted_calls, "Failed preflight advanced history");
    std::vector<std::unique_ptr<ALMetalCommandBuffer>> outstanding;
    for (unsigned world = 33; world < 36; ++world)
    {
        f.frame_data->beginFrame(world);
        auto c = f.device->makeCommandBuffer("Three asynchronously submitted Aurora worlds", &f.error);
        require(bool(c), f.error);
        ALMetalAuroraReconstructor::Ticket t = 0;
        require(f.aurora->encode(*c, f.d, f.inputs(true), frame, t, &f.error), f.error);
        require(c->commit(&f.error), f.error);
        require(f.aurora->submitted(t, &f.error), f.error);
        outstanding.push_back(std::move(c));
    }
    for (auto& c : outstanding)
        require(c->waitUntilCompleted(&f.error), f.error);
    require(f.aurora->stats().submitted_calls == valid.submitted_calls + 3, "Outstanding worlds failed to preserve ordered history");
    const auto reuse = f.aurora->stats();
    require(reuse.prepare_reuses >= 1, "Prepare reuse not observed");
    auto new_d        = f.d;
    new_d.input_width = 60;
    require(f.aurora->prepare(new_d, &f.error), f.error);
    require(!f.aurora->stats().history_valid, "Resize did not invalidate history");
    require(f.aurora->prepare(f.d, &f.error), f.error);
    frame.reset = false;
    render(36, true);
    require(f.aurora->stats().reset_calls > reuse.reset_calls, "Resize recovery did not reset");
    frame.jitter_x = .25f;
    frame.jitter_y = -1.f / 6.f;
    render(37, true);
    const auto jittered        = f.pixels(*f.output);
    double     glow_edge_error = 0;
    for (unsigned y = 0; y < 48; ++y)
        for (unsigned x = 0; x < 96; ++x)
        {
            const float sx = (x + .5f) * 64.f / 96.f - .5f + frame.jitter_x, sy = (y + .5f) * 32.f / 48.f - .5f + frame.jitter_y;
            const int   ix = int(std::floor(sx)), iy = int(std::floor(sy));
            const float tx = sx - ix, ty = sy - iy;
            const auto  a = [&](int px, int py)
            {
                return float(color[std::clamp(py, 0, 31) * 64 + std::clamp(px, 0, 63)][3]);
            };
            const float expected =
                (a(ix, iy) * (1 - tx) + a(ix + 1, iy) * tx) * (1 - ty) + (a(ix, iy + 1) * (1 - tx) + a(ix + 1, iy + 1) * tx) * ty;
            glow_edge_error = std::max(glow_edge_error, double(std::abs(jittered[y * 96 + x][3] - expected)));
        }
    require(glow_edge_error < .002, "Jitter-adjusted glow alpha disagrees with independent bilinear edge oracle");
    json(
        out / "checks.json", @{
            @"complete": @YES,
            @"cases": @[
                @"MASK identical opaque/final yields zero generated mask", @"genuine blended contribution generates upstream mask",
                @"glow resolve preserves RGB and foreground glow alpha", @"nonzero jitter glow edges match independent bilinear oracle",
                @"output depth keeps finite foreground and clear sky", @"fixed configuration texture+constant allocation plateau",
                @"abandon idempotent and does not submit or advance parity", @"previous valid output retained after abandonment",
                @"recovery resets", @"invalid required input rejects before transaction",
                @"three worlds submitted before completion preserve history", @"resize and return invalidate and reset"
            ],
            @"after_warmup": warm,
            @"after_steady_state": end,
            @"rgb_resolve_max_error": @(rgb_resolve_max),
            @"glow_interior_max_error": @(glow_error_max),
            @"glow_jittered_edge_max_error": @(glow_edge_error),
            @"glow_jittered_edge_bound": @.002,
            @"reactive_peak": @(*std::max_element(changed_mask.begin(), changed_mask.end()))
        });
}
void benchmark(const fs::path& out, unsigned warm, unsigned samples, bool validation)
{
    require(!fs::exists(out), "Output must be fresh");
    fs::create_directories(out);
    require(validation || (warm >= 360 && samples >= 300), "Timed observations require360warm and300samples");
    if (!validation)
        for (const char* name : { "MTL_DEBUG_LAYER", "MTL_SHADER_VALIDATION", "MTL_CAPTURE_ENABLED", "DYLD_INSERT_LIBRARIES" })
        {
            const char* v = std::getenv(name);
            require(!v || std::string(v) == "0", std::string("Unset instrumentation for timed run: ") + name);
        }
    Fixture f(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, AE_OUTPUT_WIDTH, AE_OUTPUT_HEIGHT);
    struct Phase
    { std::unique_ptr<ALMetalTexture> c, z, m; };
    std::array<Phase, 18>  phases;
    const auto             n = std::size_t(AE_INPUT_WIDTH) * AE_INPUT_HEIGHT;
    std::vector<HalfPixel> color(n);
    std::vector<float>     depth(n);
    std::vector<Motion>    motion(n);
    NSMutableArray*        fingerprints = [NSMutableArray array];
    for (unsigned p = 0; p < 18; ++p)
    {
        require(ae_workload_fill(p, reinterpret_cast<std::uint16_t*>(color.data()), depth.data(),
                                 reinterpret_cast<std::uint16_t*>(motion.data())) == 1,
                "Workload producer failed");
        auto hash = ae_workload_fingerprint(color.data(), color.size() * sizeof(HalfPixel), 14695981039346656037ull);
        hash      = ae_workload_fingerprint(depth.data(), depth.size() * 4, hash);
        hash      = ae_workload_fingerprint(motion.data(), motion.size() * 4, hash);
        char text[17];
        snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
        [fingerprints addObject:[NSString stringWithUTF8String:text]];
        phases[p] = { f.texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, ALMetalPixelFormat::rgba16_float),
                      f.texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, ALMetalPixelFormat::r32_float),
                      f.texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, ALMetalPixelFormat::rg16_float) };
        auto cmd  = f.device->makeCommandBuffer("One-time resident benchmark input preparation", &f.error);
        require(bool(cmd), f.error);
        require(phases[p].c->upload(*cmd, 0, 0, {}, color.data(), AE_INPUT_WIDTH * 8, &f.error), f.error);
        require(phases[p].z->upload(*cmd, 0, 0, {}, depth.data(), AE_INPUT_WIDTH * 4, &f.error), f.error);
        require(phases[p].m->upload(*cmd, 0, 0, {}, motion.data(), AE_INPUT_WIDTH * 4, &f.error), f.error);
        require(cmd->commitAndWait(&f.error), f.error);
    }
    NSDictionary*   prepared = allocation(*f.device);
    const auto thermal_before = [NSProcessInfo processInfo].thermalState;
    const auto started_at = [NSDate date].timeIntervalSince1970;
    NSDictionary*   warmed   = nil;
    NSMutableArray* data     = [NSMutableArray array];
    for (unsigned world = 0; world < warm + samples; ++world)
        @autoreleasepool
        {
            const auto phase    = world % 18;
            auto       producer = f.device->makeCommandBuffer("Untimed GPU-resident world inputs", &f.error);
            require(bool(producer), f.error);
            require(f.color->copyFrom(*producer, *phases[phase].c, {}, &f.error), f.error);
            require(f.depth->copyFrom(*producer, *phases[phase].z, {}, &f.error), f.error);
            require(f.motion->copyFrom(*producer, *phases[phase].m, {}, &f.error), f.error);
            require(producer->commitAndWait(&f.error), f.error);
            ALMetalAuroraFrame frame;
            float              jitter[2];
            ae_workload_jitter(phase, jitter);
            frame.jitter_x = jitter[0];
            frame.jitter_y = jitter[1];
            frame.reset    = world == 0;
            ae_workload_projection(frame.projection.data());
            auto cmd = f.device->makeCommandBuffer("Native Aurora complete reconstruction including adapters and glow", &f.error);
            require(bool(cmd), f.error);
            ALMetalAuroraReconstructor::Ticket ticket = 0;
            const auto                         begin  = Clock::now();
            f.frame_data->beginFrame(world + 1);
            // The live integration preserves opaque-before-transparency color.
            // Include that copy even though this static MASK cost workload has
            // identical opaque/final RGB; it is required work in a live scene.
            require(f.opaque->copyFrom(*cmd, *f.color, {}, &f.error), f.error);
            require(f.aurora->encode(*cmd, f.d, f.inputs(false), frame, ticket, &f.error), f.error);
            const auto encoded = Clock::now();
            require(cmd->commit(&f.error), f.error);
            require(f.aurora->submitted(ticket, &f.error), f.error);
            const auto submitted = Clock::now();
            require(cmd->waitUntilCompleted(&f.error), f.error);
            const auto            completed = Clock::now();
            ALMetalCommandGPUTime time;
            require(cmd->completionToken()->gpuTime(time) == ALMetalCommandTimingStatus::ready, "GPU time unavailable");
            if (world + 1 == warm)
                warmed = allocation(*f.device);
            if (world >= warm)
                [data addObject:@{
                    @"sample": @(world - warm),
                    @"world": @(world),
                    @"phase": @(phase),
                    @"gpu_start_seconds": @(time.start_seconds),
                    @"gpu_end_seconds": @(time.end_seconds),
                    @"total_gpu_ms": @(time.elapsed_nanoseconds / 1.e6),
                    @"cpu_encode_ms": @(ms(begin, encoded)),
                    @"cpu_submit_ms": @(ms(encoded, submitted)),
                    @"excluded_completion_wait_ms": @(ms(submitted, completed))
                }];
        }
    json(
        out / "cost.json", @{
            @"schema_version": @1,
            @"scope":
                @"Native production component; complete command GPU envelope, opaque HDR preservation copy+adapters+history clears+all "
                @"upstream passes+display depth+Alchemy glow resolve. No upload, input producer, file IO/readback or deliberate CPU "
                @"completion waits inside GPU time. Quiescent isolated benchmark only; viewer execution remains asynchronous.",
            @"input": @[@(AE_INPUT_WIDTH), @(AE_INPUT_HEIGHT)],
            @"output": @[@(AE_OUTPUT_WIDTH), @(AE_OUTPUT_HEIGHT)],
            @"warmup_worlds": @(warm),
            @"samples": data,
            @"input_fingerprints": fingerprints,
            @"prepared": prepared,
            @"after_warmup": warmed ?: @{},
            @"after_measurement": allocation(*f.device),
            @"owned_memory": ownedMemory(f),
            @"device": [NSString stringWithUTF8String:f.device->info().name.c_str()],
            @"registry_id": @(f.device->info().registry_id),
            @"os": [NSProcessInfo processInfo].operatingSystemVersionString,
            @"thermal_state_before": @(thermal_before),
            @"thermal_state_after": @([NSProcessInfo processInfo].thermalState),
            @"wall_start_unix_seconds": @(started_at),
            @"wall_end_unix_seconds": @([NSDate date].timeIntervalSince1970),
            @"validation": @(validation)
        });
}
} // namespace
int main(int argc, char** argv)
{
    @autoreleasepool
    {
        try
        {
            require(argc >= 2, "Usage: aurora-native-validation replay CSV PREFIX OUTPUT EXPECTED | checks OUTPUT | benchmark OUTPUT WARM "
                               "SAMPLES [validation]");
            const std::string mode = argv[1];
            if (mode == "replay")
            {
                require(argc == 6, "replay requires CSV PREFIX OUTPUT EXPECTED");
                replay(argv[2], argv[3], argv[4], argv[5]);
            }
            else if (mode == "benchmark")
            {
                require(argc == 5 || argc == 6, "benchmark requires OUTPUT WARM SAMPLES [validation]");
                benchmark(argv[2], std::stoul(argv[3]), std::stoul(argv[4]), argc == 6);
            }
            else if (mode == "checks")
            {
                require(argc == 3, "checks requires OUTPUT");
                const auto before = alMetalResourceAllocationStats();
                checks(argv[2]);
                const auto after = alMetalResourceAllocationStats();
                require(after.buffers_created - after.buffers_destroyed == before.buffers_created - before.buffers_destroyed,
                        "Focused fixture retained buffer wrappers after teardown");
                require(after.textures_created - after.textures_destroyed == before.textures_created - before.textures_destroyed,
                        "Focused fixture retained texture wrappers after teardown");
                require(after.buffer_bytes_live == before.buffer_bytes_live && after.texture_bytes_live == before.texture_bytes_live,
                        "Focused fixture retained owned GPU payload after teardown");
                json(
                    fs::path(argv[2]) / "teardown.json", @{
                        @"passed": @YES,
                        @"buffer_bytes_live": @(after.buffer_bytes_live),
                        @"texture_bytes_live": @(after.texture_bytes_live)
                    });
            }
            else
                throw std::runtime_error("Unknown mode " + mode);
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Native Aurora qualification failed: " << e.what() << '\n';
            return 1;
        }
    }
}
