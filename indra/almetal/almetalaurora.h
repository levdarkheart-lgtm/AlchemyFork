/**
 * @file almetalaurora.h
 * @brief Experimental native Metal execution of Aurora's pinned WGSL FSR 3.1.
 * Copyright (C) 2026, Alchemy Viewer Project
 * LGPL-2.1-only; translated algorithm retains upstream MIT/GPL notices.
 */
#pragma once
#include "almetalresource.h"
#include <array>
#include <memory>
#include <string>

class ALMetalWorldFrameData;
struct ALMetalAuroraDescriptor
{
    std::uint32_t input_width = 0, input_height = 0;
    std::uint32_t output_width = 0, output_height = 0;
    bool          operator==(const ALMetalAuroraDescriptor&) const = default;
};
struct ALMetalAuroraInputs
{
    const ALMetalTexture* color  = nullptr; // final linear HDR RGBA16F; alpha is glow
    const ALMetalTexture* opaque = nullptr; // genuine pre-transparency HDR
    const ALMetalTexture* depth  = nullptr; // reverse-Z R32F or Depth32F
    const ALMetalTexture* motion = nullptr; // RG16F previous-current input pixels
    // Optional producer confidence mask: zero means the visible contributions have
    // valid content/pose history and a representable layered motion. Positive
    // means history is unsafe (animation, invalid pose or mixed-layer motion).
    // Transparency alone is NOT unsafe. This is not an upstream composition map
    // or opacity: the adapter explicitly maps unsafe history to Aurora's binary
    // composition marker, separately from its opaque/final transparency reactivity.
    const ALMetalTexture* history_unsafe = nullptr;
    const ALMetalTexture* output      = nullptr; // RGBA16F output; RGB reconstructed, glow resolved
};
struct ALMetalAuroraFrame
{
    // Column-major current unjittered clip -> previous unjittered clip.
    std::array<float, 16> motion_reprojection{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    // Actual Metal [0,1] reverse-Z perspective projection (not an invented near plane).
    std::array<float, 16> projection{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float                 jitter_x = 0.f, jitter_y = 0.f; // raster image displacement, input pixels, Y down
    std::uint32_t         jitter_phase_count = 0;         // zero derives pinned Aurora width-based phase count
    bool                  reset              = false;
};
struct ALMetalAuroraStats
{
    std::uint64_t preparations = 0, creation_successes = 0, prepare_reuses = 0, encoded_calls = 0;
    std::uint64_t submitted_calls = 0, abandoned_calls = 0, reset_calls = 0, gpu_failures = 0;
    std::uint64_t texture_bytes = 0, buffer_bytes = 0;
    std::uint32_t pending_submissions = 0, parity = 0;
    bool          history_valid = false;
};

// Read-only observation of the last successfully encoded algorithm graph.
// Textures are borrowed and mutable: enqueue any ordered diagnostic copy before
// another encode, incompatible prepare, invalidate, abandon or destruction.
// The completion token identifies the producer; it does not freeze these
// textures or imply submission/success. Reading performs no work and never waits.
struct ALMetalAuroraDiagnosticSnapshot
{
    ALMetalAuroraDescriptor descriptor;
    const ALMetalTexture* generated_reactive = nullptr;     // R8, input size
    const ALMetalTexture* dilated_motion = nullptr;         // RG16F, input size
    const ALMetalTexture* shading_change = nullptr;         // R8, half input size
    const ALMetalTexture* masks = nullptr;                  // RGBA8, input size
    const ALMetalTexture* accumulation = nullptr;           // R8, encoded write parity
    const ALMetalTexture* previous_accumulation = nullptr;  // R8, encoded read parity
    const ALMetalTexture* luma_instability = nullptr;       // R16F, input size
    const ALMetalTexture* luma_history = nullptr;           // RGBA16F, encoded write parity
    const ALMetalTexture* internal_history = nullptr;       // RGBA16F, output size/write parity
    const ALMetalTexture* immediate_output = nullptr;       // RGBA16F, output size
    std::shared_ptr<const ALMetalCommandCompletion> completion;
    std::uint64_t ticket = 0;
    float frame_index = 0.f;
    std::uint32_t write_parity = 0, read_parity = 0, jitter_phase_count = 0;
    float jitter[2]{}, previous_jitter[2]{};
    bool reset = true;
    bool submitted = false;
};

// One serialized main-world stream. Same native queue orders GPU history writes.
// Encoding does NOT advance parity/frame/jitter; submitted() is the commit point.
class ALMetalAuroraReconstructor final
{
public:
    using Ticket = std::uint64_t;
    static std::unique_ptr<ALMetalAuroraReconstructor> create(const ALMetalDevice&, std::string* error = nullptr);
    ~ALMetalAuroraReconstructor();
    ALMetalAuroraReconstructor(const ALMetalAuroraReconstructor&)            = delete;
    ALMetalAuroraReconstructor& operator=(const ALMetalAuroraReconstructor&) = delete;
    bool                        prepare(const ALMetalAuroraDescriptor&, std::string* error = nullptr);
    bool encode(ALMetalCommandBuffer&, const ALMetalAuroraDescriptor&, const ALMetalAuroraInputs&, const ALMetalAuroraFrame&, Ticket&,
                std::string* error = nullptr);
    bool submitted(Ticket, std::string* error = nullptr);
    void abandon(Ticket) noexcept;
    void invalidate() noexcept;
    void setFrameData(std::shared_ptr<ALMetalWorldFrameData>);
    const ALMetalTexture* rawOutput() const noexcept;
    const ALMetalTexture* displayDepth() const noexcept;
    const ALMetalTexture* generatedReactive() const noexcept;
    ALMetalAuroraDiagnosticSnapshot diagnostics() const noexcept;
    ALMetalAuroraStats    stats() const noexcept;
    static const char*    algorithmIdentity() noexcept;

private:
    struct Impl;
    explicit ALMetalAuroraReconstructor(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
