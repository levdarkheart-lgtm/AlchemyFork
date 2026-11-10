/**
 * @file almetalavatarresources.cpp
 * @brief Coherent native avatar impostor resource ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalavatarresources.h"

#include <new>
#include <utility>

namespace
{
bool validRecord(const ALMetalAvatarResources::Record& record)
{
    if (!record.device || record.width == 0 || record.height == 0)
        return false;
    const std::array<ALMetalPixelFormat, 4> formats{
        ALMetalPixelFormat::rgba8_unorm_srgb,
        ALMetalPixelFormat::rgba8_unorm,
        record.normal_hdr ? ALMetalPixelFormat::rgba16_unorm
                          : ALMetalPixelFormat::rgb10a2_unorm,
        record.hdr ? ALMetalPixelFormat::rg11b10_float
                   : ALMetalPixelFormat::rgba8_unorm};
    for (std::size_t index = 0; index < formats.size(); ++index)
    {
        const auto& texture = record.textures[index];
        if (!texture || !texture->nativeHandle()) return false;
        const auto& d = texture->descriptor();
        if (d.type != ALMetalTextureType::texture_2d ||
            d.width != record.width || d.height != record.height ||
            d.array_length != 1 || d.sample_count != 1 ||
            d.format != formats[index] ||
            !alMetalHasUsage(d.usage, ALMetalTextureUsage::shader_read))
            return false;
    }
    return true;
}
}

std::size_t ALMetalAvatarResources::Hash::operator()(const Key& key) const noexcept
{
    std::uint64_t hash = 14695981039346656037ull;
    for (std::uint8_t byte : key)
    {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return static_cast<std::size_t>(hash);
}

bool ALMetalAvatarResources::publish(
    const Key& key, Record record, std::string* error)
{
    if (!validRecord(record))
    {
        if (error) *error = "Metal avatar impostor publication is invalid";
        return false;
    }
    try
    {
        Lease publication = std::make_shared<Record>(std::move(record));
        const auto existing = mRecords.find(key);
        if (existing != mRecords.end())
            existing->second = std::move(publication);
        else
            mRecords.emplace(key, std::move(publication));
        return true;
    }
    catch (const std::bad_alloc&)
    {
        if (error) *error = "Metal avatar impostor publication allocation failed";
        return false;
    }
}

ALMetalAvatarResources::Lease ALMetalAvatarResources::find(
    const Key& key, const ALMetalDevice& device,
    bool hdr, bool normal_hdr) const noexcept
{
    const auto found = mRecords.find(key);
    if (found == mRecords.end()) return {};
    const auto& record = found->second;
    if (record->device != &device || record->hdr != hdr ||
        record->normal_hdr != normal_hdr)
        return {};
    return record;
}

void ALMetalAvatarResources::erase(const Key& key) noexcept
{
    mRecords.erase(key);
}

void ALMetalAvatarResources::clear() noexcept
{
    mRecords.clear();
}

void ALMetalAvatarResources::discardIncompatible(
    const ALMetalDevice& device, bool hdr, bool normal_hdr) noexcept
{
    for (auto current = mRecords.begin(); current != mRecords.end();)
    {
        const auto& record = current->second;
        if (record->device != &device || record->hdr != hdr ||
            record->normal_hdr != normal_hdr)
            current = mRecords.erase(current);
        else
            ++current;
    }
}

std::size_t ALMetalAvatarResources::size() const noexcept
{
    return mRecords.size();
}
