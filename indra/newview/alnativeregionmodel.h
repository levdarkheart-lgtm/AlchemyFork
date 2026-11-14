/**
 * @file alnativeregionmodel.h
 * @brief Owner-published copied region transforms for native scene sources.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "alnativepublication.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "v3math.h"
#include <array>
#include <cstdint>
#include <cstring>

// This is a model publication, not a geometry producer. Only the region owner
// updates/subscribes/withdraws it. Detached/native readers receive Snapshot by
// value; neither that value nor its destruction refers back to a viewer owner.
class ALNativeRegionModel final
{
public:
    struct Snapshot
    {
        std::uint64_t identity = 0, revision = 0;
        std::array<float, 16> model{};
        explicit operator bool() const noexcept { return identity && revision; }
    };

    ALNativeRegionModel() = default;
    ALNativeRegionModel(const ALNativeRegionModel&) = delete;
    ALNativeRegionModel& operator=(const ALNativeRegionModel&) = delete;
    ~ALNativeRegionModel() { withdraw(); }

    Snapshot snapshot() const noexcept
    {
        return mAvailable ? Snapshot{mPublication.identity(), mRevision, mModel} : Snapshot{};
    }
    ALNativePublication::Signal& publication() noexcept { return mPublication; }

    // The exact existing LLViewerRegion transform arithmetic. Computing the
    // candidate first avoids epochs for unchanged bits; the authoritative
    // matrix is not replaced until pre-write dependents have withdrawn.
    // A reentrant newer update/withdrawal wins and explicitly rejects this
    // stale write. There is no allocation or geometry invalidation here.
    bool update(LLMatrix4a& authoritative, const LLVector3& origin_agent) noexcept
    {
        if (mRetired) return false;
        LLMatrix4a candidate;
        candidate.setIdentity();
        candidate.setTranslation(origin_agent);
        if (mAvailable && std::memcmp(mModel.data(), candidate.getF32ptr(), sizeof(mModel)) == 0)
        {
            authoritative = candidate;
            return true;
        }
        advance(mMutation);
        const auto mutation = mMutation;
        mAvailable = false;
        mPublication.willWrite();
        if (mRetired || mutation != mMutation) return false;
        authoritative = candidate;
        std::memcpy(mModel.data(), candidate.getF32ptr(), sizeof(mModel));
        advance(mRevision);
        mAvailable = true;
        mPublication.didPublish();
        return !mRetired && mAvailable && mutation == mMutation;
    }

    // Call at the beginning of region destruction, before tearing down scene
    // owners. Idempotence also permits the helper's final destructor fallback.
    void withdraw() noexcept
    {
        if (mRetired) return;
        mRetired = true;
        mAvailable = false;
        advance(mMutation);
        mPublication.withdraw();
    }

private:
    static void advance(std::uint64_t& revision) noexcept
    {
        if (++revision == 0) ++revision;
    }

    ALNativePublication::Signal mPublication;
    std::array<float, 16> mModel{};
    std::uint64_t mRevision = 0, mMutation = 0;
    bool mAvailable = false, mRetired = false;
};
