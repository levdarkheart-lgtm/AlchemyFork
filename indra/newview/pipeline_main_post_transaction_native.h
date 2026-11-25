/**
 * @file pipeline_main_post_transaction_native.h
 * @brief Main-view display eligibility and Temporal transaction finalization.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <utility>

// These are the exact required resources/policies at the viewer's display
// consumer boundary. Keep this decision separate from failures inside a later
// renderer pass: an ineligible consumer never gets called at all.
struct ALMetalMainPostDisplayEligibility
{
    bool presentation = false, sky = false, exposure = false;
    bool depth_of_field = false, bloom = false, cross_filter = false;
    bool lens_flare = false, lens_dirt = false, legacy_glow_noise = false;
    bool color_grade = false;

    bool ready() const noexcept
    {
        return presentation && sky && exposure && depth_of_field && bloom &&
            cross_filter && lens_flare && lens_dirt && legacy_glow_noise && color_grade;
    }
};

// Both the normal display consumer and generic cleanup pass this boundary.
// A jittered Temporal world cannot partially submit without its current final
// presentation. Native/Spatial keep their existing partial-commit semantics.
// Clear owner flags before cancellation so reentrant/repeated cleanup cannot
// settle the same batch again. The callback uses the existing renderer cancel,
// history invalidation and owner-resource cleanup; it introduces no submission.
template<typename Cancel>
bool alMetalCancelIncompleteTemporalPost(bool temporal, bool current_presentation,
    bool& post_batch_active, bool& opaque_post_batch_active, Cancel&& cancel)
{
    if (!temporal || current_presentation || !post_batch_active) return false;
    post_batch_active = false;
    opaque_post_batch_active = false;
    std::forward<Cancel>(cancel)();
    return true;
}
