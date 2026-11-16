/**
 * @file almetalshadowcapture.h
 * @brief Validated source admission before opaque shadow record allocation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include "almetalworld.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

// The CPU packet and the native completion owner have different lifetimes.
// An encoding job owns this packet until it joins. Its command only retains
// CompletionOwners, so GPU latency does not pin the large CPU batch array or
// prevent its capacity from returning to the next capture.
class ALMetalCapturedShadowView final
{
public:
    using Batches = std::vector<ALMetalWorldSpotShadowBatch>;
    using BodyPalettes =
        std::vector<std::shared_ptr<const ALMetalBodyPaletteRecord>>;

    // Called by the capture owner before dispatch. The producer lease already
    // pins raw geometry, covered textures, source records and attachment poses;
    // BODY poses are the separate capture-owned vector transferred here.
    // Query/visibility spans need their own transaction handoff and deliberately
    // remain on the existing owner path. Rejection never consumes either input.
    static std::unique_ptr<ALMetalCapturedShadowView> take(
        const ALMetalWorldSpotShadowScene& scene, Batches& batches,
        BodyPalettes& body_palettes, std::string* error = nullptr)
    {
        if (!scene.width || !scene.height || scene.batch_count != batches.size() ||
            (scene.batch_count && scene.batches != batches.data()) ||
            (scene.batch_count && !scene.source_owner) ||
            scene.visibility_query_count || scene.visibility_bounds_count ||
            scene.native_visibility || scene.gpu_visibility || scene.metal4_visibility)
        {
            if (error) *error = "Captured shadow packet requires owned non-query inputs";
            return nullptr;
        }
        try
        {
            // Complete all fallible allocations before transferring capture
            // storage. The caller can still use its ordinary native path if
            // packet admission fails.
            auto owners = std::make_shared<CompletionOwners>();
            owners->producer = scene.source_owner;
            auto packet = std::unique_ptr<ALMetalCapturedShadowView>(
                new ALMetalCapturedShadowView(scene));
            owners->body_palettes.swap(body_palettes);
            packet->mBatches.swap(batches);
            packet->mOwners = std::move(owners);
            return packet;
        }
        catch (const std::bad_alloc&)
        {
            if (error) *error = "Captured shadow packet allocation failed";
            return nullptr;
        }
    }

    ALMetalCapturedShadowView(const ALMetalCapturedShadowView&) = delete;
    ALMetalCapturedShadowView& operator=(const ALMetalCapturedShadowView&) = delete;

    // The span is valid only while this CPU packet is retained and unrecycled.
    // The result's source_owner independently protects every native generation
    // and body pose through uncommitted work and actual command completion.
    ALMetalWorldSpotShadowScene scene() const noexcept
    {
        auto result = mScene;
        result.batches = mBatches.data();
        result.batch_count = mBatches.size();
        result.source_owner = mOwners;
        return result;
    }

    // Owner-only, after the encoding job has joined. Refuse to overwrite the
    // next view's populated capture. Recycling need not wait for GPU completion.
    bool recycleBatches(Batches& empty_capture) noexcept
    {
        if (!empty_capture.empty()) return false;
        mBatches.clear();
        mBatches.swap(empty_capture);
        mOwners.reset();
        return true;
    }

private:
    struct CompletionOwners
    {
        std::shared_ptr<const void> producer;
        BodyPalettes body_palettes;
    };

    explicit ALMetalCapturedShadowView(const ALMetalWorldSpotShadowScene& scene)
        : mScene(scene)
    {
        // Never retain borrowed pointers to capture scratch, including empty
        // vectors whose data() can remain nonnull after an earlier view.
        mScene.batches = nullptr;
        mScene.source_owner.reset();
        mScene.visibility_queries = nullptr;
        mScene.visibility_bounds = nullptr;
    }

    ALMetalWorldSpotShadowScene mScene;
    Batches mBatches;
    std::shared_ptr<const CompletionOwners> mOwners;
};

// A view-local candidate, used synchronously while the source matrix is live.
// Only the static opaque producer may construct one: covered, rigged, terrain
// and avatar sources use admitSeparate() and retain their complete records.
struct ALMetalOpaqueShadowSource
{
    const ALMetalBuffer* vertices = nullptr;
    const ALMetalBuffer* indices = nullptr;
    const float* modelview_projection = nullptr;
    std::size_t position_offset = 0;
    std::size_t index_offset = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    bool double_sided = false;
    bool reverse_z = false;
};

// Every source is counted independently of physical records. Invalid sources
// poison reconciliation even if a previous compatible packet already exists.
// No view/caster membership or resource lifetime is cached here.
class ALMetalShadowCaptureAdmission
{
public:
    void reset() noexcept { *this = {}; }
    void barrier() noexcept { mOpaqueTail = false; }
    void invalidate() noexcept { mValid = false; barrier(); }
    bool complete(std::size_t expected_sources) const noexcept
    {
        return mValid && mSources == expected_sources;
    }
    std::size_t sourceCount() const noexcept { return mSources; }
    std::size_t mergedSourceCount() const noexcept { return mMerged; }

    bool admitSeparate() noexcept
    {
        barrier();
        return admit();
    }

    bool appendOpaque(std::vector<ALMetalWorldSpotShadowBatch>& records,
                      const ALMetalOpaqueShadowSource& source)
    {
        const bool index32 = source.index_type == ALMetalIndexType::uint32;
        const std::size_t stride = index32 ? 4 : 2;
        if (!mValid || !source.vertices || !source.indices ||
            !source.modelview_projection || source.vertex_count == 0 ||
            source.index_count == 0 ||
            (!index32 && source.index_type != ALMetalIndexType::uint16) ||
            source.position_offset % 16 != 0 || source.index_offset % stride != 0 ||
            !fits(source.position_offset, source.vertex_count, 16, source.vertices->length()) ||
            !fits(source.index_offset, source.index_count, stride, source.indices->length()))
        {
            invalidate();
            return false;
        }
        for (std::size_t value = 0; value < 16; ++value)
        {
            if (!std::isfinite(source.modelview_projection[value]))
            {
                invalidate();
                return false;
            }
        }
        if (!admit()) return false;
        if (mOpaqueTail && !records.empty())
        {
            auto& previous = records.back();
            if (!previous.terrain && !previous.avatar && !previous.skin_palette &&
                previous.coverage == ALMetalWorldSpotShadowCoverage::opaque &&
                previous.texture_count == 0 &&
                previous.vertices == source.vertices && previous.indices == source.indices &&
                previous.position_offset == source.position_offset &&
                previous.vertex_count == source.vertex_count &&
                previous.index_type == source.index_type &&
                previous.double_sided == source.double_sided &&
                mReverseZ == source.reverse_z &&
                previous.index_count % 3 == 0 && source.index_count % 3 == 0 &&
                source.index_count <= std::numeric_limits<std::uint32_t>::max() - previous.index_count &&
                previous.index_count <= (std::numeric_limits<std::size_t>::max() - previous.index_offset) / stride &&
                previous.index_offset + previous.index_count * stride == source.index_offset &&
                std::memcmp(previous.modelview_projection, source.modelview_projection,
                            sizeof(previous.modelview_projection)) == 0)
            {
                previous.index_count += source.index_count;
                ++mMerged;
                return true;
            }
        }
        ALMetalWorldSpotShadowBatch* appended = nullptr;
        try
        {
            appended = &records.emplace_back();
        }
        catch (...)
        {
            invalidate();
            throw;
        }
        auto& record = *appended;
        record.vertices = source.vertices;
        record.indices = source.indices;
        record.position_offset = source.position_offset;
        record.index_offset = source.index_offset;
        record.vertex_count = source.vertex_count;
        record.index_count = source.index_count;
        record.index_type = source.index_type;
        record.double_sided = source.double_sided;
        std::memcpy(record.modelview_projection, source.modelview_projection,
                    sizeof(record.modelview_projection));
        mOpaqueTail = true;
        mReverseZ = source.reverse_z;
        return true;
    }

private:
    static bool fits(std::size_t offset, std::size_t count, std::size_t stride,
                     std::size_t length) noexcept
    {
        return offset <= length && count <= (length - offset) / stride;
    }
    bool admit() noexcept
    {
        if (!mValid || mSources == std::numeric_limits<std::size_t>::max())
        {
            invalidate();
            return false;
        }
        ++mSources;
        return true;
    }
    std::size_t mSources = 0;
    std::size_t mMerged = 0;
    bool mValid = true;
    bool mOpaqueTail = false;
    bool mReverseZ = false;
};
