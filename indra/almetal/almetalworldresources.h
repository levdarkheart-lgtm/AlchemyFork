/**
 * @file almetalworldresources.h
 * @brief Completion-owned native G-buffer resource tables.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
/** Native immutable G-buffer resource tables; owner-thread publication only. */
#pragma once
#include "almetalpipeline.h"
#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <span>

struct ALMetalWorldSimpleBatch;
class ALMetalWorldResourceTable;
class ALMetalWorldResources;
class ALMetalWorldResourcePreparation;

enum class ALMetalWorldResourceFallback : std::uint8_t
{
    none, missing_source, unmatched_source, unsupported, capacity,
    host_allocation, native_allocation, validation, shader_abi,
    pipeline_creation, retention, declaration, injected, count
};
inline const char* alMetalWorldResourceFallbackName(ALMetalWorldResourceFallback value) noexcept
{
    constexpr std::array names{"none","missing_source","unmatched_source","unsupported","capacity",
        "host_allocation","native_allocation","validation","shader_abi","pipeline_creation","retention","declaration","injected"};
    const auto index=static_cast<std::size_t>(value);
    return index<names.size()?names[index]:"invalid";
}

struct ALMetalWorldResourceLimits
{
    std::size_t maximum_tables = 2048;
    std::size_t maximum_buffer_bytes = 8 * 1024 * 1024;
};
struct ALMetalWorldResourceStats
{
    // Current exact-library/limits context (possibly shared by renderers).
    // Shader reload/configuration may replace this context. clearLookup keeps
    // its counters and shader encoders, while invalidating source fast hits.
    std::uint64_t requests = 0, cache_hits = 0, table_creates = 0;
    std::uint64_t published_bytes = 0, failures = 0, argument_encoder_creations = 0;
    // Successful wrapper binds; the encoder may elide redundant native binds.
    std::uint64_t declared_resources = 0, buffer_bind_calls = 0;
    // Renderer-local accepted G-buffer draw commands, cumulative through reload.
    // Never GPU invocations.
    std::uint64_t requested_draws = 0, bound_draws = 0, fallback_draws = 0;
    std::size_t lookup_entries = 0, live_tables = 0, live_buffer_bytes = 0;
    // Accepted-render fallback draw decisions, distinct from helper failures
    // (which may also occur in an aborted render). No identifying payloads.
    std::array<std::uint64_t,static_cast<std::size_t>(ALMetalWorldResourceFallback::count)> fallback_reasons{};
    ALMetalWorldResourceFallback last_fallback=ALMetalWorldResourceFallback::none;
    ALMetalWorldResourceFallback last_failure=ALMetalWorldResourceFallback::none;
};

// A producer record owns exact C++ texture generations as well as native
// shader views. Native-object retention alone would not enforce producer COW.
class ALMetalWorldResourceSource final
{
public:
    static std::shared_ptr<const ALMetalWorldResourceSource> publish(
        const ALMetalDevice&, ALMetalWorldSimpleBatch& mutable_prototype,
        std::span<const std::shared_ptr<const ALMetalTexture>> generation_leases,
        std::string* error = nullptr, ALMetalWorldResourceFallback* failure = nullptr);
    ~ALMetalWorldResourceSource();
    bool matches(const ALMetalWorldSimpleBatch&) const noexcept;
    // Borrowed only while this source is alive. A native FrameView copies the
    // typed leases, never this source's CPU material certificate graph.
    std::span<const std::shared_ptr<const ALMetalTexture>> generationLeases() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldResourceSource(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldResources;
};

class ALMetalWorldResourceTable final
{
public:
    ~ALMetalWorldResourceTable();
    std::shared_ptr<const ALMetalBuffer> argumentLease() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldResourceTable(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldResources;
};

// Owner-local read-only preparation proof. Holds any existing exact table
// alive; a miss reports its shader-derived encoded range without allocating
// or writing a GPU buffer. It never escapes into GPU completion ownership.
class ALMetalWorldResourcePreparation final
{
public:
    ~ALMetalWorldResourcePreparation();
    std::shared_ptr<const ALMetalWorldResourceTable> existingTable() const noexcept;
    std::size_t publicationBytes() const noexcept;
    std::size_t publicationHash() const noexcept;
    bool samePublication(const ALMetalWorldResourcePreparation&) const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldResourcePreparation(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldResources;
};

// One exact-library context. FrameData keeps weak contexts, renderers own
// current contexts, and sources keep bounded tables without owning a context.
class ALMetalWorldResources final
{
public:
    static std::shared_ptr<ALMetalWorldResources> create(
        const ALMetalDevice&, const ALMetalShaderLibrary&,
        ALMetalWorldResourceLimits = {}, std::string* error = nullptr,
        ALMetalWorldResourceFallback* failure = nullptr);
    ~ALMetalWorldResources();
    bool supported() const noexcept;
    std::shared_ptr<const ALMetalWorldResourcePreparation> preflight(
        const std::shared_ptr<const ALMetalWorldResourceSource>&,
        const ALMetalSamplerState&, std::string* error=nullptr,
        ALMetalWorldResourceFallback* failure=nullptr) const;
    bool validatePreparations(std::span<const std::shared_ptr<const ALMetalWorldResourcePreparation>>,
        std::string* error=nullptr) const;
    std::shared_ptr<const ALMetalWorldResourceTable> prepare(
        const ALMetalWorldResourcePreparation&, std::string* error=nullptr,
        ALMetalWorldResourceFallback* failure=nullptr);
    std::shared_ptr<const ALMetalWorldResourceTable> prepare(
        const std::shared_ptr<const ALMetalWorldResourceSource>&,
        const ALMetalSamplerState&, std::string* error = nullptr,
        ALMetalWorldResourceFallback* failure = nullptr);
    bool bind(ALMetalRenderEncoder&, ALMetalCommandBuffer&,
        const std::shared_ptr<const ALMetalWorldResourceSource>&,
        const std::shared_ptr<const ALMetalWorldResourceTable>&,
        std::string* error = nullptr, ALMetalWorldResourceFallback* failure = nullptr);
    // Native-only table ownership/declaration, without a fragment slot bind or
    // Source retention. Caller independently pins the exact C++ generations.
    bool declareResources(ALMetalRenderEncoder&, ALMetalCommandBuffer&,
        const std::shared_ptr<const ALMetalWorldResourceTable>&,
        std::string* error = nullptr, ALMetalWorldResourceFallback* failure = nullptr);
    // Drop this context's publisher table cache for an evicted source. Real
    // FrameView/command table leases survive independently. Owner-only/noalloc.
    void releaseSourceCache(const std::shared_ptr<const ALMetalWorldResourceSource>&) noexcept;
    // Invalidates source fast hits too; does not rebuild shader encoders.
    void clearLookup() noexcept;
    void failNextPreparationForTesting() noexcept;
    ALMetalWorldResourceStats stats() const noexcept;
private:
    std::shared_ptr<const ALMetalWorldResourceTable> cachedSourceTable(
        const std::shared_ptr<const ALMetalWorldResourceSource>&, const ALMetalSamplerState&) const noexcept;
    struct Impl;
    explicit ALMetalWorldResources(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
