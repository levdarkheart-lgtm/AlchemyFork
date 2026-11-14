/**
 * @file alpickactionpolicy.h
 * @brief Readiness and ownership rules for the Picks context menu.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include <string_view>

inline constexpr bool alPickActionEnabled(std::string_view action, bool valid_selection,
                                         bool own_selection, bool has_location) noexcept
{
    if (!valid_selection) return false;
    // Detail panels request their own data, so Info does not need a location.
    if (action == "info") return true;
    if (action == "edit" || action == "delete") return own_selection;
    if (action == "teleport" || action == "map") return has_location;
    return false;
}
