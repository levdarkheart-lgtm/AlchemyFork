/**
 * @file almetalworldprepare.mm
 * @brief Immutable world packet ownership and bounded CPU range preparation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldprepare.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <unordered_map>

namespace
{
void fail(std::string* error, const char* message)
{
    if (error) *error = message;
}
}

struct ALMetalWorldViewInputs::Impl
{
    ALMetalWorldGBufferScene scene;
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<ALMetalWorldVisibilityQuery> queries;
    std::shared_ptr<const void> resources;
    std::vector<std::vector<float>> poses;
    std::array<float, 4> coverage_fill{};
    std::uint64_t epoch = 0;
};

ALMetalWorldViewInputs::ALMetalWorldViewInputs(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalWorldViewInputs::~ALMetalWorldViewInputs() = default;
const ALMetalWorldGBufferScene& ALMetalWorldViewInputs::scene() const noexcept { return mImpl->scene; }
std::uint64_t ALMetalWorldViewInputs::epoch() const noexcept { return mImpl->epoch; }
std::shared_ptr<const void> ALMetalWorldViewInputs::resourceLease() const noexcept { return mImpl->resources; }

bool ALMetalWorldViewInputs::reclaimCpuStorage(
    std::shared_ptr<const ALMetalWorldViewInputs>& input,
    std::vector<ALMetalWorldSimpleBatch>& empty_batches,
    std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept
{
    if (!input || input.use_count() != 1 || !empty_batches.empty() || !empty_queries.empty()) return false;
    auto& impl = *input->mImpl;
    empty_batches.swap(impl.batches);
    empty_queries.swap(impl.queries);
    empty_batches.clear();
    empty_queries.clear();
    input.reset();
    return true;
}

std::shared_ptr<const ALMetalWorldViewInputs> ALMetalWorldViewInputs::adopt(
    ALMetalWorldGBufferScene scene, std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldVisibilityQuery> queries,
    std::shared_ptr<const void> resource_owner, std::uint64_t epoch, std::string* error)
try
{
    if ((!batches.empty() || scene.destination) && !resource_owner)
    {
        fail(error, "Prepared world inputs require an immutable resource lease");
        return {};
    }
    auto impl = std::make_unique<Impl>();
    impl->batches = std::move(batches);
    impl->queries = std::move(queries);
    impl->resources = std::move(resource_owner);
    impl->epoch = epoch;
    // The caller's transient array addresses are deliberately replaced.
    scene.batches = impl->batches.data();
    scene.batch_count = impl->batches.size();
    scene.visibility_queries = impl->queries.data();
    scene.visibility_query_count = impl->queries.size();
    if (scene.opaque_coverage_fill)
    {
        std::memcpy(impl->coverage_fill.data(), scene.opaque_coverage_fill, sizeof(impl->coverage_fill));
        scene.opaque_coverage_fill = impl->coverage_fill.data();
    }
    // G-buffer preparation never consumes these forward/lighting owners.
    // Do not carry mutable viewer pointers into a detached input packet.
    scene.reflection_probes = nullptr;
    scene.hero_probe = nullptr;
    scene.screen_space_reflections = nullptr;
    scene.legacy_alpha_lighting = nullptr;
    scene.directional_shadows = nullptr;
    impl->scene = scene;
    if (!alMetalValidateWorldView(impl->scene, error)) return {};

    struct Pose { std::size_t slot, count; std::uint32_t joints; };
    std::unordered_map<const float*, Pose> snapshots;
    for (auto& batch : impl->batches)
    {
        if (!batch.skin_palette)
        {
            if (batch.skin_palette_float_count || batch.skin_joint_count)
            {
                fail(error, "Prepared world pose has no values");
                return {};
            }
            continue;
        }
        const std::size_t expected = batch.avatar_skinning
            ? 45 * 4 : 4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
        if (batch.skin_joint_count == 0 || batch.skin_joint_count > 110 ||
            (batch.avatar_skinning && batch.skin_joint_count != 15) ||
            batch.skin_palette_float_count != expected)
        {
            fail(error, "Prepared world pose length is invalid");
            return {};
        }
        const auto found = snapshots.find(batch.skin_palette);
        std::size_t slot;
        if (found == snapshots.end())
        {
            slot = impl->poses.size();
            impl->poses.emplace_back(batch.skin_palette, batch.skin_palette + expected);
            snapshots.emplace(batch.skin_palette, Pose{slot, expected, batch.skin_joint_count});
        }
        else
        {
            if (found->second.count != expected || found->second.joints != batch.skin_joint_count)
            {
                fail(error, "Prepared world pose aliases inconsistent layouts");
                return {};
            }
            slot = found->second.slot;
        }
        batch.skin_palette = impl->poses[slot].data();
    }
    return std::shared_ptr<const ALMetalWorldViewInputs>(new ALMetalWorldViewInputs(std::move(impl)));
}
catch (const std::bad_alloc&)
{
    if (error) error->clear();
    return {};
}

struct ALMetalPreparedWorldView::Impl
{
    std::shared_ptr<const ALMetalWorldViewInputs> inputs;
    std::vector<ALMetalWorldPreparedRecord> records;
    std::vector<std::byte> constants;
    ALMetalWorldPrepareStats statistics;
    std::shared_ptr<const std::atomic<std::uint64_t>> current_epoch;
    std::atomic<bool> cancelled{false};
};
ALMetalPreparedWorldView::ALMetalPreparedWorldView(std::shared_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalPreparedWorldView::~ALMetalPreparedWorldView() = default;
const ALMetalWorldGBufferScene& ALMetalPreparedWorldView::scene() const noexcept { return mImpl->inputs->scene(); }
const ALMetalWorldPrepareStats& ALMetalPreparedWorldView::stats() const noexcept { return mImpl->statistics; }
bool ALMetalPreparedWorldView::isCurrent() const noexcept
{
    return !mImpl->cancelled.load(std::memory_order_acquire) &&
        (!mImpl->current_epoch || mImpl->current_epoch->load(std::memory_order_acquire) == mImpl->inputs->epoch());
}
std::shared_ptr<const void> ALMetalPreparedWorldView::inputLease() const noexcept { return mImpl->inputs->resourceLease(); }
std::span<const ALMetalWorldPreparedRecord> ALMetalPreparedWorldView::records() const noexcept { return mImpl->records; }
std::span<const std::byte> ALMetalPreparedWorldView::constants() const noexcept { return mImpl->constants; }

struct ALMetalWorldPreparation::Impl
{
    std::shared_ptr<ALMetalPreparedWorldView::Impl> prepared;
    std::shared_ptr<const std::atomic<std::uint64_t>> current_epoch;
    std::array<std::atomic<std::uint8_t>, 2> state{}; // 0=unstarted, 1=running, 2=joined
    std::array<std::size_t, 2> completed{};
    std::array<std::string, 2> errors;
    std::atomic<bool> invalid{false};
    std::size_t jobs = 1;
    bool compacted = false;
    std::shared_ptr<const ALMetalPreparedWorldView> result;

    bool stale() const noexcept
    {
        return prepared->cancelled.load(std::memory_order_acquire) ||
            (current_epoch && current_epoch->load(std::memory_order_acquire) != prepared->inputs->epoch());
    }

    bool compactConstants() noexcept
    {
        // Same exact common ABI/comparison as the direct G-buffer publisher.
        // Save bytes separately: moving a following table may overwrite the
        // old common source range. Terrain/impostor do not replace this value.
        constexpr std::size_t common_bytes = 400;
        std::array<std::byte, common_bytes> previous_common;
        bool have_previous_common = false;
        std::size_t previous_common_offset = 0, next = 0, shared = 0;
        auto& bytes = prepared->constants;
        const auto fits = [&](std::size_t offset, std::size_t length)
        {
            return offset <= bytes.size() && length <= bytes.size() - offset;
        };
        const auto move = [&](std::size_t source, std::size_t length, std::size_t& destination)
        {
            if (!length) { destination = 0; return true; }
            // The original dense layout is naturally 16-byte aligned. Only
            // remove ranges, so every move stays at or before its old source;
            // FrameData still aligns the whole publication's base to 256.
            if (!fits(source, length) || next > source) return false;
            destination = next;
            if (next != source) std::memmove(bytes.data() + next, bytes.data() + source, length);
            next += length;
            return true;
        };
        for (auto& record : prepared->records)
        {
            const auto old = record;
            if (record.family == ALMetalWorldPreparedRecord::Family::simple)
            {
                if (old.length != common_bytes || !fits(old.offset, common_bytes)) return false;
                const auto* common = bytes.data() + old.offset;
                if (have_previous_common && std::memcmp(previous_common.data(), common, common_bytes) == 0)
                {
                    record.offset = previous_common_offset;
                    ++shared;
                }
                else
                {
                    std::memcpy(previous_common.data(), common, common_bytes);
                    if (!move(old.offset, old.length, record.offset)) return false;
                    previous_common_offset = record.offset;
                    have_previous_common = true;
                }
            }
            else if (!move(old.offset, old.length, record.offset)) return false;
            // Material tables keep their original per-record semantics/order.
            // Common equality never implies table equality.
            if (!move(old.material_offset, old.material_length, record.material_offset) ||
                !move(old.pbr_offset, old.pbr_length, record.pbr_offset)) return false;
        }
        bytes.resize(next); // shrinking only: no allocation in compaction
        prepared->statistics.constant_bytes = next;
        prepared->statistics.shared_common_records = shared;
        compacted = true;
        return true;
    }
};
ALMetalWorldPreparation::ALMetalWorldPreparation(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalWorldPreparation::~ALMetalWorldPreparation() = default;

std::shared_ptr<ALMetalWorldPreparation> ALMetalWorldPreparation::create(
    std::shared_ptr<const ALMetalWorldViewInputs> inputs, std::size_t requested_jobs,
    std::shared_ptr<const std::atomic<std::uint64_t>> current_epoch, std::string* error)
try
{
    if (!inputs)
    {
        fail(error, "World preparation requires owned inputs");
        return {};
    }
    auto impl = std::make_unique<Impl>();
    impl->prepared = std::make_shared<ALMetalPreparedWorldView::Impl>();
    impl->prepared->inputs = std::move(inputs);
    impl->current_epoch = std::move(current_epoch);
    impl->prepared->current_epoch = impl->current_epoch;
    const auto& scene = impl->prepared->inputs->scene();
    // A single record cannot be usefully divided. The shared frame owner,
    // rather than this helper, makes all other workload/admission decisions.
    impl->jobs = std::clamp<std::size_t>(requested_jobs, 1, scene.batch_count >= 2 ? 2 : 1);
    impl->prepared->records.resize(scene.batch_count);
    std::size_t bytes = 0;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
        if (!alMetalLayoutWorldRecord(scene.batches[index], bytes, impl->prepared->records[index], error)) return {};
    impl->prepared->constants.resize(bytes);
    impl->prepared->statistics = {scene.batch_count, 0, bytes, impl->jobs, false};
    impl->prepared->statistics.uncompacted_constant_bytes = bytes;
    return std::shared_ptr<ALMetalWorldPreparation>(new ALMetalWorldPreparation(std::move(impl)));
}
catch (const std::bad_alloc&)
{
    if (error) error->clear();
    return {};
}

std::size_t ALMetalWorldPreparation::jobCount() const noexcept { return mImpl->jobs; }
void ALMetalWorldPreparation::cancel() noexcept { mImpl->prepared->cancelled.store(true, std::memory_order_release); }

bool ALMetalWorldPreparation::runJob(std::size_t index) noexcept
{
    if (index >= mImpl->jobs) { mImpl->invalid.store(true, std::memory_order_release); return false; }
    std::uint8_t expected = 0;
    if (!mImpl->state[index].compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
    {
        mImpl->invalid.store(true, std::memory_order_release);
        return false;
    }
    bool success = true;
    try
    {
        const auto& scene = mImpl->prepared->inputs->scene();
        const std::size_t first = scene.batch_count * index / mImpl->jobs;
        const std::size_t last = scene.batch_count * (index + 1) / mImpl->jobs;
        for (std::size_t at = first; at < last; ++at)
        {
            if (mImpl->stale() || !alMetalPrepareWorldRecord(scene, at,
                    mImpl->prepared->records[at], mImpl->prepared->constants, &mImpl->errors[index]))
            {
                success = false;
                break;
            }
            ++mImpl->completed[index];
        }
        success = success && !mImpl->stale();
    }
    catch (...)
    {
        // No allocation or logging while recovering from a worker exception.
        success = false;
    }
    if (!success) mImpl->invalid.store(true, std::memory_order_release);
    mImpl->state[index].store(2, std::memory_order_release);
    return success;
}

std::shared_ptr<const ALMetalPreparedWorldView> ALMetalWorldPreparation::finish(std::string* error)
try
{
    for (std::size_t index = 0; index < mImpl->jobs; ++index)
        if (mImpl->state[index].load(std::memory_order_acquire) != 2)
        {
            fail(error, "World preparation jobs have not joined");
            return {};
        }
    if (mImpl->stale())
    {
        mImpl->prepared->statistics.cancelled = true;
        fail(error, "World preparation epoch was cancelled");
        return {};
    }
    if (mImpl->invalid.load(std::memory_order_acquire))
    {
        for (std::size_t index = 0; index < mImpl->jobs; ++index)
            if (!mImpl->errors[index].empty()) { if (error) *error = mImpl->errors[index]; return {}; }
        fail(error, "World preparation did not produce a complete view");
        return {};
    }
    if (!mImpl->result)
    {
        if (!mImpl->compacted && !mImpl->compactConstants())
        {
            mImpl->invalid.store(true, std::memory_order_release);
            fail(error, "World preparation constant compaction is invalid");
            return {};
        }
        // An epoch can change during the owner join/compaction. Never publish
        // stale work even though every detached range completed successfully.
        if (mImpl->stale())
        {
            mImpl->prepared->statistics.cancelled = true;
            fail(error, "World preparation epoch was cancelled");
            return {};
        }
        mImpl->prepared->statistics.prepared_count = mImpl->completed[0] + mImpl->completed[1];
        mImpl->result = std::shared_ptr<const ALMetalPreparedWorldView>(new ALMetalPreparedWorldView(mImpl->prepared));
    }
    return mImpl->result;
}
catch (const std::bad_alloc&)
{
    if (error) error->clear();
    return {};
}
