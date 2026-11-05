/**
 * @file class1/deferred/deferredBlock.glsl
 * @brief THE declaration of the shared "Deferred" engine UBO (UB_DEFERRED).
 *        Not compiled directly: loadShaderFile() splices this file's contents
 *        wherever a shader says //[ENGINE_BLOCK Deferred], so every consumer
 *        shares one declaration instead of a per-file copy.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 * $/LicenseInfo$
 *
 * The member order/types MUST byte-match (std140) the C++
 * LLPipeline::DeferredUBOData, packed by packDeferredConstants and registered for
 * debug validation with offsetof on that struct. ssao_effect_mat is COLUMN-major
 * (std140's default) and symmetric, so orientation is moot. The debug validator
 * asserts the layout at shader load.
 */

layout (std140) uniform Deferred
{
    mat3  ssao_effect_mat;
    float ssao_radius;
    float ssao_max_radius;
    float ssao_factor;
    float ssao_factor_inv;
    float specular_exponent;   // the Blinn-Phong exponent at full glossiness
};
