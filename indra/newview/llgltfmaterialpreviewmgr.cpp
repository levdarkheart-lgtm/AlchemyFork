/**
 * @file llgltfmaterialpreviewmgr.cpp
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llgltfmaterialpreviewmgr.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "llavatarappearancedefines.h"
#include "lldrawpoolalpha.h"
#include "llenvironment.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvolumemgr.h"
#include "pipeline.h"

#if LL_DARWIN
#include "../almetal/almetalpbrpreview.h"
#include "../almetal/almetalresource.h"
#endif

LLGLTFMaterialPreviewMgr gGLTFMaterialPreviewMgr;

namespace
{
    constexpr S32 FULLY_LOADED = 0;
    constexpr S32 NOT_LOADED = 99;

    std::array<F32, 4> pbr_preview_tonemap_params(S32 type)
    {
        std::array<F32, 4> params{};
        switch (type)
        {
            case 2:
            {
                F32 white = llmax(
                    1.f,
                    gSavedSettings.get<F32>("RenderTonemapACESWhite"));
                constexpr F32 exposure_bias = 1.2f;
                constexpr F32 a = 0.0245786f;
                constexpr F32 b = 0.000090537f;
                constexpr F32 c = 0.983729f;
                constexpr F32 d = 0.432951f;
                constexpr F32 e = 0.238081f;
                white *= exposure_bias;
                params[0] = (white * (white + a) - b) /
                            (white * (c * white + d) + e);
                break;
            }
            case 3:
            {
                const F32 white = llmax(
                    1.f,
                    gSavedSettings.get<F32>("RenderTonemapReinhardWhite"));
                params[0] = white * white;
                break;
            }
            case 4:
            {
                const F32 white = llmax(
                    1.f,
                    gSavedSettings.get<F32>("RenderTonemapFilmicWhite"));
                constexpr F32 exposure_bias = 2.f;
                constexpr F32 a = 0.22f * exposure_bias * exposure_bias;
                constexpr F32 b = 0.30f * exposure_bias;
                constexpr F32 c = 0.10f;
                constexpr F32 d = 0.20f;
                constexpr F32 e = 0.01f;
                constexpr F32 f = 0.30f;
                params[0] =
                    ((white * (a * white + c * b) + d * e) /
                     (white * (a * white + b) + d * f)) - e / f;
                break;
            }
            case 6:
            {
                const F32 contrast = gSavedSettings.get<F32>(
                    "RenderTonemapAgxContrast");
                const F32 white = llmax(
                    2.f,
                    gSavedSettings.get<F32>("RenderTonemapAgxWhite"));
                constexpr F32 crossover = 0.18f;
                constexpr F32 shoulder_max = 1.f - crossover;
                const F32 toe_a = (1.f / crossover - 1.f) *
                    std::pow(crossover, contrast);
                const F32 slope_denom =
                    std::pow(crossover, contrast) + toe_a;
                const F32 slope =
                    (contrast * std::pow(crossover, contrast - 1.f) *
                     toe_a) / (slope_denom * slope_denom);
                F32 width = white - crossover;
                width = width * width / shoulder_max * slope;
                params = {{contrast, toe_a, slope, width}};
                break;
            }
            default:
                break;
        }
        return params;
    }
};

LLGLTFPreviewTexture::MaterialLoadLevels::MaterialLoadLevels()
{
    for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        levels[i] = NOT_LOADED;
    }
}

bool LLGLTFPreviewTexture::MaterialLoadLevels::isFullyLoaded()
{
    for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        if (levels[i] != FULLY_LOADED) { return false; }
    }
    return true;
}

S32& LLGLTFPreviewTexture::MaterialLoadLevels::operator[](size_t i)
{
    llassert(i >= 0 && i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT);
    return levels[i];
}

const S32& LLGLTFPreviewTexture::MaterialLoadLevels::operator[](size_t i) const
{
    llassert(i >= 0 && i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT);
    return levels[i];
}

bool LLGLTFPreviewTexture::MaterialLoadLevels::operator<(const MaterialLoadLevels& other) const
{
    bool less = false;
    for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        if (((*this)[i] > other[i])) { return false; }
        less = less || ((*this)[i] < other[i]);
    }
    return less;
}

bool LLGLTFPreviewTexture::MaterialLoadLevels::operator>(const MaterialLoadLevels& other) const
{
    bool great = false;
    for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        if (((*this)[i] < other[i])) { return false; }
        great = great || ((*this)[i] > other[i]);
    }
    return great;
}

namespace
{
    void fetch_texture_for_ui(LLPointer<LLViewerFetchedTexture>& img, const LLUUID& id)
    {
        if (!img && id.notNull())
        {
            if (LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::isBakedImageId(id))
            {
                LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
                if (obj)
                {
                    LLViewerTexture* viewerTexture = obj->getBakedTextureForMagicId(id);
                    img = viewerTexture ? dynamic_cast<LLViewerFetchedTexture*>(viewerTexture) : NULL;
                }
            }
            else
            {
                img = LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
            }
        }
        if (img)
        {
            img->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
            img->forceToSaveRawImage(0);
        }
    };

    // *NOTE: Does not use the same conventions as texture discard level. Lower is better.
    S32 get_texture_load_level(const LLPointer<LLViewerFetchedTexture>& texture)
    {
        if (!texture) { return FULLY_LOADED; }
        const S32 raw_level = texture->getDiscardLevel();
        if (raw_level < 0) { return NOT_LOADED; }
        return raw_level;
    }

    LLGLTFPreviewTexture::MaterialLoadLevels get_material_load_levels(LLFetchedGLTFMaterial& material)
    {
        llassert(!material.isFetching());

        using MaterialTextures = LLPointer<LLViewerFetchedTexture>*[LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT];

        MaterialTextures textures;

        textures[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] = &material.mBaseColorTexture;
        textures[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL] = &material.mNormalTexture;
        textures[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS] = &material.mMetallicRoughnessTexture;
        textures[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE] = &material.mEmissiveTexture;

        LLGLTFPreviewTexture::MaterialLoadLevels levels;

        for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
        {
            fetch_texture_for_ui(*textures[i], material.mTextureId[i]);
            levels[i] = get_texture_load_level(*textures[i]);
        }

        return levels;
    }

    // Is the material loaded enough to start rendering a preview?
    bool is_material_loaded_enough_for_ui(LLFetchedGLTFMaterial& material)
    {
        if (material.isFetching())
        {
            return false;
        }

        LLGLTFPreviewTexture::MaterialLoadLevels levels = get_material_load_levels(material);

        for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
        {
            if (levels[i] == NOT_LOADED)
            {
                return false;
            }
        }

        return true;
    }

};  // namespace

U32 LLGLTFPreviewTexture::sLiveCount = 0;

LLGLTFPreviewTexture::LLGLTFPreviewTexture(LLPointer<LLFetchedGLTFMaterial> material, S32 width)
    : LLViewerDynamicTexture(width, width, 4, EOrder::ORDER_MIDDLE)
    , mGLTFMaterial(material)
{
    ++sLiveCount;
}

LLGLTFPreviewTexture::~LLGLTFPreviewTexture()
{
    // The last preview gone, the probe pack's post targets and bloom, there for previews alone, are freed with the next
    // frame's resources, and with them the pack where reflection probes are off.
    if (--sLiveCount == 0)
    {
        gPipeline.markRenderInputsDirty();
    }
}

// static
LLPointer<LLGLTFPreviewTexture> LLGLTFPreviewTexture::create(LLPointer<LLFetchedGLTFMaterial> material)
{
    // No larger than the probe pack the preview renders through (render()): the probe resolution
    // times ALProbeSuperSample, which is below MAX_PREVIEW_WIDTH at a supersample under 4, and a
    // larger viewport would render only the corner of the sphere that fits. That is the size the
    // pack is planned at whether or not it is built. Its post targets and bloom are planned only
    // while a preview is alive, and with reflection probes off the rest of it too: they are built
    // for the previews with the next frame's resources, and needsRender waits for them.
    const ALRenderResourceInputs& inputs = gPipeline.resourceInputs();
    const U32 pack_width = inputs.mProbeResolution * inputs.mProbeSuperSample;
    if (!gPipeline.resourcePlan().wants(ALRenderResourcePlan::Id::ProbePostPing))
    {
        gPipeline.markRenderInputsDirty();
    }
    const U32 width = llmin(LLPipeline::MAX_PREVIEW_WIDTH, pack_width);
    return new LLGLTFPreviewTexture(material, (S32)width);
}

bool LLGLTFPreviewTexture::needsRender()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    // Not before the probe pack it renders through is there, and planned with the post target the
    // preview takes from the pool; asked again each frame until it is.
    if (!gPipeline.mAuxillaryRT.screen.isComplete() ||
        !gPipeline.resourcePlan().wants(ALRenderResourcePlan::Id::ProbePostPing))
    {
        return false;
    }

    if (!mShouldRender && mBestLoad.isFullyLoaded()) { return false; }
    MaterialLoadLevels current_load = get_material_load_levels(*mGLTFMaterial.get());
    if (current_load < mBestLoad)
    {
        mShouldRender = true;
        mPendingLoad = current_load;
        return true;
    }
    return false;
}

void LLGLTFPreviewTexture::preRender(bool clear_depth)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    llassert(mShouldRender);
    if (!mShouldRender) { return; }

    LLViewerDynamicTexture::preRender(clear_depth);
}


namespace {

struct GLTFPreviewModel
{
    GLTFPreviewModel(LLPointer<LLDrawInfo>& info, const LLMatrix4a& mat)
    : mDrawInfo(info)
    , mModelMatrix(mat)
    {
        mDrawInfo->mModelMatrix = &mModelMatrix;
    }
    GLTFPreviewModel(GLTFPreviewModel&) = delete;
    ~GLTFPreviewModel()
    {
        // No model matrix necromancy
        llassert(gGLLastMatrix != &mModelMatrix);
        gGLLastMatrix = nullptr;
    }
    LLPointer<LLDrawInfo> mDrawInfo;
    LLMatrix4a mModelMatrix; // Referenced by mDrawInfo
};

using PreviewSpherePart = std::unique_ptr<GLTFPreviewModel>;
using PreviewSphere = std::vector<PreviewSpherePart>;

// Like LLVolumeGeometryManager::registerFace but without batching or too-many-indices/vertices checking.
PreviewSphere create_preview_sphere(LLPointer<LLFetchedGLTFMaterial>& material, const LLMatrix4a& model_matrix)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    // sRGB in the attribute, as LLFace stores it; pbralphaV linearises it
    const LLColor4U vertex_color(srgbColor4(material->mBaseColor));

    LLPrimitive prim;
    prim.init_primitive(LL_PCODE_VOLUME);
    LLVolumeParams params;
    params.setType(LL_PCODE_PROFILE_CIRCLE_HALF, LL_PCODE_PATH_CIRCLE);
    params.setBeginAndEndS(0.f, 1.f);
    params.setBeginAndEndT(0.f, 1.f);
    params.setRatio(1, 1);
    params.setShear(0, 0);
    constexpr auto MAX_LOD = LLVolumeLODGroup::NUM_LODS - 1;
    prim.setVolume(params, MAX_LOD);

    LLVolume* volume = prim.getVolume();
    llassert(volume);
    for (LLVolumeFace& face : volume->getVolumeFaces())
    {
        face.createTangents();
    }

    PreviewSphere preview_sphere;
    preview_sphere.reserve(volume->getNumFaces());

    LLPointer<LLVertexBuffer> buf = new LLVertexBuffer(
        LLVertexBuffer::MAP_VERTEX |
        LLVertexBuffer::MAP_NORMAL |
        LLVertexBuffer::MAP_TEXCOORD0 |
        LLVertexBuffer::MAP_COLOR |
        LLVertexBuffer::MAP_TANGENT
    );
    U32 nv = 0;
    U32 ni = 0;
    for (LLVolumeFace& face : volume->getVolumeFaces())
    {
        nv += face.mNumVertices;
        ni += face.mNumIndices;
    }
    buf->allocateBuffer(nv, ni);

    // UV hacks
    // Higher factor helps to see more details on the preview sphere
    const LLVector2 uv_factor(2.0f, 2.0f);
    // Offset places center of material in center of view
    const LLVector2 uv_offset(-0.5f, -0.5f);

    LLStrider<U16> indices;
    LLStrider<LLVector4a> positions;
    LLStrider<LLVector4a> normals;
    LLStrider<LLVector2> texcoords;
    LLStrider<LLColor4U> colors;
    LLStrider<LLVector4a> tangents;
    buf->getIndexStrider(indices);
    buf->getVertexStrider(positions);
    buf->getNormalStrider(normals);
    buf->getTexCoord0Strider(texcoords);
    buf->getColorStrider(colors);
    buf->getTangentStrider(tangents);
    U32 index_offset = 0;
    U32 vertex_offset = 0;
    for (const LLVolumeFace& face : volume->getVolumeFaces())
    {
        for (S32 i = 0; i < face.mNumIndices; ++i)
        {
            *indices++ = face.mIndices[i] + vertex_offset;
        }
        for (S32 v = 0; v < face.mNumVertices; ++v)
        {
            *positions++ = face.mPositions[v];
            *normals++ = face.mNormals[v];
            LLVector2 uv(face.mTexCoords[v]);
            uv.scaleVec(uv_factor);
            uv += uv_offset;
            *texcoords++ = uv;
            *colors++ = vertex_color;
            *tangents++ = face.mTangents[v];
        }

        constexpr LLViewerTexture* no_media = nullptr;
        LLPointer<LLDrawInfo> info = new LLDrawInfo(U16(vertex_offset), U16(vertex_offset + face.mNumVertices - 1), face.mNumIndices, index_offset, no_media, buf.get());
        info->mGLTFMaterial = material;
        preview_sphere.emplace_back(std::make_unique<GLTFPreviewModel>(info, model_matrix));
        index_offset += face.mNumIndices;
        vertex_offset += face.mNumVertices;
    }

    buf->unmapBuffer();

    return preview_sphere;
}

void set_preview_sphere_material(PreviewSphere& preview_sphere, LLPointer<LLFetchedGLTFMaterial>& material)
{
    llassert(!preview_sphere.empty());
    if (preview_sphere.empty()) { return; }

    const LLColor4U vertex_color(srgbColor4(material->mBaseColor));

    // See comments about unmapBuffer in llvertexbuffer.h
    for (PreviewSpherePart& part : preview_sphere)
    {
        LLDrawInfo* info = part->mDrawInfo.get();
        info->mGLTFMaterial = material;
        LLVertexBuffer* buf = info->mVertexBuffer.get();
        LLStrider<LLColor4U> colors;
        const S32 count = info->mEnd - info->mStart + 1;
        buf->getColorStrider(colors, info->mStart, count);
        for (S32 i = 0; i < count; ++i)
        {
            *colors++ = vertex_color;
        }
        buf->unmapBuffer();
    }
}

PreviewSphere& get_preview_sphere(LLPointer<LLFetchedGLTFMaterial>& material, const LLMatrix4a& model_matrix)
{
    static PreviewSphere preview_sphere;
    if (preview_sphere.empty())
    {
        preview_sphere = create_preview_sphere(material, model_matrix);
    }
    else
    {
        set_preview_sphere_material(preview_sphere, material);
    }
    return preview_sphere;
}

// Final, direct modifications to shader constants, just before render
void fixup_shader_constants(LLGLSLShader& shader)
{
    // Lit by the sun, with no haze. Its white light is the Environment block's
    // (bindEnvironmentUBOLitBy, around the draw).
    shader.uniform1i(LLShaderMgr::SUN_UP_FACTOR, 1);
    shader.uniform1f(LLShaderMgr::DENSITY_MULTIPLIER, 0.0f);

    // Ignore shadows (if enabled): the preview has none of the maps, the sun's four cascades and the spot pair, so each
    // reads lit without a sample. Their units keep the depth maps bindShadowMaps put there, under the sampler the
    // program declares them for, and the next bind through bindShadowMaps hands the program its real set again.
    constexpr S32 every_shadow_map = (1 << 6) - 1;
    shader.uniform1i(LLShaderMgr::UNBUILT_SHADOW_MAPS, every_shadow_map);

    // Every alpha mode draws through the alpha pool's program here, and the material's bind sets a
    // mask or opaque cutoff but leaves a blended one to the pass: without this a blended sphere
    // would keep the cutoff of whichever preview drew before it.
    LLDrawPoolAlpha::setCoverageCutoff(&shader);
}

// Set a variable to a value temporarily, and restor the variable's old value
// when this object leaves scope.
template<typename T>
struct SetTemporarily
{
    T* mRef;
    T mOldVal;
    SetTemporarily(T* var, T temp_val)
    {
        mRef = var;
        mOldVal = *mRef;
        *mRef = temp_val;
    }
    ~SetTemporarily()
    {
        *mRef = mOldVal;
    }
};

}; // namespace

bool LLGLTFPreviewTexture::render()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    if (!mShouldRender) { return false; }

#if LL_DARWIN
    mMetalPreview.reset();
    if (mGLTexturep.notNull() && mGLTexturep->getMetalTexture())
    {
        mMetalPreviewDescriptor =
            std::make_unique<ALMetalTextureDescriptor>(
                mGLTexturep->getMetalTexture()->descriptor());
    }
    std::vector<ALMetalPBRPreviewBatch> metal_preview_batches;
#endif

    // The probe pack's post target the colour correction writes, from the pool with the bloom
    // until the preview is drawn; acquired ahead of every bind below.
    ALTransientTargets    post(gPipeline.mTransientTargets);
    LLRenderTarget* const post_ping = gPipeline.acquireResource(post, ALRenderResourcePlan::Id::ProbePostPing);
    if (!post_ping)
    {
        return false;
    }

    // The swatch's alpha is its coverage, drawn over a checkerboard. Without this the
    // preview takes whatever mask the last frame ended on, and after a HUD pass that leaves
    // alpha unwritten from the clears through the final blit.
    LLGLSColorMask color_mask(true, true);

    gGL.setClearColor(LLColor4::transparent);
    gGL.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    LLGLDepthTest depth(GL_FALSE);
    LLGLDisable stencil(GL_STENCIL_TEST);
    LLGLDisable scissor(GL_SCISSOR_TEST);
    SetTemporarily<bool> no_dof(&LLPipeline::RenderDepthOfField, false);
    SetTemporarily<bool> no_glow(&LLPipeline::sRenderGlow, false);
    SetTemporarily<LLPipeline::RenderTargetPack*> use_auxiliary_render_target(&gPipeline.mRT, &gPipeline.mAuxillaryRT);

    LLVector3 light_dir3(1.0f, 1.0f, 1.0f);
    light_dir3.normalize();
    const LLVector4 light_dir = LLVector4(light_dir3, 0);
    const S32 old_local_light_count = gSavedSettings.get<S32>("RenderLocalLightCount");
    gSavedSettings.set<S32>("RenderLocalLightCount", 0);

    gPipeline.mReflectionMapManager.forceDefaultProbeAndUpdateUniforms();

    // setPerspective makes the preview camera current; the UI draw this
    // returns into wants the one it was using
    const LLCamera saved_camera = LLViewerCamera::getCurrent();
    LLViewerCamera camera;

    // Calculate the object distance at which the object of a given radius will
    // span the partial width of the screen given by fill_ratio.
    // Assume the primitive has a scale of 1 (this is the default).
    constexpr F32 fill_ratio = 0.8f;
    constexpr F32 object_radius = 0.5f;
    const F32 object_distance = (object_radius / fill_ratio) * tan(camera.getDefaultFOV());
    // Negative coordinate shows the textures on the sphere right-side up, when
    // combined with the UV hacks in create_preview_sphere
    const LLVector3 object_position(0.0, -object_distance, 0.0);
    LLMatrix4a object_transform;
    object_transform.setIdentity();
    object_transform.setTranslation(object_position);

    // Set up camera and viewport
    const LLVector3 origin(0.0, 0.0, 0.0);
    camera.lookAt(origin, object_position);
    camera.setAspect((F32)(mFullWidth / mFullHeight));
    const LLRect texture_rect(0, mFullHeight, mFullWidth, 0);
    camera.setPerspective(NOT_FOR_SELECTION, texture_rect.mLeft, texture_rect.mBottom, texture_rect.getWidth(), texture_rect.getHeight(), false, camera.getNear(), FINITE_PROJECTION_FAR);

    // Generate sphere object on-the-fly. Discard afterwards. (Vertex buffer is
    // discarded, but the sphere should be cached in LLVolumeMgr.)
    PreviewSphere& preview_sphere = get_preview_sphere(mGLTFMaterial, object_transform);

    gPipeline.setupHWLights();
    LLVector4a light_dir_in, transformed_light_dir;
    light_dir_in.loadua(light_dir.mV);
    LLViewerCamera::getCurrent().getModelview().transform4(light_dir_in, transformed_light_dir);
    SetTemporarily<LLVector4> force_sun_direction_high_graphics(&gPipeline.mTransformedSunDir, LLVector4(transformed_light_dir.getF32ptr()));
    // Override lights to ensure the sun is always shining from a certain direction (low graphics)
    // See also force_sun_direction_high_graphics and fixup_shader_constants
    {
        LLLightState* light = gGL.getLight(0);
        light->setPosition(light_dir);
        constexpr bool sun_up = true;
        light->setSunPrimary(sun_up);
    }

    LLRenderTarget& screen = gPipeline.mAuxillaryRT.screen;

    // *HACK: Force reset of the model matrix
    gGLLastMatrix = nullptr;

#if 0
    if (mGLTFMaterial->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_OPAQUE || mGLTFMaterial->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
    {
        // *TODO: Opaque/alpha mask rendering
    }
    else
#endif
    {
        // Alpha blend rendering

        screen.bindTarget();
        // Colour only: the sphere draws with the depth test off, and depth writes are off
        // with it, so a depth clear here would do nothing.
        gGL.clear(GL_COLOR_BUFFER_BIT);

        LLGLSLShader& shader = *gDeferredPBRAlphaProgram.selectVariant();

        gPipeline.bindDeferredShader(shader);
        fixup_shader_constants(shader);

        // A white sun through no atmosphere, whatever the sky's light is.
        LLEnvironment::instance().bindEnvironmentUBOLitBy(LLColor3::white);

        LLFetchedGLTFMaterial* lastMat = nullptr;
        LLViewerTexture* lastTex = nullptr;
        for (PreviewSpherePart& part : preview_sphere)
        {
            LLRenderPass::pushGLTFBatch(*part->mDrawInfo, lastMat, lastTex);
        }

#if LL_DARWIN
        ALMetalDevice* metal_device = LLImageGL::getMetalDevice();
        LLViewerTexture* base_color_image = mGLTFMaterial->mBaseColorTexture
            ? mGLTFMaterial->mBaseColorTexture.get()
            : LLViewerFetchedTexture::sWhiteImagep.get();
        LLViewerTexture* normal_image =
            mGLTFMaterial->mNormalTexture.notNull() &&
            mGLTFMaterial->mNormalTexture->getDiscardLevel() <= 4
                ? mGLTFMaterial->mNormalTexture.get()
                : LLViewerFetchedTexture::sFlatNormalImagep.get();
        LLViewerTexture* orm_image = mGLTFMaterial->mMetallicRoughnessTexture
            ? mGLTFMaterial->mMetallicRoughnessTexture.get()
            : LLViewerFetchedTexture::sWhiteImagep.get();
        LLViewerTexture* emissive_image = mGLTFMaterial->mEmissiveTexture
            ? mGLTFMaterial->mEmissiveTexture.get()
            : LLViewerFetchedTexture::sWhiteImagep.get();
        const auto metal_texture = [](LLViewerTexture* texture)
            -> const ALMetalTexture*
        {
            LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
            return image ? image->getMetalTexture() : nullptr;
        };
        const ALMetalTexture* base_color_texture =
            metal_texture(base_color_image);
        const ALMetalTexture* normal_texture = metal_texture(normal_image);
        const ALMetalTexture* orm_texture = metal_texture(orm_image);
        const ALMetalTexture* emissive_texture = metal_texture(emissive_image);

        const F32 metal_preview_exposure = llclamp(
            gSavedSettings.get<F32>("RenderExposure"), 0.5f, 4.f);
        LLSettingsSky::ptr_t metal_preview_sky =
            LLEnvironment::instance().getCurrentSky();
        const bool metal_preview_auto_adjust = gSavedSettings.get<bool>(
            "RenderSkyAutoAdjustLegacy");
        const F32 metal_preview_tonemap_mix = metal_preview_sky
            ? llclamp(metal_preview_sky->getTonemapMix(
                          metal_preview_auto_adjust),
                      0.f, 1.f)
            : 1.f;
        const S32 metal_preview_tonemap_type = llmax(
            gSavedSettings.get<S32>("AlchemyRenderTonemapType"), 0);
        const std::array<F32, 4> metal_preview_tonemap_params =
            pbr_preview_tonemap_params(metal_preview_tonemap_type);
        const F32 metal_preview_low_fraction = llclamp(
            gSavedSettings.get<F32>("AlchemyExposureLowPercent") * 0.01f,
            0.f, 0.99f);
        const F32 metal_preview_high_fraction = llclamp(
            gSavedSettings.get<F32>("AlchemyExposureHighPercent") * 0.01f,
            metal_preview_low_fraction + 0.01f, 1.f);
        const bool metal_preview_hdr = LLPipeline::hdrEnabled();
        const bool metal_preview_legacy_gamma = metal_preview_sky &&
            metal_preview_sky->getReflectionProbeAmbiance(
                metal_preview_auto_adjust) == 0.f;
        const bool metal_preview_no_post =
            LLPipeline::suppressesLensAndPrintEffects() ||
            metal_preview_legacy_gamma ||
            (gSavedSettings.get<bool>("RenderDisablePostProcessing") &&
             gFloaterTools && gFloaterTools->isAvailable());
        const F32 metal_preview_bloom_strength = llmax(
            gSavedSettings.get<F32>("RenderBloomStrength"), 0.f);
        const bool metal_preview_halation =
            gSavedSettings.get<bool>("RenderBloomHalation");
        const F32 metal_preview_halation_strength = llmax(
            gSavedSettings.get<F32>("RenderBloomHalationStrength"), 0.f);
        const bool metal_preview_bloom = metal_preview_hdr &&
            (metal_preview_bloom_strength > 0.f ||
             (metal_preview_halation &&
              metal_preview_halation_strength > 0.f));
        const F32 metal_preview_cross_filter_strength =
            metal_preview_bloom &&
            metal_preview_bloom_strength > 0.f &&
            !LLPipeline::suppressesLensAndPrintEffects()
                ? llclamp(gSavedSettings.get<F32>(
                              "RenderCrossFilterStrength"),
                          0.f, CROSS_FILTER_MAX_STRENGTH)
                : 0.f;
        const bool metal_preview_cross_filter =
            metal_preview_cross_filter_strength > 0.f;

        bool metal_preview_eligible = metal_device &&
            mMetalPreviewDescriptor && base_color_texture && normal_texture &&
            orm_texture && emissive_texture && metal_preview_hdr &&
            !metal_preview_no_post;
        const LLMatrix4a& modelview = gGL.getModelviewMatrix();
        LLMatrix4a inverse_modelview;
        LLMatrix4a modelview_projection;
        if (metal_preview_eligible &&
            !inverse_modelview.setAffineInverse(modelview))
        {
            metal_preview_eligible = false;
        }
        if (metal_preview_eligible)
        {
            modelview_projection.setMul(
                modelview, gGL.getProjectionMatrix());
        }

        for (PreviewSpherePart& part : preview_sphere)
        {
            LLDrawInfo* info = part->mDrawInfo.get();
            LLVertexBuffer* buffer = info ? info->mVertexBuffer.get() : nullptr;
            if (!metal_preview_eligible || !buffer ||
                !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
                !buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL) ||
                !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) ||
                !buffer->hasDataType(LLVertexBuffer::TYPE_COLOR) ||
                !buffer->hasDataType(LLVertexBuffer::TYPE_TANGENT) ||
                !buffer->getMetalVertexBuffer() ||
                !buffer->getMetalIndexBuffer())
            {
                metal_preview_eligible = false;
                metal_preview_batches.clear();
                break;
            }

            ALMetalPBRPreviewBatch batch;
            batch.vertices = buffer->getMetalVertexBuffer();
            batch.position_offset = buffer->getOffset(
                LLVertexBuffer::TYPE_VERTEX);
            batch.normal_offset = buffer->getOffset(
                LLVertexBuffer::TYPE_NORMAL);
            batch.texcoord_offset = buffer->getOffset(
                LLVertexBuffer::TYPE_TEXCOORD0);
            batch.color_offset = buffer->getOffset(
                LLVertexBuffer::TYPE_COLOR);
            batch.tangent_offset = buffer->getOffset(
                LLVertexBuffer::TYPE_TANGENT);
            batch.vertex_count = buffer->getNumVerts();
            batch.indices = buffer->getMetalIndexBuffer();
            batch.index_type = buffer->getIndexStride() == 4
                ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
            batch.index_offset = static_cast<std::size_t>(info->mOffset) *
                                 buffer->getIndexStride();
            batch.index_count = info->mCount;
            batch.base_color_texture = base_color_texture;
            batch.normal_texture = normal_texture;
            batch.orm_texture = orm_texture;
            batch.emissive_texture = emissive_texture;
            std::memcpy(batch.modelview_projection,
                        modelview_projection.getF32ptr(),
                        sizeof(batch.modelview_projection));
            std::memcpy(batch.modelview, modelview.getF32ptr(),
                        sizeof(batch.modelview));
            const F32* inverse = inverse_modelview.getF32ptr();
            for (U32 column = 0; column < 3; ++column)
            {
                batch.normal_matrix[column][0] = inverse[0 * 4 + column];
                batch.normal_matrix[column][1] = inverse[1 * 4 + column];
                batch.normal_matrix[column][2] = inverse[2 * 4 + column];
            }
            for (U32 texture = 0;
                 texture < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT;
                 ++texture)
            {
                mGLTFMaterial->mTextureTransform[texture].getPacked(
                    batch.texture_transform[texture]);
            }
            std::memcpy(batch.emissive_color,
                        mGLTFMaterial->mEmissiveColor.mV,
                        sizeof(F32) * 3);
            std::memcpy(batch.sun_direction,
                        gPipeline.mTransformedSunDir.mV,
                        sizeof(F32) * 3);
            // bindEnvironmentUBOLitBy(white) makes atmos_sunlit_linear one;
            // calcAtmosphericVarsLinear then applies the HDR sunlight scale.
            // Carry the same scalar explicitly into the native pass.
            const F32 preview_sun_scale =
                gSavedSettings.get<F32>("RenderHDRSkySunlightScale");
            batch.sun_color[0] = preview_sun_scale;
            batch.sun_color[1] = preview_sun_scale;
            batch.sun_color[2] = preview_sun_scale;
            batch.metallic_factor = mGLTFMaterial->mMetallicFactor;
            batch.roughness_factor = mGLTFMaterial->mRoughnessFactor;
            batch.alpha_cutoff =
                mGLTFMaterial->mAlphaMode ==
                    LLGLTFMaterial::ALPHA_MODE_MASK
                ? mGLTFMaterial->mAlphaCutoff
                : mGLTFMaterial->mAlphaMode ==
                    LLGLTFMaterial::ALPHA_MODE_BLEND
                    ? 0.004f : -1.f;
            batch.double_sided = mGLTFMaterial->mDoubleSided;
            metal_preview_batches.push_back(batch);
        }

        if (metal_preview_eligible && !metal_preview_batches.empty())
        {
            static ALMetalDevice* renderer_device = nullptr;
            static std::unique_ptr<ALMetalPBRPreviewRenderer> renderer;
            std::string error;
            if (renderer_device != metal_device)
            {
                renderer.reset();
                renderer_device = metal_device;
            }
            if (!renderer)
            {
                renderer = ALMetalPBRPreviewRenderer::create(
                    *metal_device, &error);
            }
            if (renderer)
            {
                ALMetalPBRPreviewScene metal_scene;
                metal_scene.batches = metal_preview_batches.data();
                metal_scene.batch_count = metal_preview_batches.size();
                metal_scene.destination_descriptor =
                    mMetalPreviewDescriptor.get();
                metal_scene.exposure = metal_preview_exposure;
                metal_scene.tonemap_mix = metal_preview_tonemap_mix;
                metal_scene.tonemap_type = static_cast<U32>(
                    metal_preview_tonemap_type);
                std::memcpy(metal_scene.tonemap_params,
                            metal_preview_tonemap_params.data(),
                            sizeof(metal_scene.tonemap_params));
                metal_scene.meter_exposure = metal_preview_hdr &&
                    gSavedSettings.get<bool>(
                        "RenderDynamicExposureEnabled") &&
                    !LLRender::sClassicMode;
                metal_scene.exposure_compensation =
                    gSavedSettings.get<F32>(
                        "AlchemyExposureCompensation");
                metal_scene.exposure_min_ev = gSavedSettings.get<F32>(
                    "AlchemyExposureMinEV");
                metal_scene.exposure_max_ev = gSavedSettings.get<F32>(
                    "AlchemyExposureMaxEV");
                metal_scene.exposure_low_fraction =
                    metal_preview_low_fraction;
                metal_scene.exposure_high_fraction =
                    metal_preview_high_fraction;
                metal_scene.exposure_metering_mode =
                    gSavedSettings.get<U32>(
                        "AlchemyExposureMeteringMode");
                metal_scene.bloom_enabled = metal_preview_bloom;
                metal_scene.bloom_mip_count = static_cast<U32>(llclamp(
                    gSavedSettings.get<S32>("RenderBloomMipCount"),
                    3, 7));
                metal_scene.bloom_resolution_scale = static_cast<U32>(
                    llclamp(gSavedSettings.get<S32>(
                                "RenderBloomResolutionScale"),
                            0, 4));
                metal_scene.bloom_threshold = llmax(
                    gSavedSettings.get<F32>("RenderBloomThreshold"), 0.f);
                metal_scene.bloom_knee = llmax(
                    gSavedSettings.get<F32>("RenderBloomKnee"), 0.f);
                metal_scene.bloom_firefly_clamp = llmax(
                    gSavedSettings.get<F32>("RenderBloomFireflyClamp"),
                    0.f);
                metal_scene.bloom_scatter = llmax(
                    gSavedSettings.get<F32>("RenderBloomScatter"), 0.f);
                metal_scene.bloom_alpha_glow_boost = llmax(
                    gSavedSettings.get<F32>("RenderBloomAlphaGlowBoost"),
                    0.f);
                metal_scene.bloom_strength = metal_preview_bloom_strength;
                metal_scene.bloom_halation = metal_preview_halation;
                metal_scene.bloom_halation_strength =
                    metal_preview_halation_strength;
                const LLColor3 halation_tint =
                    gSavedSettings.get<LLColor3>(
                        "RenderBloomHalationTint");
                std::memcpy(metal_scene.bloom_halation_tint,
                            halation_tint.mV,
                            sizeof(metal_scene.bloom_halation_tint));
                std::memcpy(metal_scene.bloom_halation_weights,
                            LLPipeline::RenderGlowWarmthWeights.mV,
                            sizeof(metal_scene.bloom_halation_weights));
                metal_scene.cross_filter_enabled =
                    metal_preview_cross_filter;
                metal_scene.cross_filter_strength =
                    metal_preview_cross_filter_strength;
                metal_scene.cross_filter_points = static_cast<U32>(llclamp(
                    gSavedSettings.get<S32>("RenderCrossFilterPoints"),
                    2, 12));
                metal_scene.cross_filter_angle = llclamp(
                    gSavedSettings.get<F32>("RenderCrossFilterAngle"),
                    0.f, 360.f);
                metal_scene.cross_filter_length = llclamp(
                    gSavedSettings.get<F32>("RenderCrossFilterLength"),
                    0.25f, 2.f);
                metal_scene.cross_filter_falloff = llclamp(
                    gSavedSettings.get<F32>("RenderCrossFilterFalloff"),
                    0.1f, 3.f);
                metal_scene.cross_filter_chromatic = llclamp(
                    gSavedSettings.get<F32>("RenderCrossFilterChromatic"),
                    0.f, 1.f);
                metal_scene.chromatic_aberration_strength = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationStrength"),
                    0.f, 1.f);
                metal_scene.chromatic_aberration_falloff = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationFalloff"),
                    0.5f, 4.f);
                metal_scene.chromatic_aberration_angle = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationAngle"),
                    0.f, 360.f);
                metal_scene.chromatic_aberration_offset_r[0] = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationOffsetRX"),
                    -1.f, 1.f);
                metal_scene.chromatic_aberration_offset_r[1] = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationOffsetRY"),
                    -1.f, 1.f);
                metal_scene.chromatic_aberration_offset_b[0] = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationOffsetBX"),
                    -1.f, 1.f);
                metal_scene.chromatic_aberration_offset_b[1] = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationOffsetBY"),
                    -1.f, 1.f);
                metal_scene.chromatic_aberration_anisotropy = llclamp(
                    gSavedSettings.get<F32>(
                        "RenderChromaticAberrationAnisotropy"),
                    -1.f, 1.f);
                mMetalPreview = renderer->render(metal_scene, &error);
            }
            if (!mMetalPreview)
            {
                LL_WARNS("Metal")
                    << "Could not mirror the PBR material preview: "
                    << error << LL_ENDL;
            }
        }
        else
        {
            LL_WARNS_ONCE("Metal")
                << "PBR material preview retained GL because its native "
                   "geometry, textures, device or destination is incomplete"
                << LL_ENDL;
        }
#endif

        LLEnvironment::instance().bindEnvironmentUBO();

        gPipeline.unbindDeferredShader(shader);

        screen.flush();
    }

    // *HACK: Hide mExposureMap from generateExposure
    gPipeline.mExposureMap.swapFBORefs(gPipeline.mLastExposure);

    const bool hdr = LLPipeline::hdrEnabled();
    LLPipeline::Bloom bloom;
    if (hdr)
    {
        gPipeline.generateLuminance(&screen, &gPipeline.mLuminanceMap);
        gPipeline.generateExposure(&gPipeline.mLuminanceMap, &gPipeline.mExposureMap, /*use_history = */ false);

        // Bloom composite is folded into colorCorrect's tonemap variants, so we
        // only generate the pyramid here.
        bloom = gPipeline.generateBloomHDR(&screen, post);
    }

    gPipeline.colorCorrect(&screen, post_ping, true, false, &bloom);
    LLVertexBuffer::unbind();

    if (!hdr)
    {
        if (LLRenderTarget* glow = gPipeline.generateGlow(post_ping, post))
        {
            gPipeline.combineGlow(post_ping, &screen, glow);
        }
    }

    // *HACK: Restore mExposureMap (it will be consumed by generateExposure next frame)
    gPipeline.mExposureMap.swapFBORefs(gPipeline.mLastExposure);

    // Final render
    // Nested post-processing targets restore the full preview target's viewport.
    gGL.setViewport(mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);
    gDeferredPostNoDoFProgram.bind();

    // From LLPipeline::renderFinalize: "Whatever is last in the above post processing chain should _always_ be rendered directly here.  If not, expect problems."
    // Under HDR that is colorCorrect's output in post_ping; otherwise the glow combine has
    // written the result back to the screen.
    LLRenderTarget* last_in_chain = hdr ? post_ping : &screen;
    gDeferredPostNoDoFProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, last_in_chain);
    gDeferredPostNoDoFProgram.bindDepthTexture(LLShaderMgr::DEFERRED_DEPTH, &gPipeline.mAuxillaryRT.deferredScreen);

    {
        LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
        gPipeline.mScreenTriangleVB->setBuffer();
        gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
    }

    gDeferredPostNoDoFProgram.unbind();

    // Clean up
    gPipeline.setupHWLights();
    gPipeline.mReflectionMapManager.forceDefaultProbeAndUpdateUniforms(false);
    gSavedSettings.set<S32>("RenderLocalLightCount", old_local_light_count);
    LLViewerCamera::setCurrent(saved_camera);

    return true;
}

void LLGLTFPreviewTexture::postRender(bool success)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    if (!mShouldRender) { return; }
    mShouldRender = false;
    if (success)
    {
        mBestLoad = mPendingLoad;
    }

    LLViewerDynamicTexture::postRender(success);
#if LL_DARWIN
    if (lastPostRenderSucceeded() && mMetalPreview)
    {
        if (mGLTexturep->adoptMetalRenderTexture(std::move(mMetalPreview)))
        {
            LL_INFOS_ONCE("Metal")
                << "PBR material previews own a native Metal render target"
                << LL_ENDL;
        }
        else
        {
            LL_WARNS("Metal")
                << "Could not publish the native PBR material preview"
                << LL_ENDL;
        }
    }
    mMetalPreview.reset();
#endif
}

LLPointer<LLViewerTexture> LLGLTFMaterialPreviewMgr::getPreview(LLPointer<LLFetchedGLTFMaterial> &material)
{
    if (!material)
    {
        return nullptr;
    }

    if (!is_material_loaded_enough_for_ui(*material))
    {
        return nullptr;
    }

    return LLGLTFPreviewTexture::create(material);
}
