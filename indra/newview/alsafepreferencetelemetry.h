/** Explicit, value-typed preference telemetry boundary (PPD491). */
#pragma once

#include "llsd.h"
#include <cmath>
#include <string_view>

namespace ALSafePreferenceTelemetry
{
// Preference storage is extensible and may contain credentials or account text,
// including strings nested in LLSD controls. Never serialize that storage as
// telemetry. Add a field here only after reviewing its meaning and value type.
inline LLSD build(const LLSD& global_diffs)
{
    enum class Type { boolean, unsigned_integer, integer, real };
    struct Field { std::string_view name; Type type; };
    static constexpr Field fields[] = {
        {"ImageDecodeAdaptive", Type::boolean},
        {"RenderMetalMultiThreadedTextures", Type::boolean},
        {"RenderQualityPerformance", Type::unsigned_integer},
        {"RenderFarClip", Type::real},
        {"RenderShadowDetail", Type::integer},
        {"RenderAvatarMaxComplexity", Type::unsigned_integer},
        {"RenderReflectionProbeDetail", Type::integer},
        {"RenderReflectionProbeLevel", Type::integer},
        {"RenderReflectionProbeResolution", Type::unsigned_integer},
        {"RenderScreenSpaceReflections", Type::boolean},
        {"RenderDeferredSSAO", Type::boolean},
        {"RenderFSAASamples", Type::unsigned_integer},
        {"RenderResolutionDivisor", Type::unsigned_integer},
        {"RenderMaxTextureResolution", Type::unsigned_integer},
        {"RenderMaxVRAMBudget", Type::unsigned_integer},
        {"TextureDisable", Type::boolean},
        {"TextureDecodeDisabled", Type::boolean},
        {"EmulateCoreCount", Type::unsigned_integer},
        {"MaxFPS", Type::real},
        {"RenderAvatarMaxNonImpostors", Type::unsigned_integer},
        {"RenderAvatarLODFactor", Type::real},
        {"RenderVolumeLODFactor", Type::real},
        {"RenderTerrainLODFactor", Type::real},
        {"RenderMinFreeMainMemoryThreshold", Type::unsigned_integer}
    };

    LLSD preferences = LLSD::emptyMap();
    LLSD& settings = preferences["settings"] = LLSD::emptyMap();
    // No account preference is needed for graphics/performance telemetry.
    preferences["settings_per_account"] = LLSD::emptyMap();
    if (!global_diffs.isMap())
        return preferences;

    for (const auto& field : fields)
    {
        if (!global_diffs.has(field.name))
            continue;
        const LLSD& value = global_diffs[field.name];
        bool valid = false;
        switch (field.type)
        {
        case Type::boolean:
            // Boolean controls may retain their XML integer 0/1 representation.
            valid = value.isBoolean() || (value.isInteger() &&
                (value.asInteger() == 0 || value.asInteger() == 1));
            break;
        case Type::unsigned_integer:
            valid = value.isInteger() && value.asInteger() >= 0;
            break;
        case Type::integer:
            valid = value.isInteger();
            break;
        case Type::real:
            valid = (value.isReal() || value.isInteger()) && std::isfinite(value.asReal());
            break;
        }
        if (valid)
            settings[field.name] = value;
    }

    // ThreadPoolSizes is an LLSD map, not arbitrary serializable preferences.
    // Only these actual worker names and positive integer widths are admitted.
    const LLSD& pools = global_diffs["ThreadPoolSizes"];
    if (pools.isMap())
    {
        for (const auto name : {"General", "ImageDecode", "LLImageGL"})
        {
            const LLSD& value = pools[name];
            if (value.isInteger() && value.asInteger() > 0)
                settings["ThreadPoolSizes"][name] = value;
        }
    }
    return preferences;
}
}
