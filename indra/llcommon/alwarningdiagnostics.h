/** Opt-in, bounded private input diagnostics for PPD-311/466. */
#pragma once

#include <array>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

namespace ALWarningDiagnostics
{
enum class Kind : unsigned { skin, skin_decode, bake, bake_decode, texture, material, http, count };
inline bool enabled()
{
    static const bool active = []
    {
        const char* value = std::getenv("ALCHEMY_WARNING_DIAGNOSTICS");
        return value && std::string_view(value) == "1";
    }();
    return active;
}

// Call at an existing failure or requested bake-decode observation boundary.
// Keys contain asset/context identity,
// never capability URLs, account names or credentials. Logs remain local.
inline bool take(Kind kind, std::string_view key) noexcept
{
    if (!enabled() || key.size() > 256) return false;
    const auto index = static_cast<unsigned>(kind);
    if (index >= static_cast<unsigned>(Kind::count)) return false;
    struct Keys
    {
        std::array<std::string, 64> values;
        unsigned used = 0;
    };
    static std::array<Keys, static_cast<unsigned>(Kind::count)> keys;
    static std::mutex mutex;
    try
    {
        std::lock_guard lock(mutex);
        Keys& group = keys[index];
        for (unsigned i = 0; i < group.used; ++i)
            if (group.values[i] == key) return false;
        if (group.used == group.values.size()) return false;
        group.values[group.used].assign(key);
        ++group.used;
        return true;
    }
    catch (...)
    {
        // Observation failure must not change the viewer's failure handling.
        return false;
    }
}
}
