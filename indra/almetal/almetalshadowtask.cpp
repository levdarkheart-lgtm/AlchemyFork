/**
 * @file almetalshadowtask.cpp
 * @brief One owned shadow-view encoding job with owner-side submission.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalshadowtask.h"
#include "almetaldiagnostictelemetry.h"
#include <dispatch/dispatch.h>
#include <chrono>
#include <thread>

struct ALMetalShadowViewTask::Impl
{
    struct Job
    {
        ALMetalShadowEncodingContext* context = nullptr;
        std::unique_ptr<ALMetalCapturedShadowView> packet;
        std::shared_ptr<ALMetalTexture> target;
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        std::uint32_t slice = 0;
        bool encoded = false;
        std::uint64_t encode_ns = 0;
        std::uint64_t encode_started_ns = 0, encode_finished_ns = 0;
        std::uint64_t retained_cpu_bytes = 0;
        std::thread::id owner_thread;
        ALMetalDiagnosticTelemetry::FrameTag diagnostic_frame;
        bool worker_is_owner = false;
        std::string error;
    };
    static void run(void* opaque) noexcept
    {
        auto& job = *static_cast<Job*>(opaque);
        ALMetalDiagnosticFrameScope diagnostic_scope(job.diagnostic_frame);
        const auto started = std::chrono::steady_clock::now();
        job.encode_started_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(started.time_since_epoch()).count();
        job.worker_is_owner = std::this_thread::get_id() == job.owner_thread;
        try
        {
            job.encoded = job.context->encode(*job.target, job.slice,
                job.packet->scene(), &job.error, &job.results);
        }
        catch (...) { job.encoded = false; }
        job.encode_finished_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        job.encode_ns = job.encode_finished_ns - job.encode_started_ns;
    }
    std::unique_ptr<ALMetalShadowEncodingContext> mContext;
    std::unique_ptr<Job> mJob;
    dispatch_group_t mGroup = nullptr;
    Decline mDecline = Decline::none;
};

ALMetalShadowViewTask::ALMetalShadowViewTask(std::unique_ptr<ALMetalShadowEncodingContext> context)
    : mImpl(std::make_unique<Impl>()) { mImpl->mContext = std::move(context); }
ALMetalShadowViewTask::~ALMetalShadowViewTask() { finish(false); }
bool ALMetalShadowViewTask::active() const noexcept { return bool(mImpl->mJob); }
bool ALMetalShadowViewTask::encodingComplete() const noexcept
{
    return mImpl->mJob && dispatch_group_wait(mImpl->mGroup, DISPATCH_TIME_NOW) == 0;
}
ALMetalShadowViewTask::Decline ALMetalShadowViewTask::lastDecline() const noexcept { return mImpl->mDecline; }
std::uint64_t ALMetalShadowViewTask::shaderGeneration() const noexcept
{ return mImpl->mContext ? mImpl->mContext->shaderGeneration() : 0; }
std::uint32_t ALMetalShadowViewTask::anisotropy() const noexcept
{ return mImpl->mContext ? mImpl->mContext->anisotropy() : 1; }
ALMetalShadowIndirectStats ALMetalShadowViewTask::shadowIndirectStats() const noexcept
{ return active() || !mImpl->mContext ? ALMetalShadowIndirectStats{} : mImpl->mContext->shadowIndirectStats(); }

bool ALMetalShadowViewTask::start(ALMetalWorldGBufferRenderer& owner, std::uint64_t frame,
           std::unique_ptr<ALMetalCapturedShadowView>& packet,
           std::shared_ptr<ALMetalTexture> target, std::uint32_t slice,
           ALNativeFrameBudget* budget, std::string* error)
{
    mImpl->mDecline = Decline::unavailable;
    if (active() || !mImpl->mContext || !packet || !target || packet->mCPUReservation) return false;
    auto job = std::make_unique<Impl::Job>();
    ALNativeFrameBudget::Lease credit;
    const auto packet_bytes = packet->ownedCPUBytes();
    constexpr auto unknown = ALNativeFrameBudget::unknown_bytes;
    job->retained_cpu_bytes = packet_bytes > unknown - sizeof(Impl::Job)
        ? unknown : packet_bytes + sizeof(Impl::Job);
    if (budget)
    {
        credit = budget->tryAcquire(ALNativeFrameBudget::Work::preparation,
                                    job->retained_cpu_bytes).credit;
        if (!credit) { mImpl->mDecline = Decline::budget; return false; }
    }
    auto group = dispatch_group_create();
    if (!group) { mImpl->mDecline = Decline::native_group; return false; }
    if (!packet->retainTarget(target)) { dispatch_release(group); return false; }
    try
    {
        if (!mImpl->mContext->adopt(owner, frame, error))
        { mImpl->mDecline = Decline::adoption; dispatch_release(group); return false; }
    }
    catch (...) { dispatch_release(group); throw; }
    // No fallible work remains after adoption. Keep the combined job/byte
    // reservation with CPU storage through completion, Outcome moves and
    // caller retirement; GPU resource owners deliberately never retain it.
    packet->mCPUReservation = std::move(credit);
    job->context = mImpl->mContext.get();
    job->owner_thread = std::this_thread::get_id();
    if (ALMetalDiagnosticTelemetry::enabled())
        job->diagnostic_frame = {frame, ALMetalDiagnosticTelemetry::ViewKind::shadow,
            static_cast<std::int32_t>(slice)};
    job->packet = std::move(packet); job->target = std::move(target); job->slice = slice;
    mImpl->mJob = std::move(job); mImpl->mGroup = group;
    mImpl->mDecline = Decline::none;
    dispatch_group_async_f(mImpl->mGroup, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                           mImpl->mJob.get(), &Impl::run);
    return true;
}

ALMetalShadowViewTask::Outcome ALMetalShadowViewTask::finish(bool submit, std::string* error) noexcept
{
    Outcome outcome;
    if (!mImpl->mJob) return outcome;
    const auto diagnostic_start = ALMetalDiagnosticTelemetry::enabled()
        ? ALMetalDiagnosticTelemetry::machTimeNs() : 0;
    dispatch_group_wait(mImpl->mGroup, DISPATCH_TIME_FOREVER);
    if (diagnostic_start)
        ALMetalDiagnosticTelemetry::recordMetrics(mImpl->mJob->diagnostic_frame,
            "cpu_shadow_encoding_join", {diagnostic_start,
                ALMetalDiagnosticTelemetry::machTimeNs(), 0, 0, 0, 0, 0, 0});
    dispatch_release(mImpl->mGroup); mImpl->mGroup = nullptr;
    outcome.encoded = mImpl->mJob->encoded; outcome.encode_ns = mImpl->mJob->encode_ns;
    outcome.encode_started_ns = mImpl->mJob->encode_started_ns;
    outcome.encode_finished_ns = mImpl->mJob->encode_finished_ns;
    outcome.retained_cpu_bytes = mImpl->mJob->retained_cpu_bytes;
    outcome.worker_is_owner = mImpl->mJob->worker_is_owner;
    outcome.completion = mImpl->mContext->completion();
    try
    {
        if (submit && outcome.encoded) outcome.submitted = mImpl->mContext->commit(error);
        else mImpl->mContext->cancel();
        if (error && !outcome.encoded) *error = mImpl->mJob->error;
    }
    catch (...) { mImpl->mContext->cancel(); }
    if (outcome.submitted) outcome.visibility_results = std::move(mImpl->mJob->results);
    outcome.packet = std::move(mImpl->mJob->packet);
    mImpl->mJob.reset();
    return outcome;
}

