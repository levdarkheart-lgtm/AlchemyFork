/** Opt-in CPU metadata at the actual HUD texture-demand owner (PPD463). */
#pragma once

#include "alwarningdiagnostics.h"
#include <cstdint>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>

namespace ALHUDTextureInputs
{
enum class Type { local, media, dynamic, fetched, lod, other };
inline const char* typeName(Type type)
{
    switch (type)
    {
    case Type::local: return "local";
    case Type::media: return "media";
    case Type::dynamic: return "dynamic";
    case Type::fetched: return "fetched";
    case Type::lod: return "lod";
    default: return "other";
    }
}
struct Snapshot
{
    std::string asset, object;
    Type type = Type::other;
    std::int32_t texture_type = -1, face = -1, slot = -1, boost = -1;
    std::int32_t width = 0, height = 0, components = 0, texture_state = -1;
    std::int32_t current_discard = -1, desired_discard = -1, raw_discard = -1;
    std::int32_t fetch_state = -1;
    std::uint32_t fetch_priority = 0;
    std::int32_t raw_width = 0, raw_height = 0, raw_components = 0;
    std::uint64_t frame = 0, create_queued_frame = 0;
    bool boost_owner = false, resident = false, current_discard_known = false;
    bool parcel_media = false, fetch_known = false, has_fetcher = false, fetching = false;
    bool missing = false, create_pending = false, downscale_pending = false;
    bool raw_present = false, raw_valid = false;

    unsigned flags() const
    {
        const bool values[] = {boost_owner, resident, current_discard_known, parcel_media,
            fetch_known, has_fetcher, fetching, missing, create_pending, downscale_pending,
            raw_present, raw_valid};
        unsigned result = 0;
        for (unsigned i = 0; i < std::size(values); ++i) if (values[i]) result |= 1u << i;
        return result;
    }
};
inline std::string key(const Snapshot& s)
{
    std::ostringstream out;
    // One representative per asset/slot/state, even when shared by many HUD faces.
    // Frame, time, object/face identity and queue age deliberately do not consume slots.
    out << s.asset;
    // Logical/dynamic textures may have a null asset UUID: distinguish their HUD owner.
    if (s.asset == "00000000-0000-0000-0000-000000000000") out << ':' << s.object;
    out << ':' << s.texture_type << ':' << s.slot << ':' << s.boost
        << ':' << s.current_discard << ':' << s.desired_discard << ':' << s.raw_discard
        << ':' << s.width << ':' << s.height << ':' << s.components << ':' << s.texture_state
        << ':' << s.raw_width << ':' << s.raw_height << ':' << s.raw_components << ':' << s.fetch_state << ':' << s.flags();
    return out.str();
}
inline std::ostream& operator<<(std::ostream& out, const Snapshot& s)
{
    out << "asset=" << s.asset << " object=" << s.object
        << " face=" << s.face << " slot=" << s.slot << " frame=" << s.frame
        << " texture_type=" << s.texture_type << " type=" << typeName(s.type)
        << " hud_boost_owner=" << s.boost_owner << " boost=" << s.boost
        << " full_width=" << s.width << " full_height=" << s.height
        << " components=" << s.components << " texture_state=" << s.texture_state
        << " gpu_resident=" << s.resident << " current_discard_known=" << s.current_discard_known
        << " current_discard=" << s.current_discard << " parcel_media=" << s.parcel_media
        << " fetch_details_known=" << s.fetch_known;
    if (!s.fetch_known)
        return out << " desired_discard=unknown raw_discard=unknown raw_present=unknown raw_valid=unknown"
            << " raw_width=unknown raw_height=unknown raw_components=unknown has_fetcher=unknown"
            << " fetching=unknown missing=unknown fetch_state=unknown fetch_priority=unknown"
            << " create_queue_pending=unknown create_queued_frame=unknown"
            << " downscale_queue_pending=unknown";
    return out << " desired_discard=" << s.desired_discard
        << " raw_discard=" << s.raw_discard << " raw_present=" << s.raw_present
        << " raw_valid=" << s.raw_valid << " raw_width=" << s.raw_width
        << " raw_height=" << s.raw_height << " raw_components=" << s.raw_components
        << " has_fetcher=" << s.has_fetcher << " fetching=" << s.fetching
        << " missing=" << s.missing << " fetch_state=" << s.fetch_state << " fetch_priority=" << s.fetch_priority
        << " create_queue_pending=" << s.create_pending
        << " create_queued_frame=" << s.create_queued_frame
        << " downscale_queue_pending=" << s.downscale_pending;
}
template<class Collect, class Emit> void observe(Collect&& collect, Emit&& emit) noexcept
{
    // No metadata collection, key construction or allocation when disabled.
    if (!ALWarningDiagnostics::enabled()) return;
    try
    {
        const Snapshot snapshot = collect();
        const std::string event_key = key(snapshot);
        if (ALWarningDiagnostics::take(ALWarningDiagnostics::Kind::hud, event_key)) emit(snapshot);
    }
    catch (...) {} // Optional evidence must not affect demand, rendering or input.
}
}
