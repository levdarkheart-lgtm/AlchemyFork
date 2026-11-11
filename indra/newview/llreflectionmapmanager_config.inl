/**
 * @file llreflectionmapmanager_config.inl
 * @brief Viewer reflection settings and native constant snapshots.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
// Included by the production manager and its focused CPU configuration test.
// The test substitutes only the camera input and the owning class name.

void LLReflectionMapManager::refreshSettings()
{
    mRenderReflectionProbeDetail = gSavedSettings.getS32("RenderReflectionProbeDetail");
    mRenderReflectionProbeLevel = gSavedSettings.getS32("RenderReflectionProbeLevel");
    mRenderReflectionProbeCount = gSavedSettings.getU32("RenderReflectionProbeCount");
    mRenderReflectionProbeDynamicAllocation = gSavedSettings.getS32("RenderReflectionProbeDynamicAllocation");
}

#if LL_DARWIN
bool LLReflectionMapManager::fillMetalReflectionProbeConstants(
    ALMetalWorldReflectionProbeConstants& constants) const
{
    const S32 count = mProbeData.refmapCount;
    if (count < 0 ||
        count > static_cast<S32>(
            ALMetalWorldReflectionProbeConstants::max_probe_count))
    {
        return false;
    }

    static_assert(sizeof(LLMatrix4) == sizeof(float) * 16);
    constants = {};
    memcpy(constants.ref_box, mProbeData.refBox,
           sizeof(constants.ref_box));
    memcpy(constants.ref_sphere, mProbeData.refSphere,
           sizeof(constants.ref_sphere));
    memcpy(constants.ref_params, mProbeData.refParams,
           sizeof(constants.ref_params));
    memcpy(constants.ref_index, mProbeData.refIndex,
           sizeof(constants.ref_index));
    memcpy(constants.ref_neighbor, mProbeData.refNeighbor,
           sizeof(constants.ref_neighbor));
    memcpy(constants.ref_bucket, mProbeData.refBucket,
           sizeof(constants.ref_bucket));

    // setEnvMat uploads the transpose of the current view rotation. Spell
    // the resulting columns out for MSL's column-major multiply.
    const F32* matrix =
        LLViewerCamera::getCurrent().getModelview().getF32ptr();
    constants.env_matrix[0][0] = matrix[0];
    constants.env_matrix[0][1] = matrix[4];
    constants.env_matrix[0][2] = matrix[8];
    constants.env_matrix[1][0] = matrix[1];
    constants.env_matrix[1][1] = matrix[5];
    constants.env_matrix[1][2] = matrix[9];
    constants.env_matrix[2][0] = matrix[2];
    constants.env_matrix[2][1] = matrix[6];
    constants.env_matrix[2][2] = matrix[10];
    constants.config[0] = static_cast<F32>(count);
    constants.config[1] = mMaxProbeLOD;
    return true;
}

bool LLReflectionMapManager::fillMetalReflectionProbeShadingConstants(
    ALMetalWorldReflectionProbeConstants& constants) const
{
    if (!fillMetalReflectionProbeConstants(constants))
    {
        return false;
    }

    // Original reflectionProbeF's REFMAP_LEVEL==0 removes the ordinary
    // probe walk, not the default-zero tap or the later hero/SSR stages.
    // Native walks start at index one, so one accessible entry expresses
    // that same policy without altering the manager's complete pack, its
    // allocation/exposure data, or the debug-volume snapshot. Keep the
    // zero-count bootstrap unchanged as well.
    if (mRenderReflectionProbeLevel == 0)
    {
        constants.config[0] = llmin(constants.config[0], 1.f);
    }
    return true;
}
#endif
