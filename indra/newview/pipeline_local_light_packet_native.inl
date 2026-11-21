/**
 * @file pipeline_local_light_packet_native.inl
 * @brief One owner-only point-light value packet for a single main view.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
struct MetalLocalLightValues final
{
    std::vector<ALMetalWorldLocalLight> lights, inside_lights;
    std::size_t bounded_count = 0, additive_pass_count = 0;
};
