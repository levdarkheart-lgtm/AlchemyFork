/**
 * @file pipeline_local_light_candidate_native.inl
 * @brief Cheap original limited-prefix point-work eligibility, owner only.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
    bool point_candidate = false;
    S32 considered_lights = 0;
    for (auto it = mNearbyLights.begin(); it != mNearbyLights.end(); ++it)
    {
        if (++considered_lights > local_light_count) break;
        const auto* drawable = it->drawable;
        const auto* volume = drawable ? drawable->getVOVolume() : nullptr;
        if (volume && !(volume->isAttachment() && !sRenderAttachedLights) &&
            !volume->isLightSpotlight())
        {
            point_candidate = true;
            break;
        }
    }
