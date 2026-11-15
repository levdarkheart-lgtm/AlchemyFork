/**
 * @file almetalworlddraws.cpp
 * @brief Ordered ordinary draw and sealed cohort resolution without batch copies.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworld.h"
#include "almetalworldcohort.h"
#include <algorithm>
#include <limits>

const float* ALMetalWorldDraw::modelviewProjection() const noexcept
{return mView?mView->modelview_projection:mSource->modelview_projection;}
const float* ALMetalWorldDraw::modelview() const noexcept
{return mView?mView->modelview:mSource->modelview;}
const float* ALMetalWorldDraw::normalMatrix() const noexcept
{return mView?&mView->normal_matrix[0][0]:&mSource->normal_matrix[0][0];}
const float* ALMetalWorldDraw::riggedNormalMatrix() const noexcept
{return mView?&mView->rigged_normal_matrix[0][0]:&mSource->rigged_normal_matrix[0][0];}
const float* ALMetalWorldDraw::textureMatrix() const noexcept
{return mFixedTextureMatrix?mFixedTextureMatrix:(mView?mView->texture_matrix:mSource->texture_matrix);}

namespace
{
ALMetalWorldDraw baseDraw(const ALMetalWorldGBufferScene& scene,std::size_t index) noexcept
{
    if(!scene.draw_order_count)return ALMetalWorldDraw(scene.batches[index]);
    const auto token=scene.draw_order[index];const auto storage=alMetalWorldDrawStorageIndex(token);
    if(!alMetalWorldDrawIsReference(token))return ALMetalWorldDraw(scene.batches[storage]);
    const auto& reference=scene.references[storage];return ALMetalWorldDraw(*reference.source,&reference.view,nullptr,&reference);
}
}
std::size_t ALMetalWorldGBufferScene::drawCount() const noexcept
{
    std::size_t count=draw_order_count?draw_order_count:batch_count;
    if(cohort_count && !cohorts)return std::numeric_limits<std::size_t>::max();
    if(cohort_count)
    {
        // Shape validation proves every sorted insertion/prefix. The final
        // prefix then gives the logical count without scanning groups/draws.
        const auto& last=cohorts[cohort_count-1];
        if(!last.cohort || last.logical_begin<last.base_before ||
           last.logical_begin-last.base_before>std::numeric_limits<std::size_t>::max()-count)
            return std::numeric_limits<std::size_t>::max();
        count+=last.logical_begin-last.base_before;
        if(last.cohort->size()>std::numeric_limits<std::size_t>::max()-count)return std::numeric_limits<std::size_t>::max();
        count+=last.cohort->size();
    }
    return count;
}
ALMetalWorldDrawCursor::ALMetalWorldDrawCursor(const ALMetalWorldGBufferScene& scene,std::size_t first) noexcept
    :mScene(&scene),mLogical(first),mBase(first)
{
    if(!scene.cohort_count)return;
    const auto* upper=std::upper_bound(scene.cohorts,scene.cohorts+scene.cohort_count,first,
        [](auto index,const auto& view){return index<view.logical_begin;});
    mCohort=static_cast<std::size_t>(upper-scene.cohorts);
    if(mCohort)
    {
        const auto& prior=scene.cohorts[mCohort-1];
        const auto end=prior.logical_begin+prior.cohort->size();
        if(first<end){--mCohort;mBase=prior.base_before;}
        else mBase=first-(end-prior.base_before);
    }
}
const ALMetalWorldSourceCohortView* ALMetalWorldDrawCursor::cohort() const noexcept
{
    if(mCohort>=mScene->cohort_count)return nullptr;
    const auto& view=mScene->cohorts[mCohort];
    return mLogical>=view.logical_begin && mLogical-view.logical_begin<view.cohort->size()?&view:nullptr;
}
ALMetalWorldDraw ALMetalWorldDrawCursor::draw() const noexcept
{
    if(const auto* view=cohort())
    {
        const auto& source=view->cohort->source(mLogical-view->logical_begin);
        return ALMetalWorldDraw(source,&view->view,source.texture_matrix);
    }
    return baseDraw(*mScene,mBase);
}
void ALMetalWorldDrawCursor::advance(std::size_t count) noexcept
{
    const auto target=mLogical+count;
    while(mCohort<mScene->cohort_count)
    {
        const auto& view=mScene->cohorts[mCohort];const auto end=view.logical_begin+view.cohort->size();
        if(target<view.logical_begin){mBase+=target-mLogical;mLogical=target;return;}
        if(mLogical<view.logical_begin){mBase+=view.logical_begin-mLogical;mLogical=view.logical_begin;}
        if(target<end){mLogical=target;return;}
        mLogical=end;++mCohort;
    }
    mBase+=target-mLogical;mLogical=target;
}
ALMetalWorldDraw ALMetalWorldGBufferScene::draw(std::size_t index) const noexcept
{return ALMetalWorldDrawCursor(*this,index).draw();}
const ALMetalWorldSourceCohortView* alMetalWorldCohortAt(const ALMetalWorldGBufferScene& scene,std::size_t index) noexcept
{
    if(!scene.cohort_count || !scene.cohorts)return nullptr;
    const auto* upper=std::upper_bound(scene.cohorts,scene.cohorts+scene.cohort_count,index,
        [](auto logical,const auto& view){return logical<view.logical_begin;});
    if(upper==scene.cohorts)return nullptr;
    const auto* view=upper-1;
    return view->cohort && index-view->logical_begin<view->cohort->size()?view:nullptr;
}
bool alMetalWorldDrawStorageAvailable(const ALMetalWorldGBufferScene& scene,std::size_t index) noexcept
{
    if(index>=scene.drawCount())return false;
    if(const auto* view=alMetalWorldCohortAt(scene,index))
        return view->cohort->matchesOwners(view->cpu_owner,view->native_owner) &&
            ALMetalWorldSourceCohort::validView(view->view);
    // Use the same prefix resolution as the renderer without dereferencing a
    // source. Malformed public storage must fail before constructing a Draw.
    std::size_t base=index;
    if(scene.cohort_count)
    {
        if(!scene.cohorts)return false;
        const auto* upper=std::upper_bound(scene.cohorts,scene.cohorts+scene.cohort_count,index,
            [](auto logical,const auto& view){return logical<view.logical_begin;});
        if(upper!=scene.cohorts)
        {
            const auto& prior=*(upper-1);if(!prior.cohort)return false;
            const auto end=prior.logical_begin+prior.cohort->size();
            if(end<prior.base_before || index<end-prior.base_before)return false;
            base=index-(end-prior.base_before);
        }
    }
    const auto count=scene.draw_order_count?scene.draw_order_count:scene.batch_count;
    if(base>=count)return false;
    if(!scene.draw_order_count)return scene.batches && !scene.reference_count;
    if(!scene.draw_order)return false;
    const auto token=scene.draw_order[base];const auto storage=alMetalWorldDrawStorageIndex(token);
    if(!alMetalWorldDrawIsReference(token))return scene.batches && storage<scene.batch_count;
    return scene.references && storage<scene.reference_count && scene.references[storage].source &&
        scene.cpu_source_owner && scene.source_owner;
}
