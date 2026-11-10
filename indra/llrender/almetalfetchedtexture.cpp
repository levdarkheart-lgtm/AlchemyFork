/**
 * @file almetalfetchedtexture.cpp
 * @brief Resolve native terrain inputs with the original fetched-image fallback.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "almetalfetchedtexture.h"

const ALMetalTexture* ALMetalFetchedTextureBinding::nativeTexture() const
{
    return image.notNull() ? image->getMetalTexture() : nullptr;
}

bool alResolveMetalFetchedTexture(
    LLTexture* requested, LLTexture* default_image, LLTexture* null_image,
    ALMetalFetchedTextureBinding& binding)
{
    binding = {};
    if (!requested) return false;

    // Pin every candidate before a virtual update can replace any global
    // reference. Each distinct owner is tried once; self/cycles terminate.
    const std::array<LLPointer<LLTexture>, 3> candidates{
        requested, default_image, null_image};
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        LLTexture* owner = candidates[index].get();
        if (!owner) continue;
        bool duplicate = false;
        for (std::size_t earlier = 0; earlier < index; ++earlier)
            duplicate = duplicate || candidates[earlier].get() == owner;
        if (duplicate) continue;

        LLPointer<LLImageGL> image = owner->getGLTexture();
        if (image.isNull() || !image->getMetalTexture())
        {
            // Fetched textures test DELETED here; setActive can clear it.
            owner->forceImmediateUpdate();
            image = owner->getGLTexture();
        }
        owner->setActive();
        if (image.notNull()) image->forceUpdateBindStats();
        if (image.notNull() && image->getMetalTexture())
        {
            binding.owner = owner;
            binding.image = image;
            binding.fallback = index != 0;
            return true;
        }
    }
    return false;
}
