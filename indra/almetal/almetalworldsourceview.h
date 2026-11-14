/**
 * @file almetalworldsourceview.h
 * @brief Copied owner provenance for the native source-row consumer.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldsourcepages.h"
#include <vector>

// CPU-only view metadata. No LL object, matrix address or producer payload is
// retained here. The scene independently pins its native resource generations.
// The native FrameView copies only its required GPU resources at freeze.
struct ALMetalWorldSourceView
{
    struct Candidate
    {
        ALMetalWorldSourceKey key;
        std::size_t draw_index = 0;
        std::uint32_t region = 0;
    };
    std::vector<Candidate> candidates;
    std::vector<ALMetalWorldSourceRegion> regions;
};
