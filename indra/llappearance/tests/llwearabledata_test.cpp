/**
 * @file llwearabledata_test.cpp
 * @brief Real wearable registration distinguishes absent buckets from invalid types.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"

#include "../llwearabledata.h"
#include "llerrorcontrol.h"
#include "altestfatal.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
class TestWearable final : public LLWearable
{
public:
    explicit TestWearable(LLWearableType::EType type)
    {
        // The lookup consumes LLWearable::getType() directly. Parameter
        // cloning through setType(avatar) is unrelated to registration and
        // would require a complete avatar appearance for these CPU tests.
        mType = type;
    }
    LLUUID getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex) const override
    {
        return LLUUID::null;
    }
    void setUpdated() const override { ++updates; }
    mutable U32 updates = 0;
};

class WearableData final : public LLWearableData
{
public:
    using LLWearableData::pushWearable;
    using LLWearableData::eraseWearable;

    std::size_t bucketCount() const { return mWearableDatas.size(); }
    void seedCorruptBucket(LLWearableType::EType type, LLWearable* wearable)
    {
        // Only this negative test bypasses production admission. An invalid
        // type must fail even if a pre-existing bucket happens to match it.
        mWearableDatas[type].push_back(wearable);
    }
    std::vector<std::pair<LLWearable*, bool>> notifications;

protected:
    void wearableUpdated(LLWearable* wearable, bool removed) override
    {
        notifications.emplace_back(wearable, removed);
        wearable->setUpdated();
        // Cross-wearable avatar parameter propagation is outside this test.
        // All insertion, indexing, type and capacity logic remains real.
    }
};

class WarningRecorder final : public LLError::Recorder
{
public:
    WarningRecorder()
    {
        showTime(false);
        showTags(false);
        showLevel(false);
        showLocation(false);
        showFunctionName(false);
    }
    void recordMessage(LLError::ELevel level, const std::string& message) override
    {
        if (level == LLError::LEVEL_WARN) messages.push_back(message);
    }
    std::vector<std::string> messages;
};

class Warnings
{
public:
    Warnings()
        : mPriorFatal(LLError::getFatalFunction()),
          mPriorSettings(LLError::saveAndResetSettings()),
          mRecorder(std::make_shared<WarningRecorder>())
    {
        LLError::setFatalFunction(mPriorFatal);
        LLError::setDefaultLevel(LLError::LEVEL_WARN);
        LLError::addRecorder(mRecorder);
    }
    ~Warnings()
    {
        LLError::removeRecorder(mRecorder);
        LLError::restoreSettings(mPriorSettings);
    }
    const std::vector<std::string>& messages() const { return mRecorder->messages; }

private:
    LLError::FatalFunction mPriorFatal;
    LLError::SettingsStoragePtr mPriorSettings;
    std::shared_ptr<WarningRecorder> mRecorder;
};
}

TEST_CASE("llwearabledata: first body part and clothing registrations keep their indices and updates without warnings",
          "[llwearabledata]")
{
    Warnings warnings;
    WearableData data;
    TestWearable shape(LLWearableType::WT_SHAPE);
    TestWearable first_shirt(LLWearableType::WT_SHIRT);
    TestWearable second_shirt(LLWearableType::WT_SHIRT);
    CHECK(data.bucketCount() == 0);
    CHECK(data.canAddWearable(LLWearableType::WT_SHAPE));
    CHECK(data.canAddWearable(LLWearableType::WT_SHIRT));

    data.pushWearable(LLWearableType::WT_SHAPE, &shape);
    data.pushWearable(LLWearableType::WT_SHIRT, &first_shirt);
    data.pushWearable(LLWearableType::WT_SHIRT, &second_shirt);

    CHECK(data.bucketCount() == 2);
    CHECK(data.getWearableCount(LLWearableType::WT_SHAPE) == 1);
    CHECK(data.getWearableCount(LLWearableType::WT_SHIRT) == 2);
    CHECK(data.getClothingLayerCount() == 2);
    U32 index = 77;
    CHECK(data.getWearableIndex(&shape, index));
    CHECK(index == 0);
    index = 77;
    CHECK(data.getWearableIndex(&first_shirt, index));
    CHECK(index == 0);
    index = 77;
    CHECK(data.getWearableIndex(&second_shirt, index));
    CHECK(index == 1);
    CHECK(data.getWearable(LLWearableType::WT_SHAPE, 0) == &shape);
    CHECK(data.getWearable(LLWearableType::WT_SHIRT, 1) == &second_shirt);
    REQUIRE(data.notifications.size() == 3);
    CHECK(data.notifications[0] == std::make_pair(static_cast<LLWearable*>(&shape), false));
    CHECK(data.notifications[1] == std::make_pair(static_cast<LLWearable*>(&first_shirt), false));
    CHECK(data.notifications[2] == std::make_pair(static_cast<LLWearable*>(&second_shirt), false));
    CHECK(shape.updates == 1);
    CHECK(first_shirt.updates == 1);
    CHECK(second_shirt.updates == 1);
    CHECK(warnings.messages().empty());
}

TEST_CASE("llwearabledata: valid and null misses preserve the caller index and remain silent",
          "[llwearabledata]")
{
    Warnings warnings;
    WearableData data;
    TestWearable registered(LLWearableType::WT_SHIRT);
    TestWearable unregistered(LLWearableType::WT_SHIRT);
    TestWearable last_valid_type(LLWearableType::WT_UNIVERSAL);
    U32 index = 791;
    CHECK_FALSE(data.getWearableIndex(nullptr, index));
    CHECK(index == 791);
    CHECK_FALSE(data.getWearableIndex(&unregistered, index));
    CHECK(index == 791);
    CHECK_FALSE(data.getWearableIndex(&last_valid_type, index));
    CHECK(index == 791);
    CHECK(data.bucketCount() == 0);

    data.pushWearable(LLWearableType::WT_SHIRT, &registered, false);
    CHECK_FALSE(data.getWearableIndex(&unregistered, index));
    CHECK(index == 791);
    CHECK(data.getWearableCount(LLWearableType::WT_SHIRT) == 1);
    CHECK(data.notifications.empty());
    CHECK(registered.updates == 0);
    CHECK(warnings.messages().empty());
}

TEST_CASE("llwearabledata: genuine invalid types report their numeric value even with a matching corrupt bucket",
          "[llwearabledata]")
{
    const LLWearableType::EType invalid_types[]{
        LLWearableType::WT_NONE,
        LLWearableType::WT_COUNT,
        static_cast<LLWearableType::EType>(254),
        LLWearableType::WT_INVALID,
    };
    for (const auto type : invalid_types)
    {
        DYNAMIC_SECTION("invalid type " << static_cast<S32>(type))
        {
            Warnings warnings;
            WearableData data;
            TestWearable invalid(type);
            SECTION("no bucket") {}
            SECTION("corrupt bucket already contains the pointer")
            {
                data.seedCorruptBucket(type, &invalid);
            }
            U32 index = 593;
            CHECK_FALSE(data.getWearableIndex(&invalid, index));
            CHECK(index == 593);
            REQUIRE(warnings.messages().size() == 1);
            CHECK(warnings.messages().front().find(
                "invalid type " + std::to_string(static_cast<S32>(type))) != std::string::npos);
            CHECK(data.notifications.empty());
            CHECK(invalid.updates == 0);
        }
    }
}

TEST_CASE("llwearabledata: production body-part and total clothing limits remain authoritative",
          "[llwearabledata]")
{
    Warnings warnings;
    WearableData data;
    SECTION("a second body part of the same type is not admitted")
    {
        TestWearable first(LLWearableType::WT_SHAPE);
        TestWearable replacement(LLWearableType::WT_SHAPE);
        data.pushWearable(LLWearableType::WT_SHAPE, &first);
        CHECK_FALSE(data.canAddWearable(LLWearableType::WT_SHAPE));
        CHECK(data.canAddWearable(LLWearableType::WT_SKIN));
        data.pushWearable(LLWearableType::WT_SHAPE, &replacement);
        CHECK(data.getWearableCount(LLWearableType::WT_SHAPE) == 1);
        CHECK(data.getWearable(LLWearableType::WT_SHAPE, 0) == &first);
        CHECK(data.getClothingLayerCount() == 0);
        CHECK(data.notifications.size() == 1);
        CHECK(first.updates == 1);
        CHECK(replacement.updates == 0);
    }
    SECTION("clothing limit spans types and recovers after an ordinary erase")
    {
        std::vector<std::unique_ptr<TestWearable>> shirts;
        for (U32 count = 0; count < LLWearableData::MAX_CLOTHING_LAYERS; ++count)
        {
            shirts.push_back(std::make_unique<TestWearable>(LLWearableType::WT_SHIRT));
            data.pushWearable(LLWearableType::WT_SHIRT, shirts.back().get());
        }
        CHECK(data.getClothingLayerCount() == LLWearableData::MAX_CLOTHING_LAYERS);
        CHECK_FALSE(data.canAddWearable(LLWearableType::WT_SHIRT));
        CHECK_FALSE(data.canAddWearable(LLWearableType::WT_PANTS));
        CHECK(data.canAddWearable(LLWearableType::WT_SHAPE));
        TestWearable extra(LLWearableType::WT_PANTS);
        data.pushWearable(LLWearableType::WT_PANTS, &extra);
        CHECK(data.getWearableCount(LLWearableType::WT_PANTS) == 0);
        CHECK(data.notifications.size() == LLWearableData::MAX_CLOTHING_LAYERS);
        CHECK(extra.updates == 0);

        data.eraseWearable(shirts.back().get());
        CHECK(data.getClothingLayerCount() == LLWearableData::MAX_CLOTHING_LAYERS - 1);
        CHECK(data.notifications.back() == std::make_pair(static_cast<LLWearable*>(shirts.back().get()), true));
        CHECK(data.canAddWearable(LLWearableType::WT_PANTS));
        data.pushWearable(LLWearableType::WT_PANTS, &extra);
        CHECK(data.getClothingLayerCount() == LLWearableData::MAX_CLOTHING_LAYERS);
        CHECK(data.getWearable(LLWearableType::WT_PANTS, 0) == &extra);
        CHECK(data.notifications.size() == LLWearableData::MAX_CLOTHING_LAYERS + 2);
        CHECK(extra.updates == 1);
    }
    CHECK(warnings.messages().empty());
}

TEST_CASE("llwearabledata: duplicate pointer insertion cannot create a second layer",
          "[llwearabledata]")
{
    Warnings warnings;
    WearableData data;
    TestWearable shirt(LLWearableType::WT_SHIRT);
    data.pushWearable(LLWearableType::WT_SHIRT, &shirt, false);
    U32 index = 42;
    REQUIRE(data.getWearableIndex(&shirt, index));
    REQUIRE(index == 0);
    try
    {
        // Debug configurations assert this caller error; Release still must
        // preserve storage. The production test runner makes LL_ERRS throw.
        data.pushWearable(LLWearableType::WT_SHIRT, &shirt, false);
    }
    catch (const ll_test::FatalError&)
    {
    }
    CHECK(data.getWearableCount(LLWearableType::WT_SHIRT) == 1);
    CHECK(data.getClothingLayerCount() == 1);
    index = 42;
    CHECK(data.getWearableIndex(&shirt, index));
    CHECK(index == 0);
    CHECK(data.notifications.empty());
    CHECK(shirt.updates == 0);
    CHECK(warnings.messages().empty());
}
