/**
 * @file almetalresourceplan.h
 * @brief Native backing identity, command uses and ordered reuse authority.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

namespace ALMetalResourcePlanning
{
enum class Kind { buffer, texture, staging_buffer, indirect_commands };
enum class Origin { viewer_owned, external_unknown };
enum class Access : unsigned { read = 1, write = 2, read_write = 3 };
enum class Pass { render = 1, compute = 2, blit = 4, present = 8, readback = 16 };
enum class Outcome { completed, error, abandoned };
struct Description
{
    Kind kind = Kind::buffer;
    Origin origin = Origin::viewer_owned;
    bool memoryless = false;
    std::uint64_t declared_bytes = 0;
    std::optional<std::uint64_t> metal_reported_bytes;
};
struct Stats
{
    // Unique backing LEASES, not wrappers, logical admission debts, or physical
    // resident UMA. Driver currentAllocatedSize remains a separate metric.
    std::uint64_t owned_backings = 0, owned_declared_bytes = 0;
    std::uint64_t owned_metal_reported_bytes = 0, owned_unknown_metal_sizes = 0;
    std::uint64_t owned_unknown_declared_bytes = 0;
    std::uint64_t memoryless_backings = 0, memoryless_declared_bytes = 0;
    std::uint64_t external_unknown_records = 0;
    std::uint64_t backing_creations = 0, backing_retirements = 0;
    std::uint64_t logical_aliases = 0, active_commands = 0;
    std::uint64_t uncommitted_uses = 0, submitted_uses = 0;
    std::uint64_t opaque_uncommitted_commands = 0;
    std::uint64_t completed_commands = 0, errored_commands = 0, abandoned_commands = 0;
};
struct CommandStats
{
    // Resource counts are distinct (backing, logical generation) uses; they
    // must not be added to Plan::Stats unique backing allocation bytes.
    std::uint64_t resources = 0, read_resources = 0, write_resources = 0;
    std::uint64_t passes = 0, active_pass = 0, submission_serial = 0;
    bool opaque = false, submitted = false, retired = false, valid = true;
};
struct State;
class View;
class Command;
class Plan;

class Allocation final
{
public:
    ~Allocation();
    std::uint64_t identity() const noexcept { return mIdentity; }
    const Description& description() const noexcept { return mDescription; }
private:
    Allocation(std::shared_ptr<State>, Description, std::shared_ptr<const void>, const void*);
    std::shared_ptr<State> mState;
    Description mDescription;
    // Optional actual native backing owner. Never retains a command or viewer
    // owner; command-use leases keep this owner through native retirement.
    std::shared_ptr<const void> mNativeOwner;
    const void* mNativeIdentity = nullptr;
    std::uint64_t mIdentity = 0, mGeneration = 1, mAliases = 0;
    std::uint64_t mUncommitted = 0, mSubmitted = 0;
    std::uint64_t mUncommittedOwner = 0;
    std::uint64_t mUncommittedWrites = 0, mSubmittedWrites = 0;
    bool mRegistered = false;
    friend class View;
    friend class Command;
    friend class Plan;
};

// A CPU-visible logical alias. Shader-format views within one typed wrapper
// share this value; retainSharedBacking copies it into an independent alias.
class View final
{
public:
    View() = default;
    View(const View&);
    View& operator=(const View&);
    View(View&&) noexcept;
    View& operator=(View&&) noexcept;
    ~View();
    explicit operator bool() const noexcept { return bool(mAllocation); }
    std::uint64_t identity() const noexcept;
    // Factory-captured native backing, for validating a declared import or
    // format view. It grants no encoding permission and owns no extra lease.
    const void* nativeIdentity() const noexcept;
    std::uint64_t generation() const noexcept { return mGeneration; }
    bool canIssueGeneration(const std::shared_ptr<Command>& current = {}) const noexcept;
    bool issueGeneration(const std::shared_ptr<Command>& current = {}) noexcept;
    // A synchronous same-queue readback may settle submitted writers. It
    // cannot submit somebody else's uncommitted or unknown native producer.
    // This certifies queue/lifetime readiness, NOT successful output contents;
    // existing producer command/publication status remains authoritative.
    bool canReadAfterQueueBarrier() const noexcept;
private:
    explicit View(std::shared_ptr<Allocation>);
    void release() noexcept;
    bool mayIssueGeneration(const std::shared_ptr<Command>&) const noexcept;
    std::shared_ptr<Allocation> mAllocation;
    std::uint64_t mGeneration = 0;
    friend class Plan;
    friend class Command;
};

// One native command's exact uses. Encoding is owner-thread work; completion
// may retire concurrently. No command creation serial establishes queue order.
class Command final
{
public:
    ~Command();
    std::uint64_t beginPass(Pass);
    bool endPass(std::uint64_t) noexcept;
    bool use(const View&, Access);
    void markOpaque() noexcept;
    void invalidate() noexcept;
    bool prepareSubmit() noexcept;
    bool submitted(std::uint64_t actual_queue_serial) noexcept;
    void retire(Outcome) noexcept;
    CommandStats stats() const noexcept;
private:
    explicit Command(std::shared_ptr<State>);
    struct Key
    {
        std::uint64_t identity, generation;
        bool operator==(const Key&) const = default;
    };
    struct Hash { std::size_t operator()(const Key&) const noexcept; };
    struct Use
    {
        std::shared_ptr<Allocation> allocation;
        unsigned access = 0, pass_kinds = 0;
        std::uint64_t first_pass = 0, last_pass = 0;
    };
    std::shared_ptr<State> mState;
    std::unordered_map<Key, Use, Hash> mUses;
    CommandStats mStats;
    std::uint64_t mActivePass = 0;
    unsigned mPassKind = 0;
    bool mSubmitting = false;
    std::uint64_t mIdentity = 0;
    friend class Plan;
    friend class View;
};

class Plan final
{
public:
    Plan();
    View allocate(Description, std::shared_ptr<const void> native_owner = {},
                  const void* native_identity = nullptr);
    bool containsCurrent(const View&) const noexcept;
    std::shared_ptr<Command> command();
    Stats stats() const noexcept;
    // One-shot allocation exception seam, independent of the native allocator.
    // Normal devices never arm this; ownership must remain transactional.
    void failNextAllocationForTesting() noexcept;
private:
    std::shared_ptr<State> mState;
};
}
