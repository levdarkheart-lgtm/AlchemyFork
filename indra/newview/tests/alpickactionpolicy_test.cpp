/**
 * @file alpickactionpolicy_test.cpp
 * @brief Picks context-menu ownership and location readiness contracts.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"

#include "../alpickactionpolicy.h"

#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Picks actions retain ownership and loaded-location requirements", "[picks-action-policy]")
{
    constexpr std::array<std::string_view, 6> actions =
        {"info", "edit", "delete", "teleport", "map", "unknown"};
    struct Case
    {
        const char* description;
        bool valid_selection;
        bool own_selection;
        bool has_location;
        std::array<bool, 6> enabled;
    };
    constexpr std::array<Case, 6> cases = {{
        {"No selection", false, false, false, {false, false, false, false, false, false}},
        {"Stale or foreign-profile selection", false, true, true, {false, false, false, false, false, false}},
        {"Another avatar's selected item, location pending", true, false, false, {true, false, false, false, false, false}},
        {"Another avatar's selected item, location loaded", true, false, true, {true, false, false, true, true, false}},
        {"Own selected item, location pending", true, true, false, {true, true, true, false, false, false}},
        {"Own selected item, location loaded", true, true, true, {true, true, true, true, true, false}},
    }};
    for (const auto& state : cases)
    {
        INFO(state.description);
        for (std::size_t index = 0; index < actions.size(); ++index)
        {
            INFO(actions[index]);
            CHECK(alPickActionEnabled(actions[index], state.valid_selection,
                                      state.own_selection, state.has_location) == state.enabled[index]);
        }
    }
}
