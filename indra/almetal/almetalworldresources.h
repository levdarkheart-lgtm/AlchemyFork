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
    std::shared_ptr<const ALMetalWorldResourceTable> prepare(
        const std::shared_ptr<const ALMetalWorldResourceSource>&,
        const ALMetalSamplerState&, std::string* error = nullptr,
        ALMetalWorldResourceFallback* failure = nullptr);
    bool bind(ALMetalRenderEncoder&, ALMetalCommandBuffer&,
        const std::shared_ptr<const ALMetalWorldResourceSource>&,
        const std::shared_ptr<const ALMetalWorldResourceTable>&,
        std::string* error = nullptr, ALMetalWorldResourceFallback* failure = nullptr);
    // Invalidates source fast hits too; does not rebuild shader encoders.
    void clearLookup() noexcept;
    void failNextPreparationForTesting() noexcept;
    ALMetalWorldResourceStats stats() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldResources(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
