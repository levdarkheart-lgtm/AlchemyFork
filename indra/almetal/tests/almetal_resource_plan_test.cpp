/**
 * @file almetal_resource_plan_test.cpp
 * @brief Unique backing, alias, submission and actual last-reader contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalresourceplan.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <new>
#include <thread>

namespace
{
namespace RP = ALMetalResourcePlanning;
RP::Description owned(std::uint64_t bytes = 64)
{
    RP::Description d; d.declared_bytes = bytes; d.metal_reported_bytes = bytes + 16; return d;
}
void use(const std::shared_ptr<RP::Command>& command, const RP::View& view, RP::Access access)
{
    const auto pass = command->beginPass(RP::Pass::render); REQUIRE(pass);
    REQUIRE(command->use(view, access)); REQUIRE(command->endPass(pass));
}
}

TEST_CASE("Resource plan counts unique backing through aliases COW and last command reader",
          "[metal-resource-plan]")
{
    RP::Plan plan;
    auto original = plan.allocate(owned()); REQUIRE(original);
    auto alias = original;
    CHECK(alias.identity() == original.identity());
    CHECK(plan.stats().owned_backings == 1);
    CHECK(plan.stats().owned_declared_bytes == 64);
    CHECK(plan.stats().owned_metal_reported_bytes == 80);
    CHECK(plan.stats().logical_aliases == 2);
    CHECK_FALSE(original.canIssueGeneration());
    auto copied = plan.allocate(owned()); REQUIRE(copied);
    CHECK(copied.identity() != original.identity()); // A COW allocation is distinct.
    CHECK(plan.stats().owned_backings == 2);
    auto command = plan.command(); use(command, alias, RP::Access::read);
    REQUIRE(command->prepareSubmit()); REQUIRE(command->submitted(1));
    original = {}; alias = {};
    CHECK(plan.stats().owned_backings == 2); // The command retains its backing.
    command->retire(RP::Outcome::completed);
    CHECK(plan.stats().owned_backings == 1);
    copied = {};
    CHECK(plan.stats().owned_backings == 0);
    CHECK(plan.stats().owned_declared_bytes == 0);
    CHECK(plan.stats().backing_creations == plan.stats().backing_retirements);
}

TEST_CASE("Resource imports preserve native identity without accepting another registry",
          "[metal-resource-plan]")
{
    RP::Plan plan, other;
    int native = 0;
    auto backing = plan.allocate(owned(), {}, &native); REQUIRE(backing);
    auto alias = backing;
    CHECK(backing.nativeIdentity() == &native);
    CHECK(alias.nativeIdentity() == &native);
    CHECK(plan.containsCurrent(backing)); CHECK(plan.containsCurrent(alias));
    CHECK_FALSE(other.containsCurrent(backing)); CHECK_FALSE(plan.containsCurrent({}));
    auto foreign = other.allocate(owned(), {}, &native); REQUIRE(foreign);
    CHECK_FALSE(plan.containsCurrent(foreign)); // Pointer equality alone is insufficient.
    alias = {};
    REQUIRE(backing.issueGeneration());
    CHECK(plan.containsCurrent(backing));
    CHECK(backing.nativeIdentity() == &native);
}

TEST_CASE("Resource reuse follows actual submission not command creation or GPU completion",
          "[metal-resource-plan]")
{
    RP::Plan plan; auto view = plan.allocate(owned());
    auto made_first = plan.command(), made_second = plan.command();
    use(made_first, view, RP::Access::read);
    use(made_second, view, RP::Access::write);
    CHECK_FALSE(view.canIssueGeneration());
    REQUIRE(made_second->prepareSubmit()); REQUIRE(made_second->submitted(1));
    CHECK_FALSE(view.canIssueGeneration()); // The first-created command is still unsubmitted.
    REQUIRE(made_first->prepareSubmit()); REQUIRE(made_first->submitted(2));
    CHECK(view.canIssueGeneration()); // Both are ordered ahead of any future commit.
    const auto id = view.identity(), previous_generation = view.generation();
    REQUIRE(view.issueGeneration());
    CHECK(view.identity() == id);
    CHECK(view.generation() == previous_generation + 1);
    CHECK(plan.stats().submitted_uses == 2); // No early native retirement.
    auto later = plan.command(); use(later, view, RP::Access::write);
    CHECK_FALSE(view.canIssueGeneration());
    made_first->retire(RP::Outcome::completed);
    made_second->retire(RP::Outcome::error);
    CHECK(plan.stats().submitted_uses == 0);
    CHECK(plan.stats().uncommitted_uses == 1);
    later.reset(); // Abandonment releases an encoded but never submitted use.
    CHECK(view.canIssueGeneration());
    CHECK(plan.stats().completed_commands == 1);
    CHECK(plan.stats().errored_commands == 1);
    CHECK(plan.stats().abandoned_commands == 1);
}

TEST_CASE("Resource pass uses deduplicate backing while preserving read write and generation scopes",
          "[metal-resource-plan]")
{
    RP::Plan plan, other;
    auto view = plan.allocate(owned()); auto foreign = other.allocate(owned());
    auto command = plan.command();
    CHECK_FALSE(command->use(view, RP::Access::read));
    const auto first = command->beginPass(RP::Pass::render); REQUIRE(first);
    CHECK_FALSE(command->beginPass(RP::Pass::compute));
    CHECK_FALSE(command->use(foreign, RP::Access::read));
    REQUIRE(command->use(view, RP::Access::read));
    REQUIRE(command->use(view, RP::Access::read));
    CHECK_FALSE(command->prepareSubmit());
    CHECK_FALSE(command->endPass(first + 1));
    REQUIRE(command->endPass(first));
    const auto second = command->beginPass(RP::Pass::compute); REQUIRE(second);
    REQUIRE(command->use(view, RP::Access::read_write)); REQUIRE(command->endPass(second));
    CHECK(command->stats().passes == 2);
    CHECK(command->stats().resources == 1);
    CHECK(command->stats().read_resources == 1);
    CHECK(command->stats().write_resources == 1);
    CHECK(plan.stats().uncommitted_uses == 1);
    REQUIRE(command->prepareSubmit()); REQUIRE(command->submitted(7));
    CHECK_FALSE(command->submitted(8)); CHECK_FALSE(command->beginPass(RP::Pass::render));
    command->retire(RP::Outcome::completed);
    command->retire(RP::Outcome::error); // Idempotent: neither double retirement nor relabeling.
    CHECK(plan.stats().completed_commands == 1); CHECK(plan.stats().errored_commands == 0);
}

TEST_CASE("Unknown native scopes prevent uncommitted alias reuse and retire on all terminal paths",
          "[metal-resource-plan]")
{
    for (const auto outcome : {RP::Outcome::completed, RP::Outcome::error, RP::Outcome::abandoned})
    {
        RP::Plan plan; auto view = plan.allocate(owned()); auto command = plan.command();
        command->markOpaque(); command->markOpaque();
        CHECK(plan.stats().opaque_uncommitted_commands == 1);
        CHECK_FALSE(view.canIssueGeneration());
        if (outcome != RP::Outcome::abandoned)
        { REQUIRE(command->prepareSubmit()); REQUIRE(command->submitted(1)); CHECK(view.canIssueGeneration()); }
        command->retire(outcome);
        CHECK(plan.stats().opaque_uncommitted_commands == 0);
        CHECK(plan.stats().active_commands == 0);
        CHECK(view.canIssueGeneration());
    }
}

TEST_CASE("Resource completion before commit publication cannot resurrect backing uses",
          "[metal-resource-plan]")
{
    RP::Plan plan; auto view = plan.allocate(owned()); auto command = plan.command();
    use(command, view, RP::Access::read); REQUIRE(command->prepareSubmit());
    view = {};
    std::thread completion([&] { command->retire(RP::Outcome::completed); });
    completion.join();
    CHECK(plan.stats().owned_backings == 0);
    CHECK(plan.stats().uncommitted_uses == 0);
    REQUIRE(command->submitted(1));
    CHECK(plan.stats().submitted_uses == 0); CHECK(plan.stats().active_commands == 0);
    CHECK(command->stats().retired); CHECK(command->stats().submitted);
}

TEST_CASE("Allocation registry distinguishes memoryless external and reported native scopes",
          "[metal-resource-plan]")
{
    RP::Plan plan;
    auto unknown = owned(8); unknown.metal_reported_bytes.reset();
    auto a = plan.allocate(unknown);
    auto memoryless = owned(32); memoryless.memoryless = true;
    auto b = plan.allocate(memoryless);
    auto external = owned(1'024); external.origin = RP::Origin::external_unknown;
    auto c = plan.allocate(external);
    CHECK(plan.stats().owned_backings == 1); CHECK(plan.stats().owned_declared_bytes == 8);
    CHECK(plan.stats().owned_unknown_metal_sizes == 1); CHECK(plan.stats().owned_metal_reported_bytes == 0);
    CHECK(plan.stats().owned_unknown_declared_bytes == 8);
    CHECK(plan.stats().memoryless_backings == 1); CHECK(plan.stats().memoryless_declared_bytes == 32);
    CHECK(plan.stats().external_unknown_records == 1);
    a = {}; b = {}; c = {};
    CHECK(plan.stats().owned_backings == 0); CHECK(plan.stats().memoryless_backings == 0);
    CHECK(plan.stats().external_unknown_records == 0);
}

TEST_CASE("Allocation registration is transactional through overflow failure and destructor reentry",
          "[metal-resource-plan][PPD-415]")
{
    RP::Plan plan;
    auto maximal = owned(std::numeric_limits<std::uint64_t>::max());
    maximal.metal_reported_bytes.reset();
    auto full = plan.allocate(maximal); REQUIRE(full);
    CHECK_FALSE(plan.allocate(owned(1)));
    CHECK(plan.stats().backing_creations == 1); CHECK(plan.stats().backing_retirements == 0);
    full = {};
    CHECK(plan.stats().owned_declared_bytes == 0); CHECK(plan.stats().backing_retirements == 1);
    bool owner_retired = false;
    auto native = std::shared_ptr<const void>(new int(7), [&](const void* value) {
        CHECK(plan.stats().owned_backings == 0); // Reenters registry from native owner destruction.
        owner_retired = true; delete static_cast<const int*>(value);
    });
    plan.failNextAllocationForTesting();
    CHECK_THROWS_AS(plan.allocate(owned(), std::move(native)), std::bad_alloc);
    CHECK(owner_retired);
    CHECK(plan.stats().backing_creations == 1); CHECK(plan.stats().backing_retirements == 1);
    auto recovered = plan.allocate(owned()); REQUIRE(recovered);
    CHECK(plan.stats().owned_backings == 1);
}

TEST_CASE("Exact command identity permits ended-pass generation reuse without guessing submission order",
          "[metal-resource-plan]")
{
    RP::Plan plan; auto view = plan.allocate(owned()); auto current = plan.command();
    const auto pass = current->beginPass(RP::Pass::render); REQUIRE(pass);
    REQUIRE(current->use(view, RP::Access::write));
    CHECK_FALSE(view.canIssueGeneration(current)); // This render pass is still active.
    REQUIRE(current->endPass(pass));
    CHECK_FALSE(view.canIssueGeneration());
    CHECK(view.canIssueGeneration(current));
    REQUIRE(view.issueGeneration(current));
    use(current, view, RP::Access::read_write);
    CHECK(current->stats().resources == 2); // Two logical generations, one backing.
    CHECK(plan.stats().owned_backings == 1);
    CHECK(plan.stats().uncommitted_uses == 2);
    CHECK(view.canIssueGeneration(current));
    auto other = plan.command(); use(other, view, RP::Access::read);
    CHECK_FALSE(view.canIssueGeneration(current));
    CHECK_FALSE(view.canIssueGeneration(other));
    other->retire(RP::Outcome::abandoned);
    // A formerly mixed owner set is conservatively unknown until all its
    // pending uses are queued/retired; it must not guess which owner remains.
    CHECK_FALSE(view.canIssueGeneration(current));
    REQUIRE(current->prepareSubmit()); REQUIRE(current->submitted(1));
    CHECK(view.canIssueGeneration());
    current->retire(RP::Outcome::completed);
    CHECK(plan.stats().uncommitted_uses == 0); CHECK(plan.stats().submitted_uses == 0);
}

TEST_CASE("Readback readiness separates uncommitted writes opaque scopes and producer success",
          "[metal-resource-plan][PPD-417]")
{
    RP::Plan plan; auto view = plan.allocate(owned());
    CHECK(view.canReadAfterQueueBarrier());
    auto reader = plan.command(); use(reader, view, RP::Access::read);
    CHECK(view.canReadAfterQueueBarrier()); // Concurrent readers do not write its contents.
    reader->retire(RP::Outcome::abandoned);
    auto writer = plan.command(); use(writer, view, RP::Access::write);
    CHECK_FALSE(view.canReadAfterQueueBarrier());
    REQUIRE(writer->prepareSubmit());
    CHECK_FALSE(view.canReadAfterQueueBarrier()); // Not queued merely because commit begins.
    REQUIRE(writer->submitted(1));
    CHECK(view.canReadAfterQueueBarrier()); // A new ordered barrier can settle it.
    writer->retire(RP::Outcome::error);
    CHECK(view.canReadAfterQueueBarrier()); // Lifetime readiness is NOT content success.
    CHECK(plan.stats().errored_commands == 1);
    auto raw = plan.command(); raw->markOpaque();
    CHECK_FALSE(view.canReadAfterQueueBarrier());
    raw->retire(RP::Outcome::abandoned);
    CHECK(view.canReadAfterQueueBarrier());
}
