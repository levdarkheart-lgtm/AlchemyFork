/**
 * @file pipeline_cpu_source_lease_native.inl
 * @brief CPU description pins without a hash allocation for each source.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include in the native capture scope and the typed owner fixture.
 */
struct WorldCpuLease
{
    std::vector<std::shared_ptr<const void>> owners;
    std::array<const void*, 16> seen{};
    void retain(std::shared_ptr<const void> owner)
    {
        if (!owner) return;
        const auto* identity = owner.get();
        const auto bits = reinterpret_cast<std::uintptr_t>(identity);
        const auto slot = ((bits >> 4) ^ (bits >> 12)) & (seen.size() - 1);
        if (seen[slot] == identity) return;
        owners.push_back(std::move(owner));
        // Mark only after successful strong retention. The held vector keeps
        // every cached address alive; a collision causes an extra pin, never
        // a false hit or a released descriptor during encoding.
        seen[slot] = identity;
    }
};
