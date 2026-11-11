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

#include "llvisualeffect.h"

// ============================================================================
// LLTweenableValue class
//

template<>
float LLTweenableValueLerp<float>::get()
{
    if (!m_CurValue)
    {
        float curFactor = (F32)((LLTimer::getUptimeSeconds() - m_StartTime) / m_Duration);
        if (curFactor < 1.0f)
            return lerp(m_StartValue, m_EndValue, curFactor);
        m_CurValue = m_EndValue;
    }
    return m_CurValue.get();
}

template<>
LLColor3 LLTweenableValueLerp<LLColor3>::get()
{
    if (!m_CurValue)
    {
        float curFactor = (F32)((LLTimer::getUptimeSeconds() - m_StartTime) / m_Duration);
        if (curFactor < 1.0f)
            return lerp(m_StartValue, m_EndValue, curFactor);
        m_CurValue = m_EndValue;
    }
    return m_CurValue.get();
}

template<>
LLVector4 LLTweenableValueLerp<LLVector4>::get()
{
    if (!m_CurValue)
    {
        float curFactor = (F32)((LLTimer::getUptimeSeconds() - m_StartTime) / m_Duration);
        if (curFactor < 1.0f)
            return lerp(m_StartValue, m_EndValue, curFactor);
        m_CurValue = m_EndValue;
    }
    return m_CurValue.get();
}

// ============================================================================
// LLVfxManager class
//

LLVfxManager::LLVfxManager()
{
}

bool LLVfxManager::addEffect(LLVisualEffect* pEffectInst)
{
    // Effect IDs can be reused across effects but should be unique for all effect instances sharing the same effect code
    auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [pEffectInst](const LLVisualEffect* pEffect) { return pEffect->getCode() == pEffectInst->getCode() && pEffect->getId() == pEffectInst->getId(); });
    llassert(m_Effects.end() == itEffect);
    if (m_Effects.end() != itEffect)
        return false;

    m_Effects.insert(std::upper_bound(m_Effects.begin(), m_Effects.end(), pEffectInst, cmpEffect), pEffectInst);
    return true;
}

LLVisualEffect* LLVfxManager::getEffect(EVisualEffect eCode, const LLUUID& idEffect) const
{
    auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [eCode, &idEffect](const LLVisualEffect* pEffect) { return pEffect->getCode() == eCode && pEffect->getId() == idEffect; });
    return (m_Effects.end() != itEffect) ? *itEffect : nullptr;
}

bool LLVfxManager::removeEffect(EVisualEffect eCode, const LLUUID& idEffect)
{
    auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [eCode, &idEffect](const LLVisualEffect* pEffect) { return pEffect->getCode() == eCode && pEffect->getId() == idEffect; });
    if (m_Effects.end() == itEffect)
        return false;

    delete *itEffect;
    m_Effects.erase(itEffect);
    return true;
}

void  LLVfxManager::updateEffect(LLVisualEffect* pEffect, bool fEnabled, U32 nPriority)
{
    llassert(m_Effects.end() != std::find(m_Effects.begin(), m_Effects.end(), pEffect));

    if ( (pEffect->getEnabled() != fEnabled) || (pEffect->getPriority() != nPriority) )
    {
        pEffect->setEnabled(fEnabled);
        pEffect->setPriority(nPriority);
        std::sort(m_Effects.begin(), m_Effects.end(), cmpEffect);
    }
}

// static
bool LLVfxManager::cmpEffect(const LLVisualEffect* pLHS, const LLVisualEffect* pRHS)
{
    if (pLHS && pRHS)
    {
        // Sort by code, then priority, then memory address
        return (pLHS->getCode() == pRHS->getCode()) ? (pLHS->getPriority() == pRHS->getPriority() ? pLHS < pRHS
                                                                                                  : pLHS->getPriority() > pRHS->getPriority())
                                                    : pLHS->getCode() < pRHS->getCode();
    }
    return (pLHS);
}

// ============================================================================
