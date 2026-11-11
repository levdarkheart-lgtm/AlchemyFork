/**
 * @file almetalscenemonitor.cpp
 * @brief Native implementation of the original scene monitor's two filters.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "almetalscenemonitor.h"
#include "almetaldevice.h"
#include "almetalpipeline.h"
#include <cmath>
#include <limits>
#include <utility>

namespace
{
void fail(std::string* error, const char* message) { if (error) *error = message; }
struct Constants
{
    std::uint32_t crop_width, crop_height;
    float tolerance;
    std::uint32_t padding = 0;
};
bool validColor(const ALMetalTexture* texture)
{
    if (!texture) return false;
    const auto& d = texture->descriptor();
    return d.type == ALMetalTextureType::texture_2d && d.sample_count == 1 &&
        d.width != 0 && d.height != 0 && d.storage != ALMetalStorageMode::memoryless &&
        alMetalHasUsage(d.usage, ALMetalTextureUsage::shader_read) &&
        (d.format == ALMetalPixelFormat::rgba8_unorm ||
         d.format == ALMetalPixelFormat::bgra8_unorm);
}
}

struct ALMetalSceneDifference::Impl
{
    std::unique_ptr<ALMetalCommandBuffer> command;
    std::unique_ptr<ALMetalTexture> diff, filtered;
    std::unique_ptr<ALMetalBuffer> count;
    double denominator = 0.0;
    std::uint64_t maximum_samples = 0;
};

ALMetalSceneDifference::ALMetalSceneDifference(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalSceneDifference::~ALMetalSceneDifference() = default;
ALMetalSceneDifference::Status ALMetalSceneDifference::status() const noexcept
{
    switch (mImpl->command->status())
    {
        case ALMetalCommandStatus::completed: return Status::ready;
        case ALMetalCommandStatus::error:
        case ALMetalCommandStatus::unknown: return Status::failed;
        default: return Status::pending;
    }
}
bool ALMetalSceneDifference::result(std::uint64_t& changed_samples,
    float& normalized_difference, std::string* error) const
{
    if (error) error->clear();
    if (status() == Status::pending) return false;
    if (status() != Status::ready)
    {
        if (error) *error = mImpl->command->errorDescription();
        return false;
    }
    std::uint32_t count = 0;
    if (!mImpl->count->read(0, &count, sizeof(count), error)) return false;
    if (count > mImpl->maximum_samples)
    {
        fail(error, "Scene difference count exceeded its sampled pixel domain");
        return false;
    }
    changed_samples = count;
    // Preserve the original calibrated numeric result. The original comment
    // claimed double-sided geometry, but its helper emits one ordinary quad.
    normalized_difference = static_cast<float>(std::sqrt(count * 0.5 / mImpl->denominator));
    return true;
}
const ALMetalTexture* ALMetalSceneDifference::diffTexture() const noexcept { return mImpl->diff.get(); }
const ALMetalTexture* ALMetalSceneDifference::filteredTexture() const noexcept { return mImpl->filtered.get(); }

struct ALMetalSceneMonitor::Impl
{
    const ALMetalDevice* device = nullptr;
    std::uint64_t generation = 0;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalComputePipeline> diff_pipeline, filter_pipeline;
    std::unique_ptr<ALMetalSamplerState> bilinear_mirror;
    bool shaders(std::string* error)
    {
        const auto wanted = ALMetalShaderLibrary::builtinReloadGeneration();
        if (library && generation == wanted) return true;
        auto next_library = ALMetalShaderLibrary::loadBuiltin(*device, error);
        if (!next_library) return false;
        ALMetalComputePipelineDescriptor d;
        d.library = next_library.get();
        d.function = "almetal_scene_monitor_difference";
        d.label = "Alchemy scene loading Bayer difference";
        auto next_diff = ALMetalComputePipeline::create(*device, d, error);
        d.function = "almetal_scene_monitor_filter";
        d.label = "Alchemy scene loading filtered pixel count";
        auto next_filter = ALMetalComputePipeline::create(*device, d, error);
        if (!next_diff || !next_filter) return false;
        library = std::move(next_library);
        diff_pipeline = std::move(next_diff);
        filter_pipeline = std::move(next_filter);
        generation = wanted;
        return true;
    }
};
ALMetalSceneMonitor::ALMetalSceneMonitor(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalSceneMonitor::~ALMetalSceneMonitor() = default;
std::unique_ptr<ALMetalSceneMonitor> ALMetalSceneMonitor::create(
    const ALMetalDevice& device, std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    ALMetalSamplerDescriptor sampler;
    sampler.min_filter = sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::none;
    sampler.address_s = sampler.address_t = sampler.address_r = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.label = "Alchemy scene monitor original BilinearMirror";
    impl->bilinear_mirror = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->bilinear_mirror || !impl->shaders(error)) return nullptr;
    return std::unique_ptr<ALMetalSceneMonitor>(new ALMetalSceneMonitor(std::move(impl)));
}
std::unique_ptr<ALMetalSceneDifference> ALMetalSceneMonitor::submit(
    ALMetalRetainedBackground::Lease previous,
    ALMetalRetainedBackground::Lease current,
    const ALMetalSceneDifferenceConfig& config, std::string* error)
{
    if (error) error->clear();
    const auto max_dimension = mImpl->device->info().max_texture_2d_dimension;
    if (!previous || !current || previous->context.device != mImpl->device ||
        !(previous->context == current->context) ||
        previous->source_frame == std::numeric_limits<std::uint64_t>::max() ||
        current->source_frame == std::numeric_limits<std::uint64_t>::max() ||
        !validColor(previous->colorTexture()) || !validColor(current->colorTexture()) ||
        config.capture_width == 0 || config.capture_height == 0 ||
        config.capture_width > current->context.width || config.capture_height > current->context.height ||
        config.diff_width == 0 || config.diff_height == 0 ||
        config.diff_width > max_dimension || config.diff_height > max_dimension ||
        !std::isfinite(config.sample_ratio) || config.sample_ratio <= 0.f || config.sample_ratio > 1.f ||
        !std::isfinite(config.tolerance))
    {
        fail(error, "Scene difference requires coherent display-byte leases and a bounded crop/filter domain");
        return nullptr;
    }
    const auto width = static_cast<std::uint32_t>(config.diff_width * config.sample_ratio);
    const auto height = static_cast<std::uint32_t>(config.diff_height * config.sample_ratio);
    if (width == 0 || height == 0 ||
        static_cast<std::uint64_t>(width) * height > std::numeric_limits<std::uint32_t>::max())
    {
        fail(error, "Scene difference sampled domain is empty or too large");
        return nullptr;
    }
    if (!mImpl->shaders(error)) return nullptr;
    auto result = std::make_unique<ALMetalSceneDifference::Impl>();
    ALMetalTextureDescriptor d;
    d.width = config.diff_width; d.height = config.diff_height;
    d.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::shader_write;
    d.label = "Alchemy scene loading full Bayer difference";
    result->diff = ALMetalTexture::create(*mImpl->device, d, error);
    d.width = width; d.height = height;
    d.label = "Alchemy scene loading filtered debug image";
    result->filtered = ALMetalTexture::create(*mImpl->device, d, error);
    ALMetalBufferDescriptor buffer;
    buffer.length = sizeof(std::uint32_t);
    buffer.label = "Alchemy scene loading changed pixel count";
    result->count = ALMetalBuffer::create(*mImpl->device, buffer, error);
    const std::uint32_t zero = 0;
    if (!result->diff || !result->filtered || !result->count ||
        !result->count->write(0, &zero, sizeof(zero), error)) return nullptr;
    result->command = mImpl->device->makeCommandBuffer("Alchemy scene loading difference", error);
    if (!result->command ||
        !result->command->retainUntilCompletion(previous, error) ||
        !result->command->retainUntilCompletion(current, error)) return nullptr;
    Constants constants{config.capture_width, config.capture_height, config.tolerance};
    auto encoder = ALMetalComputeEncoder::begin(*result->command, "Scene loading Bayer difference", error);
    if (!encoder || !encoder->setPipeline(*mImpl->diff_pipeline, error) ||
        !encoder->setBytes(0, &constants, sizeof(constants), error) ||
        !encoder->setTexture(0, previous->colorTexture(), error) ||
        !encoder->setTexture(1, current->colorTexture(), error) ||
        !encoder->setTexture(2, result->diff.get(), error) ||
        !encoder->dispatch(config.diff_width, config.diff_height, 1, 8, 8, 1, error) ||
        !encoder->end(error)) return nullptr;
    encoder = ALMetalComputeEncoder::begin(*result->command, "Scene loading filtered pixel count", error);
    if (!encoder || !encoder->setPipeline(*mImpl->filter_pipeline, error) ||
        !encoder->setBytes(0, &constants, sizeof(constants), error) ||
        !encoder->setBuffer(1, *result->count, 0, error) ||
        !encoder->setTexture(0, result->diff.get(), error) ||
        !encoder->setTexture(1, result->filtered.get(), error) ||
        !encoder->setSampler(0, mImpl->bilinear_mirror.get(), error) ||
        !encoder->dispatch(width, height, 1, 8, 8, 1, error) ||
        !encoder->end(error) || !result->command->commit(error)) return nullptr;
    result->maximum_samples = static_cast<std::uint64_t>(width) * height;
    result->denominator = static_cast<double>(config.diff_width) * config.diff_height *
        config.sample_ratio * config.sample_ratio;
    return std::unique_ptr<ALMetalSceneDifference>(new ALMetalSceneDifference(std::move(result)));
}
