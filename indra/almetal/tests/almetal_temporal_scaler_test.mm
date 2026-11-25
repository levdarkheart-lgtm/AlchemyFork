/**
 * @file almetal_temporal_scaler_test.mm
 * @brief Actual classic MetalFX history, native parity and transaction tests.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetaltemporalscaler.h"
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
using Pixel = std::array<float, 4>;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalTemporalScaler> scaler;
    ALMetalTemporalScalerDescriptor desc;
    ALMetalTemporalScalerRequirements req;
    std::unique_ptr<ALMetalTexture> color, depth, motion, reactive, output;
    Fixture()
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled);
        INFO(error); REQUIRE(device);
        const auto support = ALMetalTemporalScaler::support(*device);
        REQUIRE(support.supported); REQUIRE(support.reactive_mask);
        std::cout << "Temporal supported upscale factor " << support.minimum_upscale_factor
                  << ".." << support.maximum_upscale_factor << ", reactive mask=" << support.reactive_mask << '\n';
        scaler = ALMetalTemporalScaler::create(*device, &error);
        REQUIRE(scaler); INFO(error);
        desc.input_width = 64; desc.input_height = 48;
        desc.output_width = 128; desc.output_height = 96;
        REQUIRE(scaler->prepare(desc, req, &error)); INFO(error);
        color = texture(desc.color_format, req.color_usage);
        depth = texture(desc.depth_format, req.depth_usage);
        motion = texture(desc.motion_format, req.motion_usage);
        reactive = texture(ALMetalPixelFormat::r8_unorm, req.reactive_usage);
        output = texture(desc.output_format, req.output_usage, true);
    }
    std::unique_ptr<ALMetalTexture> texture(ALMetalPixelFormat format,
        ALMetalTextureUsage usage, bool is_output = false)
    {
        ALMetalTextureDescriptor td;
        td.width = is_output ? desc.output_width : desc.input_width;
        td.height = is_output ? desc.output_height : desc.input_height;
        td.format = format; td.usage = usage | ALMetalTextureUsage::shader_read;
        auto result = ALMetalTexture::create(*device, td, &error);
        REQUIRE(result); INFO(error); return result;
    }
    ALMetalTemporalScalerInputs inputs(const ALMetalTexture* target = nullptr)
    { return {color.get(), depth.get(), motion.get(), reactive.get(), target ? target : output.get()}; }
    void fill(ALMetalCommandBuffer& command, Pixel value = {.2f, .4f, .7f, 1.f},
              float jitter_x = 0.f, float jitter_y = 0.f, bool pattern = false,
              unsigned reactive_value = 0)
    {
        const auto count = std::size_t(desc.input_width) * desc.input_height;
        std::vector<__fp16> pixels(count * 4), vectors(count * 2, 0);
        std::vector<float> depths(count, .75f);
        std::vector<std::uint8_t> masks(count, reactive_value);
        for (unsigned y = 0; y < desc.input_height; ++y)
            for (unsigned x = 0; x < desc.input_width; ++x)
                for (unsigned channel = 0; channel < 4; ++channel)
                {
                    const float p = pattern && channel < 3 ? .45f + .24f *
                        std::sin((float(x) + .5f - jitter_x) * (1.11f + .13f * channel)) *
                        std::cos((float(y) + .5f - jitter_y) * .83f) : value[channel];
                    pixels[(std::size_t(y) * desc.input_width + x) * 4 + channel] = p;
                }
        REQUIRE(color->upload(command, 0, 0, {}, pixels.data(), desc.input_width * 8, &error));
        REQUIRE(depth->upload(command, 0, 0, {}, depths.data(), desc.input_width * 4, &error));
        REQUIRE(motion->upload(command, 0, 0, {}, vectors.data(), desc.input_width * 4, &error));
        REQUIRE(reactive->upload(command, 0, 0, {}, masks.data(), desc.input_width, &error));
        INFO(error);
    }
    std::vector<Pixel> read(const ALMetalTexture& image)
    {
        std::vector<__fp16> raw(std::size_t(image.width()) * image.height() * 4);
        const bool ready = image.readback(*device, 0, 0, {}, raw.data(), image.width() * 8, &error);
        INFO(error); REQUIRE(ready);
        std::vector<Pixel> result(raw.size() / 4);
        for (std::size_t i = 0; i < raw.size(); ++i) result[i / 4][i % 4] = raw[i];
        return result;
    }
    void checkFinite(const ALMetalTexture& image)
    {
        for (const auto& p : read(image)) for (unsigned c = 0; c < 3; ++c)
        { REQUIRE(std::isfinite(p[c])); REQUIRE(p[c] > -.1f); REQUIRE(p[c] < 2.f); }
    }
};
}

TEST_CASE("Temporal preflight reuses one scaler and validates this device's scales", "[metal][temporal-scaler]")
{
    Fixture f;
    REQUIRE(f.scaler->stats().creation_attempts == 1);
    for (unsigned i = 0; i < 12; ++i) REQUIRE(f.scaler->prepare(f.desc, f.req, &f.error));
    REQUIRE(f.scaler->stats().creation_attempts == 1);
    REQUIRE(f.scaler->stats().prepare_reuses == 12);
    auto invalid = f.desc; invalid.input_width = 0;
    REQUIRE_FALSE(f.scaler->prepare(invalid, f.req, &f.error));
    REQUIRE(f.scaler->lastFailure() == ALMetalTemporalScalerFailure::invalid_descriptor);
    invalid = f.desc; invalid.color_format = ALMetalPixelFormat::rgba8_unorm_srgb;
    REQUIRE_FALSE(f.scaler->prepare(invalid, f.req, &f.error));
    REQUIRE(f.scaler->lastFailure() == ALMetalTemporalScalerFailure::unsupported_format);
    invalid = f.desc; invalid.input_width = 1; invalid.input_height = 1;
    REQUIRE_FALSE(f.scaler->prepare(invalid, f.req, &f.error));
    REQUIRE(f.scaler->stats().creation_attempts == 1);
    REQUIRE_FALSE(f.scaler->stats().history_valid);
}

TEST_CASE("Temporal wrapper matches independent native linear HDR encoding", "[metal][temporal-scaler]")
{
    Fixture f;
    auto control_output = f.texture(f.desc.output_format, f.req.output_usage, true);
    auto native_desc = [MTLFXTemporalScalerDescriptor new];
    native_desc.inputWidth = f.desc.input_width; native_desc.inputHeight = f.desc.input_height;
    native_desc.outputWidth = f.desc.output_width; native_desc.outputHeight = f.desc.output_height;
    native_desc.colorTextureFormat = MTLPixelFormatRGBA16Float;
    native_desc.depthTextureFormat = MTLPixelFormatR32Float;
    native_desc.motionTextureFormat = MTLPixelFormatRG16Float;
    native_desc.outputTextureFormat = MTLPixelFormatRGBA16Float;
    native_desc.autoExposureEnabled = NO;
    native_desc.requiresSynchronousInitialization = YES;
    native_desc.reactiveMaskTextureEnabled = YES;
    native_desc.reactiveMaskTextureFormat = MTLPixelFormatR8Unorm;
    auto native_device = (__bridge id<MTLDevice>)f.device->nativeHandle();
    auto control = [native_desc newTemporalScalerWithDevice:native_device];
    REQUIRE(control != nil);
    auto exposure_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Float width:1 height:1 mipmapped:NO];
    exposure_desc.storageMode = MTLStorageModeShared;
    exposure_desc.usage = MTLTextureUsageShaderRead;
    auto exposure = [native_device newTextureWithDescriptor:exposure_desc];
    const __fp16 one = 1.f;
    [exposure replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&one bytesPerRow:sizeof(one)];
    for (unsigned frame_index = 0; frame_index < 4; ++frame_index)
    {
        auto command = f.device->makeCommandBuffer("Temporal wrapper/native parity", &f.error);
        REQUIRE(command);
        const float jitter_x = frame_index % 2 ? .25f : -.25f;
        const float jitter_y = float(frame_index % 3) / 3.f - .3333f;
        f.fill(*command, {}, jitter_x, jitter_y, true);
        ALMetalTemporalScalerFrame frame; frame.jitter_x = jitter_x; frame.jitter_y = jitter_y;
        ALMetalTemporalScaler::Ticket ticket = 0;
        const auto committed = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(f.scaler->encode(*command, f.desc, f.inputs(), frame, ticket, &f.error)); INFO(f.error);
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == committed);
        control.colorTexture = (__bridge id<MTLTexture>)f.color->nativeHandle();
        control.depthTexture = (__bridge id<MTLTexture>)f.depth->nativeHandle();
        control.motionTexture = (__bridge id<MTLTexture>)f.motion->nativeHandle();
        control.reactiveMaskTexture = (__bridge id<MTLTexture>)f.reactive->nativeHandle();
        control.outputTexture = (__bridge id<MTLTexture>)control_output->nativeHandle();
        control.exposureTexture = exposure;
        control.inputContentWidth = f.desc.input_width; control.inputContentHeight = f.desc.input_height;
        control.jitterOffsetX = jitter_x; control.jitterOffsetY = jitter_y;
        control.motionVectorScaleX = 1.f; control.motionVectorScaleY = 1.f;
        control.preExposure = 1.f; control.depthReversed = YES; control.reset = frame_index == 0;
        [control encodeToCommandBuffer:(__bridge id<MTLCommandBuffer>)command->nativeHandle()];
        REQUIRE(command->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
        REQUIRE(command->waitUntilCompleted(&f.error)); INFO(f.error);
        const auto actual = f.read(*f.output), expected = f.read(*control_output);
        for (std::size_t p = 0; p < actual.size(); ++p) for (unsigned c = 0; c < 3; ++c)
        { REQUIRE(std::isfinite(actual[p][c])); REQUIRE(std::abs(actual[p][c] - expected[p][c]) < .001f); }
    }
    REQUIRE(f.scaler->stats().submitted_calls == 4);
    REQUIRE(f.scaler->stats().reset_calls == 1);
    REQUIRE(f.scaler->stats().history_valid);
}

TEST_CASE("Temporal history commits at submission and abandons without advancing", "[metal][temporal-scaler]")
{
    Fixture f;
    ALMetalTemporalScaler::Ticket abandoned = 0;
    {
        auto command = f.device->makeCommandBuffer("Unsubmitted Temporal world", &f.error);
        f.fill(*command);
        REQUIRE(f.scaler->encode(*command, f.desc, f.inputs(), {}, abandoned, &f.error));
        REQUIRE_FALSE(f.scaler->submitted(abandoned, &f.error));
        REQUIRE_FALSE(f.scaler->stats().history_valid);
        REQUIRE_FALSE(f.scaler->prepare(f.desc, f.req, &f.error));
        REQUIRE(f.scaler->lastFailure() == ALMetalTemporalScalerFailure::transaction_pending);
        f.scaler->abandon(abandoned);
    }
    REQUIRE(f.scaler->stats().abandoned_calls == 1);
    auto command = f.device->makeCommandBuffer("First submitted Temporal world", &f.error);
    f.fill(*command);
    ALMetalTemporalScaler::Ticket ticket = 0;
    REQUIRE(f.scaler->encode(*command, f.desc, f.inputs(), {}, ticket, &f.error));
    REQUIRE(command->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
    REQUIRE(f.scaler->stats().history_valid);
    REQUIRE(f.scaler->stats().reset_calls == 2);
    REQUIRE_FALSE(f.scaler->submitted(ticket, &f.error));
    f.scaler->invalidate();
    REQUIRE_FALSE(f.scaler->stats().history_valid);
    // Destroying the owner does not release the exact native scaler before
    // this already-submitted command and its hidden scratch have finished.
    f.scaler.reset();
    REQUIRE(command->waitUntilCompleted(&f.error)); INFO(f.error);
    f.checkFinite(*f.output);
}

TEST_CASE("Temporal validates complete inputs before encoding and retains ordered frames in flight", "[metal][temporal-scaler]")
{
    Fixture f;
    auto invalid_command = f.device->makeCommandBuffer("Missing Temporal motion", &f.error);
    auto bad_inputs = f.inputs(); bad_inputs.motion = nullptr;
    ALMetalTemporalScaler::Ticket ticket = 123;
    REQUIRE_FALSE(f.scaler->encode(*invalid_command, f.desc, bad_inputs, {}, ticket, &f.error));
    REQUIRE(ticket == 0);
    REQUIRE(f.scaler->lastFailure() == ALMetalTemporalScalerFailure::incompatible_texture);
    REQUIRE(f.scaler->stats().encoded_calls == 0);
    // Rejected native escape commands conservatively own an opaque scope.
    // Abandon it before this independent valid sequence and its CPU readback.
    invalid_command.reset();
    std::array<std::unique_ptr<ALMetalCommandBuffer>, 3> commands;
    std::array<std::unique_ptr<ALMetalTexture>, 3> outputs;
    for (unsigned index = 0; index < commands.size(); ++index)
    {
        commands[index] = f.device->makeCommandBuffer("Ordered pending Temporal world", &f.error);
        outputs[index] = f.texture(f.desc.output_format, f.req.output_usage, true);
        f.fill(*commands[index], {.2f, .4f, .7f, 1.f});
        REQUIRE(f.scaler->encode(*commands[index], f.desc, f.inputs(outputs[index].get()), {}, ticket, &f.error));
        REQUIRE(commands[index]->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
    }
    REQUIRE(f.scaler->stats().creation_attempts == 1);
    REQUIRE(f.scaler->stats().submitted_calls == 3);
    REQUIRE(f.scaler->stats().reset_calls == 1);
    REQUIRE(commands.back()->waitUntilCompleted(&f.error)); INFO(f.error);
    for (const auto& image : outputs)
        for (const auto& p : f.read(*image)) for (unsigned c = 0; c < 3; ++c)
        { REQUIRE(std::isfinite(p[c])); REQUIRE(std::abs(p[c] - Pixel{.2f, .4f, .7f, 1.f}[c]) < .008f); }
}

TEST_CASE("Temporal accumulated jitter history changes reconstruction and reactive content rejects it", "[metal][temporal-scaler]")
{
    Fixture f;
    auto fresh = ALMetalTemporalScaler::create(*f.device, &f.error); REQUIRE(fresh);
    auto fresh_output = f.texture(f.desc.output_format, f.req.output_usage, true);
    const std::array<std::array<float, 2>, 8> jitter{{{0.f,-1.f/6.f},{-.25f,1.f/6.f},
        {.25f,-7.f/18.f},{-.375f,-1.f/18.f},{.125f,5.f/18.f},{-.125f,-5.f/18.f},{.375f,1.f/18.f},{-.4375f,7.f/18.f}}};
    for (unsigned index = 0; index < jitter.size(); ++index)
    {
        auto command = f.device->makeCommandBuffer("Accumulated Temporal jitter fixture", &f.error);
        const auto j = jitter[index]; f.fill(*command, {}, j[0], j[1], true);
        ALMetalTemporalScalerFrame frame; frame.jitter_x = j[0]; frame.jitter_y = j[1];
        ALMetalTemporalScaler::Ticket ticket = 0, fresh_ticket = 0;
        REQUIRE(f.scaler->encode(*command, f.desc, f.inputs(), frame, ticket, &f.error));
        if (index + 1 == jitter.size())
            REQUIRE(fresh->encode(*command, f.desc, f.inputs(fresh_output.get()), frame, fresh_ticket, &f.error));
        REQUIRE(command->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
        if (fresh_ticket) REQUIRE(fresh->submitted(fresh_ticket, &f.error));
        REQUIRE(command->waitUntilCompleted(&f.error)); INFO(f.error);
    }
    const auto accumulated = f.read(*f.output), reset = f.read(*fresh_output);
    double difference = 0.;
    for (std::size_t p = 0; p < accumulated.size(); ++p) for (unsigned c = 0; c < 3; ++c)
    { REQUIRE(std::isfinite(accumulated[p][c])); difference += std::abs(accumulated[p][c] - reset[p][c]); }
    difference /= accumulated.size() * 3;
    std::cout << "Temporal accumulated versus reset mean RGB absolute difference=" << difference << '\n';
    REQUIRE(difference > 1.e-5);
    REQUIRE(f.scaler->stats().reset_calls == 1);
    // Independently authored continuous texture evaluates the expected
    // unjittered image at output pixel centers. Compare its alignment with
    // the known, deliberately shifted final input instead of vendor parity.
    double aligned_error = 0., shifted_error = 0.;
    for (unsigned y = 8; y + 8 < f.desc.output_height; ++y)
        for (unsigned x = 8; x + 8 < f.desc.output_width; ++x)
            for (unsigned c = 0; c < 3; ++c)
            {
                const auto field = [c](float sx, float sy)
                { return .45f + .24f * std::sin(sx * (1.11f + .13f * c)) * std::cos(sy * .83f); };
                const float sx = (float(x) + .5f) * float(f.desc.input_width) / float(f.desc.output_width);
                const float sy = (float(y) + .5f) * float(f.desc.input_height) / float(f.desc.output_height);
                const float actual = accumulated[std::size_t(y) * f.desc.output_width + x][c];
                const float centered = actual - field(sx, sy);
                const float shifted = actual - field(sx - jitter.back()[0], sy - jitter.back()[1]);
                aligned_error += centered * centered; shifted_error += shifted * shifted;
            }
    std::cout << "Temporal analytic alignment squared error=" << aligned_error
              << ", deliberately jittered reference=" << shifted_error << '\n';
    REQUIRE(aligned_error < shifted_error);
    // The documented mask value one selects current content. Change every
    // pixel to a new constant; no stale colored checker may survive its interior.
    auto command = f.device->makeCommandBuffer("Reactive history rejection", &f.error);
    f.fill(*command, {.8f, .12f, .25f, 1.f}, 0.f, 0.f, false, 255);
    ALMetalTemporalScaler::Ticket ticket = 0;
    REQUIRE(f.scaler->encode(*command, f.desc, f.inputs(), {}, ticket, &f.error));
    REQUIRE(command->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
    REQUIRE(command->waitUntilCompleted(&f.error)); INFO(f.error);
    const auto rejected = f.read(*f.output);
    for (unsigned y = 8; y + 8 < f.desc.output_height; ++y)
        for (unsigned x = 8; x + 8 < f.desc.output_width; ++x)
            for (unsigned c = 0; c < 3; ++c)
                REQUIRE(std::abs(rejected[std::size_t(y) * f.desc.output_width + x][c] - Pixel{.8f,.12f,.25f,1.f}[c]) < .025f);
}

TEST_CASE("Temporal resize replaces a history stream without rotating among cached histories", "[metal][temporal-scaler]")
{
    Fixture f;
    const auto original = f.desc;
    std::array<std::unique_ptr<ALMetalCommandBuffer>, 3> commands;
    std::array<std::unique_ptr<ALMetalTexture>, 3> outputs;
    for (unsigned index = 0; index < commands.size(); ++index)
    {
        if (index)
        {
            f.desc = original;
            if (index == 1)
            {
                f.desc.input_width = 72; f.desc.input_height = 50;
                f.desc.output_width = 144; f.desc.output_height = 100;
            }
            REQUIRE(f.scaler->prepare(f.desc, f.req, &f.error)); INFO(f.error);
            f.color = f.texture(f.desc.color_format, f.req.color_usage);
            f.depth = f.texture(f.desc.depth_format, f.req.depth_usage);
            f.motion = f.texture(f.desc.motion_format, f.req.motion_usage);
            f.reactive = f.texture(ALMetalPixelFormat::r8_unorm, f.req.reactive_usage);
        }
        outputs[index] = f.texture(f.desc.output_format, f.req.output_usage, true);
        commands[index] = f.device->makeCommandBuffer("Temporal resize stream", &f.error);
        f.fill(*commands[index]);
        ALMetalTemporalScaler::Ticket ticket = 0;
        REQUIRE(f.scaler->encode(*commands[index], f.desc, f.inputs(outputs[index].get()), {}, ticket, &f.error));
        REQUIRE(commands[index]->commit(&f.error)); REQUIRE(f.scaler->submitted(ticket, &f.error));
    }
    REQUIRE(f.scaler->stats().creation_successes == 3);
    REQUIRE(f.scaler->stats().reset_calls == 3);
    f.scaler.reset();
    REQUIRE(commands.back()->waitUntilCompleted(&f.error)); INFO(f.error);
    for (const auto& image : outputs) f.checkFinite(*image);
}
