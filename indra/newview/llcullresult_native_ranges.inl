/**
 * @file llcullresult_native_ranges.inl
 * @brief Ordered native group admission, shared with actual-owner fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
bool LLCullResult::pushNativeSimpleRange(std::shared_ptr<const void> publication,
                                       U32 draw_count, const ALNativeRegionModel::Snapshot& model) noexcept
{
    if (!publication || !draw_count || !model ||
        mNativeSimpleRanges.size() == mNativeSimpleRanges.max_size() ||
        U64(draw_count) + mNativeSimpleDrawCount + mRenderMapSize[LLRenderPass::PASS_SIMPLE] >
            std::numeric_limits<U32>::max())
        return false;
    try
    {
        mNativeSimpleRanges.push_back({std::move(publication),
            mRenderMapSize[LLRenderPass::PASS_SIMPLE], draw_count, model});
        mNativeSimpleDrawCount += draw_count;
        return true;
    }
    catch (const std::bad_alloc&) { return false; }
}

bool LLCullResult::replaceNativeSimpleRange(std::shared_ptr<const void> publication,
    U32 raw_begin, std::span<LLDrawInfo* const> expected_raw,
    const ALNativeRegionModel::Snapshot& model) noexcept
{
    const U32 raw_count = mRenderMapSize[LLRenderPass::PASS_SIMPLE];
    if (!publication || !model || expected_raw.empty() ||
        expected_raw.size() > std::numeric_limits<U32>::max() ||
        raw_begin > raw_count || expected_raw.size() > raw_count - raw_begin)
        return false;
    const auto count = static_cast<U32>(expected_raw.size());
    const U64 raw_end = U64(raw_begin) + count;
    const auto* raw = beginRenderMap(LLRenderPass::PASS_SIMPLE);
    if (!raw) return false;
    for (std::size_t index = 0; index < expected_raw.size(); ++index)
        if (!expected_raw[index] || raw[raw_begin + index] != expected_raw[index]) return false;

    std::size_t insertion = 0;
    U32 previous = 0;
    U64 previous_raw_end = 0;
    for (std::size_t index = 0; index < mNativeSimpleRanges.size(); ++index)
    {
        const auto& range = mNativeSimpleRanges[index];
        const U64 end = U64(range.legacy_offset) + range.raw_skip;
        if (!range.publication || !range.draw_count || !range.model ||
            (index && range.legacy_offset < previous) || range.legacy_offset < previous_raw_end ||
            range.legacy_offset > raw_count ||
            (range.raw_skip && range.raw_skip != range.draw_count) || end > raw_count)
            return false;
        if (range.raw_skip)
        {
            if (raw_begin < end && range.legacy_offset < raw_end) return false;
        }
        else if (range.legacy_offset > raw_begin && range.legacy_offset < raw_end)
            return false; // A warm insertion inside the slice must keep its exact position.
        if (range.legacy_offset <= raw_begin) insertion = index + 1;
        previous = range.legacy_offset;
        if (range.raw_skip) previous_raw_end = end;
    }
    if (mNativeSimpleRanges.size() == mNativeSimpleRanges.max_size()) return false;
    try
    {
        NativeSimpleRange value{std::move(publication), raw_begin, count, model, count};
        mNativeSimpleRanges.insert(mNativeSimpleRanges.begin() +
            static_cast<std::vector<NativeSimpleRange>::difference_type>(insertion), std::move(value));
        // Raw storage already contributes every replaced draw. Unlike warm
        // admission this operation adds no logical count and moves no raw row.
        return true;
    }
    catch (const std::bad_alloc&) { return false; }
}

bool LLCullResult::pushNativeMaterialRange(U32 type, std::shared_ptr<const void> publication,
                                         U32 draw_count, const ALNativeRegionModel::Snapshot& model) noexcept
{
    if (type >= LLRenderPass::NUM_RENDER_TYPES || type == LLRenderPass::PASS_SIMPLE ||
        !publication || !draw_count || !model ||
        U64(draw_count) + mNativeMaterialDrawCount[type] + mRenderMapSize[type] > std::numeric_limits<U32>::max())
        return false;
    try
    {
        mNativeMaterialRanges[type].push_back({std::move(publication), mRenderMapSize[type], draw_count, model});
        mNativeMaterialDrawCount[type] += draw_count;
        return true;
    }
    catch (const std::bad_alloc&) { return false; }
}

bool LLCullResult::replaceNativeMaterialRange(U32 type, std::shared_ptr<const void> publication,
    U32 raw_begin, std::span<LLDrawInfo* const> expected_raw,
    const ALNativeRegionModel::Snapshot& model) noexcept
{
    if (type >= LLRenderPass::NUM_RENDER_TYPES || type == LLRenderPass::PASS_SIMPLE ||
        !publication || !model || expected_raw.empty() ||
        expected_raw.size() > std::numeric_limits<U32>::max() ||
        raw_begin > mRenderMapSize[type] || expected_raw.size() > mRenderMapSize[type] - raw_begin)
        return false;
    const auto count = static_cast<U32>(expected_raw.size());
    const U64 raw_end = U64(raw_begin) + count;
    const auto* raw = beginRenderMap(type);
    if (!raw) return false;
    for (std::size_t index = 0; index < expected_raw.size(); ++index)
        if (!expected_raw[index] || raw[raw_begin + index] != expected_raw[index]) return false;

    const auto found = mNativeMaterialRanges.find(type);
    std::size_t insertion = 0;
    if (found != mNativeMaterialRanges.end())
    {
        U32 previous = 0;
        U64 previous_raw_end = 0;
        for (std::size_t index = 0; index < found->second.size(); ++index)
        {
            const auto& range = found->second[index];
            const U64 end = U64(range.legacy_offset) + range.raw_skip;
            if (!range.publication || !range.draw_count || !range.model ||
                (index && range.legacy_offset < previous) || range.legacy_offset < previous_raw_end ||
                range.legacy_offset > mRenderMapSize[type] ||
                (range.raw_skip && range.raw_skip != range.draw_count) || end > mRenderMapSize[type])
                return false;
            if (range.raw_skip)
            {
                if (raw_begin < end && range.legacy_offset < raw_end) return false;
            }
            else if (range.legacy_offset > raw_begin && range.legacy_offset < raw_end)
                return false; // A warm insertion inside the slice must keep its exact position.
            if (range.legacy_offset <= raw_begin) insertion = index + 1;
            previous = range.legacy_offset;
            if (range.raw_skip) previous_raw_end = end;
        }
        if (found->second.size() == found->second.max_size()) return false;
    }
    try
    {
        NativeMaterialRange value{std::move(publication), raw_begin, count, model, count};
        if (found == mNativeMaterialRanges.end())
        {
            // Stage first: allocation failure must not leave an empty map row.
            std::vector<NativeMaterialRange> ranges;
            ranges.push_back(std::move(value));
            return mNativeMaterialRanges.emplace(type, std::move(ranges)).second;
        }
        auto& ranges = found->second;
        ranges.insert(ranges.begin() + static_cast<std::vector<NativeMaterialRange>::difference_type>(insertion),
            std::move(value));
        // Raw storage already contributes every replaced draw. Unlike warm
        // admission this operation adds no logical count and moves no raw row.
        return true;
    }
    catch (const std::bad_alloc&) { return false; }
}


