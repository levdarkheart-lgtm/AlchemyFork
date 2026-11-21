/**
 * @file almetalworldresources.h
 * @brief Immutable native texture-generation ownership for CPU world sources.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

class ALMetalDevice;
class ALMetalTexture;
struct ALMetalWorldSimpleBatch;

enum class ALMetalWorldResourceFallback : std::uint8_t
{
    none, missing_source, unmatched_source, unsupported,
    host_allocation, native_allocation, validation, count
};
inline const char* alMetalWorldResourceFallbackName(ALMetalWorldResourceFallback value) noexcept
{
    constexpr std::array names{"none", "missing_source", "unmatched_source", "unsupported",
        "host_allocation", "native_allocation", "validation"};
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? names[index] : "invalid";
}

// A producer record owns exact C++ texture generations as well as native
// shader views. Native-object retention alone would not enforce producer COW.
class ALMetalWorldResourceSource final
{
public:
    static std::shared_ptr<const ALMetalWorldResourceSource> publish(
        const ALMetalDevice&, ALMetalWorldSimpleBatch& mutable_prototype,
        std::span<const std::shared_ptr<const ALMetalTexture>> generation_leases,
        std::string* error = nullptr, ALMetalWorldResourceFallback* failure = nullptr);
    ~ALMetalWorldResourceSource();
    bool matches(const ALMetalWorldSimpleBatch&) const noexcept;
    // Borrowed only while this immutable source is alive. Consuming native
    // owners copy these exact generations without retaining CPU descriptions.
    std::span<const std::shared_ptr<const ALMetalTexture>> generationLeases() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldResourceSource(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
