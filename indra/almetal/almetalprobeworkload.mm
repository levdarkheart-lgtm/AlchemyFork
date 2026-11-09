/**
 * @file almetalprobeworkload.mm
 * @brief Completion-only private probe constant and sparse G-buffer records.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalprobeworkload.h"
#include "almetalworld.h"

#import <Metal/Metal.h>
#include <dispatch/dispatch.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint32_t grid_width = ALMetalProbeWorkloadCapture::sample_width;
    constexpr std::uint32_t grid_height = ALMetalProbeWorkloadCapture::sample_height;
    constexpr std::size_t sample_count = grid_width * grid_height;

    struct alignas(16) Sample
    {
        std::uint32_t x, y;
        float depth, normal_alpha;
        std::uint32_t material_class, geometry;
        std::uint32_t reserved[2];
    };
    static_assert(sizeof(Sample) == 32);
    static_assert(sizeof(ALMetalWorldBaseLightingConstants) == 336);
    static_assert(sizeof(ALMetalWorldReflectionProbeConstants) == 49216);

    struct Header
    {
        char magic[8] = {'A','L','P','R','W','K','1','\0'};
        std::uint32_t version = 1, header_bytes = sizeof(Header);
        std::uint32_t sample_stride = sizeof(Sample), samples = sample_count;
        std::uint32_t grid_width = ::grid_width, grid_height = ::grid_height;
        std::uint32_t width = 0, height = 0;
        std::uint32_t lighting_bytes = sizeof(ALMetalWorldBaseLightingConstants);
        std::uint32_t probe_bytes = sizeof(ALMetalWorldReflectionProbeConstants);
        std::uint32_t reserved[4]{};
    };
    static_assert(sizeof(Header) == 64);

    struct State
    {
        int directory = -1;
        std::atomic<std::uint64_t> scheduled{0}, written{0}, failed{0}, cancelled{0};
        std::mutex mutex;
        std::string asynchronous_error;
        ~State() { if (directory >= 0) close(directory); }
        void fail(const char* message) noexcept
        {
            failed.fetch_add(1, std::memory_order_relaxed);
            try
            {
                std::lock_guard lock(mutex);
                if (asynchronous_error.empty()) asynchronous_error = message;
            }
            catch (...) {} // Reporting must not escape a GCD completion block.
        }
    };

    struct DirectoryDescriptor
    {
        int value = -1;
        ~DirectoryDescriptor() { if (value >= 0) close(value); }
        int release() { return std::exchange(value, -1); }
    };

    struct Snapshot
    {
        std::shared_ptr<State> state;
        std::unique_ptr<ALMetalBuffer> samples;
        id<MTLTexture> depth = nil, normal = nil;
        id<MTLComputePipelineState> pipeline = nil;
        ALMetalWorldBaseLightingConstants lighting{};
        ALMetalWorldReflectionProbeConstants probes{};
        ALMetalProbeWorkloadView view{};
        Header header;
        bool has_probes = false;
        std::uint64_t registry_id = 0;
        pid_t process = 0;
        std::atomic<unsigned> terminal{0};
    };

    void setError(std::string* error, const char* message)
    {
        if (error) *error = message;
    }

    bool finiteMatrix(const float* values)
    {
        for (unsigned i = 0; i < 16; ++i)
            if (!std::isfinite(values[i])) return false;
        return true;
    }

    bool writeAll(int descriptor, const void* data, std::size_t size)
    {
        auto bytes = static_cast<const char*>(data);
        while (size)
        {
            const ssize_t count = write(descriptor, bytes, size);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return false;
            bytes += count;
            size -= static_cast<std::size_t>(count);
        }
        return true;
    }

    int createRecord(int directory, const std::string& name)
    {
        const int descriptor = openat(directory, name.c_str(),
            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (descriptor < 0) return -1;
        struct stat status{};
        if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
            status.st_uid != geteuid() || fchmod(descriptor, 0600) != 0)
        {
            close(descriptor);
            unlinkat(directory, name.c_str(), 0);
            return -1;
        }
        return descriptor;
    }

    void matrixJSON(std::ostream& output, const float* values)
    {
        output << '[';
        for (unsigned i = 0; i < 16; ++i)
        {
            if (i) output << ',';
            output << values[i];
        }
        output << ']';
    }

    void writeSnapshot(const std::shared_ptr<Snapshot>& snapshot,
                       const char* terminal_status, std::int64_t gpu_error)
    {
        const bool completed = std::strcmp(terminal_status, "complete") == 0;
        const auto& view = snapshot->view;
        const std::string stem = "probe-workload-" +
            std::to_string(snapshot->process) + '-' +
            std::to_string(view.frame_index) + '-' +
            std::to_string(view.view_kind) + '-' +
            std::to_string(view.face_index);
        const std::string binary_name = stem + ".bin";
        const std::string json_name = stem + ".json";
        const int directory = snapshot->state->directory;
        int binary = -1;
        if (completed)
        {
            binary = createRecord(directory, binary_name);
            if (binary < 0)
            {
                snapshot->state->fail("Probe workload output exists or could not be created; no overwrite attempted");
                return;
            }
            const bool success =
                writeAll(binary, &snapshot->header, sizeof(Header)) &&
                writeAll(binary, &snapshot->lighting, sizeof(snapshot->lighting)) &&
                writeAll(binary, &snapshot->probes, sizeof(snapshot->probes)) &&
                writeAll(binary, snapshot->samples->mappedBytes(), sizeof(Sample) * sample_count);
            const bool closed = close(binary) == 0;
            if (!success || !closed)
            {
                unlinkat(directory, binary_name.c_str(), 0);
                snapshot->state->fail("Probe workload binary write failed");
                return;
            }
        }

        std::ostringstream json;
        json.imbue(std::locale::classic());
        json << std::setprecision(std::numeric_limits<float>::max_digits10)
             << "{\"schema_version\":1,\"status\":\""
             << terminal_status
             << "\",\"gpu_error_code\":" << gpu_error
             << ",\"gpu_completed\":" << (completed ? "true" : "false")
             << ",\"frame_index\":" << view.frame_index
             << ",\"view_kind\":" << view.view_kind
             << ",\"face_index\":" << view.face_index
             << ",\"device_registry_id\":" << snapshot->registry_id
             << ",\"width\":" << snapshot->header.width
             << ",\"height\":" << snapshot->header.height
             << ",\"reverse_z\":" << snapshot->lighting.config[0]
             << ",\"classic_mode\":" << snapshot->lighting.config[1]
             << ",\"normal_hdr\":" << snapshot->lighting.surface[1]
             << ",\"legacy_environment\":" << snapshot->lighting.environment_config[0]
             << ",\"has_probe_pack\":" << (snapshot->has_probes ? "true" : "false")
             << ",\"probe_count\":" << snapshot->probes.config[0]
             << ",\"projection_column_major\":";
        matrixJSON(json, view.projection);
        json << ",\"inverse_projection_column_major\":";
        matrixJSON(json, snapshot->lighting.inverse_projection);
        json << ",\"sampling\":{\"grid\":[64,40],\"pattern\":\"integer_cell_centers\","
                "\"stride_bytes\":32,\"fields\":[\"x:u32\",\"y:u32\",\"depth:f32\","
                "\"normal_alpha:f32\",\"material_class:u32\",\"geometry:u32\",\"reserved:u32x2\"],"
                "\"material_classes\":{\"legacy\":0,\"pbr\":1,\"sky\":2},"
                "\"pbr_rule\":\"abs(normal_alpha-0.67)<0.1\"},"
                "\"binary\":{\"file\":\"" << (completed ? binary_name : "")
             << "\",\"endianness\":\"little\",\"header_bytes\":64,"
                "\"lighting_offset\":64,\"lighting_bytes\":336,"
                "\"probe_offset\":400,\"probe_bytes\":49216,"
                "\"samples_offset\":49616,\"sample_count\":2560},"
                "\"selection_reference\":\"1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:class3/deferred/reflectionProbeF.glsl\","
                "\"limitation\":\"Sparse observed geometry samples; not full image, timing or a proof of tile-wide containment\"}\n";
        const std::string text = json.str();
        const int descriptor = createRecord(directory, json_name);
        if (descriptor < 0)
        {
            if (completed) unlinkat(directory, binary_name.c_str(), 0);
            snapshot->state->fail("Probe workload JSON exists or could not be created; no overwrite attempted");
            return;
        }
        const bool success = writeAll(descriptor, text.data(), text.size());
        const bool closed = close(descriptor) == 0;
        if (!success || !closed)
        {
            unlinkat(directory, json_name.c_str(), 0);
            if (completed) unlinkat(directory, binary_name.c_str(), 0);
            snapshot->state->fail("Probe workload JSON write failed");
            return;
        }
        if (completed) snapshot->state->written.fetch_add(1, std::memory_order_relaxed);
        else if (std::strcmp(terminal_status, "cancelled") == 0)
            snapshot->state->cancelled.fetch_add(1, std::memory_order_relaxed);
        else snapshot->state->fail("Probe workload command failed on the GPU; error record written");
    }

    constexpr const char* kernel_source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Sample { uint2 pixel; float depth; float normal_alpha; uint material_class; uint geometry; uint2 reserved; };
kernel void almetal_probe_workload_sample(
    texture2d<float> depth [[texture(0)]], texture2d<float> normal [[texture(1)]],
    device Sample* output [[buffer(0)]], constant uint& reverse_z [[buffer(1)]],
    uint2 cell [[thread_position_in_grid]])
{
    if (cell.x >= 64u || cell.y >= 40u) return;
    uint2 dimensions = uint2(depth.get_width(), depth.get_height());
    uint2 pixel = min(((cell * 2u + 1u) * dimensions) / uint2(128u,80u), dimensions - 1u);
    float d = depth.read(pixel).r;
    float alpha = normal.read(pixel).a;
    bool geometry = reverse_z != 0u ? d > 0.0 : d < 1.0;
    uint kind = !geometry ? 2u : abs(alpha - 0.67) < 0.1 ? 1u : 0u;
    output[cell.y * 64u + cell.x] = {pixel,d,alpha,kind,uint(geometry),uint2(0)};
}
)metal";
}

struct ALMetalProbeWorkloadCapture::Impl
{
    struct Pending
    {
        std::shared_ptr<Snapshot> snapshot;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        id<MTLCommandBuffer> command = nil;
    };
    const ALMetalDevice* device = nullptr;
    std::shared_ptr<State> state;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalComputePipeline> pipeline;
    std::set<std::pair<std::uint32_t, std::uint32_t>> attempted;
    std::vector<Pending> pending;
    bool armed = false;
};

ALMetalProbeWorkloadCapture::ALMetalProbeWorkloadCapture(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalProbeWorkloadCapture::~ALMetalProbeWorkloadCapture() = default;

std::unique_ptr<ALMetalProbeWorkloadCapture>
ALMetalProbeWorkloadCapture::createIfRequested(const ALMetalDevice& device, std::string* error) noexcept
try
{
    const char* path = std::getenv(directory_environment);
    if (!path || !*path) return nullptr;
    if (path[0] != '/')
    {
        setError(error, "Probe workload directory must be an absolute existing private directory");
        return nullptr;
    }
    DirectoryDescriptor descriptor{open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
    struct stat status{};
    if (descriptor.value < 0 || fstat(descriptor.value, &status) != 0 || !S_ISDIR(status.st_mode) ||
        status.st_uid != geteuid() || (status.st_mode & 0777) != 0700)
    {
        setError(error, "Probe workload directory must be owned by this user and mode0700");
        return nullptr;
    }
    auto state = std::make_shared<State>();
    state->directory = descriptor.release();
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->state = std::move(state);
    impl->library = ALMetalShaderLibrary::compileSource(device,
        kernel_source, "Alchemy diagnostic sparse probe workload", error);
    if (!impl->library) return nullptr;
    ALMetalComputePipelineDescriptor pipeline;
    pipeline.library = impl->library.get();
    pipeline.function = "almetal_probe_workload_sample";
    pipeline.label = "Alchemy diagnostic probe workload pipeline";
    impl->pipeline = ALMetalComputePipeline::create(device, pipeline, error);
    if (!impl->pipeline) return nullptr;
    return std::unique_ptr<ALMetalProbeWorkloadCapture>(new ALMetalProbeWorkloadCapture(std::move(impl)));
}
catch (...)
{
    try { setError(error, "Probe workload initialization skipped after a diagnostic exception"); }
    catch (...) {}
    return nullptr;
}

bool ALMetalProbeWorkloadCapture::captureIfReady(
    ALMetalCommandBuffer& command, const ALMetalTexture& depth, const ALMetalTexture& normal,
    const ALMetalWorldBaseLightingConstants& lighting,
    const ALMetalWorldReflectionProbeConstants* probes,
    const ALMetalProbeWorkloadView& view, std::string* error) noexcept
try
{
    for (auto it = mImpl->pending.begin(); it != mImpl->pending.end();)
    {
        if (!it->completion->isComplete()) { ++it; continue; }
        const auto status = it->command.status;
        const bool abandoned = status == MTLCommandBufferStatusNotEnqueued ||
                               status == MTLCommandBufferStatusEnqueued;
        unsigned expected = 0;
        if (abandoned && it->snapshot->terminal.compare_exchange_strong(expected, 2))
        {
            const auto snapshot = it->snapshot;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^
            {
                try { writeSnapshot(snapshot, "cancelled", 0); }
                catch (...) { snapshot->state->fail("Probe workload cancellation writer failed"); }
            });
        }
        // Completed status can precede execution of its completion handlers.
        // Never classify that gap as cancellation or read the sample buffer.
        if (it->snapshot->terminal.load() != 0) it = mImpl->pending.erase(it);
        else ++it;
    }
    if (view.view_kind == 0) return true;
    if (ALMetalDevice::commandTimingEnabled())
    {
        mImpl->armed = true;
        return true;
    }
    if (!mImpl->armed) return true;
    {
        std::lock_guard lock(mImpl->state->mutex);
        if (!mImpl->state->asynchronous_error.empty())
        {
            if (error) *error = mImpl->state->asynchronous_error;
            mImpl->state->asynchronous_error.clear();
            return false;
        }
    }
    if (view.view_kind < 1 || view.view_kind > 3 || view.face_index >= 6 ||
        (view.view_kind == 1 && view.face_index != 0))
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        setError(error, "Probe workload capture skipped: invalid classified view identity");
        return false;
    }
    const auto key = std::pair{view.view_kind, view.face_index};
    if (!mImpl->attempted.insert(key).second) return true;
    if (!finiteMatrix(view.projection) || !finiteMatrix(lighting.inverse_projection) ||
        command.isCommitted() || command.nativeDevice() != mImpl->device->nativeHandle() ||
        depth.width() == 0 || depth.height() == 0 ||
        depth.width() != normal.width() || depth.height() != normal.height())
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        setError(error, "Probe workload capture skipped: invalid view, dimensions, projection or command");
        return false;
    }
    auto native_depth = (__bridge id<MTLTexture>)depth.nativeHandle();
    auto native_normal = (__bridge id<MTLTexture>)normal.nativeHandle();
    auto native_device = (__bridge id<MTLDevice>)mImpl->device->nativeHandle();
    if (!native_depth || !native_normal || native_depth.device != native_device ||
        native_normal.device != native_device || native_depth.sampleCount != 1 ||
        native_normal.sampleCount != 1 || native_depth.textureType != MTLTextureType2D ||
        native_normal.textureType != MTLTextureType2D ||
        (native_depth.pixelFormat != MTLPixelFormatDepth32Float &&
         native_depth.pixelFormat != MTLPixelFormatDepth32Float_Stencil8) ||
        (native_normal.pixelFormat != MTLPixelFormatRGBA16Unorm &&
         native_normal.pixelFormat != MTLPixelFormatRGB10A2Unorm) ||
        !(native_depth.usage & MTLTextureUsageShaderRead) ||
        !(native_normal.usage & MTLTextureUsageShaderRead) ||
        (probes && (!std::isfinite(probes->config[0]) || probes->config[0] < 0 ||
                    probes->config[0] > 256 || std::floor(probes->config[0]) != probes->config[0])))
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        setError(error, "Probe workload capture skipped: incompatible sampled G-buffer/probe pack");
        return false;
    }
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->state = mImpl->state;
    snapshot->view = view;
    snapshot->lighting = lighting;
    if (probes) snapshot->probes = *probes;
    snapshot->has_probes = probes != nullptr;
    snapshot->header.width = depth.width();
    snapshot->header.height = depth.height();
    snapshot->registry_id = mImpl->device->info().registry_id;
    snapshot->process = getpid();
    snapshot->depth = native_depth;
    snapshot->normal = native_normal;
    snapshot->pipeline = (__bridge id<MTLComputePipelineState>)mImpl->pipeline->nativeHandle();
    ALMetalBufferDescriptor descriptor;
    descriptor.length = sizeof(Sample) * sample_count;
    descriptor.label = "Alchemy diagnostic sparse probe workload samples";
    snapshot->samples = ALMetalBuffer::create(*mImpl->device, descriptor, error);
    if (!snapshot->samples || !command.retainUntilCompletion(snapshot, error))
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    auto encoder = ALMetalComputeEncoder::begin(command, "Alchemy diagnostic probe workload sampler", error);
    if (!encoder || !encoder->setPipeline(*mImpl->pipeline, error) ||
        !encoder->setTexture(0, &depth, error) || !encoder->setTexture(1, &normal, error) ||
        !encoder->setBuffer(0, *snapshot->samples, 0, error) ||
        !encoder->setBytes(1, &lighting.config[0], sizeof(lighting.config[0]), error) ||
        !encoder->dispatch(grid_width, grid_height, 1, 8, 8, 1, error) || !encoder->end(error))
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    auto native_command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    const auto completion = command.completionToken();
    if (!completion)
    {
        mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
        setError(error, "Probe workload completion identity was unavailable");
        return false;
    }
    mImpl->pending.push_back({snapshot, completion, native_command});
    [native_command addCompletedHandler:^(id<MTLCommandBuffer> completed)
    {
        unsigned expected = 0;
        if (!snapshot->terminal.compare_exchange_strong(expected, 1)) return;
        const bool succeeded = completed.status == MTLCommandBufferStatusCompleted;
        const std::int64_t gpu_error = completed.error ? completed.error.code : 0;
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^
        {
            try { writeSnapshot(snapshot, succeeded ? "complete" : "gpu_error", gpu_error); }
            catch (...) { snapshot->state->fail("Probe workload completion writer failed"); }
        });
    }];
    mImpl->state->scheduled.fetch_add(1, std::memory_order_relaxed);
    return true;
}
catch (...)
{
    mImpl->state->failed.fetch_add(1, std::memory_order_relaxed);
    try { setError(error, "Probe workload capture skipped after a diagnostic exception"); }
    catch (...) {}
    return false;
}

ALMetalProbeWorkloadStats ALMetalProbeWorkloadCapture::stats() const noexcept
{
    return {mImpl->state->scheduled.load(std::memory_order_relaxed),
            mImpl->state->written.load(std::memory_order_relaxed),
            mImpl->state->failed.load(std::memory_order_relaxed),
            mImpl->state->cancelled.load(std::memory_order_relaxed)};
}
