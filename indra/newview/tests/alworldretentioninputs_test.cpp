/** PPD003 actual retained-boundary collection, bounded keys and serialization. */
#include "../alworldretentioninputs.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>

namespace { std::atomic<bool> fail_next_allocation{false}; }
void* operator new(std::size_t size)
{
    if (fail_next_allocation.exchange(false)) throw std::bad_alloc();
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
namespace
{
using U32 = std::uint32_t;
using U64 = std::uint64_t;
double test_clock = 0.;
unsigned clock_reads = 0;
struct LLTimer
{
    static double getTotalSeconds() { ++clock_reads; return test_clock; }
};
unsigned checks = 0, failures = 0;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::cerr << "FAILED: " << label << '\n'; }
}
struct Fixture
{
    struct HeroObject
    {
        bool dead = false;
        unsigned id = 2;
        bool isDead() const { return dead; }
        unsigned getID() const { return id; }
    } hero_object;
    struct HeroHandle
    {
        HeroObject* object = nullptr;
        bool notNull() const { return object != nullptr; }
        HeroObject* operator->() const { return object; }
    };
    struct HeroManager
    {
        HeroHandle mNearestHero;
        bool mMetalRadianceReady = true;
        struct { int heroProbeCount = 1; } mHeroData;
        unsigned mNativePublishedOwner = 1;
        U64 mNativePublishedEpoch = 3, mMetalRadianceEpoch = 3;
    } mHeroProbeManager;
    struct Background { U64 source_frame = 99; } background;
    struct Capture
    {
        ALWorldRetentionInputs::Observer retention_inputs_observer;
        Background* ui_frame_background = nullptr;
        bool hero_probe_face_capture = false, reflection_probe_face_capture = false;
        U32 continuous_world_frame = 100, first_incomplete_source_line = 123;
        bool complete = true, attempted = true, target = true;
        bool base_light_map_target = true, base_light_target = true, main_lighting_ready = false;
        bool composed_light_target = true, opaque_post_deferred_target = true;
        bool display_post_target = false, presentation_target = false, exposure_target = false;
        U32 display_post_frame = 99, presentation_frame = 99, exposure_frame = 99;
        U64 exposure_epoch = 1;
        bool hdr = true;
        U32 retention_inputs_exposure_policy_frame = 100, retention_inputs_post_policy_frame = 100;
        bool retention_inputs_meter_owner = true, retention_inputs_adaptation = true;
        bool retention_inputs_classic = false, retention_inputs_gamma = false;
        bool retention_inputs_no_post = false, retention_inputs_dynamic = true;
        bool base_lighting_attempted = true, local_lighting_attempted = true;
        bool projector_lighting_attempted = true, lighting_composition_attempted = true;
        bool opaque_post_deferred_attempted = true;
        ALWorldRetentionInputs::BaseState retention_inputs_base_state = ALWorldRetentionInputs::BaseState::hero_inputs;
        U32 retention_inputs_base_state_frame = 100;
    } capture;
    U32 gFrameCount = 100;
    U64 mMetalExposureHistoryEpoch = 1;
    bool mMetalExposureHistoryReady = false;
    unsigned collected = 0, attempts = 0;
    bool throw_emitter = false;
    std::ostringstream output;
    Fixture()
    {
        capture.ui_frame_background = &background;
        mHeroProbeManager.mNearestHero.object = &hero_object;
    }
    bool isMainWorldPass() { ++collected; return true; }
    std::ostream& log()
    {
        ++attempts;
        if (throw_emitter) throw std::runtime_error("controlled logger failure");
        return output;
    }
    void observe()
    {
#define LL_INFOS(category) log()
#define LL_INFOS_ONCE(category) log()
#define LL_ENDL std::endl
#include "../pipeline_world_retention_inputs_native.inl"
#undef LL_ENDL
#undef LL_INFOS
#undef LL_INFOS_ONCE
    }
    void next(U32 frame)
    {
        gFrameCount = frame;
        background.source_frame = frame - 1;
        capture.continuous_world_frame = frame;
        capture.retention_inputs_exposure_policy_frame = frame;
        capture.retention_inputs_post_policy_frame = frame;
        capture.retention_inputs_base_state_frame = frame;
        capture.display_post_frame = frame - 1;
        capture.presentation_frame = frame - 1;
        capture.exposure_frame = frame - 1;
        capture.exposure_epoch = mMetalExposureHistoryEpoch = frame + 123;
    }
};
void disabled()
{
    unsetenv("ALCHEMY_WARNING_DIAGNOSTICS");
    Fixture f;
    f.capture.ui_frame_background = nullptr; // Must not be dereferenced off.
    fail_next_allocation = true;
    f.observe();
    const bool did_not_allocate = fail_next_allocation.exchange(false);
    check(did_not_allocate, "disabled observer allocates nothing");
    check(f.collected == 0, "disabled observer does not collect metadata");
    check(f.attempts == 0 && f.output.str().empty(), "disabled observer emits nothing");
    check(clock_reads == 0, "disabled observer does not read the clock");
}
void recurring()
{
    using namespace ALWorldRetentionInputs;
    Observer observer;
    Snapshot input;
    input.current_frame = input.capture_frame = 100;
    input.background_frame = 99;
    input.complete = input.gbuffer = true;
    std::array<Snapshot, 128> emitted{};
    unsigned count = 0;
    double now = 10.;
    auto observe = [&]
    {
        observer.observe([&] { return now; }, [&] { return input; },
            [&](const Snapshot& s) { if (count < emitted.size()) emitted[count] = s; ++count; });
    };
    observe();
    check(count == 1 && !emitted[0].repeat_event, "episode starts with a distinct state");
    check(emitted[0].halted_time_known && emitted[0].halted_seconds == 0., "initial observed halt duration is zero");
    now = 10.09; ++input.current_frame; input.capture_frame = input.current_frame; observe();
    check(count == 1, "subthreshold retained frames do not flood output");
    now = 10.11; ++input.current_frame; input.capture_frame = input.current_frame; observe();
    check(count == 2 && emitted[1].repeat_event && emitted[1].observed_repeat_events == 1,
          "same readiness with stopped background repeats after100ms");
    check(emitted[1].current_frame == 102 && emitted[1].background_frame == 99 && emitted[1].halted_seconds > .1,
          "repeat collects fresh frame and measured stopped duration");
    now = 10.9; observe();
    check(count == 2, "later repeats wait one second");
    now = 11.12; observe();
    check(count == 3, "sustained halt refreshes at one-second cadence");
    // A retained frame can advance continuously while remaining one frame old.
    // This is not a stopped-background episode and must not spend repeat slots.
    for (unsigned i = 0; i < 10000; ++i)
    {
        now += .02; ++input.current_frame; input.capture_frame = input.current_frame;
        input.background_frame = input.current_frame - 1; observe();
    }
    check(count == 3, "progressing retained backgrounds do not consume repeat budget");
    input.base_state = BaseState::hero_inputs; input.base_state_frame = input.current_frame;
    input.hero_selected = input.hero_ready = input.hero_advertised = true;
    observe();
    check(count == 4 && emitted[3].observed_distinct_states == 2, "new base/hero readiness emits immediately");
    input.base_state = BaseState::ordinary_probes; observe();
    check(count == 5 && emitted[4].observed_distinct_states == 3, "base prerequisite reason is part of the stable state");
    input.base_state = BaseState::hero_inputs; observe();
    check(count == 6 && emitted[5].repeat_event, "returning to a seen state uses independent repeat budget");
    observer.recovered(); observe();
    check(count == 7 && emitted[6].halted_seconds == 0. && emitted[6].repeat_event,
          "recovery starts fresh timing without refilling prior keys or budgets");
    input.background_frame = input.current_frame; observe();
    check(count == 7, "current background recovers without an emission");
    --input.background_frame; observe();
    check(count == 8 && emitted[7].halted_seconds == 0., "observed recovery resets next episode onset");
    now -= 50.; input.hero_owner_match = true; observe();
    check(count == 9 && !emitted[8].halted_time_known && emitted[8].halted_seconds == 0.,
          "backward clock cannot fabricate duration on a fresh state");
    now += .11; observe();
    check(count == 10 && emitted[9].halted_time_known && emitted[9].halted_seconds >= .1,
          "clock resumes with only its new measured window");
    now = std::numeric_limits<double>::quiet_NaN(); input.hero_epoch_match = true; observe();
    check(count == 11 && !emitted[10].halted_time_known && emitted[10].halted_seconds == 0.,
          "nonfinite clock emits changed facts without invented duration");
    now = 500.; observe();
    check(count == 11, "first finite clock after invalid value rebases without an immediate repeat");
    now = 500.11; observe();
    check(count == 12 && emitted[11].halted_time_known && emitted[11].halted_seconds < .2,
          "nonfinite clock gap is excluded from measured stopped duration");

    Observer capped;
    Snapshot state;
    state.current_frame = 2; state.background_frame = 1;
    Snapshot last;
    unsigned capped_emits = 0;
    auto sample = [&]
    {
        capped.observe([&] { return now; }, [&] { return state; },
            [&](const Snapshot& s) { last = s; ++capped_emits; });
    };
    sample();
    for (unsigned i = 0; i < Observer::repeat_limit; ++i) { now += 1.01; sample(); }
    check(capped_emits == 65 && last.observed_repeat_events == 64 && last.repeat_cap_reached,
          "64th independent repeat reports repeat-cap exhaustion");
    for (unsigned i = 0; i < 10000; ++i) { now += 2.; sample(); }
    check(capped_emits == 65, "repeated identical halted frames stay bounded after exhaustion");
    for (unsigned i = 1; i < Observer::distinct_limit; ++i) { state.first_incomplete_line = i; sample(); }
    check(capped_emits == 128 && last.observed_distinct_states == 64 && last.distinct_cap_reached && last.repeat_cap_reached,
          "repeat exhaustion preserves64 distinct slots and reports both caps");
    ++state.first_incomplete_line; sample(); capped.recovered(); sample();
    check(capped_emits == 128, "new states and recovery cannot refill lifetime caps");
    std::ostringstream caps; caps << last;
    check(caps.str().find("observation(distinct/repeats/distinct_cap/repeat_cap/repeat)=64/64/1/1/0") != std::string::npos,
          "cap exhaustion is observable in actual event serialization");
    Observer distinct_first;
    unsigned reciprocal_emits = 0;
    auto reciprocal = [&]
    {
        distinct_first.observe([&] { return now; }, [&] { return state; },
            [&](const Snapshot& s) { last = s; ++reciprocal_emits; });
    };
    for (unsigned i = 0; i < Observer::distinct_limit; ++i) { state.first_incomplete_line = i; reciprocal(); }
    check(reciprocal_emits == 64 && last.distinct_cap_reached && !last.repeat_cap_reached,
          "distinct exhaustion leaves the separate repeat budget available");
    state.first_incomplete_line = 999999; reciprocal();
    check(reciprocal_emits == 64, "65th distinct state is not admitted");
    state.first_incomplete_line = 0; reciprocal();
    check(reciprocal_emits == 65 && last.repeat_event && last.observed_repeat_events == 1,
          "known-state recurrence remains observable after distinct exhaustion");
    for (unsigned i = 1; i < Observer::repeat_limit; ++i) { now += 1.01; reciprocal(); }
    check(reciprocal_emits == 128 && last.distinct_cap_reached && last.repeat_cap_reached,
          "reciprocal admission order preserves the same128-event lifetime bound");

    Observer exceptions;
    unsigned attempts = 0;
    exceptions.observe([]() -> double { throw std::runtime_error("clock"); },
        [&] { ++attempts; return input; }, [](const Snapshot&) {});
    check(attempts == 0, "clock exception prevents metadata collection and is isolated");
    exceptions.observe([] { return 0.; }, []() -> Snapshot { throw std::runtime_error("collection"); }, [](const Snapshot&) {});
    exceptions.observe([] { return 0.; }, [&] { return input; },
        [](const Snapshot&) { throw std::runtime_error("emission"); });
    exceptions.observe([] { return .11; }, [&] { return input; }, [&](const Snapshot&) { ++attempts; });
    check(attempts == 1, "collection/logger exceptions retain bounded later observation progress");
    Observer no_heap;
    bool emitted_without_allocation = false;
    fail_next_allocation = true;
    no_heap.observe([] { return 0.; }, [&] { return input; }, [&](const Snapshot&) { emitted_without_allocation = true; });
    check(fail_next_allocation.exchange(false) && emitted_without_allocation,
          "enabled key and admission bookkeeping allocate no heap storage");
}
void enabled()
{
    using namespace ALWorldRetentionInputs;
    setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    Fixture f;
    f.background.source_frame = f.gFrameCount;
    f.observe();
    check(f.attempts == 0, "current background consumes no event");
    f.background.source_frame = f.gFrameCount - 1;
    f.observe(); // world key1
    check(f.attempts == 1, "actual retained boundary emits first readiness state");
    const std::string first = f.output.str();
    check(first.find("world_retained_inputs view=main current_frame=100 background_frame=99 background_age_known=1 background_age=1") != std::string::npos,
          "actual snapshot serializes frames and age");
    check(first.find("earliest_missing_product=exposure first_incomplete_line=123") != std::string::npos,
          "earliest missing current exposure is distinguished from later post products");
    check(first.find("exposure_policy(frame/current/meter/adapt/classic)=100/1/1/1/0") != std::string::npos,
          "actual collector preserves producer policy facts");
    for (U32 frame = 101; frame < 10101; ++frame) { f.next(frame); f.observe(); }
    check(f.attempts == 1, "10000 changing frames/epochs do not spend distinct keys");
    f.capture.first_incomplete_source_line = 124; f.observe(); // key2
    check(f.attempts == 2, "different first incomplete source line is admitted");
    f.capture.hero_probe_face_capture = true; f.observe(); // key3
    check(f.attempts == 3 && f.output.str().find("view=hero") != std::string::npos,
          "view qualification is part of the stable key and serialization");
    f.capture.hero_probe_face_capture = false;
    f.capture.composed_light_target = false; f.capture.main_lighting_ready = true;
    f.capture.base_light_map_target = f.capture.base_light_target = false;
    f.observe(); // key4
    check(f.attempts == 4 && f.output.str().find("earliest_missing_product=unknown_with_lighting_readiness") != std::string::npos,
          "fused lighting is a valid alternative to separate map/base targets");
    const auto before_collect_throw = f.attempts;
    f.capture.retention_inputs_observer.observe([] { return 0.; }, []() -> Snapshot { throw std::runtime_error("controlled collection failure"); },
        [&](const Snapshot&) { ++f.attempts; });
    check(f.attempts == before_collect_throw, "collection exception is isolated");
    f.capture.first_incomplete_source_line = 777; f.throw_emitter = true; f.observe(); // key5
    check(f.attempts == 5, "emitter exception is isolated after bounded admission");
    f.throw_emitter = false; f.capture.first_incomplete_source_line = 778; f.observe(); // key6
    check(f.attempts == 6, "observation continues after an emitter exception");
    for (U32 line = 10000; line < 10058; ++line) { f.capture.first_incomplete_source_line = line; f.observe(); }
    check(f.attempts == 64, "world diagnostics admit at most64 independent readiness keys");
    f.capture.first_incomplete_source_line = 123456; f.observe();
    check(f.attempts == 64, "65th world key is rejected");
    check(f.output.str().size() < 64 * 4096, "event serialization has bounded total output");
    check(f.output.str().find("http:") == std::string::npos && f.output.str().find("asset=") == std::string::npos,
          "snapshot schema has no URL or asset identity field");
    recurring();
}
void schema()
{
    setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    Fixture f;
    f.observe();
    const auto text = f.output.str();
    check(text.find("world_retained_inputs") != std::string::npos, "actual boundary has structured retention evidence");
    check(text.find("current_frame=100 background_frame=99") != std::string::npos, "actual boundary reports frame correspondence");
    check(text.find("earliest_missing_product=exposure first_incomplete_line=123") != std::string::npos, "actual boundary identifies missing product and source marker");
    check(text.find("base_state=hero_inputs base_state(frame/current)=100/1") != std::string::npos,
          "actual collector serializes exact current base prerequisite state");
    check(text.find("hero(selected/ready/advertised/owner_match/epoch_match)=1/1/1/0/1") != std::string::npos,
          "actual collector distinguishes advertised selected hero from wrong published owner");
    Fixture absent;
    absent.mHeroProbeManager.mNearestHero.object = nullptr;
    absent.observe();
    check(absent.output.str().find("hero(selected/ready/advertised/owner_match/epoch_match)=0/1/1/0/1") != std::string::npos,
          "actual collector does not dereference an absent selected hero");
    Fixture dead;
    dead.hero_object.dead = true;
    dead.observe();
    check(dead.output.str().find("hero(selected/ready/advertised/owner_match/epoch_match)=0/1/1/0/1") != std::string::npos,
          "actual collector qualifies selected hero liveness independently of stale readiness");
    Fixture changed;
    changed.mHeroProbeManager.mNativePublishedOwner = changed.hero_object.id;
    changed.mHeroProbeManager.mNativePublishedEpoch = 4;
    changed.mHeroProbeManager.mMetalRadianceReady = false;
    changed.mHeroProbeManager.mHeroData.heroProbeCount = 0;
    changed.capture.retention_inputs_base_state_frame = std::numeric_limits<U32>::max();
    changed.observe();
    check(changed.output.str().find("hero(selected/ready/advertised/owner_match/epoch_match)=1/0/0/1/0") != std::string::npos,
          "actual collector distinguishes ready/advertised/owner/epoch predicates");
    check(changed.output.str().find("base_state(frame/current)=18446744073709551615/0") != std::string::npos,
          "actual collector preserves unknown base-state frame without claiming current provenance");
}
void classification()
{
    using namespace ALWorldRetentionInputs;
    Snapshot s;
    s.current_frame = s.capture_frame = s.display_frame = s.presentation_frame = 10;
    s.background_frame = 9;
    s.complete = s.gbuffer = s.composed_light = s.opaque_post = s.display_post = s.presentation = true;
    check(std::string_view(s.earliestMissing()) == "unknown_with_readiness_flags", "present targets do not invent a publication failure cause");
    const auto stable_key = key(s);
    s.current_frame = s.capture_frame = s.display_frame = s.presentation_frame = 42; s.background_frame = 40;
    check(key(s).view() == stable_key.view(), "frame IDs and background age are absent from keys");
    s.presentation_frame = 41;
    check(std::string_view(s.earliestMissing()) == "presentation", "stale final presentation is distinct");
    s.display_frame = 41;
    check(std::string_view(s.earliestMissing()) == "display_post", "stale display source precedes final presentation");
    s.post_policy_frame = s.current_frame; s.hdr = s.dynamic_exposure_enabled = true;
    check(std::string_view(s.earliestMissing()) == "exposure", "known exposure requirement precedes missing display");
    s.post_policy_frame = Snapshot::unknown;
    check(std::string_view(s.earliestMissing()) == "display_post", "unknown post policy does not invent an exposure requirement");
    s.opaque_post = false;
    check(std::string_view(s.earliestMissing()) == "opaque_post", "opaque post precedes optics");
    s.composed_light = false;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "absent separate targets do not invent a lighting producer cause");
    s.light_map = true;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "light map presence alone does not qualify the next lighting stage");
    s.base_light = true;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "base-ready lighting still retains an unknown causal stage");
    s.light_map = false;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "base-ready map-absent path remains conservative");
    s.fused_light = true; s.base_light = false;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "pending fused staging does not invent a producer failure");
    s.fused_light = false;
    check(std::string_view(s.earliestMissing()) == "unknown_with_lighting_readiness", "consumed fused staging may legitimately be absent");
    s.gbuffer = false;
    check(std::string_view(s.earliestMissing()) == "gbuffer", "gbuffer is an earlier required product");
    s.complete = false;
    check(std::string_view(s.earliestMissing()) == "capture_incomplete", "explicit capture failure precedes products");
    s.capture_frame = 40;
    check(std::string_view(s.earliestMissing()) == "current_capture", "uncaptured current frame is distinguished");
    s.background_frame = Snapshot::unknown;
    std::ostringstream unknown; unknown << s;
    check(unknown.str().find("background_age_known=0") != std::string::npos && unknown.str().find(" background_age=") == std::string::npos,
          "unknown background age does not underflow");
    s.background_frame = s.current_frame + 1;
    std::ostringstream future; future << s;
    check(future.str().find("background_age_known=0") != std::string::npos, "future/wrapped frame does not fabricate age");
    for (unsigned value = 0; value <= static_cast<unsigned>(BaseState::encoded); ++value)
    {
        s.base_state = static_cast<BaseState>(value);
        check(std::string_view(baseStateName(s.base_state)) != "unobserved" || value == 0,
              "every defined base producer state has a distinct named serialization");
    }
    s.base_state_frame = s.current_frame;
    const auto current_base_key = key(s);
    s.base_state_frame = s.current_frame - 1;
    check(key(s).view() != current_base_key.view(), "base producer state freshness is part of stable readiness");
    for (bool Snapshot::* flag : {&Snapshot::hero_selected, &Snapshot::hero_ready, &Snapshot::hero_advertised,
                                  &Snapshot::hero_owner_match, &Snapshot::hero_epoch_match})
    {
        const auto old_key = key(s); s.*flag = true;
        check(key(s).view() != old_key.view(), "each hero coherence predicate affects stable state admission");
    }
}
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string_view mode(argv[1]);
    if (mode == "disabled") disabled();
    else if (mode == "enabled") enabled();
    else if (mode == "classification") classification();
    else if (mode == "schema") schema();
    else return 2;
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
