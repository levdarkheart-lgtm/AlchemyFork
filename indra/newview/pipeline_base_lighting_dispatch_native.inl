/** Both ordinary and probe views need a fresh lighting color owner. PPD494. */
        renderMetalWorldBaseLighting(
            *camera, has_sun_shadow, has_ssao, ssao_scale, ssao_max);
