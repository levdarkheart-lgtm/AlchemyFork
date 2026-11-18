/**
 * @file almetalresourceplan.cpp
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalresourceplan.h"
#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ALMetalResourcePlanning
{
struct State
{
    mutable std::mutex mutex;
    Stats stats;
    std::uint64_t next_identity = 1;
    std::uint64_t next_command_identity = 1;
    bool fail_next_allocation = false;
};
namespace
{
bool room(std::uint64_t value, std::uint64_t increment) noexcept
{
    return increment <= std::numeric_limits<std::uint64_t>::max() - value;
}
void addDescription(Stats& s, const Description& d)
{
    if (d.origin == Origin::external_unknown) ++s.external_unknown_records;
    else if (d.memoryless)
    { ++s.memoryless_backings; s.memoryless_declared_bytes += d.declared_bytes; }
    else
    {
        ++s.owned_backings; s.owned_declared_bytes += d.declared_bytes;
        if (d.metal_reported_bytes) s.owned_metal_reported_bytes += *d.metal_reported_bytes;
        else { ++s.owned_unknown_metal_sizes; s.owned_unknown_declared_bytes += d.declared_bytes; }
    }
    ++s.backing_creations;
}
}
Allocation::Allocation(std::shared_ptr<State> state, Description d,
                       std::shared_ptr<const void> owner, const void* identity)
: mState(std::move(state)), mDescription(d), mNativeOwner(std::move(owner)), mNativeIdentity(identity) {}
Allocation::~Allocation()
{
    if (!mRegistered) return;
    std::lock_guard lock(mState->mutex);
    auto& s = mState->stats;
    if (mDescription.origin == Origin::external_unknown) --s.external_unknown_records;
    else if (mDescription.memoryless)
    { --s.memoryless_backings; s.memoryless_declared_bytes -= mDescription.declared_bytes; }
    else
    {
        --s.owned_backings; s.owned_declared_bytes -= mDescription.declared_bytes;
        if (mDescription.metal_reported_bytes) s.owned_metal_reported_bytes -= *mDescription.metal_reported_bytes;
        else { --s.owned_unknown_metal_sizes; s.owned_unknown_declared_bytes -= mDescription.declared_bytes; }
    }
    ++s.backing_retirements;
}
View::View(std::shared_ptr<Allocation> allocation) : mAllocation(std::move(allocation))
{
    std::lock_guard lock(mAllocation->mState->mutex);
    mGeneration = mAllocation->mGeneration;
    ++mAllocation->mAliases; ++mAllocation->mState->stats.logical_aliases;
}
View::View(const View& other) : mAllocation(other.mAllocation), mGeneration(other.mGeneration)
{
    if (mAllocation)
    {
        std::lock_guard lock(mAllocation->mState->mutex);
        ++mAllocation->mAliases; ++mAllocation->mState->stats.logical_aliases;
    }
}
View& View::operator=(const View& other)
{
    if (this != &other) { View copy(other); *this = std::move(copy); }
    return *this;
}
View::View(View&& other) noexcept
: mAllocation(std::move(other.mAllocation)), mGeneration(std::exchange(other.mGeneration, 0)) {}
View& View::operator=(View&& other) noexcept
{
    if (this != &other)
    { release(); mAllocation = std::move(other.mAllocation); mGeneration = std::exchange(other.mGeneration, 0); }
    return *this;
}
View::~View() { release(); }
void View::release() noexcept
{
    if (!mAllocation) return;
    {
        std::lock_guard lock(mAllocation->mState->mutex);
        --mAllocation->mAliases; --mAllocation->mState->stats.logical_aliases;
    }
    mAllocation.reset(); mGeneration = 0;
}
std::uint64_t View::identity() const noexcept { return mAllocation ? mAllocation->identity() : 0; }
const void* View::nativeIdentity() const noexcept
{ return mAllocation ? mAllocation->mNativeIdentity : nullptr; }
bool View::mayIssueGeneration(const std::shared_ptr<Command>& current) const noexcept
{
    if (current && (current->mState != mAllocation->mState || current->mActivePass ||
        current->mStats.opaque || current->mStats.submitted || current->mStats.retired ||
        !current->mStats.valid || current->mSubmitting)) return false;
    return mGeneration == mAllocation->mGeneration && mAllocation->mAliases == 1 &&
        (mAllocation->mUncommitted == 0 ||
            (current && mAllocation->mUncommittedOwner == current->mIdentity)) &&
        mAllocation->mState->stats.opaque_uncommitted_commands == 0 &&
        mGeneration != std::numeric_limits<std::uint64_t>::max();
}
bool View::canIssueGeneration(const std::shared_ptr<Command>& current) const noexcept
{
    if (!mAllocation) return false;
    std::lock_guard lock(mAllocation->mState->mutex);
    return mayIssueGeneration(current);
}
bool View::issueGeneration(const std::shared_ptr<Command>& current) noexcept
{
    if (!mAllocation) return false;
    std::lock_guard lock(mAllocation->mState->mutex);
    if (!mayIssueGeneration(current)) return false;
    mGeneration = ++mAllocation->mGeneration;
    return true;
}
bool View::canReadAfterQueueBarrier() const noexcept
{
    if (!mAllocation) return false;
    std::lock_guard lock(mAllocation->mState->mutex);
    return mGeneration == mAllocation->mGeneration &&
        mAllocation->mUncommittedWrites == 0 &&
        mAllocation->mState->stats.opaque_uncommitted_commands == 0;
}
Command::Command(std::shared_ptr<State> state) : mState(std::move(state))
{
    std::lock_guard lock(mState->mutex);
    if (!mState->next_command_identity) throw std::overflow_error("Metal command-use identity exhausted");
    mIdentity = mState->next_command_identity++;
    ++mState->stats.active_commands;
}
Command::~Command() { retire(Outcome::abandoned); }
std::size_t Command::Hash::operator()(const Key& key) const noexcept
{
    return std::hash<std::uint64_t>{}(key.identity) ^
        (std::hash<std::uint64_t>{}(key.generation) + 0x9e3779b9u + (key.identity << 6) + (key.identity >> 2));
}
std::uint64_t Command::beginPass(Pass kind)
{
    std::lock_guard lock(mState->mutex);
    if (!mStats.valid || mStats.retired || mSubmitting || mStats.submitted || mActivePass ||
        mStats.passes == std::numeric_limits<std::uint64_t>::max()) return 0;
    mPassKind = static_cast<unsigned>(kind); mActivePass = ++mStats.passes;
    return mActivePass;
}
bool Command::endPass(std::uint64_t pass) noexcept
{
    std::lock_guard lock(mState->mutex);
    if (!pass || mActivePass != pass) return false;
    mActivePass = 0; mPassKind = 0; return true;
}
bool Command::use(const View& view, Access access)
{
    if (!view.mAllocation || view.mAllocation->mState != mState) return false;
    const auto bits = static_cast<unsigned>(access);
    if (!bits || (bits & ~3u)) return false;
    std::lock_guard lock(mState->mutex);
    if (!mStats.valid || mStats.retired || mSubmitting || mStats.submitted || !mActivePass ||
        view.mGeneration != view.mAllocation->mGeneration) return false;
    auto [at, inserted] = mUses.try_emplace({view.identity(), view.mGeneration},
        Use{view.mAllocation, 0, 0, mActivePass, mActivePass});
    auto& use = at->second;
    if (inserted)
    {
        if (use.allocation->mUncommitted == 0) use.allocation->mUncommittedOwner = mIdentity;
        else if (use.allocation->mUncommittedOwner != mIdentity) use.allocation->mUncommittedOwner = 0;
        ++use.allocation->mUncommitted; ++mState->stats.uncommitted_uses;
        ++mStats.resources;
    }
    if ((bits & 1u) && !(use.access & 1u)) ++mStats.read_resources;
    if ((bits & 2u) && !(use.access & 2u))
    { ++mStats.write_resources; ++use.allocation->mUncommittedWrites; }
    use.access |= bits; use.pass_kinds |= mPassKind; use.last_pass = mActivePass;
    return true;
}
void Command::markOpaque() noexcept
{
    std::lock_guard lock(mState->mutex);
    if (mStats.opaque || mStats.retired || mStats.submitted) return;
    mStats.opaque = true; ++mState->stats.opaque_uncommitted_commands;
}
bool Command::prepareSubmit() noexcept
{
    std::lock_guard lock(mState->mutex);
    if (!mStats.valid || mStats.retired || mSubmitting || mStats.submitted || mActivePass) return false;
    mSubmitting = true; return true;
}
bool Command::submitted(std::uint64_t serial) noexcept
{
    std::lock_guard lock(mState->mutex);
    if (!serial || !mSubmitting || mStats.submitted) return false;
    mStats.submitted = true; mStats.submission_serial = serial;
    // A very short native command may complete before commit() returns. Its
    // callback has already retired these uses; do not resurrect or subtract twice.
    if (mStats.retired) return true;
    if (mStats.opaque) --mState->stats.opaque_uncommitted_commands;
    for (auto& [key, use] : mUses)
    {
        --use.allocation->mUncommitted; --mState->stats.uncommitted_uses;
        if (use.allocation->mUncommitted == 0) use.allocation->mUncommittedOwner = 0;
        ++use.allocation->mSubmitted; ++mState->stats.submitted_uses;
        if (use.access & 2u)
        { --use.allocation->mUncommittedWrites; ++use.allocation->mSubmittedWrites; }
    }
    return true;
}
void Command::retire(Outcome outcome) noexcept
{
    // Destroy backing leases after releasing the registry lock: an allocation
    // may be the final owner and its destructor updates this same registry.
    decltype(mUses) retired;
    {
        std::lock_guard lock(mState->mutex);
        if (mStats.retired) return;
        mStats.retired = true;
        --mState->stats.active_commands;
        if (mStats.opaque && !mStats.submitted) --mState->stats.opaque_uncommitted_commands;
        for (auto& [key, use] : mUses)
        {
            if (mStats.submitted)
            {
                --use.allocation->mSubmitted; --mState->stats.submitted_uses;
                if (use.access & 2u) --use.allocation->mSubmittedWrites;
            }
            else
            {
                --use.allocation->mUncommitted; --mState->stats.uncommitted_uses;
                if (use.allocation->mUncommitted == 0) use.allocation->mUncommittedOwner = 0;
                if (use.access & 2u) --use.allocation->mUncommittedWrites;
            }
        }
        if (outcome == Outcome::completed) ++mState->stats.completed_commands;
        else if (outcome == Outcome::error) ++mState->stats.errored_commands;
        else ++mState->stats.abandoned_commands;
        retired.swap(mUses);
    }
}
CommandStats Command::stats() const noexcept
{ std::lock_guard lock(mState->mutex); auto result = mStats; result.active_pass = mActivePass; return result; }
void Command::invalidate() noexcept { std::lock_guard lock(mState->mutex); mStats.valid = false; }
Plan::Plan() : mState(std::make_shared<State>()) {}
View Plan::allocate(Description d, std::shared_ptr<const void> owner, const void* native_identity)
{
    bool fail_allocation = false;
    {
        std::lock_guard lock(mState->mutex);
        fail_allocation = std::exchange(mState->fail_next_allocation, false);
    }
    // Both the record and shared control block are created outside the
    // registry lock. Failed shared_ptr construction deletes its raw record;
    // that unregistered record must neither lock nor subtract a live charge.
    auto pending = std::unique_ptr<Allocation>(new Allocation(mState, d, std::move(owner), native_identity));
    if (fail_allocation) throw std::bad_alloc();
    std::shared_ptr<Allocation> allocation(pending.release());
    {
        std::lock_guard lock(mState->mutex);
        const auto& s = mState->stats;
        if (!mState->next_identity || !room(s.backing_creations, 1) ||
            (d.origin == Origin::viewer_owned && d.memoryless && !room(s.memoryless_declared_bytes, d.declared_bytes)) ||
            (d.origin == Origin::viewer_owned && !d.memoryless &&
             (!room(s.owned_declared_bytes, d.declared_bytes) ||
              (d.metal_reported_bytes && !room(s.owned_metal_reported_bytes, *d.metal_reported_bytes))))) return {};
        allocation->mIdentity = mState->next_identity;
        allocation->mRegistered = true;
        ++mState->next_identity; addDescription(mState->stats, d);
    }
    return View(std::move(allocation));
}
bool Plan::containsCurrent(const View& view) const noexcept
{
    std::lock_guard lock(mState->mutex);
    return view.mAllocation && view.mAllocation->mState == mState &&
        view.mGeneration == view.mAllocation->mGeneration;
}
std::shared_ptr<Command> Plan::command() { return std::shared_ptr<Command>(new Command(mState)); }
Stats Plan::stats() const noexcept { std::lock_guard lock(mState->mutex); return mState->stats; }
void Plan::failNextAllocationForTesting() noexcept
{ std::lock_guard lock(mState->mutex); mState->fail_next_allocation = true; }
}
