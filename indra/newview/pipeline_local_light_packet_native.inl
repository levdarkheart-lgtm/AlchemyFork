/**
 * @file pipeline_local_light_packet_native.inl
 * @brief One owner-only point-light value packet for a single main view.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
struct MetalLocalLightValues final
{
    // Declared first: both arrays die before their shared reservation retires.
    ALNativeFrameBudget::Lease cpu_reservation;
    std::vector<ALMetalWorldLocalLight> lights, inside_lights;
    std::size_t bounded_count = 0, additive_pass_count = 0;
    U32 frame = 0, width = 0, height = 0;
    S32 limit = 0;
    F32 scale = 0.f, modelview[16]{}, projection[16]{}, origin[3]{};
    bool attached = false, ready = false;

    void setKey(U32 current_frame, U32 w, U32 h, S32 count, F32 light_scale,
                bool render_attached, const LLMatrix4a& view,
                const LLMatrix4a& project, const LLVector3& camera_origin) noexcept
    {
        frame = current_frame; width = w; height = h; limit = count;
        scale = light_scale; attached = render_attached;
        std::memcpy(modelview, view.getF32ptr(), sizeof(modelview));
        std::memcpy(projection, project.getF32ptr(), sizeof(projection));
        std::memcpy(origin, camera_origin.mV, sizeof(origin));
    }
    bool matches(U32 current_frame, U32 w, U32 h, S32 count, F32 light_scale,
                 bool render_attached, const LLMatrix4a& view,
                 const LLMatrix4a& project, const LLVector3& camera_origin) const noexcept
    {
        return ready && frame == current_frame && width == w && height == h &&
            limit == count && scale == light_scale && attached == render_attached &&
            std::memcmp(modelview, view.getF32ptr(), sizeof(modelview)) == 0 &&
            std::memcmp(projection, project.getF32ptr(), sizeof(projection)) == 0 &&
            std::memcmp(origin, camera_origin.mV, sizeof(origin)) == 0;
    }
    static std::uint64_t requiredCPUBytes(std::size_t capacity, std::size_t job_bytes) noexcept
    {
        constexpr auto unknown = ALNativeFrameBudget::unknown_bytes;
        if (capacity > (unknown - sizeof(MetalLocalLightValues)) /
                (2 * sizeof(ALMetalWorldLocalLight))) return unknown;
        const auto bytes = sizeof(MetalLocalLightValues) + capacity * (2 * sizeof(ALMetalWorldLocalLight));
        return job_bytes > unknown - bytes ? unknown : bytes + job_bytes;
    }
    std::uint64_t ownedCPUBytes(std::size_t job_bytes) const noexcept
    {
        constexpr auto unknown = ALNativeFrameBudget::unknown_bytes;
        const auto capacity = lights.capacity();
        if (inside_lights.capacity() > unknown - capacity) return unknown;
        const auto total = capacity + inside_lights.capacity();
        if (total > (unknown - sizeof(*this)) / sizeof(ALMetalWorldLocalLight)) return unknown;
        const auto bytes = sizeof(*this) + total * sizeof(ALMetalWorldLocalLight);
        return job_bytes > unknown - bytes ? unknown : bytes + job_bytes;
    }
};
