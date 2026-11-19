/** Actual HUD texture-owner observation; no resource or fetch operations (PPD463). */
namespace
{
void observeHUDTextureInputs(const LLUUID& object_id, S32 face_index, U32 slot,
                             LLViewerTexture* texture, bool boost_owner, U32 frame) noexcept
{
    ALHUDTextureInputs::observe([&]
    {
        ALHUDTextureInputs::Snapshot s;
        // Determine the actual dynamic texture type before considering a fetched cast.
        s.texture_type = static_cast<S32>(texture->getType());
        switch (s.texture_type)
        {
        case LLViewerTexture::LOCAL_TEXTURE: s.type = ALHUDTextureInputs::Type::local; break;
        case LLViewerTexture::MEDIA_TEXTURE: s.type = ALHUDTextureInputs::Type::media; break;
        case LLViewerTexture::DYNAMIC_TEXTURE: s.type = ALHUDTextureInputs::Type::dynamic; break;
        case LLViewerTexture::FETCHED_TEXTURE: s.type = ALHUDTextureInputs::Type::fetched; break;
        case LLViewerTexture::LOD_TEXTURE: s.type = ALHUDTextureInputs::Type::lod; break;
        default: break;
        }
        s.asset = texture->getID().asString();
        s.object = object_id.asString();
        s.face = face_index;
        s.slot = static_cast<S32>(slot);
        s.frame = frame;
        s.boost_owner = boost_owner;
        s.boost = texture->getBoostLevel();
        s.width = texture->getFullWidth();
        s.height = texture->getFullHeight();
        s.components = static_cast<S32>(texture->getComponents());
        s.texture_state = static_cast<S32>(texture->getTextureState());
        s.resident = texture->hasGPUTexture(); // CPU backing-presence check, no GPU query.
        s.parcel_media = texture->hasParcelMedia();
        s.fetch_known = s.type == ALHUDTextureInputs::Type::fetched || s.type == ALHUDTextureInputs::Type::lod;
        // Fetched/LOD constructors own LLImageGL even before first publication.
        // Other logical textures can lack it; only known residency permits its getter.
        s.current_discard_known = s.fetch_known || s.resident;
        if (s.current_discard_known) s.current_discard = texture->getDiscardLevel();
        if (s.fetch_known)
        {
            auto* fetched = static_cast<LLViewerFetchedTexture*>(texture);
            s.desired_discard = fetched->getDesiredDiscardLevel();
            s.raw_discard = fetched->getRawImageLevel();
            s.raw_valid = fetched->isRawImageValid();
            s.has_fetcher = fetched->hasFetcher();
            s.fetching = fetched->isFetching();
            s.missing = fetched->isMissingAsset();
            s.create_pending = fetched->mCreatePending;
            s.create_queued_frame = fetched->mCreateQueuedFrame;
            s.downscale_pending = fetched->mDownScalePending;
            const LLImageRaw* raw = fetched->getRawImage();
            s.raw_present = raw != nullptr;
            if (raw)
            {
                s.raw_width = raw->getWidth();
                s.raw_height = raw->getHeight();
                s.raw_components = static_cast<S32>(raw->getComponents());
            }
        }
        return s;
    }, [](const ALHUDTextureInputs::Snapshot& snapshot)
    {
        LL_INFOS("WarningInputs") << "kind=hud_texture " << snapshot << LL_ENDL;
    });
}
}
