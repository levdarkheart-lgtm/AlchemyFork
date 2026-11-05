/**
 * @file lldrawpoolavatar.cpp
 * @brief LLDrawPoolAvatar class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "lldrawpoolavatar.h"
#include "llskinningutil.h"
#include "llrender.h"

#include "llvoavatar.h"
#include "m3math.h"
#include "llmatrix4a.h"

#include "llagent.h" //for gAgent.needsRenderAvatar()
#include "lldrawable.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llmeshrepository.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "noise.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llvovolume.h"
#include "llvolume.h"
#include "llappviewer.h"
#include "llrendersphere.h"
#include "llviewerpartsim.h"
#include "llviewercontrol.h" // for gSavedSettings
#include "llviewertexturelist.h"

static U32 sShaderLevel = 0;

LLGLSLShader* LLDrawPoolAvatar::sVertexProgram = NULL;
bool    LLDrawPoolAvatar::sSkipOpaque = false;
bool    LLDrawPoolAvatar::sSkipTransparent = false;
S32     LLDrawPoolAvatar::sShadowPass = -1;
LLDrawPoolAvatar::eShadowBakes LLDrawPoolAvatar::sShadowBakes = LLDrawPoolAvatar::SHADOW_BAKES_ALL;
bool    LLDrawPoolAvatar::sShadowCutBakeLeft = false;
S32 LLDrawPoolAvatar::sDiffuseChannel = 0;
F32 LLDrawPoolAvatar::sMinimumAlpha = 0.2f;

static bool is_deferred_render = false;
static bool is_post_deferred_render = false;

extern bool gUseGLPick;

F32 CLOTHING_GRAVITY_EFFECT = 0.7f;
F32 CLOTHING_ACCEL_FORCE_FACTOR = 0.2f;

struct AvatarClothUniforms
{
    LLVector4 wind;
    LLVector4 sin_wave;
    LLVector4 gravity;
    LLVector4 depth;
};

// The cloth uniforms (avatarSkinV's getClothedPosition) for one avatar, turned
// into the eye space getModelView() skins it into: the camera's, or the shadow
// map's. The native world owner consumes the exact same derived values.
static AvatarClothUniforms avatar_cloth_uniforms(const LLVOAvatar* avatarp)
{
    const LLMatrix4& view = LLDrawPoolAvatar::getModelView();

    AvatarClothUniforms uniforms;
    uniforms.wind.setVec(avatarp->mWindVec);
    uniforms.wind.mV[VW] = 0;
    uniforms.wind = uniforms.wind * view;
    uniforms.wind.mV[VW] = avatarp->mWindVec.mV[VW];

    F32 phase = -1.f * (avatarp->mRipplePhase);
    F32 freq = 7.f + (noise1(avatarp->mRipplePhase) * 2.f);
    uniforms.sin_wave.set(freq, freq, freq, phase);

    uniforms.gravity.set(0.f, 0.f, -CLOTHING_GRAVITY_EFFECT, 0.f);
    uniforms.gravity = uniforms.gravity * view;

    // The ripple's phase runs along the main camera's depth: in a shadow or capture pass the depth column of the
    // current eye space carried back to the agent's frame and on into the main camera's.
    uniforms.depth.set(0.f, 0.f, 1.f, 0.f);
    LLViewerCamera* main_camera = LLViewerCamera::getInstance();
    if (&LLViewerCamera::getCurrent() != main_camera)
    {
        LLMatrix4 main_view;
        main_camera->getModelview().store(main_view);
        LLMatrix4 to_main = view;
        to_main.invert();
        to_main *= main_view;
        uniforms.depth.set(to_main.mMatrix[0][2], to_main.mMatrix[1][2],
                           to_main.mMatrix[2][2], to_main.mMatrix[3][2]);
    }
    return uniforms;
}

static void set_cloth_uniforms(LLGLSLShader* shader,
                               const AvatarClothUniforms& uniforms)
{
    shader->uniform4fv(LLViewerShaderMgr::AVATAR_WIND, 1,
                       uniforms.wind.mV);
    shader->uniform4fv(LLViewerShaderMgr::AVATAR_SINWAVE, 1,
                       uniforms.sin_wave.mV);
    shader->uniform4fv(LLViewerShaderMgr::AVATAR_GRAVITY, 1,
                       uniforms.gravity.mV);
    shader->uniform4fv(LLViewerShaderMgr::AVATAR_CLOTH_DEPTH, 1,
                       uniforms.depth.mV);
}

// Format for gAGPVertices
// vertex format for bumpmapping:
//  vertices   12
//  pad         4
//  normals    12
//  pad         4
//  texcoords0  8
//  texcoords1  8
// total       48
//
// for no bumpmapping
//  vertices       12
//  texcoords   8
//  normals    12
// total       32
//

S32 AVATAR_OFFSET_POS = 0;
S32 AVATAR_OFFSET_NORMAL = 16;
S32 AVATAR_OFFSET_TEX0 = 32;
S32 AVATAR_OFFSET_TEX1 = 40;
S32 AVATAR_VERTEX_BYTES = 48;

bool gAvatarEmbossBumpMap = false;
static bool sRenderingSkinned = false;
S32 cube_channel = -1;

LLDrawPoolAvatar::LLDrawPoolAvatar(U32 type) :
    LLFacePool(type)
{
}

LLDrawPoolAvatar::~LLDrawPoolAvatar()
{
    if (!isDead())
    {
        LL_WARNS() << "Destroying avatar drawpool that still contains faces" << LL_ENDL;
    }
}

// virtual
bool LLDrawPoolAvatar::isDead()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (!LLFacePool::isDead())
    {
        return false;
    }

    return true;
}

S32 LLDrawPoolAvatar::getShaderLevel() const
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    return (S32) LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);
}

void LLDrawPoolAvatar::prerender()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);

    sShaderLevel = mShaderLevel;
}

LLMatrix4& LLDrawPoolAvatar::getModelView()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    static LLMatrix4 ret;

    LLViewerCamera::getCurrent().getModelview().store(ret);

    return ret;
}

//-----------------------------------------------------------------------------
// render()
//-----------------------------------------------------------------------------



void LLDrawPoolAvatar::beginDeferredPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    const S32 effective_pass = pass +
        (LLPipeline::sImpostorRender ? 1 : 0);
    if (effective_pass == 0)
    {
        LLVOAvatar::sNumVisibleAvatars = 0;
    }
    sSkipTransparent = true;
    is_deferred_render = true;
}

void LLDrawPoolAvatar::endDeferredPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    (void)pass;
    sSkipTransparent = false;
    is_deferred_render = false;
}

void LLDrawPoolAvatar::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (LLPipeline::sImpostorRender)
    {
        ++pass;
        if (pass >= getNumDeferredPasses())
        {
            return;
        }
    }
#if LL_DARWIN
    captureMetalDeferred(pass);
#else
    (void)pass;
#endif
}

LLVOAvatar* LLDrawPoolAvatar::getMetalAvatar(S32 pass) const
{
    if (mDrawFace.empty())
    {
        return nullptr;
    }

    const LLFace* face = mDrawFace[0];
    if (!face || !face->getDrawable())
    {
        return nullptr;
    }
    LLVOAvatar* avatar = static_cast<LLVOAvatar*>(
        face->getDrawable()->getVObj().get());
    if (!avatar || avatar->isDead() || avatar->mDrawable.isNull())
    {
        return nullptr;
    }
    if (!avatar->isFullyLoaded())
    {
        // The retained pass can substitute an immediate-mode debug sphere.
        // Until that diagnostic has a direct native producer, retain the full
        // GL face instead of publishing a probe with the marker omitted.
        gPipeline.invalidateMetalReflectionProbeScene();
        return nullptr;
    }

    static LLCachedControl<bool> friends_only(
        gSavedSettings, "RenderAvatarFriendsOnly", false);
    if (friends_only() && !avatar->isUIAvatar() &&
        !avatar->isControlAvatar() && !avatar->isSelf() &&
        !avatar->isBuddy())
    {
        return nullptr;
    }

    const bool impostor = !LLPipeline::sImpostorRender &&
        avatar->isImpostor();
    const bool appearance_impostor =
        LLVOAvatar::AOA_NORMAL != avatar->getOverallAppearance() &&
        !avatar->needsImpostorUpdate();
    if ((avatar->isInMuteList() || impostor || appearance_impostor) &&
        pass != 0)
    {
        return nullptr;
    }

    LLVOAvatar* attached_avatar = avatar->getAttachedAvatar();
    if (attached_avatar &&
        (LLVOAvatar::AOA_NORMAL !=
             attached_avatar->getOverallAppearance() ||
         !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_AVATAR)))
    {
        return nullptr;
    }
    return avatar;
}

void LLDrawPoolAvatar::captureMetalDeferred(S32 pass)
{
#if LL_DARWIN
    if (pass < 0 || pass >= getNumDeferredPasses())
    {
        return;
    }
    LLVOAvatar* avatar = getMetalAvatar(pass);
    if (!avatar)
    {
        return;
    }
    const bool impostor = !LLPipeline::sImpostorRender &&
        avatar->isImpostor();
    const bool appearance_impostor =
        LLVOAvatar::AOA_NORMAL != avatar->getOverallAppearance() &&
        !avatar->needsImpostorUpdate();

    if (pass == 0)
    {
        ++LLVOAvatar::sNumVisibleAvatars;
        if (!impostor && LLPipeline::sUnderWaterRender)
        {
            return;
        }
        if ((impostor || appearance_impostor) &&
            avatar->mImpostor.isComplete())
        {
            gPipeline.beginMetalWorldImpostorPass();
            avatar->captureMetalDeferredImpostor();
            gPipeline.endMetalWorldGBufferPass();
        }
        return;
    }

    if (pass == 1)
    {
        gPipeline.beginMetalWorldAvatarPass(false, false);
        avatar->captureMetalDeferredRigid();
        gPipeline.endMetalWorldGBufferPass();
        return;
    }

    const bool avatar_cloth =
        gPipeline.shaderInputs().mLocal.mAvatarCloth;
    AvatarClothUniforms cloth_uniforms;
    if (avatar_cloth)
    {
        cloth_uniforms = avatar_cloth_uniforms(avatar);
    }
    gPipeline.beginMetalWorldAvatarPass(
        true, avatar_cloth,
        avatar_cloth ? cloth_uniforms.wind.mV : nullptr,
        avatar_cloth ? cloth_uniforms.sin_wave.mV : nullptr,
        avatar_cloth ? cloth_uniforms.gravity.mV : nullptr,
        avatar_cloth ? cloth_uniforms.depth.mV : nullptr);
    avatar->captureMetalDeferredOpaque();
    if (LLPipeline::sImpostorRender)
    {
        // Impostor G-buffer baking has no later avatar-alpha pool. Hair,
        // eyelashes and skirts therefore use the same alpha-mask writer as
        // the retained deferred pass, but are enumerated directly once.
        avatar->captureMetalDeferredTransparentMask();
    }
    gPipeline.endMetalWorldGBufferPass();
#else
    (void)pass;
#endif
}

void LLDrawPoolAvatar::captureMetalPostDeferred()
{
#if LL_DARWIN
    LLVOAvatar* avatar = getMetalAvatar(2);
    if (!avatar)
    {
        return;
    }
    const bool avatar_cloth =
        gPipeline.shaderInputs().mLocal.mAvatarCloth;
    AvatarClothUniforms cloth_uniforms;
    if (avatar_cloth)
    {
        cloth_uniforms = avatar_cloth_uniforms(avatar);
    }
    gPipeline.beginMetalWorldAvatarPass(
        true, avatar_cloth,
        avatar_cloth ? cloth_uniforms.wind.mV : nullptr,
        avatar_cloth ? cloth_uniforms.sin_wave.mV : nullptr,
        avatar_cloth ? cloth_uniforms.gravity.mV : nullptr,
        avatar_cloth ? cloth_uniforms.depth.mV : nullptr,
        true);
    avatar->captureMetalPostDeferredTransparent();
    gPipeline.endMetalWorldGBufferPass();
#endif
}

S32 LLDrawPoolAvatar::getNumPostDeferredPasses()
{
    if (LLPipeline::sImpostorRender)
    {
        // Nothing left for this pass to do in a bake, and what it did was wrong.
        //
        // The rigid pass already wrote the eyes into the impostor G-buffer.
        // Hair and skirts are enumerated explicitly by captureMetalDeferred()
        // in that same G-buffer transaction, so no later avatar-alpha pass is
        // required for an impostor bake.
        //
        // Returning zero skips begin/render/end entirely, including the deferred shader bind.
        return 0;
    }

    return 1;
}

void LLDrawPoolAvatar::beginPostDeferredPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    (void)pass;
    sSkipOpaque = true;
    sShaderLevel = mShaderLevel;
    sRenderingSkinned = true;
}

void LLDrawPoolAvatar::endPostDeferredPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    (void)pass;
    sRenderingSkinned = false;
    sSkipOpaque = false;
    sShaderLevel = mShaderLevel;
}

void LLDrawPoolAvatar::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    (void)pass;
    is_post_deferred_render = true;
#if LL_DARWIN
    captureMetalPostDeferred();
#endif
    is_post_deferred_render = false;
}


bool LLDrawPoolAvatar::hasShadowCasters() const
{
    return true;
}

void LLDrawPoolAvatar::captureMetalDirectionalShadow()
{
#if LL_DARWIN
    if (mDrawFace.empty())
    {
        return;
    }
    const LLFace* face = mDrawFace[0];
    if (!face || !face->getDrawable())
    {
        return;
    }
    LLVOAvatar* avatar = static_cast<LLVOAvatar*>(
        face->getDrawable()->getVObj().get());
    if (!avatar || avatar->isDead() || avatar->isUIAvatar() ||
        avatar->mDrawable.isNull())
    {
        return;
    }

    static LLCachedControl<bool> friends_only(
        gSavedSettings, "RenderAvatarFriendsOnly", false);
    if (friends_only() && !avatar->isControlAvatar() &&
        !avatar->isSelf() && !avatar->isBuddy())
    {
        return;
    }
    const bool impostor = !LLPipeline::sImpostorRender &&
        avatar->isImpostor();
    if (avatar->isTooSlow() || impostor ||
        avatar->getOverallAppearance() == LLVOAvatar::AOA_INVISIBLE)
    {
        return;
    }

    static LLCachedControl<S32> avatar_shadow_detail(
        gSavedSettings, "RenderAvatarShadowDetail", 2);
    for (S32 pass = SHADOW_PASS_AVATAR_OPAQUE;
         pass < NUM_SHADOW_PASSES; ++pass)
    {
        if ((pass == SHADOW_PASS_AVATAR_ALPHA_BLEND &&
             avatar_shadow_detail() < 2) ||
            (pass == SHADOW_PASS_AVATAR_ALPHA_MASK &&
             avatar_shadow_detail() != 1))
        {
            continue;
        }

        sShadowPass = pass;
        sShadowBakes = pass == SHADOW_PASS_AVATAR_OPAQUE
            ? SHADOW_BAKES_SOLID
            : pass == SHADOW_PASS_AVATAR_OPAQUE_CUT
            ? SHADOW_BAKES_CUT : SHADOW_BAKES_ALL;
        const bool avatar_cloth =
            gPipeline.shaderInputs().mLocal.mAvatarCloth &&
            (pass == SHADOW_PASS_AVATAR_OPAQUE ||
             (pass == SHADOW_PASS_AVATAR_OPAQUE_CUT &&
              mShadowCutBakes));
        AvatarClothUniforms cloth_uniforms;
        if (avatar_cloth)
        {
            cloth_uniforms = avatar_cloth_uniforms(avatar);
        }
        gPipeline.beginMetalSpotShadowAvatarPass(
            avatar_cloth,
            avatar_cloth ? cloth_uniforms.wind.mV : nullptr,
            avatar_cloth ? cloth_uniforms.sin_wave.mV : nullptr,
            avatar_cloth ? cloth_uniforms.gravity.mV : nullptr,
            avatar_cloth ? cloth_uniforms.depth.mV : nullptr);

        if (pass == SHADOW_PASS_AVATAR_OPAQUE)
        {
            sSkipTransparent = true;
            sShadowCutBakeLeft = false;
            avatar->captureMetalDeferredOpaque();
            mShadowCutBakes = sShadowCutBakeLeft;
            sSkipTransparent = false;
        }
        else if (pass == SHADOW_PASS_AVATAR_OPAQUE_CUT)
        {
            if (mShadowCutBakes)
            {
                sSkipTransparent = true;
                avatar->captureMetalDeferredOpaque();
                sSkipTransparent = false;
            }
        }
        else
        {
            sSkipOpaque = true;
            avatar->captureMetalDeferredTransparentMask();
            sSkipOpaque = false;
        }
        gPipeline.endMetalSpotShadowAvatarPass();
    }
    sShadowBakes = SHADOW_BAKES_ALL;
    sShadowPass = -1;
#endif
}

// static
bool LLDrawPoolAvatar::shadowDrawsBake(const LLGLTexture* bake)
{
    if (sShadowBakes == SHADOW_BAKES_ALL)
    {
        return true;
    }

    // Cut where some texel is under the deferred pass's cutoff, or where the alpha is not known: a local
    // composite, which is never analysed, or an analysis not yet back.
    const LLImageGL* image = bake ? bake->getGLTexture() : nullptr;
    const bool cut = !image || image->getMinAlpha() < sMinimumAlpha * 255.f;
    if (sShadowBakes == SHADOW_BAKES_SOLID)
    {
        sShadowCutBakeLeft |= cut;
        return !cut;
    }
    return cut;
}

S32 LLDrawPoolAvatar::getNumPasses()
{
    return 3;
}


S32 LLDrawPoolAvatar::getNumDeferredPasses()
{
    return 3;
}


void LLDrawPoolAvatar::render(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;
    if (LLPipeline::sImpostorRender)
    {
        ++pass;
        if (pass >= getNumPasses())
        {
            return;
        }
        renderAvatars(NULL, pass);
        return;
    }

    renderAvatars(NULL, pass); // render all avatars
}

void LLDrawPoolAvatar::beginRenderPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;
    //reset vertex buffer mappings
    LLVertexBuffer::unbind();

    if (LLPipeline::sImpostorRender)
    { //impostor render does not have impostors or rigid rendering
        ++pass;
    }

    switch (pass)
    {
    case 0:
        beginImpostor();
        break;
    case 1:
        beginRigid();
        break;
    case 2:
        beginSkinned();
        break;
    }

    if (pass == 0)
    { //make sure no stale colors are left over from a previous render
        gGL.diffuseColor4f(1,1,1,1);
    }
}

void LLDrawPoolAvatar::endRenderPass(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (LLPipeline::sImpostorRender)
    {
        ++pass;
    }

    switch (pass)
    {
    case 0:
        endImpostor();
        break;
    case 1:
        endRigid();
        break;
    case 2:
        endSkinned();
        break;
    }
}

void LLDrawPoolAvatar::beginImpostor()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    LLVOAvatar::sNumVisibleAvatars = 0;

    gImpostorProgram.bind();
    gImpostorProgram.setMinimumAlpha(0.01f);

    gPipeline.enableLightsFullbright();
    sDiffuseChannel = 0;
}

void LLDrawPoolAvatar::endImpostor()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    gImpostorProgram.unbind();
    gPipeline.enableLightsDynamic();
}

void LLDrawPoolAvatar::beginRigid()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (gPipeline.shadersLoaded())
    {
        sVertexProgram = &gObjectAlphaMaskNoColorProgram;

        if (sVertexProgram != NULL)
        {   //eyeballs render with the specular shader
            sVertexProgram->bind();
            sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
        }
    }
    else
    {
        sVertexProgram = NULL;
    }
}

void LLDrawPoolAvatar::endRigid()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    sShaderLevel = mShaderLevel;
    if (sVertexProgram != NULL)
    {
        sVertexProgram->unbind();
    }
}

void LLDrawPoolAvatar::beginSkinned()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    // Used for preview only, like LLVisualParamHint
    // Uses deprecated logic!!!
    if (!gAvatarProgram.isComplete())
    {
        llassert(false); // Avatar shader shouldn't have failed if deferred shader loaded
        sVertexProgram = nullptr;
        LLGLSLShader::unbind();
        return;
    }

    sVertexProgram = &gAvatarProgram;

    sRenderingSkinned = true;

    sVertexProgram->bind();
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
}

void LLDrawPoolAvatar::endSkinned()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (sVertexProgram == nullptr)
    {
        return;
    }

    // if we're in software-blending, remember to set the fence _after_ we draw so we wait till this rendering is done
    sRenderingSkinned = false;
    sVertexProgram->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
    sVertexProgram->disableTexture(LLViewerShaderMgr::BUMP_MAP);
    sVertexProgram->unbind();
    sShaderLevel = mShaderLevel;

}

void LLDrawPoolAvatar::renderAvatars(LLVOAvatar* single_avatar, S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (pass == -1)
    {
        for (S32 i = 1; i < getNumPasses(); i++)
        { //skip impostor pass
            prerender();
            beginRenderPass(i);
            renderAvatars(single_avatar, i);
            endRenderPass(i);
        }

        return;
    }

    if (mDrawFace.empty() && !single_avatar)
    {
        return;
    }

    LLVOAvatar *avatarp = NULL;

    if (single_avatar)
    {
        avatarp = single_avatar;
    }
    else
    {
        const LLFace *facep = mDrawFace[0];
        if (!facep->getDrawable())
        {
            return;
        }
        avatarp = (LLVOAvatar *)facep->getDrawable()->getVObj().get();
    }

    if (avatarp->isDead() || avatarp->mDrawable.isNull())
    {
        return;
    }

    if (!single_avatar && !avatarp->isFullyLoaded() )
    {
        if (pass==0 && (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) || LLViewerPartSim::getMaxPartCount() <= 0))
        {
            // debug code to draw a sphere in place of avatar
            gGL.getTextureSlot(0)->bindSampled(LLViewerFetchedTexture::sWhiteImagep, ALSamplers::AnisoWrap);
            // Scoped: pass 0 runs for every avatar pool before pass 1 does, and the G-buffer
            // passes behind this one write their flags to alpha.
            LLGLSColorMask mask(true, true);
            LLVector3 pos = avatarp->getPositionAgent();
            gGL.color4f(1.0f, 1.0f, 1.0f, 0.7f);

            gGL.pushMatrix();
            gGL.translatef((F32)(pos.mV[VX]),
                           (F32)(pos.mV[VY]),
                            (F32)(pos.mV[VZ]));
             gGL.scalef(0.15f, 0.15f, 0.3f);

             gSphere.renderGGL();

             gGL.popMatrix();
        }
        // don't render please
        return;
    }

    static LLCachedControl<bool> friends_only(gSavedSettings, "RenderAvatarFriendsOnly", false);
    if (!single_avatar
        && friends_only()
        && !avatarp->isUIAvatar()
        && !avatarp->isControlAvatar()
        && !avatarp->isSelf()
        && !avatarp->isBuddy())
    {
        return;
    }

    bool impostor = !LLPipeline::sImpostorRender && avatarp->isImpostor() && !single_avatar;

    if (( avatarp->isInMuteList()
          || impostor
          || (LLVOAvatar::AOA_NORMAL != avatarp->getOverallAppearance() && !avatarp->needsImpostorUpdate()) ) && pass != 0)
//        || (LLVOAvatar::AV_DO_NOT_RENDER == avatarp->getVisualMuteSettings() && !avatarp->needsImpostorUpdate()) ) && pass != 0)
    { //don't draw anything but the impostor for impostored avatars
        return;
    }

    if (pass == 0 && !impostor && LLPipeline::sUnderWaterRender)
    { //don't draw foot shadows under water
        return;
    }

    LLVOAvatar *attached_av = avatarp->getAttachedAvatar();
    if (attached_av && (LLVOAvatar::AOA_NORMAL != attached_av->getOverallAppearance() || !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_AVATAR)))
    {
        // Animesh attachment of a jellydolled or invisible parent - don't show
        return;
    }

    if (pass == 0)
    {
        LLVOAvatar::sNumVisibleAvatars++;

//      if (impostor || (LLVOAvatar::AV_DO_NOT_RENDER == avatarp->getVisualMuteSettings() && !avatarp->needsImpostorUpdate()))
        if (impostor || (LLVOAvatar::AOA_NORMAL != avatarp->getOverallAppearance() && !avatarp->needsImpostorUpdate()))
        {
            avatarp->renderImpostor(avatarp->getMutedAVColor(), sDiffuseChannel);
        }
        return;
    }

    if (pass == 1)
    {
        // render rigid meshes (eyeballs) first
#if LL_DARWIN
        gPipeline.beginMetalWorldAvatarPass(false, false);
#endif
        avatarp->renderRigid();
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
        return;
    }

    const bool avatar_cloth =
        gPipeline.shaderInputs().mLocal.mAvatarCloth;
    AvatarClothUniforms cloth_uniforms;
    if (avatar_cloth)
    {
        cloth_uniforms = avatar_cloth_uniforms(avatarp);
        set_cloth_uniforms(sVertexProgram, cloth_uniforms);
    }

    if( !single_avatar || (avatarp == single_avatar) )
    {
#if LL_DARWIN
        gPipeline.beginMetalWorldAvatarPass(
            true, avatar_cloth,
            avatar_cloth ? cloth_uniforms.wind.mV : nullptr,
            avatar_cloth ? cloth_uniforms.sin_wave.mV : nullptr,
            avatar_cloth ? cloth_uniforms.gravity.mV : nullptr,
            avatar_cloth ? cloth_uniforms.depth.mV : nullptr);
#endif
        avatarp->renderSkinned();
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
    }
}

//-----------------------------------------------------------------------------
// getDebugTexture()
//-----------------------------------------------------------------------------
LLViewerTexture *LLDrawPoolAvatar::getDebugTexture()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (mReferences.empty())
    {
        return NULL;
    }
    LLFace *face = mReferences[0];
    if (!face->getDrawable())
    {
        return NULL;
    }
    const LLViewerObject *objectp = face->getDrawable()->getVObj();

    // Avatar should always have at least 1 (maybe 3?) TE's.
    return objectp->getTEImage(0);
}


LLColor3 LLDrawPoolAvatar::getDebugColor() const
{
    return LLColor3(0.f, 1.f, 0.f);
}


