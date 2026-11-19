/** CPU checks for the opt-in PPD-311/466/003/463 private warning diagnostic gate. */
#include "../../llcommon/alwarningdiagnostics.h"

static_assert(static_cast<unsigned>(ALWarningDiagnostics::Kind::count) == 9);

// Compile this source a second time with this definition to check the actual
// inline gate's process-wide state across independent translation units.
#if defined(AL_WARNING_DIAGNOSTICS_SECOND_TU)
bool warningDiagnosticsEnabledFromOtherTU()
{
    return ALWarningDiagnostics::enabled();
}

bool warningDiagnosticsTakeFromOtherTU(ALWarningDiagnostics::Kind kind, std::string_view key)
{
    return ALWarningDiagnostics::take(kind, key);
}
#else
#include <atomic>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

namespace
{
std::atomic<bool> fail_next_allocation{false};
}

// A serial fault at the actual std::string assignment exercises the gate's
// failure handling without allocating a key or changing the production API.
void* operator new(std::size_t size)
{
    if (fail_next_allocation.exchange(false, std::memory_order_relaxed)) throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

bool warningDiagnosticsEnabledFromOtherTU();
bool warningDiagnosticsTakeFromOtherTU(ALWarningDiagnostics::Kind, std::string_view);

namespace
{
unsigned checks = 0;
unsigned failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

void checkDisabled(const char* value)
{
    using namespace ALWarningDiagnostics;
    if (value) setenv("ALCHEMY_WARNING_DIAGNOSTICS", value, 1);
    else unsetenv("ALCHEMY_WARNING_DIAGNOSTICS");
    check(!enabled(), "gate starts disabled without the exact opt-in value");
    check(!warningDiagnosticsEnabledFromOtherTU(), "disabled state is shared across translation units");
    for (unsigned i = 0; i < static_cast<unsigned>(Kind::count); ++i)
    {
        check(!take(static_cast<Kind>(i), "disabled-input"), "disabled gate rejects each category");
        check(!warningDiagnosticsTakeFromOtherTU(static_cast<Kind>(i), "disabled-input"),
              "other translation unit cannot bypass disabled gate");
    }
    setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    check(!enabled(), "later environment changes do not enable a cached disabled gate");
    check(!warningDiagnosticsEnabledFromOtherTU(), "other translation unit shares the cached disabled gate");
    check(!take(Kind::skin, "after-environment-change"), "cached disabled gate rejects later input");
}

void checkEnabled()
{
    using namespace ALWarningDiagnostics;
    setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    check(enabled(), "exact opt-in enables diagnostics");
    check(warningDiagnosticsEnabledFromOtherTU(), "enabled state is shared across translation units");
    unsetenv("ALCHEMY_WARNING_DIAGNOSTICS");
    check(enabled(), "later environment changes do not disable a cached enabled gate");
    check(warningDiagnosticsEnabledFromOtherTU(), "other translation unit shares the cached enabled gate");

    check(take(Kind::skin, "shared-across-tu"), "first translation unit admits a new key");
    check(!warningDiagnosticsTakeFromOtherTU(Kind::skin, "shared-across-tu"),
          "second translation unit sees the first translation unit's deduplication");
    check(warningDiagnosticsTakeFromOtherTU(Kind::skin, "second-tu-input"),
          "second translation unit admits a distinct key");
    check(!take(Kind::skin, "second-tu-input"),
          "first translation unit sees the second translation unit's deduplication");

    constexpr unsigned thread_count = 16;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<unsigned> admitted{0};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (unsigned i = 0; i < thread_count; ++i)
    {
        threads.emplace_back([&, i]
        {
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (unsigned attempt = 0; attempt < 16; ++attempt)
            {
                const bool took = (i & 1)
                    ? warningDiagnosticsTakeFromOtherTU(Kind::skin_decode, "concurrent-input")
                    : take(Kind::skin_decode, "concurrent-input");
                if (took) admitted.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != thread_count) std::this_thread::yield();
    go.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    check(admitted.load(std::memory_order_relaxed) == 1,
          "concurrent calls across translation units admit exactly one copy of a key");

    check(!take(Kind::count, "invalid-kind"), "count sentinel is rejected");
    check(!take(static_cast<Kind>(~0u), "invalid-kind"), "out-of-range category is rejected");
    const std::string oversized(257, 'x');
    check(!take(Kind::bake, oversized), "oversized key is rejected");
    check(!warningDiagnosticsTakeFromOtherTU(Kind::bake, oversized),
          "other translation unit rejects oversized key");
    check(take(Kind::bake, std::string(256, 'x')), "exact maximum key length is admitted");
    check(!take(Kind::bake, std::string(256, 'x')), "maximum-length key is deduplicated");
    check(take(Kind::texture, "shared-across-tu"), "same key is independent in a different category");
    check(take(Kind::material, ""), "empty key has a bounded deduplicated slot");
    check(!take(Kind::material, ""), "empty key is deduplicated");
    std::array<char, 256> allocation_key;
    allocation_key.fill('a');
    const std::string_view long_key(allocation_key.data(), allocation_key.size());
    fail_next_allocation.store(true, std::memory_order_relaxed);
    const bool allocation_admitted = warningDiagnosticsTakeFromOtherTU(Kind::http, long_key);
    const bool allocation_failed = !fail_next_allocation.exchange(false, std::memory_order_relaxed);
    check(allocation_failed, "long-key assignment exercised the intended allocation failure");
    check(!allocation_admitted, "allocation failure returns false without escaping the gate");
    check(take(Kind::http, long_key), "same key can be admitted after allocation failure");
    check(!take(Kind::http, long_key), "recovered key is deduplicated");

    // Invalid/oversized attempts must consume no category slots. The initial
    // admissions above are included when verifying each independent 64-key cap.
    std::array<unsigned, static_cast<unsigned>(Kind::count)> initial{};
    initial[static_cast<unsigned>(Kind::skin)] = 2;
    for (Kind kind : {Kind::skin_decode, Kind::bake, Kind::texture, Kind::material, Kind::http})
        initial[static_cast<unsigned>(kind)] = 1;
    for (unsigned i = 0; i < static_cast<unsigned>(Kind::count); ++i)
    {
        const Kind kind = static_cast<Kind>(i);
        for (unsigned n = initial[i]; n < 64; ++n)
        {
            const std::string key = "fill-" + std::to_string(i) + '-' + std::to_string(n);
            check((n & 1) ? warningDiagnosticsTakeFromOtherTU(kind, key) : take(kind, key),
                  "each distinct key below the category cap is admitted");
            check(!take(kind, key), "admitted fill key is deduplicated");
        }
        check(!take(kind, "overflow-input"), "65th distinct category key is rejected");
        check(!warningDiagnosticsTakeFromOtherTU(kind, "other-tu-overflow"),
              "category cap is shared across translation units");
    }
}
}

int main(int argc, char** argv)
{
    const std::string_view mode = argc == 2 ? argv[1] : "";
    if (mode == "disabled") checkDisabled(nullptr);
    else if (mode == "not-one") checkDisabled("true");
    else if (mode == "enabled") checkEnabled();
    else
    {
        std::cerr << "Usage: alwarningdiagnostics_test {disabled|not-one|enabled}\n";
        return 2;
    }
    std::cout << "checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
#endif
