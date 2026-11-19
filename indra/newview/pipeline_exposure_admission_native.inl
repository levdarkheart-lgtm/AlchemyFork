/**
 * @file pipeline_exposure_admission_native.inl
 * @brief Function-local native exposure owner, shared with physical Metal tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// PPD082: this function-local body is also executed by the native transition fixture.
const bool exact_dynamic_exposure = !capture.hdr || no_post ||
    !dynamic_exposure ||
    (mMetalExposureHistoryReady &&
     capture.exposure_target &&
     capture.exposure_epoch == mMetalExposureHistoryEpoch &&
     (capture.exposure_frame == gFrameCount ||
      cutout_snapshot));
