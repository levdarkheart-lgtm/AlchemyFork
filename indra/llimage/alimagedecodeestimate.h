/** Checked transient CPU decode-pressure estimate, not a physical RAM cap (PPD486). */
#pragma once

#include <cstdint>
#include <limits>

namespace ALImageDecodeEstimate
{
struct Cost
{
    std::uint64_t bytes = 0;
    bool known = false;
};

inline Cost metadata(std::uint64_t width, std::uint64_t height,
                     std::uint64_t components, bool needs_aux, bool openht) noexcept
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (!width || !height || !components || width > maximum / height) return {};
    const auto pixels = width * height;
    if (components > maximum / pixels) return {};
    const auto primary = pixels * components;
    // The current worker initially allocates full metadata dimensions even
    // when a reduced discard was requested. OpenHT separately owns int32
    // component planes before copying into that raw image. Its private
    // internal workspace is not claimed by this known-parts estimate.
    const auto multiplier = openht ? 1u + sizeof(std::int32_t) : 1u;
    if (primary > maximum / multiplier) return {};
    const auto decoded = primary * multiplier;
    if (needs_aux && pixels > maximum - decoded) return {};
    return {decoded + (needs_aux ? pixels : 0), true};
}
}
