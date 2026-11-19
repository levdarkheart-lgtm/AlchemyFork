/** Actual hero uniform publication body. PPD492. */
    // Count is the publication gate for every consumer. Clear it before the
    // mirrors-off return so a disabled resource plan cannot leave the last
    // native radiance owner advertised to the visible world.
    mHeroData.heroProbeCount = 0;
    if (!gPipeline.resourceInputs().mMirrors)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("hpmu - uniforms")

    const LLMatrix4a& modelview = LLViewerCamera::getCurrent().getModelview();
    LLVector4a oa; // scratch space for transformed origin
    oa.set(0, 0, 0, 0);
    // With no mirror, the box and sphere below keep the last one's, and the cube array its
    // picture; 0 tells tapHeroProbe there is nothing to reflect.
    // Called every frame while mirrors are on, whether or not update() has
    // built the default probe since cleanup(). The original GL owner allocates
    // its hero cube before advertising count one. Metal must likewise avoid
    // advertising a mirror until its complete radiance cube is publishable.
    const bool radiance_ready =
#if LL_DARWIN
        // Selection follows the camera, while completed radiance can still
        // belong to the previous mirror when capture is deferred/occluded.
        // Advertise only the selected owner's completed publication. A
        // coherent same-owner refresh keeps its previous radiance available.
        mMetalRadianceReady && mNearestHero != nullptr &&
        !mNearestHero->isDead() &&
        mNativePublishedOwner == mNearestHero->getID() &&
        mNativePublishedEpoch == mMetalRadianceEpoch;
#else
        true;
#endif
    if (!mProbes.empty() && mProbes[0].notNull() &&
        mNearestHero != nullptr && !mNearestHero->isDead() &&
        radiance_ready)
    {
        mHeroData.heroProbeCount = 1;
        if (mNearestHero->getReflectionProbeIsBox())
        {
            LLVector3 s = mNearestHero->getScale().scaledVec(LLVector3(0.5f, 0.5f, 0.5f));
            mProbes[0]->mRadius = s.magVec();
        }
        else
        {
            mProbes[0]->mRadius = mNearestHero->getScale().mV[0] * 0.5f;
        }

        modelview.affineTransform(mProbes[0]->mOrigin, oa);
        mHeroData.heroShape = 0;
        if (!mProbes[0]->getBox(mHeroData.heroBox))
        {
            mHeroData.heroShape = 1;
        }

        mHeroData.heroSphere.set(oa.getF32ptr());
        mHeroData.heroSphere.mV[3] = mProbes[0]->mRadius;
    }

    mHeroData.heroMaxLod = mMaxProbeLOD;
