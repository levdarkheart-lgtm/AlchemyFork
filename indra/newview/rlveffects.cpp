/**
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Copyright (c) 2021, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#include "llviewerprecompiledheaders.h"
#include "llviewercamera.h"

#include "llagent.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

#include "rlveffects.h"
#include "rlvhandler.h"

// ====================================================================================
// RlvOverlayEffect class
//

const float c_DefaultAlpha = 1.0f;
const float c_DefaultColor[3] = { 1.0f, 1.0f, 1.0f };

RlvOverlayEffect::RlvOverlayEffect(const LLUUID& idRlvObj)
    : LLVisualEffect(idRlvObj, EVisualEffect::RlvOverlay)
    , m_nAlpha(c_DefaultAlpha)
    , m_fBlockTouch(false)
    , m_Color(LLColor3(c_DefaultColor))
{
    m_fEnabled = false;
}

RlvOverlayEffect::~RlvOverlayEffect()
{
    clearImage();
}

// static
ERlvCmdRet RlvOverlayEffect::onAlphaValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvOverlayEffect* pEffect = LLVfxManager::instance().getEffect<RlvOverlayEffect>(idRlvObj))
    {
        pEffect->m_nAlpha = (newValue) ? boost::get<float>(newValue.value()) : c_DefaultAlpha;
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvOverlayEffect::onColorValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvOverlayEffect* pEffect = LLVfxManager::instance().getEffect<RlvOverlayEffect>(idRlvObj))
    {
        pEffect->m_Color = LLColor3( (newValue) ? boost::get<LLVector3>(newValue.value()).mV : c_DefaultColor);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvOverlayEffect::onTextureChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvOverlayEffect* pEffect = LLVfxManager::instance().getEffect<RlvOverlayEffect>(idRlvObj))
    {
        if (newValue)
            pEffect->setImage(boost::get<LLUUID>(newValue.value()));
        else
            pEffect->clearImage();
    }
    return RLV_RET_SUCCESS;
}

void RlvOverlayEffect::clearImage()
{
    if (m_pImage)
    {
        m_pImage->setBoostLevel(m_nImageOrigBoost);
        m_pImage = nullptr;
    }
}

bool RlvOverlayEffect::hitTest(const LLCoordGL& ptMouse) const
{
    if (!m_pImage)
        return false;

    return (m_fBlockTouch) && (m_pImage->getMask(LLVector2((float)ptMouse.mX / gViewerWindow->getWorldViewWidthScaled(), (float)ptMouse.mY / gViewerWindow->getWorldViewHeightScaled())));
}

void RlvOverlayEffect::setImage(const LLUUID& idTexture)
{
    if ( (m_pImage) && (m_pImage->getID() == idTexture) )
        return;

    clearImage();
    m_pImage = LLViewerTextureManager::getFetchedTexture(idTexture, FTT_DEFAULT, MIPMAP_YES, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
    m_nImageOrigBoost = m_pImage->getBoostLevel();
    m_pImage->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
    m_pImage->forceToSaveRawImage(0);
}

void RlvOverlayEffect::run()
{
    if (m_pImage)
    {
        gUIProgram.bind();

        int nWidth = gViewerWindow->getWorldViewWidthScaled();
        int nHeight = gViewerWindow->getWorldViewHeightScaled();

        m_pImage->addTextureStats((F32)(nWidth * nHeight));
        m_pImage->setKnownDrawSize(nWidth, nHeight);

        gGL.pushMatrix();
        LLGLSUIDefault glsUI;
        gViewerWindow->setup2DRender();

        const LLVector2& displayScale = gViewerWindow->getDisplayScale();
        gGL.scalef(displayScale.mV[VX], displayScale.mV[VY], 1.f);

        gGL.getTextureSlot(0)->bindSampled(m_pImage, ALSamplers::AnisoWrap);
        const LLColor3 col = m_Color.get();
        gGL.color4f(col.mV[0], col.mV[1], col.mV[2], llclamp(m_nAlpha.get(), 0.0f, 1.0f));

        gl_rect_2d_simple_tex(nWidth, nHeight);

        gGL.getTextureSlot(0)->unbind();

        gGL.popMatrix();
        gGL.flush();
        gViewerWindow->setup3DRender();

        gUIProgram.unbind();
    }
}

// ====================================================================================
// RlvSphereEffect class
//

const int   c_SphereDefaultMode = 0;
const int   c_SphereDefaultOrigin = 0;
const float c_SphereDefaultColor[4] = { 0.0, 0.f, 0.f, 0.f };
const float c_SphereDefaultDistance = 0.0f;
const int   c_SphereDefaultDistanceExtend = 1;
const float c_SphereDefaultAlpha = 1.0f;

RlvSphereEffect::RlvSphereEffect(const LLUUID& idRlvObj)
    : LLVisualEffect(idRlvObj, EVisualEffect::RlvSphere)
    , m_eMode((ESphereMode)c_SphereDefaultMode)
    , m_eOrigin((ESphereOrigin)c_SphereDefaultOrigin)
    , m_Params(LLVector4(c_SphereDefaultColor))
    , m_nDistanceMin(c_SphereDefaultDistance), m_nDistanceMax(c_SphereDefaultDistance)
    , m_eDistExtend((ESphereDistExtend)c_SphereDefaultDistanceExtend)
    , m_nValueMin(c_SphereDefaultAlpha), m_nValueMax(c_SphereDefaultAlpha)
    , m_nTweenDuration(0.f)
{
}

RlvSphereEffect::~RlvSphereEffect()
{
}

// static
ERlvCmdRet RlvSphereEffect::onModeChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        pEffect->m_eMode = (ESphereMode)((newValue) ? boost::get<int>(newValue.value()) : c_SphereDefaultMode);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onOriginChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        pEffect->m_eOrigin = (ESphereOrigin)((newValue) ? boost::get<int>(newValue.value()) : c_SphereDefaultOrigin);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onColorChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        LLVector4 vecColor = (newValue) ? LLVector4(boost::get<LLVector3>(newValue.value()), 1.0f) : LLVector4(c_SphereDefaultColor);
        if (!pEffect->m_nTweenDuration)
            pEffect->m_Params = vecColor;
        else
            pEffect->m_Params.start(vecColor, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onDistMinChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        float nDistanceMin = (newValue) ? boost::get<float>(newValue.value()) : c_SphereDefaultDistance;
        if (!pEffect->m_nTweenDuration)
            pEffect->m_nDistanceMin = nDistanceMin;
        else
            pEffect->m_nDistanceMin.start(nDistanceMin, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onDistMaxChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        float nDistanceMax = (newValue) ? boost::get<float>(newValue.value()) : c_SphereDefaultDistance;
        if (!pEffect->m_nTweenDuration)
            pEffect->m_nDistanceMax = nDistanceMax;
        else
            pEffect->m_nDistanceMax.start(nDistanceMax, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onDistExtendChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        pEffect->m_eDistExtend = (ESphereDistExtend)((newValue) ? boost::get<int>(newValue.value()) : c_SphereDefaultDistanceExtend);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onParamsChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        LLVector4 params = LLVector4((newValue) ? boost::get<LLVector4>(newValue.value()).mV : c_SphereDefaultColor);
        if (!pEffect->m_nTweenDuration)
            pEffect->m_Params = params;
        else
            pEffect->m_Params.start(params, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onTweenDurationChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        pEffect->m_nTweenDuration = (newValue) ? boost::get<float>(newValue.value()) : 0;
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onValueMinChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        float nValueMin = (newValue) ? boost::get<float>(newValue.value()) : c_SphereDefaultAlpha;;
        if (!pEffect->m_nTweenDuration)
            pEffect->m_nValueMin = nValueMin;
        else
            pEffect->m_nValueMin.start(nValueMin, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvSphereEffect::onValueMaxChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
    if (RlvSphereEffect* pEffect = LLVfxManager::instance().getEffect<RlvSphereEffect>(idRlvObj))
    {
        float nValueMax = (newValue) ? boost::get<float>(newValue.value()) : c_SphereDefaultAlpha;
        if (!pEffect->m_nTweenDuration)
            pEffect->m_nValueMax = nValueMax;
        else
            pEffect->m_nValueMax.start(nValueMax, pEffect->m_nTweenDuration);
    }
    return RLV_RET_SUCCESS;
}

bool RlvSphereEffect::getShaderParams(ShaderParams& params)
{
    const S32 raw_mode = static_cast<S32>(m_eMode);
    params.mode = llclamp(raw_mode, 0, static_cast<S32>(ESphereMode::Count));

    // Pass the sphere origin to the shader
    LLVector4 posSphereOrigin;
    switch (m_eOrigin)
    {
        case ESphereOrigin::Camera:
            posSphereOrigin.setVec(LLViewerCamera::instance().getOrigin(), 1.0f);
            break;
        case ESphereOrigin::Avatar:
        default:
            posSphereOrigin.setVec((isAgentAvatarValid()) ? gAgentAvatarp->getRenderPosition() : gAgent.getPositionAgent(), 1.0f);
            break;
    }
    LLVector4a posSphereOriginIn, posSphereOriginGl;
    posSphereOriginIn.loadua(posSphereOrigin.mV);
    LLViewerCamera::getCurrent().getModelview().transform4(posSphereOriginIn, posSphereOriginGl);
    params.origin.set(posSphereOriginGl.getF32ptr());

    // Pack min/max distance and alpha together
    float nDistMin = m_nDistanceMin.get(), nDistMax = m_nDistanceMax.get();
    params.distance_value.set(
        m_nValueMin.get(), nDistMin, m_nValueMax.get(),
        (nDistMax >= nDistMin) ? nDistMax : nDistMin);

    // Pass dist extend
    int eDistExtend = (int)m_eDistExtend;
    params.distance_extend.set(
        static_cast<F32>(eDistExtend & static_cast<int>(ESphereDistExtend::Min)),
        static_cast<F32>(eDistExtend & static_cast<int>(ESphereDistExtend::Max)));

    // Pass effect params
    params.effect = m_Params.get();

    return raw_mode >= 0 &&
        raw_mode < static_cast<S32>(ESphereMode::Count);
}

// ====================================================================================
