/**
 * @file almetalprobeworkload.h
 * @brief Explicitly armed, post-benchmark reflection-probe workload samples.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

class ALMetalDevice;
class ALMetalCommandBuffer;
class ALMetalTexture;
struct ALMetalWorldBaseLightingConstants;
struct ALMetalWorldReflectionProbeConstants;
struct ALMetalWorldHeroProbeConstants;

// Explicit packet versions let offline readers keep accepting archived v1
// evidence. The v2 sample retains the complete v1 prefix, including reserved0.
struct alignas(16) ALMetalProbeWorkloadSampleV1
{
    std::uint32_t x, y;
    float depth, normal_alpha;
    std::uint32_t material_class, geometry;
    std::uint32_t reserved[2];
};
struct alignas(16) ALMetalProbeWorkloadSampleV2
{
    ALMetalProbeWorkloadSampleV1 prefix;
    float glossiness, legacy_environment_intensity;
    float hero_gloss_weight, hero_legacy_weight;
    float eye_position[3];
    std::uint32_t channel_flags;
    enum : std::uint32_t
    {
        uses_gloss = 1u, uses_legacy = 2u,
        full_hero_gloss = 4u, full_hero_legacy = 8u,
    };
};
static_assert(sizeof(ALMetalProbeWorkloadSampleV1) == 32);
static_assert(sizeof(ALMetalProbeWorkloadSampleV2) == 64);
static_assert(offsetof(ALMetalProbeWorkloadSampleV2, glossiness) == 32);
static_assert(offsetof(ALMetalProbeWorkloadSampleV2, eye_position) == 48);

struct ALMetalProbeWorkloadPacketLayout
{
    std::uint32_t lighting_offset, probe_offset, hero_offset, hero_bytes;
    std::uint32_t samples_offset, sample_stride;
    static constexpr std::optional<ALMetalProbeWorkloadPacketLayout>
    forVersion(std::uint32_t version) noexcept
    {
        if (version == 1) return ALMetalProbeWorkloadPacketLayout{64,400,0,0,49616,32};
        if (version == 2) return ALMetalProbeWorkloadPacketLayout{64,400,49616,112,49728,64};
        return std::nullopt;
    }
};

struct ALMetalProbeWorkloadView
{
    std::uint64_t frame_index = 0;
    // Same identities as native benchmark provenance: main1/hero2/ordinary3.
    std::uint32_t view_kind = 0;
    std::uint32_t face_index = 0;
    float projection[16]{};
};

struct ALMetalProbeWorkloadStats
{
    std::uint64_t scheduled = 0;
    std::uint64_t written = 0;
    std::uint64_t failed = 0;
    std::uint64_t cancelled = 0;
};

// Single-CPU-owner, default-off diagnostics. No full image or scene/account
// metadata is recorded. Call only after the producing base encoder has ended
// on the same uncommitted command; this helper never commits or waits.
class ALMetalProbeWorkloadCapture final
{
public:
    static constexpr const char* directory_environment =
        "ALCHEMY_METAL_PROBE_WORKLOAD_DIRECTORY";
    static constexpr std::uint32_t sample_width = 64;
    static constexpr std::uint32_t sample_height = 40;

    // Unset/empty environment returns nullptr without allocating or compiling.
    // An explicit path must be absolute, existing, owned and mode0700. The
    // tiny diagnostic kernel compiles here; capture dispatches only after
    // command timing has armed and subsequently disarmed.
    static std::unique_ptr<ALMetalProbeWorkloadCapture> createIfRequested(
        const ALMetalDevice& device, std::string* error = nullptr) noexcept;
    ~ALMetalProbeWorkloadCapture();
    ALMetalProbeWorkloadCapture(const ALMetalProbeWorkloadCapture&) = delete;
    ALMetalProbeWorkloadCapture& operator=(const ALMetalProbeWorkloadCapture&) = delete;

    // Timing enabled only arms collection. After it becomes false, captures
    // each (kind,face) once. Kind0 is ignored. For ordinary(kind3), this is
    // the first observed probe for each face, not every ordinary probe.
    // false reports a diagnostic error only; callers
    // must keep normal rendering independent of this result.
    bool captureIfReady(
        ALMetalCommandBuffer& command,
        const ALMetalTexture& depth, const ALMetalTexture& normal,
        const ALMetalTexture& specular,
        const ALMetalWorldBaseLightingConstants& lighting,
        const ALMetalWorldReflectionProbeConstants* probes,
        const ALMetalWorldHeroProbeConstants* hero,
        const ALMetalProbeWorkloadView& view,
        std::string* error = nullptr) noexcept;
    ALMetalProbeWorkloadStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalProbeWorkloadCapture(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
