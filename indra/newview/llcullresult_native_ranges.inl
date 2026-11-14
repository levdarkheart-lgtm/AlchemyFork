/**
 * @file llcullresult_native_ranges.inl
 * @brief Ordered native group admission, shared with actual-owner fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
bool LLCullResult::pushNativeSimpleRange(std::shared_ptr<const void> publication,
                                       U32 draw_count) noexcept
{
    if (!publication || !draw_count ||
        U64(draw_count) + mNativeSimpleDrawCount + mRenderMapSize[LLRenderPass::PASS_SIMPLE] >
            std::numeric_limits<U32>::max())
        return false;
    try
    {
        mNativeSimpleRanges.push_back({std::move(publication),
            mRenderMapSize[LLRenderPass::PASS_SIMPLE], draw_count});
        mNativeSimpleDrawCount += draw_count;
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


