/**
 * @file almetalprobepublication.h
 * @brief Retain face admission state until its publication transaction succeeds.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#pragma once
#include <cstdint>

class ALMetalProbeFacePublication final
{
public:
    explicit ALMetalProbeFacePublication(std::uint32_t& valid_faces) noexcept
        : mValidFaces(valid_faces), mPrevious(valid_faces)
    {
    }
    ~ALMetalProbeFacePublication() noexcept
    {
        if (!mCommitted) mValidFaces = mPrevious;
    }
    ALMetalProbeFacePublication(const ALMetalProbeFacePublication&) = delete;
    ALMetalProbeFacePublication& operator=(const ALMetalProbeFacePublication&) = delete;
    ALMetalProbeFacePublication(ALMetalProbeFacePublication&&) = delete;
    ALMetalProbeFacePublication& operator=(ALMetalProbeFacePublication&&) = delete;

    // Call only after every operation owned by this logical face succeeds.
    void commit() noexcept { mCommitted = true; }

private:
    std::uint32_t& mValidFaces;
    const std::uint32_t mPrevious;
    bool mCommitted = false;
};
