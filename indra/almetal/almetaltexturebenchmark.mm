/**
 * @file almetaltexturebenchmark.mm
 * @brief Native implementation of the original Developer texture benchmark.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "almetaltexturebenchmark.h"
#include "almetalpipeline.h"
#include "almetalstagetiming.h"
#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <thread>

namespace
{
constexpr std::size_t draw_count = ALMetalTextureBenchmark::texture_count *
                                   ALMetalTextureBenchmark::iterations;
using Clock = std::chrono::steady_clock;

void setError(std::string* error, const char* message)
{
    if (error) *error = message;
}

bool diagnosticsAvailable(std::string* error)
{
    const auto* scope = ALMetalStageTimingScope::current();
    if (ALMetalDevice::commandTimingEnabled() || (scope && scope->window()) ||
        [MTLCaptureManager sharedCaptureManager].isCapturing)
    {
        setError(error, "Developer benchmark unavailable during an active GPU diagnostic or capture");
        return false;
    }
    return true;
}

struct Resources
{
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalSamplerState> sampler;
    std::array<std::unique_ptr<ALMetalTexture>, ALMetalTextureBenchmark::texture_count> sources;
    std::array<std::unique_ptr<ALMetalRenderTarget>, ALMetalTextureBenchmark::texture_count> targets;
};

struct RunResources
{
    std::shared_ptr<Resources> workload;
    std::unique_ptr<ALMetalBuffer> counts;
};

bool waitForCommand(const std::shared_ptr<const ALMetalCommandCompletion>& completion,
                    Clock::time_point deadline, std::string* error)
{
    while (!completion->isComplete())
    {
        if (Clock::now() >= deadline)
        {
            setError(error, "Developer benchmark exceeded its thirty-second GPU completion timeout");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
}

struct ALMetalTextureBenchmark::Impl
{
    const ALMetalDevice* device = nullptr;
    std::shared_ptr<Resources> resources;
    std::shared_ptr<const ALMetalCommandCompletion> previous;
};

struct ALMetalTextureBenchmarkRun::Impl
{
    std::shared_ptr<RunResources> resources;
    std::unique_ptr<ALMetalCommandBuffer> command;
    std::shared_ptr<const ALMetalCommandCompletion> completion;
};

ALMetalTextureBenchmark::ALMetalTextureBenchmark(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalTextureBenchmark::~ALMetalTextureBenchmark() = default;
ALMetalTextureBenchmarkRun::ALMetalTextureBenchmarkRun(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalTextureBenchmarkRun::~ALMetalTextureBenchmarkRun() = default;

std::unique_ptr<ALMetalTextureBenchmark> ALMetalTextureBenchmark::create(
    const ALMetalDevice& device, std::span<const std::uint8_t> pixels, std::string* error)
{
    @autoreleasepool
    {
        if (!diagnosticsAvailable(error)) return nullptr;
        if (pixels.size() != source_bytes || !device.nativeHandle())
        {
            setError(error, "Developer benchmark requires exactly one 1024-square RGBA8 source");
            return nullptr;
        }
        const auto deadline = Clock::now() + std::chrono::seconds(30);
        auto resources = std::make_shared<Resources>();
        resources->library = ALMetalShaderLibrary::loadBuiltin(device, error);
        if (!resources->library) return nullptr;
        ALMetalRenderPipelineDescriptor pd;
        pd.library = resources->library.get();
        pd.vertex_function = "almetal_benchmark_vertex";
        pd.fragment_function = "almetal_benchmark_fragment";
        pd.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        pd.label = "Developer benchmark point-sampled RGBA8";
        resources->pipeline = ALMetalRenderPipeline::create(device, pd, error);
        ALMetalSamplerDescriptor sd;
        sd.min_filter = sd.mag_filter = ALMetalSamplerFilter::nearest;
        sd.mip_filter = ALMetalSamplerMipFilter::none;
        sd.label = "Developer benchmark point wrap, no mips";
        resources->sampler = ALMetalSamplerState::create(device, sd, error);
        auto prepare = device.makeCommandBuffer("Developer benchmark untimed upload and clear", error);
        if (!resources->pipeline || !resources->sampler || !prepare) return nullptr;
        if (!prepare->retainUntilCompletion(resources, error)) return nullptr;
        ALMetalTextureDescriptor td;
        td.width = td.height = resolution;
        td.format = ALMetalPixelFormat::rgba8_unorm;
        td.label = "Developer benchmark random RGBA8 source";
        ALMetalRenderTargetDescriptor rt;
        rt.width = rt.height = resolution;
        rt.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        rt.label = "Developer benchmark RGBA8 target";
        for (std::uint32_t index = 0; index < texture_count; ++index)
        {
            resources->sources[index] = ALMetalTexture::create(device, td, error);
            resources->targets[index] = ALMetalRenderTarget::create(device, rt, error);
            if (!resources->sources[index] || !resources->targets[index] ||
                !resources->sources[index]->upload(*prepare, 0, 0, {}, pixels.data(), resolution * 4, error))
                return nullptr;
            auto pass = resources->targets[index]->makeRenderPass({}, error);
            auto encoder = pass ? ALMetalRenderEncoder::begin(*prepare, *pass,
                "Developer benchmark untimed target clear", error) : nullptr;
            if (!encoder || !encoder->end(error)) return nullptr;
            if (Clock::now() >= deadline)
            {
                setError(error, "Developer benchmark allocation exceeded thirty seconds");
                return nullptr;
            }
        }
        const auto completion = prepare->completionToken();
        if (!prepare->commit(error) || !waitForCommand(completion, deadline, error) ||
            prepare->status() != ALMetalCommandStatus::completed)
        {
            if (prepare->status() == ALMetalCommandStatus::error && error)
                *error = prepare->errorDescription();
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = &device;
        impl->resources = std::move(resources);
        return std::unique_ptr<ALMetalTextureBenchmark>(new ALMetalTextureBenchmark(std::move(impl)));
    }
}

std::unique_ptr<ALMetalTextureBenchmarkRun> ALMetalTextureBenchmark::encodeRun(std::string* error)
{
    @autoreleasepool
    {
        if (!diagnosticsAvailable(error)) return nullptr;
        if (mImpl->previous && !mImpl->previous->isComplete())
        {
            setError(error, "Developer benchmark already has an outstanding run");
            return nullptr;
        }
        auto impl = std::make_unique<ALMetalTextureBenchmarkRun::Impl>();
        impl->resources = std::make_shared<RunResources>();
        impl->resources->workload = mImpl->resources;
        ALMetalBufferDescriptor bd;
        bd.length = draw_count * sizeof(std::uint64_t);
        bd.storage = ALMetalStorageMode::shared;
        bd.label = "Developer benchmark actual passing sample counts";
        impl->resources->counts = ALMetalBuffer::create(*mImpl->device, bd, error);
        impl->command = mImpl->device->makeCommandBuffer("Developer benchmark measured texture draws", error);
        if (!impl->resources->counts || !impl->command ||
            !impl->command->retainUntilCompletion(impl->resources, error)) return nullptr;
        ALMetalRenderPassConfig config;
        config.colors[0].load = ALMetalLoadAction::load;
        auto pass = mImpl->resources->targets[0]->makeRenderPass(config, error);
        if (!pass) return nullptr;
        // Visibility-result attachment is physical render-pass state. Keep a
        // fresh slot for every draw; Metal resets the buffer at encoder start.
        MTLRenderPassDescriptor* native_pass = (__bridge MTLRenderPassDescriptor*)pass->nativeHandle();
        native_pass.visibilityResultBuffer = (__bridge id<MTLBuffer>)impl->resources->counts->nativeHandle();
        auto encoder = ALMetalRenderEncoder::begin(*impl->command, *pass,
            "Developer benchmark 64 x 32 texture samples", error);
        if (!encoder || !encoder->setPipeline(*mImpl->resources->pipeline, error) ||
            !encoder->setFragmentSampler(0, mImpl->resources->sampler.get(), error) ||
            !encoder->setViewport({0, 0, resolution, resolution, 0, 1}, error) ||
            !encoder->setCullMode(ALMetalCullMode::none, error)) return nullptr;
        std::size_t draw = 0;
        for (std::uint32_t iteration = 0; iteration < iterations; ++iteration)
            for (const auto& source : mImpl->resources->sources)
            {
                if (!encoder->setFragmentTexture(0, source.get(), error) ||
                    !encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,
                        draw++ * sizeof(std::uint64_t), error) ||
                    !encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error))
                    return nullptr;
            }
        if (!encoder->end(error)) return nullptr;
        impl->completion = impl->command->completionToken();
        mImpl->previous = impl->completion;
        return std::unique_ptr<ALMetalTextureBenchmarkRun>(new ALMetalTextureBenchmarkRun(std::move(impl)));
    }
}

bool ALMetalTextureBenchmarkRun::commit(std::string* error)
{
    return diagnosticsAvailable(error) && mImpl->command->commit(error);
}

ALMetalTextureBenchmarkRun::Status ALMetalTextureBenchmarkRun::poll(
    ALMetalTextureBenchmarkResult& output, std::string* error) const
{
    output = {};
    ALMetalCommandGPUTime timing;
    const auto status = mImpl->completion->gpuTime(timing);
    if (status == ALMetalCommandTimingStatus::pending) return Status::pending;
    if (status != ALMetalCommandTimingStatus::ready)
    {
        if (error) *error = mImpl->command->errorDescription();
        if (!error || error->empty()) setError(error, "Developer benchmark GPU timestamps unavailable or command failed");
        return Status::failed;
    }
    const auto* counts = static_cast<const std::uint64_t*>(mImpl->resources->counts->mappedBytes());
    std::uint64_t samples = 0;
    for (std::size_t draw = 0; draw < draw_count; ++draw)
    {
        if (counts[draw] != std::uint64_t(ALMetalTextureBenchmark::resolution) * ALMetalTextureBenchmark::resolution)
        {
            setError(error, "Developer benchmark passing-sample count unavailable or incomplete");
            return Status::failed;
        }
        samples += counts[draw];
    }
    output.gpu_time = timing;
    output.samples_drawn = samples;
    output.sampled_bytes = samples * 4;
    // bytes/(10^9 * seconds) == bytes/nanoseconds. This is the original
    // four-byte sampled throughput, not a measurement of physical DRAM IO.
    output.gigabytes_per_second = double(output.sampled_bytes) / timing.elapsed_nanoseconds;
    return Status::ready;
}

bool ALMetalTextureBenchmarkRun::wait(ALMetalTextureBenchmarkResult& output, std::string* error) const
{
    output = {};
    if (!mImpl->command->isCommitted())
    {
        setError(error, "Developer benchmark must be committed before waiting");
        return false;
    }
    return waitForCommand(mImpl->completion, Clock::now() + std::chrono::seconds(30), error) &&
           poll(output, error) == Status::ready;
}

const ALMetalTexture* ALMetalTextureBenchmarkRun::outputTexture() const noexcept
{
    return mImpl->resources->workload->targets[0]->colorTexture(0);
}

std::shared_ptr<const ALMetalCommandCompletion> ALMetalTextureBenchmarkRun::completion() const noexcept
{
    return mImpl->completion;
}
