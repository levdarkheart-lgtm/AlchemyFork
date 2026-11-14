/**
 * @file alnativepublication.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alnativepublication.h"

#include <algorithm>
#include <atomic>

namespace ALNativePublication
{
namespace
{
std::atomic<Revision> sIdentity{0};

Revision nextIdentity() noexcept
{
    return sIdentity.fetch_add(1, std::memory_order_relaxed) + 1;
}

void advance(Revision& revision) noexcept
{
    // Zero remains the never-published sentinel. Identity and live shared
    // control blocks, rather than a recycled raw address, identify producers.
    if (++revision == 0) ++revision;
}

bool sameOwner(const std::weak_ptr<Observer>& a,
               const std::shared_ptr<Observer>& b) noexcept
{
    return !a.owner_before(b) && !b.owner_before(a);
}
}

Signal::Signal() noexcept : mIdentity(nextIdentity()) {}

void Signal::willWrite() noexcept
{
    advance(mMutationRevision);
    notify(Phase::will_write);
}

void Signal::didPublish() noexcept
{
    advance(mMutationRevision);
    advance(mPublishedRevision);
    notify(Phase::published);
}

void Signal::withdraw() noexcept
{
    advance(mMutationRevision);
    advance(mPublishedRevision);
    notify(Phase::withdrawn);
}

void Signal::observe(const std::shared_ptr<Observer>& observer)
{
    if (!observer) return;
    if (mNotificationDepth == 0) prune();
    for (const auto& entry : mObservers)
        if (sameOwner(entry, observer)) return;
    mObservers.emplace_back(observer);
}

void Signal::forget(const std::shared_ptr<Observer>& observer) noexcept
{
    if (!observer) return;
    for (auto& entry : mObservers)
        if (sameOwner(entry, observer)) entry.reset();
    if (mNotificationDepth == 0) prune();
}

void Signal::notify(Phase phase) noexcept
{
    ++mNotificationDepth;
    const auto count = mObservers.size();
    for (std::size_t at = 0; at < count; ++at)
    {
        // Keep no iterator/reference into the vector across the callback:
        // observe() may grow it, and forget() only tombstones while notifying.
        auto observer = mObservers[at].lock();
        if (observer) observer->nativePublicationChanged(phase);
    }
    if (--mNotificationDepth == 0) prune();
}

void Signal::prune() noexcept
{
    mObservers.erase(std::remove_if(mObservers.begin(), mObservers.end(),
        [](const auto& entry) { return entry.expired(); }), mObservers.end());
}

State::State() noexcept : mIdentity(nextIdentity()) {}

void State::invalidate() noexcept
{
    advance(mRevision);
    mDirty = true;
    mPublication.reset();
}

bool State::publish(Revision expected_revision,
                    std::shared_ptr<const void> value) noexcept
{
    if (!value || expected_revision != mRevision) return false;
    const std::weak_ptr<const void> installed(value);
    auto previous = std::move(mPublication);
    mPublication = std::move(value);
    mDirty = false;
    // Dropping the old payload may synchronously retire a working-set entry
    // or invalidate/re-publish this producer. Never acknowledge stale work or
    // overwrite the state established by that callback after it returns.
    previous.reset();
    const std::weak_ptr<const void> current(mPublication);
    return expected_revision == mRevision && !mDirty && mPublication &&
        !installed.owner_before(current) && !current.owner_before(installed);
}

void State::evict() noexcept
{
    mDirty = true;
    mPublication.reset();
}
}
