/**
 * @file llfloaterpreferencesgraphicsadvanced.cpp
 * @brief floater for adjusting camera position
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2021, Linden Research, Inc.
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
#include "llfloaterpreferencesgraphicsadvanced.h"

#include "alrendercapabilities.h"
#include "alrenderresourceinputs.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfeaturemanager.h"
#include "llfloaterpreference.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llvoavatar.h"
#include "pipeline.h"
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvactions.h"
// [/RLVa:KB]


LLFloaterPreferenceGraphicsAdvanced::LLFloaterPreferenceGraphicsAdvanced(const LLSD& key)
    : LLFloater(key)
{
    mCommitCallbackRegistrar.add("Pref.RenderOptionUpdate",            boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onRenderOptionEnable, this));
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxNonImpostors", boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors,this));
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxComplexity",   boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity,this));

    mCommitCallbackRegistrar.add("Pref.Cancel", boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onBtnCancel, this, _2));
    mCommitCallbackRegistrar.add("Pref.OK",     boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onBtnOK, this, _2));

    mImpostorsChangedSignal = gSavedSettings.getControl("RenderAvatarMaxNonImpostors")->getSignal()->connect(boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateIndirectMaxNonImpostors, this, _2));
}

LLFloaterPreferenceGraphicsAdvanced::~LLFloaterPreferenceGraphicsAdvanced()
{
    mImpostorsChangedSignal.disconnect();
    mComplexityChangedSignal.disconnect();
    mComplexityModeChangedSignal.disconnect();
    mLODFactorChangedSignal.disconnect();
    mNumImpostorsChangedSignal.disconnect();
}

bool LLFloaterPreferenceGraphicsAdvanced::postBuild()
{
    mMainResolutionStatus = getChild<LLTextBox>("main resolution status");
    refreshMainResolutionStatus();

    mComplexityChangedSignal = gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(
        [this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
        {
            updateComplexityText();
        });
    mComplexityModeChangedSignal = gSavedSettings.getControl("RenderAvatarComplexityMode")->getSignal()->connect(
        [this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
        {
            updateComplexityMode(new_val);
        });
    mLODFactorChangedSignal = gSavedSettings.getControl("RenderVolumeLODFactor")->getCommitSignal()->connect(
        [this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
        {
            updateObjectMeshDetailText();
        });
    mNumImpostorsChangedSignal = gSavedSettings.getControl("RenderAvatarMaxNonImpostors")->getSignal()->connect(
        [this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
        {
            updateIndirectMaxNonImpostors(new_val);
        });
    return true;
}

void LLFloaterPreferenceGraphicsAdvanced::draw()
{
    refreshMainResolutionStatus();
    LLFloater::draw();
}

void LLFloaterPreferenceGraphicsAdvanced::refreshMainResolutionStatus()
{
    // Rendering publishes the last successfully committed ordinary world view.
    // Reading this status must never prepare a scaler or alter saved settings.
    const auto& status = gPipeline.getMetalMainResolutionStatus();
    const auto choice = ALRenderResourceInputs::mainResolutionModeFromSetting(
        gSavedSettings.getS32("RenderMetalMainResolutionMode"));
    const S32 requested = static_cast<S32>(choice);
    const auto policy = ALRenderResourceInputs::mainResolutionPolicy(choice);
    std::string text;
    std::string tooltip;
    if (!status.known || status.requested != requested)
    {
        text = getString("main_resolution_pending");
    }
    else
    {
        LLStringUtil::format_map_t args;
        args["[SCALE]"] = std::to_string(policy.percentage());
        switch (status.effective)
        {
            case 0: args["[MODE]"] = getString("main_resolution_standard"); break;
            case 1: args["[MODE]"] = getString("main_resolution_linear", args); break;
            case 2: args["[MODE]"] = getString("main_resolution_metalfx", args); break;
            case 3: args["[MODE]"] = getString("main_resolution_temporal", args); break;
            case 4: args["[MODE]"] = getString("main_resolution_aurora", args); break;
            default:
                // An unrecognized producer state cannot establish an active mode.
                text = getString("main_resolution_pending");
                break;
        }
        if (text.empty())
        {
            if (status.effective != static_cast<S32>(policy.method))
            {
                args["[REASON]"] = status.reason.empty()
                    ? getString("main_resolution_unavailable") : status.reason;
                text = getString("main_resolution_fallback", args);
            }
            else
            {
                text = getString("main_resolution_active", args);
            }
            if (status.scene_width && status.scene_height && status.output_width && status.output_height)
            {
                args["[SCENE]"] = std::to_string(status.scene_width) + "×" + std::to_string(status.scene_height);
                args["[OUTPUT]"] = std::to_string(status.output_width) + "×" + std::to_string(status.output_height);
                text += "\n" + getString("main_resolution_dimensions", args);
                args["[BIAS]"] = llformat("%.2f", status.material_mip_bias);
                tooltip = text + "\n" + getString("main_resolution_mip_adjustment", args);
            }
        }
    }
    if (tooltip.empty()) tooltip = text;
    if (text != mMainResolutionStatus->getText())
    {
        mMainResolutionStatus->setText(text);
    }
    if (tooltip != mMainResolutionStatus->getToolTip())
        mMainResolutionStatus->setToolTip(tooltip);
}

void LLFloaterPreferenceGraphicsAdvanced::onOpen(const LLSD& key)
{
    refresh();
}

void LLFloaterPreferenceGraphicsAdvanced::onClickCloseBtn(bool app_quitting)
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->cancel({"RenderQualityPerformance"});
    }
    updateMaxComplexity();
}

void LLFloaterPreferenceGraphicsAdvanced::onRenderOptionEnable()
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->refresh();
    }

    refreshEnabledGraphics();
}

void LLFloaterPreferenceGraphicsAdvanced::refresh()
{
    refreshMainResolutionStatus();

    LLSliderCtrl* draw_distance = getChild<LLSliderCtrl>("DrawDistance");
    cap_draw_distance_control(draw_distance, draw_distance);

    // sliders and their text boxes
    //  mPostProcess = gSavedSettings.getS32("RenderGlowResolutionPow");
    // slider text boxes
    updateSliderText(getChild<LLSliderCtrl>("ObjectMeshDetail",     true), getChild<LLTextBox>("ObjectMeshDetailText",      true));
    updateSliderText(getChild<LLSliderCtrl>("FlexibleMeshDetail",   true), getChild<LLTextBox>("FlexibleMeshDetailText",    true));
    updateSliderText(getChild<LLSliderCtrl>("TreeMeshDetail",       true), getChild<LLTextBox>("TreeMeshDetailText",        true));
    updateSliderText(getChild<LLSliderCtrl>("AvatarMeshDetail",     true), getChild<LLTextBox>("AvatarMeshDetailText",      true));
    updateSliderText(getChild<LLSliderCtrl>("AvatarPhysicsDetail",  true), getChild<LLTextBox>("AvatarPhysicsDetailText",       true));
    updateSliderText(getChild<LLSliderCtrl>("TerrainMeshDetail",    true), getChild<LLTextBox>("TerrainMeshDetailText",     true));
    updateSliderText(getChild<LLSliderCtrl>("RenderPostProcess",    true), getChild<LLTextBox>("PostProcessText",           true));
    updateSliderText(getChild<LLSliderCtrl>("GlowResolution",        true), getChild<LLTextBox>("GlowResolutionText",         true));
    LLAvatarComplexityControls::setIndirectControls();
    setMaxNonImpostorsText(
        gSavedSettings.getU32("RenderAvatarMaxNonImpostors"),
        getChild<LLTextBox>("IndirectMaxNonImpostorsText", true));
    LLAvatarComplexityControls::setText(
        gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
    refreshEnabledState();

    bool enable_complexity = gSavedSettings.getS32("RenderAvatarComplexityMode") != LLVOAvatar::AV_RENDER_ONLY_SHOW_FRIENDS;
    getChild<LLSliderCtrl>("IndirectMaxComplexity")->setEnabled(enable_complexity);
    getChild<LLSliderCtrl>("IndirectMaxNonImpostors")->setEnabled(enable_complexity);
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledGraphics()
{
    refreshEnabledState();
}

void LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity()
{
    // Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(
        getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText"));
}

void LLFloaterPreferenceGraphicsAdvanced::updateComplexityMode(const LLSD& newvalue)
{
    bool enable_complexity = newvalue.asInteger() != LLVOAvatar::AV_RENDER_ONLY_SHOW_FRIENDS;
    getChild<LLSliderCtrl>("IndirectMaxComplexity")->setEnabled(enable_complexity);
    getChild<LLSliderCtrl>("IndirectMaxNonImpostors")->setEnabled(enable_complexity);
}

void LLFloaterPreferenceGraphicsAdvanced::updateComplexityText()
{
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
}

void LLFloaterPreferenceGraphicsAdvanced::updateObjectMeshDetailText()
{
    updateSliderText(getChild<LLSliderCtrl>("ObjectMeshDetail", true), getChild<LLTextBox>("ObjectMeshDetailText", true));
}

void LLFloaterPreferenceGraphicsAdvanced::updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box)
{
    if (text_box == NULL || ctrl== NULL)
        return;

    // get range and points when text should change
    F32 value = (F32)ctrl->getValue().asReal();
    F32 min = ctrl->getMinValue();
    F32 max = ctrl->getMaxValue();
    F32 range = max - min;
    llassert(range > 0);
    F32 midPoint = min + range / 3.0f;
    F32 highPoint = min + (2.0f * range / 3.0f);

    // choose the right text
    if (value < midPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityLow"));
    }
    else if (value < highPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityMid"));
    }
    else
    {
        text_box->setText(LLTrans::getString("GraphicsQualityHigh"));
    }
}

void LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors()
{
    // Called when the IndirectMaxNonImpostors control changes
    // Responsible for fixing the slider label (IndirectMaxNonImpostorsText) and setting RenderAvatarMaxNonImpostors
    LLSliderCtrl* ctrl = getChild<LLSliderCtrl>("IndirectMaxNonImpostors",true);
    U32 value = ctrl->getValue().asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
    LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
    setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreferenceGraphicsAdvanced::updateIndirectMaxNonImpostors(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();
    if ((value != 0) && (value != gSavedSettings.getU32("IndirectMaxNonImpostors")))
    {
        gSavedSettings.setU32("IndirectMaxNonImpostors", value);
    }
    setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreferenceGraphicsAdvanced::setMaxNonImpostorsText(U32 value, LLTextBox* text_box)
{
    if (0 == value)
    {
        text_box->setText(LLTrans::getString("no_limit"));
    }
    else
    {
        text_box->setText(llformat("%d", value));
    }
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledState()
{
    // Native SSAO, depth of field and shadows are independent quality
    // features. None depends on an optional GL shader or VBO mode.
    const bool ssao_available = LLFeatureManager::instance().isFeatureAvailable("RenderDeferredSSAO");
    LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
    ctrl_ssao->setEnabled(ssao_available);
    if (!ssao_available) ctrl_ssao->setValue(false);

    getChild<LLCheckBoxCtrl>("UseDoF")->setEnabled(true);

    const bool shadows_available = LLFeatureManager::instance().isFeatureAvailable("RenderShadowDetail");
    LLComboBox* ctrl_shadow = getChild<LLComboBox>("ShadowDetail");
    ctrl_shadow->setEnabled(shadows_available);
    getChild<LLTextBox>("RenderShadowDetailText")->setEnabled(shadows_available);
    if (!shadows_available) ctrl_shadow->setValue(0);

    static LLCachedControl<bool> is_not_vintage(gSavedSettings, "RenderDisableVintageMode");
    getChild<LLComboBox>("TonemapType")->setEnabled(is_not_vintage);
    getChild<LLTextBox>("TonemapTypeText")->setEnabled(is_not_vintage);
    getChild<LLSliderCtrl>("TonemapMix")->setEnabled(is_not_vintage);
    getChild<LLSliderCtrl>("RenderExposure")->setEnabled(is_not_vintage);
    getChild<LLSliderCtrl>("RenderSharpness")->setEnabled(is_not_vintage);

    LLComboBox* af_combo = getChild<LLComboBox>("anisotropic_filter");
    const F32 max_anisotropy = gRenderCapabilities.maxAnisotropy();
    af_combo->setEnabled(LLFeatureManager::instance().isFeatureAvailable("RenderAnisotropicLevel") &&
        gRenderCapabilities.hasAnisotropicFiltering() && max_anisotropy >= 2.f);
    if (2.f > max_anisotropy) af_combo->remove("2x");
    if (4.f > max_anisotropy) af_combo->remove("4x");
    if (8.f > max_anisotropy) af_combo->remove("8x");
    if (16.f > max_anisotropy) af_combo->remove("16x");
}

void LLFloaterPreferenceGraphicsAdvanced::onBtnOK(const LLSD& userdata)
{
    LLFloaterPreference* instance = LLFloaterReg::getTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->onBtnOK(userdata);
    }
}

void LLFloaterPreferenceGraphicsAdvanced::onBtnCancel(const LLSD& userdata)
{
    LLFloaterPreference* instance = LLFloaterReg::getTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->onBtnCancel(userdata);
    }
}
