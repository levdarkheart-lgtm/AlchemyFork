/**
 * @file alsculptcopy.h
 * @brief The CPU copy a sculpt builds from, at its map's proportions.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#ifndef AL_SCULPT_COPY_H
#define AL_SCULPT_COPY_H

#include "llimage.h"

namespace ALSculptCopy
{
struct Size
{
    S32 width = 0;
    S32 height = 0;
    S32 discard = -1;
    S32 pixels() const { return width * height; }
};

inline Size size(S32 width, S32 height, S32 discard, S32 budget_width, S32 budget_height)
{
    const S64 budget = S64(llmax(budget_width, 1)) * llmax(budget_height, 1);
    while (S64(width) * height > budget && width > 1 && height > 1)
    {
        width >>= 1;
        height >>= 1;
        ++discard;
    }
    return {width, height, discard};
}

struct Selection
{
    LLImageRaw* image;
    S32 discard;
    S32 pixels() const { return image ? image->getWidth() * image->getHeight() : 0; }
};

inline Selection select(LLImageRaw* raw, S32 raw_discard, LLImageRaw* saved, S32 saved_discard)
{
    const S32 raw_pixels = raw ? raw->getWidth() * raw->getHeight() : 0;
    return saved && saved->getWidth() * saved->getHeight() > raw_pixels
        ? Selection{saved, saved_discard} : Selection{raw, raw_discard};
}

// The actual saved-copy producer, shared by decoded and native-readback maps.
// Callers own a source shared lock, as saveRawImage already does.
inline LLPointer<LLImageRaw> image(const LLImageRaw* source, Size dimensions)
{
    if (!source || dimensions.width <= 0 || dimensions.height <= 0)
    {
        return nullptr;
    }
    if (source->getWidth() == dimensions.width && source->getHeight() == dimensions.height)
    {
        return new LLImageRaw(source->getData(), dimensions.width,
            dimensions.height, source->getComponents());
    }
    LLPointer<LLImageRaw> copy = new LLImageRaw(
        dimensions.width, dimensions.height, source->getComponents());
    copy->copyScaled(source);
    return copy;
}

inline bool needsSave(S32 discard, S32 pixels, S32 saved_discard, S32 saved_pixels, bool sculpt)
{
    return saved_discard < 0 || discard < saved_discard ||
        (sculpt && discard == saved_discard && pixels > saved_pixels);
}

// A saved image can have the decode's level but fewer actual pixels. A later
// larger image at that level is therefore a new sculpt, not a duplicate.
inline bool needsRebuild(S32 discard, S32 pixels, S32 built_discard, S32 built_pixels)
{
    return discard >= 0 && (built_discard < 0 || discard < built_discard ||
        (discard == built_discard && pixels > built_pixels));
}

inline bool alreadyBuilt(S32 discard, S32 pixels, S32 built_discard, S32 built_pixels)
{
    return discard == built_discard && pixels <= built_pixels;
}
}
#endif
