/**
 * @file pipeline_local_light_values_native.inl
 * @brief Actual owner-thread point-light value gather, without native resources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Function-local include; no viewer pointer enters the owned numeric result.
 * Original1eb44 pipeline11928ff limits before filters, fades/frustum-culls,
 * and orders camera-outside lights before camera-inside lights.
 */
    auto& bounded_lights = values.lights;
    auto& fullscreen_lights = values.inside_lights;
    bounded_lights.clear();
    fullscreen_lights.clear();
    const std::size_t light_capacity = std::min<std::size_t>(
        static_cast<std::size_t>(std::max(local_light_count, 0)),
        mNearbyLights.size());
    bounded_lights.reserve(light_capacity);
    fullscreen_lights.reserve(light_capacity);
    const LLVector3& camera_origin = camera.getOrigin();
    S32 count = 0;
    for (light_set_t::iterator iter = mNearbyLights.begin();
         iter != mNearbyLights.end(); ++iter)
    {
        ++count;
        if (count > local_light_count)
        {
            break;
        }

        LLDrawable* drawable = iter->drawable;
        LLVOVolume* volume = drawable ? drawable->getVOVolume() : nullptr;
        if (!volume || (volume->isAttachment() && !sRenderAttachedLights))
        {
            continue;
        }

        LLVector4a center;
        center.load3(drawable->getPositionAgent().mV);
        const F32 radius = volume->getLightRadius() * 1.5f;
        LLColor3 color = volume->getLightLinearColor() * light_scale;
        color *= light_fade_brightness(iter->fade);
        if (color.magVecSquared() < 0.001f || radius <= 0.001f)
        {
            continue;
        }

        LLVector4a extent;
        extent.splat(radius);
        if (camera.AABBInFrustumNoFarClip(center, extent) == 0 ||
            volume->isLightSpotlight())
        {
            // Projectors own cookie, ambiance and optional spot-shadow
            // semantics and remain a separate downstream migration pass.
            continue;
        }

        LLVector4a eye_center;
        modelview.affineTransform(center, eye_center);
        ALMetalWorldLocalLight light;
        light.position_radius[0] = eye_center[0];
        light.position_radius[1] = eye_center[1];
        light.position_radius[2] = eye_center[2];
        light.position_radius[3] = radius;
        light.color_falloff[0] = color.mV[0];
        light.color_falloff[1] = color.mV[1];
        light.color_falloff[2] = color.mV[2];
        light.color_falloff[3] =
            volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF);
        const F32* world_center = center.getF32ptr();
        const bool camera_outside =
            camera_origin.mV[0] > world_center[0] + radius + 0.2f ||
            camera_origin.mV[0] < world_center[0] - radius - 0.2f ||
            camera_origin.mV[1] > world_center[1] + radius + 0.2f ||
            camera_origin.mV[1] < world_center[1] - radius - 0.2f ||
            camera_origin.mV[2] > world_center[2] + radius + 0.2f ||
            camera_origin.mV[2] < world_center[2] - radius - 0.2f;
        if (camera_outside)
        {
            setMetalLightScreenRect(
                light.position_radius, light.screen_rect,
                camera.getProjection(), light_width, light_height);
            bounded_lights.push_back(light);
        }
        else
        {
            fullscreen_lights.push_back(light);
        }
    }

    values.bounded_count = bounded_lights.size();
    bounded_lights.insert(
        bounded_lights.end(), fullscreen_lights.begin(), fullscreen_lights.end());
    fullscreen_lights.clear();
    values.additive_pass_count = light_capacity;
