/**
 * @file alpipeline_source_cache_lifetime_native.inl
 * @brief Actual same-frame world source reuse versus publisher withdrawal.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the actual Publication/MaterialGroupPublication fixtures.
 */
namespace
{
namespace SourceCacheLifetime
{
using namespace Inputs::ALScenePublicationNative;
constexpr U32 sources = 6;
constexpr U64 native_byte_budget = 64ULL * 1024 * 1024;

void checkMisses(const SourceMissStats& before, const SourceMissStats& after,
                std::array<U64, 3> current, std::array<U64, 3> world)
{
    for (std::size_t at = 0; at < current.size(); ++at)
    {
        CAPTURE(at);
        CHECK(after.current_misses[at] - before.current_misses[at] == current[at]);
        CHECK(after.world_misses[at] - before.world_misses[at] == world[at]);
        CHECK(after.shadow_misses[at] == before.shadow_misses[at]);
    }
}
}
}

TEST_CASE("Actual same-frame main and probe source factories distinguish entry overflow from producer changes",
          "[almetal][pipeline-buffer-publication][source-cache-lifetime]")
{
    using namespace SourceCacheLifetime;
    for (bool entry_overflow : {false, true}) for (bool retain_cpu_reader : {false, true})
    {
        CAPTURE(entry_overflow, retain_cpu_reader);
        // One real native host; six distinct LLVB publications and completed
        // Legacy material inputs. Indexed/scalar sources share the ordinary
        // main/probe flavor policy, with distinct numeric bytes per producer.
        MaterialGroupPublication f(1, 12);
        f.flavor.native_resources = false;
        Inputs::gSavedSettings.native_resources = false;
        for (U32 row = 0; row < sources; ++row)
        {
            auto& info = f.materialDraw(row % 2 != 0);
            info.mEnvIntensity = .1f * (row + 1);
            for (auto& slot : info.mMaterialSlotList) slot.mEnvIntensity = info.mEnvIntensity;
        }
        const U32 frame = Inputs::gFrameCount; // Main, probe 0 and probe 1: one outer frame.
        const U32 entries = entry_overflow ? 2 : 8;
        ALNativeSourceWorkingSet residency(entries, native_byte_budget, 2);
        std::array<U64, sources> revisions{}, keys{}, main_serials{};
        std::array<std::weak_ptr<const Payload>, sources> metadata;
        std::array<std::shared_ptr<const Payload>, sources> cpu_readers;
        std::array<std::shared_ptr<const NativeTuple>, sources> native_readers;
        std::array<std::weak_ptr<const ALMetalBuffer>, sources> geometry, numeric;
        std::weak_ptr<const ALMetalTexture> diffuse;
        U64 gross_native_bytes = 0;
        for (U32 row = 0; row < sources; ++row)
        {
            revisions[row] = f.infos[row]->state->revision();
            auto flavor = f.flavor; flavor.indexed_materials = row % 2 != 0;
            keys[row] = flavor.key();
        }
        for (U32 view = 0; view < 3; ++view)
        {
            CAPTURE(view); // 0 main; 1 and 2 ordinary-probe source consumers.
            // The factory does not consume camera matrices. Move the actual
            // view/model owner while keeping native source generations and
            // semantic flavor unchanged, as the separate capture does.
            REQUIRE(f.region.update(f.model, LLVector3(.25f * view, 0.f, 0.f)));
            const auto before = source_miss_statistics;
            const auto before_serial = Inputs::sNativeSceneSerial;
            U64 builds = 0, hits = 0;
            for (U32 row = 0; row < sources; ++row)
            {
                auto& info = *f.infos[row];
                auto flavor = f.flavor; flavor.indexed_materials = row % 2 != 0;
                CHECK(flavor.key() == keys[row]); CHECK(info.state->revision() == revisions[row]);
                const bool admitted = row < entries;
                const bool expected_hit = view != 0 && (admitted || retain_cpu_reader);
                if (view && !admitted) CHECK(metadata[row].expired() == !retain_cpu_reader);
                auto lease = world(info, flavor, residency, Inputs::sNativeSceneSerial, frame);
                INFO((lease.failure ? lease.failure : "")); REQUIRE(lease);
                CHECK(lease.reused == expected_hit);
                if (lease.reused) ++hits; else ++builds;
                REQUIRE(lease.owner->worlds.size() == 1);
                CHECK(lease.owner->worlds.front().key == keys[row]);
                CHECK(lease.owner->producer_revision == revisions[row]);
                CHECK(info.state->revision() == revisions[row]);
                REQUIRE(lease.owner->native_tuple);
                REQUIRE(lease.prototype->native_material_source);
                REQUIRE(lease.prototype->native_material_source->matches(*lease.prototype));
                auto uniforms = lease.prototype->native_material_source->materialLease(); REQUIRE(uniforms);
                if (view == 0)
                {
                    main_serials[row] = lease.owner->serial;
                    metadata[row] = lease.owner;
                    native_readers[row] = lease.owner->native_tuple;
                    for (const auto& resource : native_readers[row]->resources)
                    {
                        REQUIRE(resource.bytes < native_byte_budget - gross_native_bytes);
                        gross_native_bytes += resource.bytes;
                    }
                    geometry[row] = lease.owner->vertices; numeric[row] = uniforms;
                    if (row == 0) diffuse = imageLease(info.mTexture.get());
                    if (retain_cpu_reader) cpu_readers[row] = lease.owner;
                }
                else
                {
                    CHECK((lease.owner->serial == main_serials[row]) == expected_hit);
                    // An exact native tuple is sufficient to keep already
                    // published uniform bytes alive, even after CPU metadata
                    // expires; rebuilding can find those bytes in its weak
                    // collision-checked native material cache.
                    CHECK(uniforms == numeric[row].lock());
                }
                if (admitted)
                {
                    CHECK(info.state->publication().get() == lease.owner.get());
                    CHECK(residency.touch(info.state->identity(), lease.owner->serial, frame));
                }
                else
                {
                    CHECK_FALSE(info.state->publication());
                    CHECK(info.state->dirty());
                    // Retaining a CPU reader does NOT re-admit its publisher.
                    // current() can reuse the exact weak transient while this
                    // touch still fails; no cache capacity was bypassed.
                    CHECK_FALSE(residency.touch(info.state->identity(), lease.owner->serial, frame));
                }
                CHECK(residency.entryCount() == (view ? std::min<U32>(entries, sources) :
                    std::min<U32>(row + 1, std::min<U32>(entries, sources))));
                CHECK(residency.estimatedRetainedBytes() < native_byte_budget);
            }
            const U64 rebuilt = view == 0 ? sources : entry_overflow && !retain_cpu_reader ? sources - entries : 0;
            CHECK(builds == rebuilt); CHECK(hits == sources - rebuilt);
            CHECK(Inputs::sNativeSceneSerial - before_serial == rebuilt);
            // Even the gross sum (before shared-resource deduplication) fits
            // the byte budget. The constructed pressure is the entry limit,
            // not the byte ceiling, source age, invalidation or shader key.
            CHECK(gross_native_bytes < native_byte_budget);
            // world() checks current() at entry and once again in extend().
            // These are getter events, deliberately not unique-source counts.
            checkMisses(before, source_miss_statistics,
                view == 0 ? std::array<U64, 3>{2 * sources, 0, 0} : std::array<U64, 3>{0, 0, 2 * rebuilt},
                {rebuilt, 0, 0});
            for (U32 row = 0; row < sources; ++row)
            {
                CHECK(metadata[row].expired() == (entry_overflow && row >= entries && !retain_cpu_reader));
                CHECK_FALSE(geometry[row].expired()); CHECK_FALSE(numeric[row].expired());
                if (row) CHECK(numeric[row].lock().get() != numeric[row - 1].lock().get());
            }
            CHECK(Inputs::gFrameCount == frame);
            std::cout << "NATIVE_SOURCE_CACHE_LIFETIME view=" << view << " sources=" << sources
                << " entry_limit=" << entries << " retained_cpu_reader=" << retain_cpu_reader
                << " source_builds=" << builds << " source_hits=" << hits
                << " current_first/revision/expired="
                << source_miss_statistics.current_misses[0] - before.current_misses[0] << '/'
                << source_miss_statistics.current_misses[1] - before.current_misses[1] << '/'
                << source_miss_statistics.current_misses[2] - before.current_misses[2]
                << " factory_absent/variant/promotion="
                << source_miss_statistics.world_misses[0] - before.world_misses[0] << '/'
                << source_miss_statistics.world_misses[1] - before.world_misses[1] << '/'
                << source_miss_statistics.world_misses[2] - before.world_misses[2]
                << " native_residency_entries=" << residency.entryCount()
                << " native_retained_estimated_bytes=" << residency.estimatedRetainedBytes()
                << " gross_all_source_native_bytes=" << gross_native_bytes
                << " same_outer_frame=1 semantic_flavor_unchanged=1 actual_factory_frontier=1 viewer_cause_claim=0\n";
        }
        // Independent controls demonstrate the different census signatures:
        // adding one real semantic variant is not eviction, and optional CPU
        // bounds promotion preserves both variants and the native tuple.
        {
            auto& info = *f.infos.front(); auto flavor = f.flavor;
            auto masked = flavor; masked.material_alpha_mode = 2;
            auto before = source_miss_statistics; auto serial = Inputs::sNativeSceneSerial;
            auto variant = world(info, masked, residency, Inputs::sNativeSceneSerial, frame); REQUIRE(variant);
            CHECK_FALSE(variant.reused); CHECK(variant.owner->worlds.size() == 2);
            CHECK(info.state->revision() == revisions[0]); CHECK(Inputs::sNativeSceneSerial == serial + 1);
            checkMisses(before, source_miss_statistics, {0, 0, 0}, {0, 1, 0});
            const auto tuple = variant.owner->native_tuple;
            auto bounds = masked; bounds.cohort_bounds = true;
            CHECK(bounds.key() == masked.key()); before = source_miss_statistics; serial = Inputs::sNativeSceneSerial;
            auto promotion = world(info, bounds, residency, Inputs::sNativeSceneSerial, frame); REQUIRE(promotion);
            CHECK(promotion.reused); CHECK(promotion.owner->worlds.size() == 2);
            CHECK(promotion.owner->native_tuple == tuple); CHECK(Inputs::sNativeSceneSerial == serial + 1);
            checkMisses(before, source_miss_statistics, {0, 0, 0}, {0, 0, 1});
            before = source_miss_statistics; serial = Inputs::sNativeSceneSerial;
            auto original = world(info, flavor, residency, Inputs::sNativeSceneSerial, frame); REQUIRE(original);
            CHECK(original.reused); CHECK(original.owner == promotion.owner); CHECK(Inputs::sNativeSceneSerial == serial);
            checkMisses(before, source_miss_statistics, {0, 0, 0}, {0, 0, 0});
        }
        {
            auto& info = *f.infos[1]; auto flavor = f.flavor; flavor.indexed_materials = true;
            const auto before = source_miss_statistics; const auto serial = Inputs::sNativeSceneSerial;
            f.positions(*info.mVertexBuffer, true); info.mVertexBuffer->unmapBuffer();
            REQUIRE(info.state->revision() != revisions[1]);
            auto changed = world(info, flavor, residency, Inputs::sNativeSceneSerial, frame); REQUIRE(changed);
            CHECK_FALSE(changed.reused); CHECK(changed.owner->producer_revision == info.state->revision());
            CHECK(changed.owner->vertices != geometry[1].lock());
            CHECK(Inputs::sNativeSceneSerial == serial + 1);
            checkMisses(before, source_miss_statistics, {0, 2, 0}, {1, 0, 0});
        }
        // No GPU rendering is needed to prove this ownership boundary. Drop
        // every CPU description and original LL geometry/image owner; the
        // copied native-only tuples independently preserve exact generations.
        cpu_readers = {}; residency.clear();
        for (const auto& weak : metadata) CHECK(weak.expired());
        f.infos.clear(); f.materials.clear(); f.image.image = nullptr; f.white = nullptr;
        for (U32 row = 0; row < sources; ++row)
        { CHECK_FALSE(geometry[row].expired()); CHECK_FALSE(numeric[row].expired()); }
        CHECK_FALSE(diffuse.expired());
        native_readers = {};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        const auto resources_retired = [&]
        {
            for (U32 row = 0; row < sources; ++row)
                if (!geometry[row].expired() || !numeric[row].expired()) return false;
            return diffuse.expired();
        };
        while (!resources_retired() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        CHECK(resources_retired());
    }
}
