/**
 * @file almetalpipelinecache.mm
 * @brief Optional, corruption-resilient Metal binary archive ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "almetalpipelinecache.h"
#include "almetaldevice.h"
#include "almetalpipeline.h"
#import <Metal/Metal.h>
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace
{
namespace fs = std::filesystem;
constexpr char prefix[] = "alchemy-metal-pipeline-v1-";
constexpr std::uintmax_t maximum_archive_bytes = 256ULL * 1024 * 1024;

void setError(std::string* error, const std::string& message)
{
    if (error) *error = message;
}
std::string text(NSString* value)
{
    const char* bytes = value.UTF8String;
    return bytes ? bytes : "";
}
NSString* string(const std::string& value)
{
    return [[NSString alloc] initWithBytes:value.data() length:value.size()
                                  encoding:NSUTF8StringEncoding];
}
std::string describe(NSError* error)
{
    return error ? text(error.localizedDescription) : "Native pipeline operation failed";
}
std::string digest(const std::string& value)
{
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(value.data(), static_cast<CC_LONG>(value.size()), bytes);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(sizeof(bytes) * 2);
    for (unsigned char byte : bytes)
    {
        result.push_back(hex[byte >> 4]);
        result.push_back(hex[byte & 15]);
    }
    return result;
}
bool normalizeDirectory(const std::string& value, std::string& normalized,
                        std::string* error)
{
    if (value.empty() || value.find('\0') != std::string::npos || !string(value))
    {
        setError(error, "Native shader cache directory is empty or invalid");
        return false;
    }
    fs::path path(value);
    for (const auto& part : path)
        if (part == "." || part == "..")
        {
            setError(error, "Native shader cache directory contains relative components");
            return false;
        }
    path = path.lexically_normal();
    if (path.filename().empty()) path = path.parent_path();
    if (!path.is_absolute() || path == path.root_path() ||
        path.parent_path() == path.root_path())
    {
        setError(error, "Native shader cache requires an absolute, non-root cache directory");
        return false;
    }
    // Resolve real directory aliases (including Darwin /tmp -> /private/tmp)
    // before matching live owners. Reject dangling symlink prefixes rather
    // than accepting a spelling whose physical destination is unknowable.
    std::error_code ec;
    fs::path component;
    for (const auto& part : path)
    {
        component /= part;
        const auto status = fs::symlink_status(component, ec);
        if (ec == std::errc::no_such_file_or_directory) { ec.clear(); break; }
        if (ec || (fs::is_symlink(status) && !fs::exists(component, ec)))
        {
            setError(error, "Native shader cache directory has an unresolved filesystem alias");
            return false;
        }
    }
    path = fs::weakly_canonical(path, ec);
    if (ec || path == path.root_path() || path.parent_path() == path.root_path())
    {
        setError(error, "Native shader cache directory cannot resolve to a root directory");
        return false;
    }
    normalized = path.string();
    return true;
}
bool ownedFilename(const std::string& name)
{
    return name.starts_with(prefix) &&
        (name.ends_with(".metalarc") || name.find(".metalarc.tmp-") != std::string::npos);
}
}

struct ALMetalPipelineCache::Impl
{
    id<MTLDevice> device;
    std::uint64_t registry_id = 0;
    std::uint32_t apple_family = 0, mac_family = 0;
    std::atomic<bool> active{false};
    mutable std::mutex mutex;
    ALMetalPipelineCachePolicy policy;
    ALMetalPipelineCacheStats statistics;
    std::string directory;
    std::string compatibility;
    struct Archive
    {
        id<MTLBinaryArchive> object = nil;
        bool dirty = false;
        NSMutableArray* pending = nil;
        std::string file_path;
    };
    // Keep pipeline kinds separate: extending a loaded mixed render/compute
    // archive fails native stage packing on the measured M1 Max driver.
    std::array<Archive, 2> archives;

    void recordError(const std::string& message)
    {
        ++statistics.archive_errors;
        statistics.last_error = message;
    }
    void invalidate()
    {
        active.store(false, std::memory_order_release);
        for (auto& archive : archives)
        {
            archive.object = nil;
            archive.dirty = false;
            archive.pending = nil;
        }
        statistics.loaded_from_disk = false;
        ++statistics.generation;
    }

    template<bool Compute>
    static bool instrumented(id state)
    {
        if (@available(macOS 15.0, *))
        {
            if constexpr (Compute)
                return ((id<MTLComputePipelineState>)state).shaderValidation == MTLShaderValidationEnabled;
            else
                return ((id<MTLRenderPipelineState>)state).shaderValidation == MTLShaderValidationEnabled;
        }
        return false;
    }

    // A failed serialize can consume Metal's pending data (observed "Nothing
    // to serialize" on retry). Retain only descriptors added since the last
    // durable save, so a fresh archive can replay that exact incremental set.
    bool restoreDirtyArchive(Archive& archive, bool compute)
    {
        MTLBinaryArchiveDescriptor* descriptor = [[MTLBinaryArchiveDescriptor alloc] init];
        std::error_code ec;
        const auto status = fs::symlink_status(archive.file_path, ec);
        if (!ec && fs::is_regular_file(status))
        {
            const auto bytes = fs::file_size(archive.file_path, ec);
            if (!ec && bytes > 0 && bytes <= maximum_archive_bytes)
                descriptor.url = [NSURL fileURLWithPath:string(archive.file_path)];
        }
        NSError* native_error = nil;
        id<MTLBinaryArchive> restored = [device newBinaryArchiveWithDescriptor:descriptor error:&native_error];
        if (!restored)
        {
            descriptor.url = nil;
            restored = [device newBinaryArchiveWithDescriptor:descriptor error:&native_error];
        }
        if (!restored) return false;
        for (id entry in archive.pending)
            if (compute ? ![restored addComputePipelineFunctionsWithDescriptor:entry error:&native_error]
                        : ![restored addRenderPipelineFunctionsWithDescriptor:entry error:&native_error])
                return false;
        archive.object = restored;
        return true;
    }

    template<bool Compute, class Descriptor>
    id build(Descriptor* descriptor, std::string* error)
    {
        NSError* native_error = nil;
        id state = nil;
        auto& entry = archives[Compute ? 1 : 0];
        id<MTLBinaryArchive> archive = entry.object;
        if (active.load(std::memory_order_relaxed) && archive)
        {
            descriptor.binaryArchives = @[archive];
            // Success here proves this app archive supplied the complete PSO;
            // Metal's separate system cache cannot turn an archive miss into a hit.
            if constexpr (Compute)
                state = [device newComputePipelineStateWithDescriptor:descriptor
                    options:MTLPipelineOptionFailOnBinaryArchiveMiss reflection:nil error:&native_error];
            else
                state = [device newRenderPipelineStateWithDescriptor:descriptor
                    options:MTLPipelineOptionFailOnBinaryArchiveMiss reflection:nil error:&native_error];
            if (state)
            {
                if constexpr (Compute) ++statistics.compute_hits;
                else ++statistics.render_hits;
                return state;
            }
            ++statistics.misses;
            statistics.last_miss = describe(native_error);
        }
        // The fallback is the ordinary native factory, preserving the actual
        // validation selection. Its successful state owns the real executable.
        descriptor.binaryArchives = nil;
        native_error = nil;
        if constexpr (Compute)
            state = [device newComputePipelineStateWithFunction:descriptor.computeFunction error:&native_error];
        else
            state = [device newRenderPipelineStateWithDescriptor:descriptor error:&native_error];
        if (!state)
        {
            setError(error, describe(native_error));
            return nil;
        }
        if (instrumented<Compute>(state))
        {
            ++statistics.shader_validation_bypasses;
            return state;
        }
        if (active.load(std::memory_order_relaxed) && archive && !policy.read_only)
        {
            BOOL added;
            if constexpr (Compute)
                added = [archive addComputePipelineFunctionsWithDescriptor:descriptor error:&native_error];
            else
                added = [archive addRenderPipelineFunctionsWithDescriptor:descriptor error:&native_error];
            if (!added)
            {
                recordError(describe(native_error));
                invalidate();
                return state;
            }
            // Optional cache bookkeeping must not discard an already valid PSO.
            try
            {
                @try
                {
                    Descriptor* copy = [descriptor copy];
                    copy.binaryArchives = nil;
                    if (!copy)
                    {
                        invalidate();
                        recordError("Could not retain the native cache descriptor");
                        return state;
                    }
                    if (!entry.pending) entry.pending = [[NSMutableArray alloc] init];
                    if (!entry.pending)
                    {
                        invalidate();
                        recordError("Could not retain native cache descriptors");
                        return state;
                    }
                    [entry.pending addObject:copy];
                    entry.dirty = true;
                }
                @catch (NSException* exception)
                {
                    invalidate();
                    recordError(text(exception.reason));
                }
            }
            catch (const std::bad_alloc&)
            {
                invalidate();
                ++statistics.archive_errors;
                statistics.last_error.clear();
            }
        }
        return state;
    }
};

struct ALMetalPipelineCache::Registry
{
    // PSO creation/flush hold a shared purge lease; clear holds it exclusively
    // through invalidation and deletion. There is no lock on the draw path.
    std::shared_mutex purge;
    std::vector<std::weak_ptr<Impl>> owners;
};

ALMetalPipelineCache::Registry& ALMetalPipelineCache::registry()
{
    static Registry value;
    return value;
}

ALMetalPipelineCache::ALMetalPipelineCache(const ALMetalDevice& device)
: mImpl(std::make_shared<Impl>())
{
    mImpl->device = (__bridge id<MTLDevice>)device.nativeHandle();
    mImpl->registry_id = device.info().registry_id;
    mImpl->apple_family = device.info().highest_apple_family;
    mImpl->mac_family = device.info().highest_mac_family;
    auto& all = registry();
    std::unique_lock lock(all.purge);
    std::erase_if(all.owners, [](const auto& owner) { return owner.expired(); });
    all.owners.push_back(mImpl);
}

ALMetalPipelineCache::~ALMetalPipelineCache()
{
    flush(); // Optional best effort; explicit shutdown flush reports any error.
}

bool ALMetalPipelineCache::enabled() const noexcept
{
    return mImpl->active.load(std::memory_order_acquire);
}

bool ALMetalPipelineCache::configure(const ALMetalPipelineCachePolicy& policy,
                                    std::string* error)
{
    @autoreleasepool
    {
        auto& all = registry();
        std::shared_lock purge(all.purge);
        std::lock_guard lock(mImpl->mutex);
        if (!policy.enabled)
        {
            mImpl->invalidate();
            mImpl->policy = policy;
            mImpl->directory.clear();
            mImpl->statistics.file_path.clear();
            mImpl->statistics.compute_file_path.clear();
            return true;
        }
        std::string directory;
        if (!normalizeDirectory(policy.directory, directory, error) ||
            policy.application_version.empty() || policy.application_version.size() > 4096)
        {
            mImpl->invalidate();
            setError(error, "Native shader cache requires a valid directory and app/build version");
            return false;
        }
        const std::string source = ALMetalShaderLibrary::builtinCacheIdentity(error);
        if (source.empty())
        {
            mImpl->invalidate();
            return false;
        }
        // Shader validation changes generated programs on the measured host.
        // Keep opaque launch requests in compatibility, without interpreting
        // overrides or changing actual instrumentation. Separate pruning
        // namespaces preserve the useful default archive during diagnostics.
        std::string validation;
        bool validation_configured = false;
        for (const char* name : {"MTL_SHADER_VALIDATION", "MTL_SHADER_VALIDATION_DEFAULT_STATE", "MTL_SHADER_VALIDATION_ENABLE_PIPELINES",
                                 "MTL_SHADER_VALIDATION_DISABLE_PIPELINES"})
        {
            const char* value = std::getenv(name);
            validation_configured = validation_configured || value != nullptr;
            validation += name;
            validation += value ? "=present:" : "=unset:";
            if (value) validation += value;
            validation += '\n';
        }
        const std::string compatibility = digest(
            policy.application_version + "\n" + source + "\n" + validation +
            text([NSProcessInfo processInfo].operatingSystemVersionString) + "\n" +
            std::to_string(mImpl->registry_id) + ":" +
            std::to_string(mImpl->apple_family) + ":" + std::to_string(mImpl->mac_family));
        if (mImpl->policy == policy && mImpl->directory == directory &&
            mImpl->compatibility == compatibility && enabled())
            return true;
        mImpl->invalidate();
        mImpl->policy = policy;
        mImpl->directory = directory;
        mImpl->compatibility = compatibility;
        const std::string device_prefix = std::string(prefix) + std::to_string(mImpl->registry_id) +
            (validation_configured ? "-validation-" : "-default-");
        const std::array<fs::path,2> paths = {
            fs::path(directory) / (device_prefix + compatibility + "-render.metalarc"),
            fs::path(directory) / (device_prefix + compatibility + "-compute.metalarc")};
        mImpl->statistics.file_path = paths[0].string();
        mImpl->statistics.compute_file_path = paths[1].string();
        mImpl->statistics.last_error.clear();
        std::error_code ec;
        if (!policy.read_only && fs::is_directory(directory, ec))
        {
            // Retain one namespace per physical device; secondary instances
            // use a read-only policy and never prune another version's files.
            fs::directory_iterator entries(directory, ec), end;
            for (; !ec && entries != end; entries.increment(ec))
            {
                const auto& entry = *entries;
                const auto name = entry.path().filename().string();
                std::error_code entry_error;
                if (name.starts_with(device_prefix) && ownedFilename(name) &&
                    entry.path() != paths[0] && entry.path() != paths[1] &&
                    fs::is_regular_file(entry.symlink_status(entry_error)))
                    fs::remove(entry.path(), entry_error);
            }
            if (ec) mImpl->recordError(ec.message());
        }
        for (std::size_t i = 0; i < paths.size(); ++i)
        {
            auto& archive = mImpl->archives[i];
            archive.file_path = paths[i].string();
            MTLBinaryArchiveDescriptor* descriptor = [[MTLBinaryArchiveDescriptor alloc] init];
            ec.clear();
            const auto status = fs::symlink_status(paths[i], ec);
            if (!ec && fs::exists(status))
            {
                const auto bytes = fs::is_regular_file(status) ? fs::file_size(paths[i], ec) : 0;
                if (!ec && bytes > 0 && bytes <= maximum_archive_bytes)
                    descriptor.url = [NSURL fileURLWithPath:string(archive.file_path)];
                else mImpl->recordError("Native shader archive is not a bounded regular file");
            }
            NSError* native_error = nil;
            archive.object = [mImpl->device newBinaryArchiveWithDescriptor:descriptor error:&native_error];
            if (archive.object && descriptor.url)
            {
                mImpl->statistics.loaded_from_disk = true;
                ++mImpl->statistics.archives_loaded;
            }
            if (!archive.object)
            {
                mImpl->recordError(describe(native_error));
                descriptor.url = nil;
                native_error = nil;
                archive.object = [mImpl->device newBinaryArchiveWithDescriptor:descriptor error:&native_error];
            }
            if (!archive.object)
            {
                mImpl->recordError(describe(native_error));
                mImpl->invalidate();
                setError(error, mImpl->statistics.last_error);
                return false;
            }
            archive.object.label = i ? @"Alchemy application compute archive" : @"Alchemy application render archive";
        }
        mImpl->active.store(true, std::memory_order_release);
        return true;
    }
}

bool ALMetalPipelineCache::flush(std::string* error)
{
    @autoreleasepool
    {
        auto& all = registry();
        std::shared_lock purge(all.purge);
        std::lock_guard lock(mImpl->mutex);
        if (!enabled() || mImpl->policy.read_only ||
            std::none_of(mImpl->archives.begin(), mImpl->archives.end(),
                [](const auto& archive) { return archive.dirty && archive.object; }))
            return true;
        std::error_code ec;
        fs::create_directories(mImpl->directory, ec);
        if (ec)
        {
            mImpl->recordError(ec.message());
            setError(error, mImpl->statistics.last_error);
            return false;
        }
        for (std::size_t i = 0; i < mImpl->archives.size(); ++i)
        {
            auto& archive = mImpl->archives[i];
            if (!archive.dirty || !archive.object) continue;
            const fs::path path(archive.file_path);
            const fs::path temporary(path.string() + ".tmp-" + text([NSUUID UUID].UUIDString));
            NSError* native_error = nil;
            bool saved = [archive.object serializeToURL:[NSURL fileURLWithPath:string(temporary.string())]
                                                 error:&native_error];
            if (saved)
            {
                const auto bytes = fs::file_size(temporary, ec);
                saved = !ec && bytes > 0 && bytes <= maximum_archive_bytes;
                if (saved)
                {
                    fs::rename(temporary, path, ec);
                    saved = !ec;
                }
            }
            if (!saved)
            {
                mImpl->recordError(ec ? ec.message() : native_error ? describe(native_error)
                    : "Native shader archive exceeded its bounded file size");
                const std::string failure = mImpl->statistics.last_error;
                fs::remove(temporary, ec);
                if (!mImpl->restoreDirtyArchive(archive, i == 1)) mImpl->invalidate();
                setError(error, failure);
                return false;
            }
            archive.dirty = false;
            archive.pending = nil;
            ++mImpl->statistics.serializations;
        }
        return true;
    }
}

ALMetalPipelineCacheStats ALMetalPipelineCache::stats() const
{
    std::lock_guard lock(mImpl->mutex);
    auto result = mImpl->statistics;
    result.enabled = enabled();
    result.read_only = mImpl->policy.read_only;
    return result;
}

bool ALMetalPipelineCache::clearDirectory(const std::string& directory, std::string* error)
{
    @autoreleasepool
    {
        std::string normalized;
        if (!normalizeDirectory(directory, normalized, error)) return false;
        auto& all = registry();
        std::unique_lock purge(all.purge);
        for (const auto& weak : all.owners)
            if (auto owner = weak.lock())
            {
                std::lock_guard lock(owner->mutex);
                std::error_code same_error;
                if (owner->directory == normalized ||
                    (!owner->directory.empty() && fs::equivalent(owner->directory, normalized, same_error)))
                    owner->invalidate();
            }
        std::error_code ec;
        if (!fs::exists(normalized, ec))
        {
            if (ec) setError(error, ec.message());
            return !ec;
        }
        fs::directory_iterator entries(normalized, ec), end;
        if (ec)
        {
            setError(error, ec.message());
            return false;
        }
        for (; !ec && entries != end; entries.increment(ec))
        {
            const auto& entry = *entries;
            if (ownedFilename(entry.path().filename().string()) &&
                fs::is_regular_file(entry.symlink_status(ec)))
            {
                fs::remove(entry.path(), ec);
                if (ec)
                {
                    setError(error, ec.message());
                    return false;
                }
            }
        }
        if (ec)
        {
            setError(error, ec.message());
            return false;
        }
        return true;
    }
}

void* ALMetalPipelineCache::createRenderPipeline(void* descriptor, std::string* error)
{
    auto& all = registry();
    std::shared_lock purge(all.purge);
    std::lock_guard lock(mImpl->mutex);
    id state = mImpl->build<false>((__bridge MTLRenderPipelineDescriptor*)descriptor, error);
    return (__bridge_retained void*)state;
}

void* ALMetalPipelineCache::createComputePipeline(void* descriptor, std::string* error)
{
    auto& all = registry();
    std::shared_lock purge(all.purge);
    std::lock_guard lock(mImpl->mutex);
    id state = mImpl->build<true>((__bridge MTLComputePipelineDescriptor*)descriptor, error);
    return (__bridge_retained void*)state;
}
