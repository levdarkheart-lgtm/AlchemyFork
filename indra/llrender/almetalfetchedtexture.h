/**
 * @file almetalfetchedtexture.h
 * @brief Native fetched-image bindings retain the viewer's loading placeholder.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include "llimagegl.h"
#include "lltexture.h"

#include <array>

struct ALMetalFetchedTextureBinding
{
    LLPointer<LLTexture> owner;
    LLPointer<LLImageGL> image;
    bool fallback = false;

    const ALMetalTexture* nativeTexture() const;
};

// No slot mutation or draw: reproduce the slow viewer bind's immediate
// revival and Default -> Null selection. Callers name the ready/default/null
// images explicitly, so this also works without a viewer or GL context.
// A fallback uses AnisoWrap and raw encoded RGB, irrespective of the
// requested image's sampler. Alpha is unchanged by the color-space view.
bool alResolveMetalFetchedTexture(
    LLTexture* requested, LLTexture* default_image, LLTexture* null_image,
    ALMetalFetchedTextureBinding& binding);

// One pool owns this snapshot through queued capture and command completion.
// Generated height/composition maps are deliberately not fetched inputs.
struct ALMetalTerrainFetchedBindings
{
    std::array<ALMetalFetchedTextureBinding, 4> base;
    std::array<ALMetalFetchedTextureBinding, 4> normal;
    std::array<ALMetalFetchedTextureBinding, 4> orm;
    std::array<ALMetalFetchedTextureBinding, 4> emissive;
    ALMetalFetchedTextureBinding blend;
    ALMetalFetchedTextureBinding overlay;
};
