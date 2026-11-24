/**
 * @file almetal_spatial_scaler_test.mm
 * @brief Actual classic MetalFX encoding, caching and pending-command ownership.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalspatialscaler.h"
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
    std::unique_ptr<ALMetalSpatialScaler> scaler;
    Fixture()
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled);
        INFO(error); REQUIRE(device);
        REQUIRE(ALMetalSpatialScaler::supported(*device));
        scaler = ALMetalSpatialScaler::create(*device, &error);
        INFO(error); REQUIRE(scaler);
    }
    static ALMetalSpatialScalerDescriptor descriptor(unsigned index = 0)
    {
        ALMetalSpatialScalerDescriptor result;
        result.input_width = 32 + index * 4;
        result.input_height = 24 + index * 2;
        result.output_width = result.input_width * 2;
        result.output_height = result.input_height * 2;
        return result;
    }
    std::unique_ptr<ALMetalTexture> texture(
        const ALMetalSpatialScalerDescriptor& desc, bool output,
        ALMetalStorageMode storage = ALMetalStorageMode::private_gpu)
    {
        ALMetalSpatialScalerRequirements requirements;
        REQUIRE(scaler->prepare(desc, requirements, &error)); INFO(error);
        ALMetalTextureDescriptor td;
        td.width = output ? desc.output_width : desc.input_width;
        td.height = output ? desc.output_height : desc.input_height;
        td.format = output ? desc.output_format : desc.input_format;
        td.usage = (output ? requirements.output_usage : requirements.input_usage) |
            ALMetalTextureUsage::shader_read;
        td.storage = storage;
        auto result = ALMetalTexture::create(*device, td, &error);
        INFO(error); REQUIRE(result); return result;
    }
    void fill(ALMetalCommandBuffer& command, ALMetalTexture& texture,
              Pixel color, bool pattern = false)
    {
        REQUIRE(texture.descriptor().format == ALMetalPixelFormat::rgba16_float);
        const auto width = texture.width(), height = texture.height();
        std::vector<std::uint16_t> values(std::size_t(width) * height * 4);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
                for (unsigned channel = 0; channel < 4; ++channel)
                {
                    const float value = pattern && channel < 3
                        ? (((x / 4 + y / 3 + channel) % 2) ? .12f : .79f)
                        : color[channel];
                    const __fp16 half = value;
                    std::memcpy(&values[(std::size_t(y) * width + x) * 4 + channel],
                        &half, sizeof(half));
                }
        REQUIRE(texture.upload(command, 0, 0, {}, values.data(), width * 8, &error));
        INFO(error);
    }
    std::vector<Pixel> read(const ALMetalTexture& texture)
    {
        REQUIRE(texture.descriptor().format == ALMetalPixelFormat::rgba16_float);
        std::vector<std::uint16_t> raw(std::size_t(texture.width()) * texture.height() * 4);
        REQUIRE(texture.readback(*device, 0, 0, {}, raw.data(), texture.width() * 8, &error));
        INFO(error);
        std::vector<Pixel> pixels(raw.size() / 4);
        for (std::size_t index = 0; index < raw.size(); ++index)
        {
            __fp16 value;
            std::memcpy(&value, &raw[index], sizeof(value));
            pixels[index / 4][index % 4] = value;
        }
        return pixels;
    }
    void expectConstant(const ALMetalTexture& texture, Pixel color)
    {
        for (const auto& pixel : read(texture))
        {
            // Spatial color processing is RGB. World final presentation owns
            // opaque alpha; MetalFX alpha preservation is not assumed here.
            for (unsigned channel = 0; channel < 3; ++channel)
            { REQUIRE(std::isfinite(pixel[channel])); REQUIRE(std::abs(pixel[channel] - color[channel]) < .004f); }
        }
    }
};
}

TEST_CASE("MetalFX spatial descriptors cache support and negative results", "[metal][spatial-scaler]")
{
    Fixture fixture;
    auto desc = Fixture::descriptor();
    ALMetalSpatialScalerRequirements requirements;
    REQUIRE(fixture.scaler->prepare(desc, requirements, &fixture.error)); INFO(fixture.error);
    REQUIRE(requirements.output_storage == ALMetalStorageMode::private_gpu);
    REQUIRE(requirements.native_input_usage != 0);
    REQUIRE(requirements.native_output_usage != 0);
    const auto created = fixture.scaler->stats().creation_attempts;
    REQUIRE(created == 1);
    for (unsigned n = 0; n < 8; ++n)
        REQUIRE(fixture.scaler->prepare(desc, requirements, &fixture.error));
    REQUIRE(fixture.scaler->stats().creation_attempts == created);
    REQUIRE(fixture.scaler->stats().cached_entries == 1);

    auto invalid = desc;
    invalid.input_width = 0;
    REQUIRE_FALSE(fixture.scaler->prepare(invalid, requirements, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::invalid_descriptor);
    REQUIRE_FALSE(fixture.scaler->prepare(invalid, requirements, &fixture.error));
    REQUIRE(fixture.scaler->stats().creation_attempts == created);
    REQUIRE(fixture.scaler->stats().cached_failures == 1);
    auto srgb = desc;
    srgb.input_format = ALMetalPixelFormat::rgba8_unorm_srgb;
    REQUIRE_FALSE(fixture.scaler->prepare(srgb, requirements, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::unsupported_format);

    // This is an actual format query, not an assumption that current display
    // RGB10A2 is supported because the GPU supports MetalFX generally.
    auto packed = desc;
    packed.input_format = ALMetalPixelFormat::rgb10a2_unorm;
    const bool accepted = fixture.scaler->prepare(packed, requirements, &fixture.error);
    const auto attempts = fixture.scaler->stats().creation_attempts;
    const auto reason = fixture.scaler->lastFailure();
    std::cout << "MetalFX RGB10A2 to perceptual RGBA16Float accepted=" << accepted
              << ", failure=" << static_cast<unsigned>(reason) << '\n';
    REQUIRE(fixture.scaler->prepare(packed, requirements, &fixture.error) == accepted);
    REQUIRE(fixture.scaler->stats().creation_attempts == attempts);
    REQUIRE(fixture.scaler->lastFailure() == reason);
    if (!accepted)
        REQUIRE((reason == ALMetalSpatialScalerFailure::creation_failed ||
                 reason == ALMetalSpatialScalerFailure::unsupported_usage));
}

TEST_CASE("MetalFX spatial helper matches a direct perceptual native encoding", "[metal][spatial-scaler]")
{
    Fixture fixture;
    const auto desc = Fixture::descriptor();
    auto input = fixture.texture(desc, false);
    auto output = fixture.texture(desc, true);
    auto reference = fixture.texture(desc, true);
    auto command = fixture.device->makeCommandBuffer("Spatial perceptual native comparison", &fixture.error);
    REQUIRE(command);
    fixture.fill(*command, *input, {.25f, .5f, .75f, 1.f}, true);
    const auto submitted = ALMetalDevice::committedCommandBufferCount();
    REQUIRE(fixture.scaler->encode(*command, desc, *input, *output, &fixture.error)); INFO(fixture.error);
    REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);

    // The independent vendor call fixes perceptual processing explicitly and
    // uses the same uploaded nonuniform encoded values. A float format alone
    // is not evidence for linear/HDR processing.
    auto native_desc = [MTLFXSpatialScalerDescriptor new];
    native_desc.inputWidth = desc.input_width; native_desc.inputHeight = desc.input_height;
    native_desc.outputWidth = desc.output_width; native_desc.outputHeight = desc.output_height;
    native_desc.colorTextureFormat = MTLPixelFormatRGBA16Float;
    native_desc.outputTextureFormat = MTLPixelFormatRGBA16Float;
    native_desc.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;
    auto control = [native_desc newSpatialScalerWithDevice:(__bridge id<MTLDevice>)fixture.device->nativeHandle()];
    REQUIRE(control != nil);
    control.colorTexture = (__bridge id<MTLTexture>)input->nativeHandle();
    control.outputTexture = (__bridge id<MTLTexture>)reference->nativeHandle();
    control.inputContentWidth = desc.input_width; control.inputContentHeight = desc.input_height;
    [control encodeToCommandBuffer:(__bridge id<MTLCommandBuffer>)command->nativeHandle()];
    REQUIRE(command->commitAndWait(&fixture.error)); INFO(fixture.error);
    const auto actual = fixture.read(*output), expected = fixture.read(*reference);
    REQUIRE(actual.size() == expected.size());
    for (std::size_t pixel = 0; pixel < actual.size(); ++pixel)
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            REQUIRE(std::isfinite(actual[pixel][channel]));
            REQUIRE(std::abs(actual[pixel][channel] - expected[pixel][channel]) < .001f);
        }
}

TEST_CASE("MetalFX same-command images survive scaler retirement", "[metal][spatial-scaler]")
{
    Fixture fixture;
    const auto desc = Fixture::descriptor();
    auto first = fixture.texture(desc, false), second = fixture.texture(desc, false);
    auto first_output = fixture.texture(desc, true), second_output = fixture.texture(desc, true);
    auto command = fixture.device->makeCommandBuffer("Two pending spatial images one command", &fixture.error);
    REQUIRE(command);
    fixture.fill(*command, *first, {.2f, .4f, .7f, 1.f});
    fixture.fill(*command, *second, {.7f, .3f, .1f, 1.f});
    REQUIRE(fixture.scaler->encode(*command, desc, *first, *first_output, &fixture.error));
    REQUIRE(fixture.scaler->encode(*command, desc, *second, *second_output, &fixture.error));
    REQUIRE(fixture.scaler->stats().creation_attempts == 1);
    REQUIRE(fixture.scaler->stats().encoded_calls == 2);
    const auto plan = command->resourceUsePlan();
    REQUIRE(plan); REQUIRE(plan->stats().opaque);
    // Native resource references plus the command's retained scaler protect
    // accepted encodes; no caller pool exists in this fixture.
    first.reset(); second.reset(); fixture.scaler.reset();
    REQUIRE(command->commitAndWait(&fixture.error)); INFO(fixture.error);
    fixture.expectConstant(*first_output, {.2f, .4f, .7f, 1.f});
    fixture.expectConstant(*second_output, {.7f, .3f, .1f, 1.f});
}

TEST_CASE("MetalFX pending commands reuse one descriptor and survive resize eviction", "[metal][spatial-scaler]")
{
    Fixture fixture;
    struct Work
    {
        std::unique_ptr<ALMetalCommandBuffer> command;
        std::unique_ptr<ALMetalTexture> output;
        Pixel color;
    };
    std::vector<Work> pending;
    for (unsigned index = 0; index < 6; ++index)
    {
        // The first two uncommitted commands use the SAME effect with
        // different images. Subsequent keys evict it before either commits.
        const auto desc = Fixture::descriptor(index ? index - 1 : 0);
        auto input = fixture.texture(desc, false);
        auto output = fixture.texture(desc, true);
        auto command = fixture.device->makeCommandBuffer("Pending resized spatial output", &fixture.error);
        REQUIRE(command);
        const Pixel color{.1f + index * .1f, .6f, .3f, 1.f};
        fixture.fill(*command, *input, color);
        REQUIRE(fixture.scaler->encode(*command, desc, *input, *output, &fixture.error));
        pending.push_back({std::move(command), std::move(output), color});
    }
    REQUIRE(fixture.scaler->stats().cached_entries == 4);
    REQUIRE(fixture.scaler->stats().creation_attempts == 5);
    REQUIRE(fixture.scaler->stats().encoded_calls == 6);
    fixture.scaler.reset();
    for (auto& work : pending) REQUIRE(work.command->commit(&fixture.error));
    for (auto& work : pending)
    {
        REQUIRE(work.command->waitUntilCompleted(&fixture.error)); INFO(fixture.error);
        fixture.expectConstant(*work.output, work.color);
    }
}

TEST_CASE("MetalFX rejects incompatible storage commands and texture usage", "[metal][spatial-scaler]")
{
    Fixture fixture;
    const auto desc = Fixture::descriptor();
    auto input = fixture.texture(desc, false), output = fixture.texture(desc, true);
    auto shared = fixture.texture(desc, true, ALMetalStorageMode::shared);
    auto command = fixture.device->makeCommandBuffer("Rejected spatial texture binding", &fixture.error);
    REQUIRE(command);
    REQUIRE_FALSE(fixture.scaler->encode(*command, desc, *input, *shared, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::incompatible_texture);

    ALMetalTextureDescriptor td;
    td.width = desc.input_width; td.height = desc.input_height;
    td.format = desc.input_format; td.usage = ALMetalTextureUsage::render_target;
    auto unsampleable = ALMetalTexture::create(*fixture.device, td, &fixture.error); REQUIRE(unsampleable);
    auto missing_usage = fixture.device->makeCommandBuffer("Rejected spatial source usage", &fixture.error);
    REQUIRE(missing_usage);
    REQUIRE_FALSE(fixture.scaler->encode(*missing_usage, desc, *unsampleable, *output, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::incompatible_texture);

    auto other = ALMetalDevice::create(&fixture.error, ALMetalResourcePlanMode::accounting_only); REQUIRE(other);
    REQUIRE(other->nativeHandle() == fixture.device->nativeHandle());
    REQUIRE(other->nativeCommandQueue() != fixture.device->nativeCommandQueue());
    auto foreign_queue = other->makeCommandBuffer("Rejected spatial foreign queue", &fixture.error); REQUIRE(foreign_queue);
    REQUIRE_FALSE(fixture.scaler->encode(*foreign_queue, desc, *input, *output, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::incompatible_command);

    auto committed = fixture.device->makeCommandBuffer("Rejected already submitted spatial command", &fixture.error);
    REQUIRE(committed); REQUIRE(committed->commitAndWait(&fixture.error));
    REQUIRE_FALSE(fixture.scaler->encode(*committed, desc, *input, *output, &fixture.error));
    REQUIRE(fixture.scaler->lastFailure() == ALMetalSpatialScalerFailure::incompatible_command);
}
