/**
 * @file almetalpipelinecache.h
 * @brief Optional app-owned native Metal pipeline binary archive.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

class ALMetalDevice;
class ALMetalRenderPipeline;
class ALMetalComputePipeline;

struct ALMetalPipelineCachePolicy
{
    bool enabled = false;
    bool read_only = false;
    std::string directory;
    std::string application_version;
    bool operator==(const ALMetalPipelineCachePolicy&) const = default;
};

struct ALMetalPipelineCacheStats
{
    bool enabled = false;
    bool read_only = false;
    bool loaded_from_disk = false;
    std::uint64_t generation = 0;
    std::uint64_t archives_loaded = 0;
    std::uint64_t render_hits = 0;
    std::uint64_t compute_hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t archive_errors = 0;
    // Ordinary fallback PSOs whose actual validation skipped archive addition.
    std::uint64_t shader_validation_bypasses = 0;
    std::uint64_t serializations = 0;
    // The two kinds are serialized independently; counts are per archive/PSO.
    std::string file_path; // Render archive.
    std::string compute_file_path;
    std::string last_error;
    std::string last_miss;
};

// Owned by one ALMetalDevice. Starts disabled and performs no cache IO until
// explicitly configured. Cache failure never prevents ordinary native PSO
// compilation and clearing it never invalidates already-created pipelines.
class ALMetalPipelineCache final
{
public:
    ~ALMetalPipelineCache();
    ALMetalPipelineCache(const ALMetalPipelineCache&) = delete;
    ALMetalPipelineCache& operator=(const ALMetalPipelineCache&) = delete;

    // Apply before constructing/reloading native pipelines. Disabled means
    // no load, source hashing, directory creation or write, including teardown.
    // application_version namespaces the caller's app/build compatibility;
    // native source-artifact, OS and device identities are included here too.
    bool configure(const ALMetalPipelineCachePolicy& policy,
                   std::string* error = nullptr);
    bool enabled() const noexcept;
    bool flush(std::string* error = nullptr);
    ALMetalPipelineCacheStats stats() const;

    // Explicit app-cache purge, including startup before any device exists.
    // Invalidates live matching owners before deleting only this helper's
    // archive/temp filename prefix. Does not touch Metal's system cache or
    // the bundled metallib. A purge and flush cannot resurrect an old archive.
    static bool clearDirectory(const std::string& directory,
                               std::string* error = nullptr);

private:
    struct Impl;
    struct Registry;
    static Registry& registry();
    explicit ALMetalPipelineCache(const ALMetalDevice& device);
    std::shared_ptr<Impl> mImpl;

    // The factory returns a retained Objective-C pipeline (+1), adopted once
    // by the typed PSO owner. Native descriptors are borrowed for this call.
    void* createRenderPipeline(void* descriptor, std::string* error);
    void* createComputePipeline(void* descriptor, std::string* error);
    friend class ALMetalDevice;
    friend class ALMetalRenderPipeline;
    friend class ALMetalComputePipeline;
};
