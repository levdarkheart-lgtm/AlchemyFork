/**
 * @file pipeline_hero_capture_readiness_native.inl
 * @brief Prepare the native probe owner before opening a mirror face.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-only
 */

bool LLPipeline::prepareMetalHeroProbeCapture()
{
    if (!isInit() || mMetalHeroWorldCaptureActive ||
        mMetalReflectionWorldCaptureActive || mRT != &mMainRT ||
        LLStartUp::getStartupState() < STATE_STARTED ||
        !mMetalWorldSimpleCapture || !mMetalWorldSimpleCapture->renderer)
    {
        return false;
    }

    const ALMetalDevice* device = getViewerMetalDevice();
    if (!device || mMetalWorldSimpleCapture->device != device)
    {
        return false;
    }
    if (!mMetalProbeWorldSimpleCapture)
    {
        mMetalProbeWorldSimpleCapture =
            std::make_unique<MetalWorldSimpleCapture>();
        mMetalProbeWorldSimpleCapture->avatar_resources =
            mMetalWorldSimpleCapture->avatar_resources;
    }

    // PPD-125: the main renderer does not establish separate probe readiness.
    // Start/poll the existing async build before swapping or opening a face.
    // Same-device polls retain radiance, valid faces and owner identity.
    auto& capture = *mMetalProbeWorldSimpleCapture;
    initializeMetalWorldCaptureDevice(capture, device, false);
    prepareMetalWorldRenderer(capture, device, true);
    if (!capture.renderer_error.empty())
    {
        LL_WARNS_ONCE("Metal")
            << "Could not prepare native mirror capture: "
            << capture.renderer_error << LL_ENDL;
    }
    return static_cast<bool>(capture.renderer);
}
