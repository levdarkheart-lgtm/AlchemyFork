/**
 * @file almetal_scene_monitor_test.mm
 * @brief Physical GPU oracles for native scene-loading differences and leases.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetalscenemonitor.h"
#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

namespace
{
using Pixel = std::array<std::uint8_t, 4>;
using Backing = ALMetalRetainedBackground;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalSceneMonitor> monitor;
    std::shared_ptr<ALMetalRenderTargetPool> pool;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        pool = ALMetalRenderTargetPool::create(*device); REQUIRE(pool);
        monitor = ALMetalSceneMonitor::create(*device, &error); INFO(error); REQUIRE(monitor);
    }
    Backing::Context context(std::uint32_t width, std::uint32_t height, std::uint64_t generation = 1)
    { return {device.get(), this, generation, width, height, false}; }
    ALMetalRenderTargetDescriptor descriptor(const Backing::Context& c)
    {
        ALMetalRenderTargetDescriptor d;
        d.width = c.width; d.height = c.height;
        d.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_shader_read = true;
        d.label = "Scene monitor authored display bytes and depth";
        return d;
    }
    Backing::Lease frame(const Backing::Context& c, const std::vector<Pixel>& rgba, std::uint64_t index)
    {
        REQUIRE(rgba.size() == static_cast<size_t>(c.width) * c.height);
        std::shared_ptr<ALMetalRenderTarget> target = pool->acquire(descriptor(c), &error);
        REQUIRE(target);
        std::vector<Pixel> bgra(rgba);
        for (auto& pixel : bgra) std::swap(pixel[0], pixel[2]);
        auto command = device->makeCommandBuffer("Scene monitor authored input", &error); REQUIRE(command);
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(
            *command, 0, 0, {}, bgra.data(), c.width * 4, &error));
        REQUIRE(command->commitAndWait(&error));
        Backing backing;
        REQUIRE(backing.publish(c, index, target, target, &error));
        auto lease = backing.find(c); REQUIRE(lease);
        return lease;
    }
    ALMetalSceneDifferenceConfig config(const Backing::Context& c)
    { return {c.width, c.height, c.width, c.height, 0.5f, 0.1f}; }
    void ready(const ALMetalSceneDifference& run, std::uint64_t count, float expected)
    {
        REQUIRE(device->waitUntilIdle(&error));
        REQUIRE(run.status() == ALMetalSceneDifference::Status::ready);
        std::uint64_t actual = ~std::uint64_t(0); float difference = -1.f;
        REQUIRE(run.result(actual, difference, &error)); INFO(error);
        CHECK(actual == count);
        CHECK(std::abs(difference - expected) < 1.e-6f);
    }
    std::vector<Pixel> pixels(const ALMetalTexture* texture)
    {
        REQUIRE(texture);
        CHECK(texture->descriptor().format == ALMetalPixelFormat::rgba8_unorm);
        std::vector<Pixel> result(static_cast<size_t>(texture->width()) * texture->height());
        REQUIRE(texture->readback(*device, 0, 0, {}, result.data(), texture->width() * 4, &error));
        return result;
    }
};
}

TEST_CASE("Native scene monitor preserves raw Bayer channels and RGB8 alpha semantics", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(4, 4);
    auto previous = f.frame(context, std::vector<Pixel>(16, {200, 100, 5, 255}), 1);
    auto current = f.frame(context, std::vector<Pixel>(16, {73, 165, 255, 0}), 2);
    auto config = f.config(context); config.sample_ratio = 1.f; config.tolerance = 1.f;
    auto run = f.monitor->submit(previous, current, config, &f.error); INFO(f.error); REQUIRE(run);
    f.ready(*run, 8, 0.5f);
    // Authored original 4x4 thresholds: R127 passes eight cells, G65 four,
    // B250 all sixteen. No transfer decode, and input alpha never contributes.
    const std::array<Pixel, 16> expected{{
        {127,65,250,0}, {0,0,250,0}, {127,65,250,0}, {0,0,250,0},
        {0,0,250,0}, {127,0,250,0}, {0,0,250,0}, {127,0,250,0},
        {127,65,250,0}, {0,0,250,0}, {127,65,250,0}, {0,0,250,0},
        {0,0,250,0}, {127,0,250,0}, {0,0,250,0}, {127,0,250,0}}};
    const auto diff = f.pixels(run->diffTexture());
    const auto filtered = f.pixels(run->filteredTexture());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        CAPTURE(i); CHECK(diff[i] == expected[i]);
        const Pixel visible = expected[i][0] ? Pixel{expected[i][0], expected[i][1], 250, 242} : Pixel{0,0,0,0};
        CHECK(filtered[i] == visible);
    }
}

TEST_CASE("Native scene monitor Bayer cutoff includes equality and quantizes the difference", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(4, 4);
    auto previous = f.frame(context, std::vector<Pixel>(16, {0, 0, 0, 255}), 1);
    const std::array<std::uint8_t,16> cutoff{15,135,45,165,195,75,225,105,60,180,30,150,240,120,210,90};
    for (const int offset : {-1, 0, 1})
    {
        CAPTURE(offset);
        std::vector<Pixel> input;
        for (auto value : cutoff) input.push_back({static_cast<std::uint8_t>(value + offset), 0, 0, 255});
        auto current = f.frame(context, input, 2);
        auto config = f.config(context); config.sample_ratio = 1.f; config.tolerance = 0.001f;
        auto run = f.monitor->submit(previous, current, config, &f.error); INFO(f.error); REQUIRE(run);
        f.ready(*run, offset < 0 ? 0 : 16, offset < 0 ? 0.f : std::sqrt(0.5f));
        const auto actual = f.pixels(run->diffTexture());
        for (size_t i = 0; i < input.size(); ++i)
        { CAPTURE(i); CHECK(actual[i] == (offset < 0 ? Pixel{0,0,0,0} : Pixel{input[i][0],0,0,0})); }
    }
}

TEST_CASE("Native scene monitor filters before counting and keeps strict tolerance equality", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(8, 8);
    std::vector<Pixel> black(64, {0,0,0,255}), one(black); one[0] = {255,255,255,255};
    auto previous = f.frame(context, black, 1), current = f.frame(context, one, 2);
    for (const float tolerance : {0.7f, 0.8f})
    {
        auto config = f.config(context); config.tolerance = tolerance;
        auto run = f.monitor->submit(previous, current, config, &f.error); REQUIRE(run);
        const bool visible = tolerance < 0.75f;
        f.ready(*run, visible ? 1 : 0, visible ? std::sqrt(0.5f / 16.f) : 0.f);
        const auto actual = f.pixels(run->filteredTexture());
        REQUIRE(actual.size() == 16);
        CHECK(actual[0] == (visible ? Pixel{64,64,64,242} : Pixel{0,0,0,0}));
        for (size_t i = 1; i < actual.size(); ++i) CHECK(actual[i] == Pixel{0,0,0,0});
    }
    // Original discard is sum<tolerance: a supported all-zero result at zero
    // tolerance counts every sample, whereas positive tolerance counts none.
    for (const float tolerance : {-1.f, 0.f, 0.001f, 4.f})
    {
        auto config = f.config(context); config.tolerance = tolerance;
        auto run = f.monitor->submit(previous, previous, config, &f.error); REQUIRE(run);
        f.ready(*run, tolerance <= 0.f ? 16 : 0, tolerance <= 0.f ? std::sqrt(0.5f) : 0.f);
    }
}

TEST_CASE("Native scene monitor crops window bytes before PointClamp comparison", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(8, 8);
    std::vector<Pixel> input(64, {0,0,0,255}); input[0] = input[63] = {255,255,255,255};
    auto previous = f.frame(context, std::vector<Pixel>(64, {0,0,0,255}), 1);
    auto current = f.frame(context, input, 2);
    auto config = f.config(context); config.capture_width = config.capture_height = 4;
    auto run = f.monitor->submit(previous, current, config, &f.error); REQUIRE(run);
    f.ready(*run, 1, std::sqrt(0.5f / 16.f));
    const auto actual = f.pixels(run->diffTexture());
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x)
    {
        CAPTURE(x,y);
        CHECK(actual[y*8+x] == (x < 2 && y < 2 ? Pixel{255,255,255,0} : Pixel{0,0,0,0}));
    }
}

TEST_CASE("Native scene monitor keeps odd sampled extents and original normalization", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(5, 3);
    auto previous = f.frame(context, std::vector<Pixel>(15, {0,0,0,255}), 1);
    auto current = f.frame(context, std::vector<Pixel>(15, {255,255,255,255}), 2);
    auto run = f.monitor->submit(previous, current, f.config(context), &f.error); REQUIRE(run);
    // Original S32(size*ratio) raster extent is2x1, while the denominator is
    // still5*3*0.5*0.5. Do not silently normalize by the rounded output area.
    f.ready(*run, 2, std::sqrt(1.f / 3.75f));
    CHECK(run->filteredTexture()->width() == 2);
    CHECK(run->filteredTexture()->height() == 1);
}

TEST_CASE("Native scene monitor rejects unavailable inputs without fabricating a result", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(4, 4);
    auto source = f.frame(context, std::vector<Pixel>(16, {0,0,0,255}), 1);
    auto config = f.config(context);
    CHECK_FALSE(f.monitor->submit({}, source, config, &f.error)); CHECK_FALSE(f.error.empty());
    auto other_context = f.context(4,4,2);
    auto other = f.frame(other_context, std::vector<Pixel>(16, {0,0,0,255}), 2);
    CHECK_FALSE(f.monitor->submit(source, other, config, &f.error));
    auto clear = Backing::makeCleared(context, &f.error); REQUIRE(clear);
    CHECK_FALSE(f.monitor->submit(source, clear, config, &f.error));
    for (const float ratio : {0.f, -1.f, 2.f, std::numeric_limits<float>::quiet_NaN()})
    { auto invalid = config; invalid.sample_ratio = ratio; CHECK_FALSE(f.monitor->submit(source, source, invalid, &f.error)); }
    for (const float tolerance : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    { auto invalid = config; invalid.tolerance = tolerance; CHECK_FALSE(f.monitor->submit(source, source, invalid, &f.error)); }
    auto invalid = config; invalid.capture_width = 5;
    CHECK_FALSE(f.monitor->submit(source, source, invalid, &f.error));
    invalid = config; invalid.diff_width = 0;
    CHECK_FALSE(f.monitor->submit(source, source, invalid, &f.error));
    invalid = config; invalid.diff_width = 1;
    CHECK_FALSE(f.monitor->submit(source, source, invalid, &f.error));
}

TEST_CASE("Native scene monitor reset pins actual pool leases until GPU completion", "[almetal][native-scene-monitor]")
{
    Fixture f; const auto context = f.context(8, 8);
    auto previous = f.frame(context, std::vector<Pixel>(64, {0,0,0,255}), 1);
    auto current = f.frame(context, std::vector<Pixel>(64, {255,255,255,255}), 2);
    const auto first = previous->colorTexture()->nativeHandle(), second = current->colorTexture()->nativeHandle();
    std::weak_ptr<const Backing::Record> weak_previous = previous, weak_current = current;
    auto blocker = f.device->makeCommandBuffer("Hold scene comparison for reset test", &f.error); REQUIRE(blocker);
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
    id<MTLSharedEvent> gate = [native.device newSharedEvent]; REQUIRE(gate);
    struct Signal
    {
        id<MTLSharedEvent> event;
        const ALMetalDevice* device;
        ~Signal() { event.signaledValue = 1; device->waitUntilIdle(); }
    } signal{gate, f.device.get()};
    [native encodeWaitForEvent:gate value:1]; REQUIRE(blocker->commit(&f.error));
    auto run = f.monitor->submit(previous, current, f.config(context), &f.error); REQUIRE(run);
    REQUIRE(run->status() == ALMetalSceneDifference::Status::pending);
    std::uint64_t count = 987; float value = 123.f;
    CHECK_FALSE(run->result(count, value, &f.error)); CHECK(count == 987); CHECK(value == 123.f);
    auto retained_output = ALMetalTexture::retainSharedBacking(*run->diffTexture(), &f.error); REQUIRE(retained_output);
    previous.reset(); current.reset(); run.reset(); f.monitor.reset();
    CHECK_FALSE(weak_previous.expired()); CHECK_FALSE(weak_current.expired());
    auto growth_a = f.pool->acquire(f.descriptor(context), &f.error);
    auto growth_b = f.pool->acquire(f.descriptor(context), &f.error);
    REQUIRE(growth_a); REQUIRE(growth_b);
    for (const auto* target : {growth_a.get(), growth_b.get()})
    { CHECK(target->colorTexture(0)->nativeHandle() != first); CHECK(target->colorTexture(0)->nativeHandle() != second); }
    gate.signaledValue = 1; REQUIRE(f.device->waitUntilIdle(&f.error));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!weak_previous.expired() || !weak_current.expired() || f.pool->cachedTargetCount() < 2) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(weak_previous.expired()); CHECK(weak_current.expired());
    REQUIRE(f.pool->cachedTargetCount() >= 2);
    auto reused = f.pool->acquire(f.descriptor(context), &f.error); REQUIRE(reused);
    CHECK((reused->colorTexture(0)->nativeHandle() == first || reused->colorTexture(0)->nativeHandle() == second));
    // The ticket and shader owner are gone, yet the actual queued comparison
    // still read both original immutable native generations before recycling.
    const auto pixels = f.pixels(retained_output.get());
    for (const auto& pixel : pixels) CHECK(pixel == Pixel{255,255,255,0});
}
