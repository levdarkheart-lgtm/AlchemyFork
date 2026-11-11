/**
 * @file pipeline_finalize_config.inl
 * @brief CPU policy and fit state at the native frame completion boundary.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// The original cutout seed quads drained the immediate stream and left no
// shader bound, with MODELVIEW selected. Keep that real CPU boundary at its
// original pool position; native coverage does not need duplicate geometry.
void LLPipeline::prepareCutoutAlphaStream()
{
    LLGLSLShader::unbind();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}

// Production method bodies are also compiled by the focused CPU policy test.
// No viewer instance or Metal resource is constructed by that test.

bool LLPipeline::consumeReferenceStillRequest()
{
    // Called only after the native post source is valid. A cutout or probe
    // leaves the request armed for the next ordinary frame.
    if (!mReferenceStillWanted || sCutoutRender || gCubeSnapshot)
    {
        return false;
    }
    mReferenceStillWanted = false;
    return true;
}

std::unique_ptr<ALMetalRenderTarget> LLPipeline::captureReferenceStill(
    const ALMetalRenderTarget& source)
{
    if (sCutoutRender || gCubeSnapshot || !mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->renderer)
    {
        return nullptr;
    }
    auto& capture = *mMetalWorldSimpleCapture;
    const auto& source_desc = source.descriptor();
    if (capture.reference_still_target)
    {
        const auto& still_desc = capture.reference_still_target->descriptor();
        if (still_desc.width != source_desc.width ||
            still_desc.height != source_desc.height)
        {
            capture.reference_still_target.reset();
        }
    }
    if (!consumeReferenceStillRequest())
    {
        return nullptr;
    }
    // Keep the preceding committed still until a replacement commits.
    // Allocation/encoding failure consumes the attempt, not the old image.
    std::string error;
    auto pending = capture.renderer->capturePresentationReference(source, &error);
    if (!pending)
    {
        LL_WARNS("Metal") << "Native presentation-reference capture failed: " << error << LL_ENDL;
    }
    return pending;
}

void LLPipeline::publishReferenceStill(std::unique_ptr<ALMetalRenderTarget> pending)
{
    if (pending && mMetalWorldSimpleCapture)
    {
        mMetalWorldSimpleCapture->reference_still_target = std::move(pending);
    }
}

F32 LLPipeline::lensDistortionFitScale(const LensDistortFit::Inputs& inputs)
{
    const auto& [k1, k2, p1, p2, cx, cy, axis_x, axis_y, sq_x, fit_mode] = inputs;
    const F32 sq_y = 1.f;
    // The shader's warp with the fit scale left at 1. Scale is a pure
    // multiplier on the result, so solving for it afterwards is exact
    // rather than iterative.
    auto base_offset = [&](F32 u, F32 v, F32& out_x, F32& out_y)
    {
        const F32 qx = (u - 0.5f - cx) * axis_x;
        const F32 qy = (v - 0.5f - cy) * axis_y;
        const F32 r2 = qx * qx + qy * qy;
        const F32 radial = 1.f + r2 * (k1 + r2 * k2);
        const F32 tx = 2.f * p1 * qx * qy + p2 * (r2 + 2.f * qx * qx);
        const F32 ty = p1 * (r2 + 2.f * qy * qy) + 2.f * p2 * qx * qy;
        out_x = (qx * radial + tx) / axis_x * sq_x;
        out_y = (qy * radial + ty) / axis_y * sq_y;
    };

    // Scale at which the ray from the optical axis along `base` leaves
    // the source frame -- a two-slab exit test. Below that scale the
    // sample is inside the image; above it, off the edge and black.
    const F32 origin_x = 0.5f + cx;
    const F32 origin_y = 0.5f + cy;
    auto exit_scale = [&](F32 bx, F32 by) -> F32
    {
        F32 best = 1e30f;
        if (bx > 1e-6f || bx < -1e-6f)
        {
            best = llmin(best, ((bx > 0.f ? 1.f : 0.f) - origin_x) / bx);
        }
        if (by > 1e-6f || by < -1e-6f)
        {
            best = llmin(best, ((by > 0.f ? 1.f : 0.f) - origin_y) / by);
        }
        return best;
    };

    F32 fit_scale = 1.f;
    if (inputs == mLensDistortFit.inputs)
    {
        fit_scale = mLensDistortFit.scale;
    }
    else if (fit_mode != 0)
    {
        // Walk the frame boundary densely instead of probing only the
        // corners and edge midpoints.
        //
        // With a non-zero secondary coefficient the radial polynomial
        // 1 + k1*r^2 + k2*r^4 stops being monotonic in r -- that
        // non-monotonicity *is* the moustache bend -- so the largest
        // outward displacement along an edge can fall between two
        // sparse probes. Eight probes let that region escape the solve,
        // which showed up as curved black arcs along the edges at
        // extreme settings even in Fit mode. Dense sampling is also
        // what keeps this honest once the tangential terms are
        // non-zero, since those break the clean radial structure a
        // corners-dominate argument leans on.
        //
        // Sixty-four evaluations of a short polynomial, once per change
        // of the inputs above.
        const S32 probes_per_edge = 16;

        // Fit (1): the smallest exit scale over every probe with a
        // satisfiable constraint, so nothing that scaling can save
        // ever goes black. Not quite "nothing goes black anywhere":
        // with the optical axis pinned on the frame edge, fold-over
        // can fling a pixel straight off that edge, and no positive
        // scale brings it back -- Fit degrades to best-effort there
        // rather than collapsing the whole frame chasing an
        // impossible constraint.
        // Fill (2): the largest, so every probe is reachable -- the
        // whole source stays visible, at the cost of black corners.
        F32 solved = (fit_mode == 1) ? 1e30f : 0.f;

        auto consider_probe = [&](F32 u, F32 v)
        {
            F32 bx, by;
            base_offset(u, v, bx, by);
            const F32 s = exit_scale(bx, by);
            // >= 1e30 is no constraint at all (base ~ 0 near the
            // optical axis); ~0 is the unsatisfiable case above.
            if (s < 1e-4f || s >= 1e30f)
            {
                return;
            }
            solved = (fit_mode == 1) ? llmin(solved, s) : llmax(solved, s);
        };

        for (S32 edge = 0; edge < 4; ++edge)
        {
            for (S32 i = 0; i < probes_per_edge; ++i)
            {
                // t == 0 lands exactly on a corner, so all four corners
                // are still probed; the rest subdivide each edge.
                const F32 t = (F32)i / (F32)probes_per_edge;
                switch (edge)
                {
                    case 0:  consider_probe(t, 0.f);        break;  // top
                    case 1:  consider_probe(1.f, t);        break;  // right
                    case 2:  consider_probe(1.f - t, 1.f);  break;  // bottom
                    default: consider_probe(0.f, 1.f - t);  break;  // left
                }
            }
        }

        // Fit also probes the interior. Boundary-only probing assumes
        // the binding constraint lies on the frame edge, which holds
        // while the polynomial is monotonic over the frame -- but
        // strong barrel folds it over (the radial factor goes negative
        // past its turning point), and then an interior pixel can be
        // flung further than any boundary pixel: at clamp-edge settings
        // a boundary-only solve passes all 64 probes while a mid-frame
        // island escapes. A 15x15 grid catches every satisfiable
        // interior bind for a few hundred cheap evaluations. Fit only:
        // Fill takes the max, which the
        // near-axis interior would poison with huge exit scales.
        if (fit_mode == 1)
        {
            const S32 grid = 15;
            for (S32 gy = 1; gy <= grid; ++gy)
            {
                for (S32 gx = 1; gx <= grid; ++gx)
                {
                    consider_probe((F32)gx / (F32)(grid + 1),
                                   (F32)gy / (F32)(grid + 1));
                }
            }
        }

        if (solved > 0.f && solved < 1e30f)
        {
            fit_scale = llclamp(solved, 0.1f, 10.f);
        }
    }
    mLensDistortFit.inputs = inputs;
    mLensDistortFit.scale = fit_scale;
    return fit_scale;
}

bool LLPipeline::configureMetalPresentationLens(ALMetalWorldPresentationScene& presentation)
{
    if (presentation.output_width == 0 || presentation.output_height == 0)
    {
        return false;
    }
    static LLCachedControl<F32> distort_amount(gSavedSettings, "RenderLensDistortionAmount", 0.f);
    static LLCachedControl<F32> distort_k1(gSavedSettings, "RenderLensDistortionK1", -.2f);
    static LLCachedControl<F32> distort_k2(gSavedSettings, "RenderLensDistortionK2", 0.f);
    static LLCachedControl<F32> distort_squeeze(gSavedSettings, "RenderLensDistortionSqueeze", 1.f);
    static LLCachedControl<S32> distort_fit(gSavedSettings, "RenderLensDistortionFit", 1);
    static LLCachedControl<LLVector3> distort_center(gSavedSettings, "RenderLensDistortionCenter", LLVector3::zero);
    static LLCachedControl<LLVector3> distort_tangential(gSavedSettings, "RenderLensDistortionTangential", LLVector3::zero);
    const F32 distortion = suppressesLensAndPrintEffects() ? 0.f : llclamp(distort_amount(), 0.f, 1.f);
    presentation.lens_distortion_amount = distortion;
    if (distortion <= 0.f)
    {
        return true;
    }
    const F32 k1 = llclamp(distort_k1(), -.5f, .5f) * distortion;
    const F32 k2 = llclamp(distort_k2(), -.25f, .25f) * distortion;
    const F32 p1 = llclamp(distort_tangential().mV[0], -.05f, .05f) * distortion;
    const F32 p2 = llclamp(distort_tangential().mV[1], -.05f, .05f) * distortion;
    const F32 cx = llclamp(distort_center().mV[0], -.5f, .5f) * distortion;
    const F32 cy = llclamp(distort_center().mV[1], -.5f, .5f) * distortion;
    const F32 squeeze = 1.f + (llclamp(distort_squeeze(), .5f, 2.5f) - 1.f) * distortion;
    // The original solver and final blit use the final viewport's aspect.
    // Render-resolution scaling can change the retained source independently.
    const F32 aspect = F32(presentation.output_width) / F32(presentation.output_height);
    const F32 axis_x = llmax(aspect, 1.f);
    const F32 axis_y = llmax(1.f / llmax(aspect, 1e-4f), 1.f);
    const F32 sq_x = 1.f / squeeze;
    const LensDistortFit::Inputs inputs{ k1, k2, p1, p2, cx, cy, axis_x, axis_y, sq_x, llclamp(distort_fit(), 0, 2) };
    presentation.lens_distortion_k[0] = k1;
    presentation.lens_distortion_k[1] = k2;
    presentation.lens_distortion_scale = lensDistortionFitScale(inputs);
    presentation.lens_distortion_squeeze[0] = sq_x;
    presentation.lens_distortion_squeeze[1] = 1.f;
    presentation.lens_distortion_center[0] = cx;
    presentation.lens_distortion_center[1] = cy;
    presentation.lens_distortion_tangential[0] = p1;
    presentation.lens_distortion_tangential[1] = p2;
    return true;
}

bool LLPipeline::wantsMetalDepthOfField(const DepthOfFieldConfig& dof) const
{
    return mMetalWorldSimpleCapture &&
        dof.enabled && !sCutoutRender && !gCubeSnapshot &&
        isMainWorldPass() && mRT == &mMainRT;
}

U32 LLPipeline::requestedMetalBloomMipCount(
    bool exact_dof, F32 bloom_strength, F32 halation_strength,
    U32 source_width, U32 source_height) const
{
    const bool cutout_snapshot = sCutoutRender && mMetalWorldSnapshotRequested;
    const auto requested = mResourceInputs.bloomShape(source_width, source_height);
    const bool eligible = mMetalWorldSimpleCapture && mMetalWorldSimpleCapture->hdr &&
        !cutout_snapshot && exact_dof &&
        (bloom_strength > 0.f || halation_strength > 0.f) &&
        isMainWorldPass() && mRT == &mMainRT &&
        requested.mMipCount >= 3 && canCaptureMetalWorldPresentation();
    return eligible ? requested.mMipCount : 0;
}

bool LLPipeline::metalScopeSourceCurrent(U32 width, U32 height) const
{
    const MetalWorldSimpleCapture* capture = mMetalWorldSimpleCapture.get();
    return capture && capture->renderer && capture->device &&
        capture->display_post_target &&
        capture->display_post_frame == gFrameCount &&
        capture->depth_of_field_frame == gFrameCount &&
        canCaptureMetalScopeSample(false) &&
        capture->display_post_target->descriptor().width == width &&
        capture->display_post_target->descriptor().height == height;
}

void LLPipeline::updateNativeExposureReadiness()
{
    // Original generateExposure was called only for a non-cutout HDR frame.
    // Its fixed-EV cleanup/readiness was the sole live tail of that draw path.
    if (!hdrEnabled() || sCutoutRender)
    {
        return;
    }
    static LLCachedControl<bool> dynamic_exposure_enabled(gSavedSettings, "RenderDynamicExposureEnabled", true);
    const bool meter = dynamic_exposure_enabled && !LLRender::sClassicMode;
    MetalWorldSimpleCapture* capture = mMetalWorldSimpleCapture.get();
    if (capture && !meter)
    {
        capture->exposure_target.reset();
        capture->exposure_frame = std::numeric_limits<U32>::max();
        capture->exposure_epoch = 0;
    }
    mMetalExposureHistoryReady = meter && capture &&
        capture->exposure_target &&
        capture->exposure_epoch == mMetalExposureHistoryEpoch;
}


void LLPipeline::requestScenePixel(S32 x, S32 y, scene_pixel_cb_t callback)
{
    if (!callback)
    {
        return;
    }
    mScenePixelX = x;
    mScenePixelY = y;
    mScenePixelCallback = std::move(callback);
    mScenePixelPending = true;
}

bool LLPipeline::worldViewUV(S32 scaled_x, S32 scaled_y, F32& u, F32& v)
{
    // Everything here stays in scaled window space, and the answer comes out as
    // a fraction of the world view -- which is all a buffer covering that view
    // needs, whatever resolution it happens to be rendered at.
    //
    // The previous version converted the mouse to raw pixels by the *window's*
    // raw/scaled ratio and then measured it against the *world view's* raw
    // rect. Those are two different scalings, and mixing them leaves an error
    // proportional to the distance from the origin: dead on in one corner and
    // visibly adrift in the far one. Staying in one space cannot express that
    // mistake. It is also what the viewer itself does when it asks whether the
    // cursor is over the world (llviewerwindow.cpp, getWorldViewRectScaled
    // against mCurrentMousePoint).
    const LLRect world = gViewerWindow->getWorldViewRectScaled();
    if (world.getWidth() <= 0 || world.getHeight() <= 0)
    {
        return false;
    }

    u = (F32)(scaled_x - world.mLeft)   / (F32)world.getWidth();
    v = (F32)(scaled_y - world.mBottom) / (F32)world.getHeight();
    return (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f);
}

void LLPipeline::serviceScenePixelProbe()
{
    if (!mScenePixelPending || gCubeSnapshot)
    {
        return;
    }

    // Cleared before the callback runs, not after: the callback is free to ask
    // for another sample, and swallowing that request would make a tool that
    // stays armed sample exactly once.
    mScenePixelPending = false;
    scene_pixel_cb_t callback;
    callback.swap(mScenePixelCallback);

    // The mouse arrives in scaled window coordinates; the buffer is in raw
    // pixels, is only the world view rather than the whole window, and may be
    // rendered at a fraction of that again under resolution scaling. All three
    // conversions live in worldViewUV, in one place, because the scopes'
    // cursor readout needs exactly the same walk -- and the first version of
    // that one skipped the scaled-to-raw step, which is invisible on a machine
    // where the two spaces happen to match.
    F32 u = 0.f, v = 0.f;
    if (!worldViewUV(mScenePixelX, mScenePixelY, u, v))
    {
        // Clicked outside the 3D view. Nothing there to sample.
        return;
    }

#if LL_DARWIN
    if (mMetalWorldSimpleCapture &&
        !mMetalHeroWorldCaptureActive &&
        !mMetalReflectionWorldCaptureActive)
    {
        MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
        const ALMetalRenderTargetDescriptor* descriptor =
            capture.opaque_post_deferred_target
                ? &capture.opaque_post_deferred_target->descriptor()
                : nullptr;
        if (capture.renderer && descriptor &&
            capture.continuous_world_frame == gFrameCount &&
            descriptor->width != 0 && descriptor->height != 0)
        {
            const S32 px = llclamp(S32(u * F32(descriptor->width)),
                                  0, S32(descriptor->width) - 1);
            const S32 py = llclamp(S32(v * F32(descriptor->height)),
                                  0, S32(descriptor->height) - 1);
            F32 texel[4]{};
            // Native raw row zero is the original lower-left row. The
            // mouse-to-world UV therefore crosses without a vertical flip.
            std::string error;
            if (capture.renderer->readSceneColorPixel(
                    *capture.opaque_post_deferred_target,
                    static_cast<U32>(px), static_cast<U32>(py), texel,
                    &error))
            {
                callback(LLColor3(texel[0], texel[1], texel[2]));
                return;
            }
            LL_WARNS("Metal")
                << "Native scene-pixel readback failed; request dropped: "
                << error << LL_ENDL;
            return;
        }
    }
    LL_WARNS("Metal")
        << "Native scene-pixel source was not exact; request dropped"
        << LL_ENDL;
#endif
}


bool LLPipeline::hasReferenceStill() const
{
    return mMetalWorldSimpleCapture &&
        mMetalWorldSimpleCapture->reference_still_target != nullptr;
}

void LLPipeline::clearReferenceStill()
{
    mReferenceStillWanted = false;
#if LL_DARWIN
    if (mMetalWorldSimpleCapture)
    {
        mMetalWorldSimpleCapture->reference_still_target.reset();
    }
#endif
}


void LLPipeline::resetExposureHistory()
{
    // Native exposure bootstraps at EV 0, and flare at exact black. Retire
    // both histories even when no legacy target allocation exists.
    clearLensFlareState();
#if LL_DARWIN
    ++mMetalExposureHistoryEpoch;
    if (mMetalExposureHistoryEpoch == 0)
    {
        ++mMetalExposureHistoryEpoch;
    }
    mMetalExposureHistoryReady = false;
    for (const auto* capture_owner :
         { &mMetalWorldSimpleCapture,
           &mMetalProbeWorldSimpleCapture })
    {
        if (*capture_owner)
        {
            (*capture_owner)->exposure_target.reset();
            (*capture_owner)->exposure_frame =
                std::numeric_limits<U32>::max();
            (*capture_owner)->exposure_epoch = 0;
        }
    }
#endif
}

void LLPipeline::clearLensFlareState()
{
#if LL_DARWIN
    ++mMetalLensFlareHistoryEpoch;
    if (mMetalLensFlareHistoryEpoch == 0)
    {
        ++mMetalLensFlareHistoryEpoch;
    }
    mMetalLensFlareHistoryReady = false;
    if (mMetalWorldSimpleCapture)
    {
        mMetalWorldSimpleCapture->lens_flare_state_target.reset();
        mMetalWorldSimpleCapture->lens_flare_state_frame =
            std::numeric_limits<U32>::max();
        mMetalWorldSimpleCapture->lens_flare_state_epoch = 0;
    }
#endif
}

void LLPipeline::clearSceneMap()
{
    // Isolation clears the shared scene before its cutout flag is raised.
    // Drop both view owners so returning to an ordinary view cannot expose
    // the preceding scene through a swapped or same-sized history pair.
    for (auto* capture : {mMetalWorldSimpleCapture.get(),
                          mMetalProbeWorldSimpleCapture.get()})
    {
        if (!capture) continue;
        capture->impostor_screen_space_reflections = {};
        capture->impostor_ssr_history_target.reset();
    }
    // Binding the former scene target drained immediate work before the
    // isolation caller raised cutout. Preserve that ordering without a
    // duplicate attachment, mask/depth change or viewport mutation.
    if (mResourcePlan.usesScreenSpaceReflections())
    {
        gGL.flush();
    }
}


bool LLPipeline::wantsMetalScreenSpaceReflections() const
{
    return mResourcePlan.usesScreenSpaceReflections() &&
        !gCubeSnapshot && !sCutoutRender;
}

bool LLPipeline::hasMetalScreenSpaceReflectionHistory() const
{
    if (sCutoutRender || !mMetalWorldSimpleCapture)
    {
        return false;
    }
    const MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    const auto& reflections =
        capture.impostor_screen_space_reflections;
    if (!reflections.scene_color || !reflections.scene_depth ||
        capture.width == 0 || capture.height == 0 ||
        reflections.scene_color->descriptor().width !=
            capture.width ||
        reflections.scene_color->descriptor().height !=
            capture.height ||
        reflections.scene_color->descriptor().format !=
            (hdrEnabled() ? ALMetalPixelFormat::rg11b10_float
                          : ALMetalPixelFormat::rgba8_unorm) ||
        reflections.scene_depth->descriptor().width != capture.width ||
        reflections.scene_depth->descriptor().height != capture.height ||
        reflections.scene_depth->descriptor().format !=
            ALMetalPixelFormat::r32_float)
    {
        return false;
    }

    return true;
}

void LLPipeline::reconcileNativeHistoryState(
    const ALRenderResourcePlan& plan, bool hdr, bool render_sized_recreated)
{
    using Id = ALRenderResourcePlan::Id;
    const auto& before = mResourcePlan.desc(Id::MainGBuffer);
    const auto& after = plan.desc(Id::MainGBuffer);
    // The old SSR target rebuilt on enable, applied render size, HDR color
    // format, depth format, or a render-sized retry/recreation. Its native
    // replacement must forget that same preceding scene, even when all
    // remaining physical descriptions compare equal on an SSR-only toggle.
    if (plan.usesScreenSpaceReflections() &&
        (!mResourcePlan.usesScreenSpaceReflections() || render_sized_recreated ||
         !before || !after || before->width != after->width ||
         before->height != after->height || before->depth != after->depth ||
         mResourceInputs.mHDR != hdr))
    {
        clearSceneMap();
        // On first resources or SSR enable, the old target was already built
        // even though the previous plan had no SSR intent. Drain once for that
        // newly eligible next plan before the caller publishes it.
        if (!mResourcePlan.usesScreenSpaceReflections())
        {
            gGL.flush();
        }
    }
    // Every applied plan has a main G-buffer description; releaseRenderTargets
    // restores the empty plan. The old fixed exposure/flare descriptors built
    // only on that first reconciliation or recreation, never on a resize.
    // Existing explicit history resets (including HDR changes) stay with their
    // callers; this adds no new setting- or render-size reset.
    if (!before)
    {
        resetExposureHistory();
    }
}
