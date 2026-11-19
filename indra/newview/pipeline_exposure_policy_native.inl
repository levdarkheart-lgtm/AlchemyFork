/**
 * @file pipeline_exposure_policy_native.inl
 * @brief Function-local native exposure owner, shared with physical Metal tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// PPD082: this function-local body is also executed by the native transition fixture.
static LLCachedControl<bool> dynamic_exposure(
    gSavedSettings, "RenderDynamicExposureEnabled", true);
const bool main_exposure_meter = capture.hdr &&
    isMainWorldPass() && mRT == &mMainRT &&
    !mMetalHeroWorldCaptureActive &&
    !mMetalReflectionWorldCaptureActive && !sCutoutRender;
const bool main_dynamic_exposure = main_exposure_meter &&
    dynamic_exposure && !LLRender::sClassicMode;
const bool use_exposure_history =
    capture.exposure_target &&
    capture.exposure_epoch == mMetalExposureHistoryEpoch;
