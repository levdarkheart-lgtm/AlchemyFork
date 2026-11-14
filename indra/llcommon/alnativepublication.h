/**
 * @file alnativepublication.h
 * @brief Owner-thread invalidation and publication of immutable native inputs.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#ifndef AL_NATIVE_PUBLICATION_H
#define AL_NATIVE_PUBLICATION_H

#include <cstdint>
#include <memory>
#include <vector>

namespace ALNativePublication
{
using Revision = std::uint64_t;

enum class Phase : std::uint8_t
{
    will_write,
    published,
    withdrawn
};

// All operations run on the resource/scene publication owner, including observer
// destruction. Worker staging never walks live scene dependencies. An observer
// may forget itself, release other observers, or add a subscription in a callback;
// new subscriptions are first notified by the next event. The signal's owner must
// remain alive until its notification returns.
class Observer
{
public:
    virtual ~Observer() = default;
    virtual void nativePublicationChanged(Phase phase) noexcept = 0;
};

class Signal final
{
public:
    Signal() noexcept;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    Revision identity() const noexcept { return mIdentity; }
    Revision mutationRevision() const noexcept { return mMutationRevision; }
    Revision publishedRevision() const noexcept { return mPublishedRevision; }

    // Call BEFORE testing backing-resource leases or beginning an in-place
    // write. Observers drop their publisher/cache references synchronously;
    // actual queued/in-flight immutable readers retain their own references.
    void willWrite() noexcept;
    // Call only after the complete visible tuple has published successfully.
    // CPU content stamps and pending upload generations are not publication.
    void didPublish() noexcept;
    // Call after a complete absent tuple replaces authoritative backing.
    void withdraw() noexcept;

    // Subscription allocates only when a new observer is added. Signals with
    // no observers have no heap allocation. Notification never allocates and
    // takes no global lock. Weak control-block identity prevents pointer ABA
    // and producer/consumer ownership cycles.
    void observe(const std::shared_ptr<Observer>& observer);
    void forget(const std::shared_ptr<Observer>& observer) noexcept;

private:
    void notify(Phase phase) noexcept;
    void prune() noexcept;

    const Revision mIdentity;
    Revision mMutationRevision = 0;
    Revision mPublishedRevision = 0;
    std::vector<std::weak_ptr<Observer>> mObservers;
    unsigned mNotificationDepth = 0;
};

// One source producer's state, owned by LLDrawInfo (not by detached workers).
// The payload is the publisher's SOLE strong cache lease for this revision.
// Workers/packets receive copies of that immutable payload. Invalidation drops
// only this publisher's lease, without reclaiming any actual reader's payload.
// A cache must retain this state/its weak handle, not a second payload copy.
class State final : public Observer
{
public:
    State() noexcept;
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    Revision identity() const noexcept { return mIdentity; }
    Revision revision() const noexcept { return mRevision; }
    bool dirty() const noexcept { return mDirty; }
    const std::shared_ptr<const void>& publication() const noexcept { return mPublication; }

    // Owner-only dependency edge to a derived publisher (for example source ->
    // spatial-group packet). The graph must be acyclic. Observers are weak and
    // deduplicated by shared ownership. A Signal is allocated only for the
    // first nonnull subscription. The State owner must remain alive throughout
    // notification.
    void observe(const std::shared_ptr<Observer>& observer);
    // Dependents see will_write before the old publisher lease is released.
    // Actual CPU/GPU readers remain owners of their immutable payloads.
    void invalidate() noexcept;
    void nativePublicationChanged(Phase) noexcept override { invalidate(); }
    // Capture revision before sealing. If a resource publishes while sealing,
    // stale work must not replace the current source. Failed sealing leaves
    // the producer dirty and eligible for the established owner retry/fallback.
    bool publish(Revision expected_revision, std::shared_ptr<const void> value) noexcept;
    // Working-set eviction also withdraws derived publisher caches, before
    // releasing this publisher lease. It does not change the source revision
    // or invalidate existing immutable readers. Dependents see withdrawn for
    // cache unavailability, not an authoritative backing-resource mutation.
    void evict() noexcept;

private:
    const Revision mIdentity;
    Revision mRevision = 1;
    bool mDirty = true;
    std::shared_ptr<const void> mPublication;
    std::unique_ptr<Signal> mDependents;
};
}

#endif
