/** PPD003 actual retained-boundary collection, bounded keys and serialization. */
#include "../alworldretentioninputs.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
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
unsigned checks = 0, failures = 0;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::cerr << "FAILED: " << label << '\n'; }
}
struct Fixture
{
    struct Background { U64 source_frame = 99; } background;
    struct Capture
    {
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
    } capture;
    U32 gFrameCount = 100;
    U64 mMetalExposureHistoryEpoch = 1;
    bool mMetalExposureHistoryReady = false;
    unsigned collected = 0, attempts = 0;
    bool throw_emitter = false;
    std::ostringstream output;
    Fixture() { capture.ui_frame_background = &background; }
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
    ALWorldRetentionInputs::observe([]() -> Snapshot { throw std::runtime_error("controlled collection failure"); },
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
