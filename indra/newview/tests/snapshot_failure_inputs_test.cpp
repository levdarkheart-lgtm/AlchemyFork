/**
 * @file snapshot_failure_inputs_test.cpp
 * @brief Actual exact-current failure branch with synthetic CPU-only inputs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The included production body assigns the original error, optionally appends
 * producer metadata, and returns false. This fixture does not reproduce the
 * outer admission predicate or a renderer. Capture deliberately has no UI
 * background member; no clock, texture, readback, window or account API exists.
 */
#include "../alworldretentioninputs.h"
#include <fmt/format.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
std::atomic<bool> fail_next_allocation{false};
unsigned injected_allocation_failures = 0;
}

void* operator new(std::size_t size)
{
    if (fail_next_allocation.exchange(false))
    {
        ++injected_allocation_failures;
        throw std::bad_alloc();
    }
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

namespace
{
using U32 = std::uint32_t;
using U64 = std::uint64_t;
using BaseState = ALWorldRetentionInputs::BaseState;

struct Reads
{
    unsigned handle = 0, arrow = 0, dead = 0, id = 0, base = 0;
} reads;

struct ObservedBaseState
{
    BaseState value = BaseState::hero_inputs;
    operator BaseState() const { ++reads.base; return value; }
};

struct HeroObject
{
    bool dead = false, throw_on_dead = false, fail_append_allocation = false;
    U32 id = 987654321; // Synthetic; the diagnostic must emit only equality.
    bool isDead() const
    {
        ++reads.dead;
        if (throw_on_dead) throw std::runtime_error("controlled metadata failure");
        return dead;
    }
    U32 getID() const
    {
        ++reads.id;
        // Called after the original fmt error assignment. The subsequent
        // real formatting/append allocation must be caught by the actual body.
        if (fail_append_allocation) fail_next_allocation = true;
        return id;
    }
};

struct HeroHandle
{
    HeroObject* value = nullptr;
    bool notNull() const { ++reads.handle; return value != nullptr; }
    HeroObject* operator->() const { ++reads.arrow; return value; }
};

struct HeroManager
{
    HeroHandle mNearestHero;
    bool mMetalRadianceReady = true;
    struct { int heroProbeCount = 1; } mHeroData;
    U32 mNativePublishedOwner = 1;
    U64 mNativePublishedEpoch = 3, mMetalRadianceEpoch = 3;
};

struct Device {};
struct Capture
{
    bool world_ui_chain_snapshot = true, world_ui_capture_active = false;
    bool world_ui_chain_active = true, complete = true, frame_active = false;
    U32 first_incomplete_source_line = 321;
    bool renderer = true, target = true, composed_light_target = false;
    bool lighting_composition_attempted = false, opaque_post_deferred_attempted = false;
    bool presentation_target = false, world_ui_target = false;
    U32 presentation_frame = 99, world_ui_frame = 99;
    const Device* device = nullptr;
    ObservedBaseState retention_inputs_base_state;
    U32 retention_inputs_base_state_frame = 100;
};

struct Fixture
{
    Device owned_device;
    Capture capture_data;
    HeroObject hero;
    HeroManager mHeroProbeManager;
    U32 gFrameCount = 100;
    bool RenderDeferredAtmospheric = true;

    Fixture()
    {
        reads = {};
        capture_data.device = &owned_device;
        mHeroProbeManager.mNearestHero.value = &hero;
    }
    bool incomplete(std::string* error) const
    {
        const Capture& capture = capture_data;
        const Device* device = &owned_device;
#include "../pipeline_snapshot_incomplete_native.inl"
    }
};

unsigned cases = 0, checks = 0, failures = 0;
void check(bool value, const char* expression, unsigned line)
{
    ++checks;
    if (!value)
    {
        ++failures;
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
    }
}
#define CHECK(value) check(bool(value), #value, __LINE__)

// An expected output contract, not a second implementation of branch logic.
const std::string original_error =
    "Metal world snapshot did not complete exactly: "
    "chain_snapshot=true, capture_active=false, chain_active=true, "
    "complete=true, first_incomplete_line=321, frame_active=false, "
    "renderer=true, target=true, composed_light=false, "
    "lighting_attempted=false, post_attempted=false, "
    "presentation_target=false, presentation_frame=99, "
    "world_ui_target=false, world_ui_frame=99, current_frame=100, "
    "device=true, device_match=true";

void check_original(const std::string& error)
{
    CHECK(error.starts_with(original_error));
    CHECK(error.find("987654321") == std::string::npos);
    CHECK(error.find("http:") == std::string::npos && error.find("https:") == std::string::npos);
    CHECK(error.find("asset=") == std::string::npos && error.find("avatar=") == std::string::npos);
}

void disabled()
{
    unsetenv("ALCHEMY_WARNING_DIAGNOSTICS");
    ++cases;
    Fixture f;
    // The existing message does not need any selected-object access. A missing
    // outer gate would dereference this protected synthetic invalid pointer.
    f.mHeroProbeManager.mNearestHero.value = reinterpret_cast<HeroObject*>(std::uintptr_t{1});
    std::string error = "replace this synthetic prior error";
    CHECK(!f.incomplete(&error));
    CHECK(error == original_error);
    CHECK(reads.handle == 0 && reads.arrow == 0 && reads.dead == 0 && reads.id == 0 && reads.base == 0);
    CHECK(error.find("diagnostic_inputs") == std::string::npos);

    ++cases;
    fail_next_allocation = true;
    const bool result = f.incomplete(nullptr);
    const bool did_not_allocate = fail_next_allocation.exchange(false);
    CHECK(!result && did_not_allocate);
    CHECK(reads.handle == 0 && reads.arrow == 0 && reads.base == 0);
}

void enabled()
{
    setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    {
        ++cases;
        Fixture f;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("; diagnostic_inputs base_state=hero_inputs, base_frame=100, base_current=true, deferred_atmospheric=true, ") == original_error.size());
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=true/true/true/false/true") != std::string::npos);
        CHECK(reads.handle == 1 && reads.dead == 1 && reads.id == 1 && reads.base == 1);
    }
    {
        ++cases;
        Fixture f;
        f.mHeroProbeManager.mNativePublishedOwner = f.hero.id;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=true/true/true/true/true") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.mHeroProbeManager.mNearestHero.value = nullptr;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=false/true/true/false/true") != std::string::npos);
        CHECK(reads.arrow == 0 && reads.dead == 0 && reads.id == 0);
    }
    {
        ++cases;
        Fixture f;
        f.hero.dead = true;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=false/true/true/false/true") != std::string::npos);
        CHECK(reads.dead == 1 && reads.id == 0);
    }
    {
        ++cases;
        Fixture f;
        f.mHeroProbeManager.mNativePublishedOwner = f.hero.id;
        f.mHeroProbeManager.mNativePublishedEpoch = 2;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=true/true/true/true/false") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.mHeroProbeManager.mNativePublishedOwner = f.hero.id;
        f.mHeroProbeManager.mMetalRadianceReady = false;
        f.mHeroProbeManager.mHeroData.heroProbeCount = 0;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("hero(selected/ready/advertised/owner_match/epoch_match)=true/false/false/true/true") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.capture_data.retention_inputs_base_state.value = BaseState::encoding_failed;
        f.capture_data.retention_inputs_base_state_frame = 99;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("base_state=encoding_failed, base_frame=99, base_current=false") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.capture_data.retention_inputs_base_state.value = BaseState::unobserved;
        f.capture_data.retention_inputs_base_state_frame = std::numeric_limits<U32>::max();
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("base_state=unobserved, base_frame=4294967295, base_current=false") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.RenderDeferredAtmospheric = false;
        std::string error;
        CHECK(!f.incomplete(&error));
        check_original(error);
        CHECK(error.find("deferred_atmospheric=false") != std::string::npos);
    }
    {
        ++cases;
        Fixture f;
        f.mHeroProbeManager.mNearestHero.value = reinterpret_cast<HeroObject*>(std::uintptr_t{1});
        fail_next_allocation = true;
        const bool result = f.incomplete(nullptr);
        const bool did_not_allocate = fail_next_allocation.exchange(false);
        CHECK(!result && did_not_allocate);
        CHECK(reads.handle == 0 && reads.arrow == 0 && reads.base == 0);
    }
    {
        ++cases;
        Fixture f;
        f.hero.fail_append_allocation = true;
        const auto previous_failures = injected_allocation_failures;
        std::string error;
        const bool result = f.incomplete(&error);
        const bool failure_not_consumed = fail_next_allocation.exchange(false);
        CHECK(!result);
        CHECK(!failure_not_consumed && injected_allocation_failures == previous_failures + 1);
        CHECK(error == original_error);
        CHECK(reads.id == 1);
    }
    {
        ++cases;
        Fixture f;
        f.hero.throw_on_dead = true;
        std::string error;
        CHECK(!f.incomplete(&error));
        CHECK(error == original_error);
        CHECK(reads.dead == 1 && reads.id == 0 && reads.base == 0);
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string_view mode(argv[1]);
    if (mode == "disabled") disabled();
    else if (mode == "enabled") enabled();
    else return 2;
    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
