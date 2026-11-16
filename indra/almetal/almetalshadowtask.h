/**
 * @file almetalshadowtask.h
 * @brief One owned shadow-view encoding job; submission stays on its caller.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalshadowcapture.h"
#include "almetalshadowindirect.h"
#include "../llcommon/alnativeframebudget.h"

// Complete-path costs reject dispatch at 256 and show useful overlap at 1158.
// This conservative native-record boundary still requires its own 1024 cost
// cell; it is not a logical-source threshold or a viewer FPS guarantee.
inline constexpr std::size_t alMetalShadowViewMinimumBatches = 1024;
enum class ALMetalShadowViewWorkEligibility : std::uint8_t { eligible, below_work };
constexpr ALMetalShadowViewWorkEligibility alMetalShadowViewWorkEligibility(
    std::size_t native_batch_count) noexcept
{
    return native_batch_count >= alMetalShadowViewMinimumBatches
        ? ALMetalShadowViewWorkEligibility::eligible
        : ALMetalShadowViewWorkEligibility::below_work;
}
// This is the production capture/encode overlap boundary, not a work queue.
// Owner methods are sequential. Only run() sees the packet/private encoder;
// no query pool, ticket, viewer pointer or owner callback enters that worker.
class ALMetalShadowViewTask final
{
public:
    struct Outcome
    {
        bool encoded = false, submitted = false;
        std::uint64_t encode_ns = 0;
        std::uint64_t encode_started_ns = 0, encode_finished_ns = 0;
        std::uint64_t retained_cpu_bytes = 0;
        bool worker_is_owner = false;
        std::shared_ptr<ALMetalVisibilityResultBatch> visibility_results;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        std::unique_ptr<ALMetalCapturedShadowView> packet;
    };
    explicit ALMetalShadowViewTask(std::unique_ptr<ALMetalShadowEncodingContext> context);
    ~ALMetalShadowViewTask();
    ALMetalShadowViewTask(const ALMetalShadowViewTask&) = delete;
    ALMetalShadowViewTask& operator=(const ALMetalShadowViewTask&) = delete;
    bool active() const noexcept;
    // Nonblocking Dispatch completion observation; no mutable worker fields
    // are polled. Completed work still owns its command, packet and CPU debt.
    bool encodingComplete() const noexcept;
    enum class Decline { none, unavailable, budget, native_group, adoption };
    Decline lastDecline() const noexcept;
    std::uint64_t shaderGeneration() const noexcept;
    std::uint32_t anisotropy() const noexcept;
    // Owner-only diagnostics never inspect mutable state during encoding.
    ALMetalShadowIndirectStats shadowIndirectStats() const noexcept;

    // Declines without consuming the packet or original command. Successful
    // adoption preserves its completion, admission and settlement observers.
    bool start(ALMetalWorldGBufferRenderer& owner, std::uint64_t frame,
               std::unique_ptr<ALMetalCapturedShadowView>& packet,
               std::shared_ptr<ALMetalTexture> target, std::uint32_t slice,
               ALNativeFrameBudget* budget = nullptr, std::string* error = nullptr);
    // Owner joins then commits/cancels. CPU array capacity can recycle now;
    // native resource generations remain independently completion-owned. The
    // preparation reservation follows the returned packet until its CPU arrays
    // are recycled/restored or it is destroyed, including Outcome/packet moves.
    Outcome finish(bool submit = true, std::string* error = nullptr) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
