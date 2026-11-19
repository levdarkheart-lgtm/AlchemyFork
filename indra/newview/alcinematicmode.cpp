/**
* @file alcinematicmode.cpp
* @brief Cinematic UI Mode
*
* $LicenseInfo:firstyear=2017&license=viewerlgpl$
* Copyright (C) 2017-2020 XenHat <me@xenh.at>
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
* $/LicenseInfo$
**/

#include "llviewerprecompiledheaders.h"

#include "alcinematicmode.h"

#include "llagentcamera.h"
#include "llchicletbar.h"
#include "llhudtext.h"
#include "llmoveview.h"
#include "llviewerwindow.h"

bool ALCinematicMode::_enabled = false;
bool ALCinematicMode::_restoreUI = true;
bool ALCinematicMode::_restoreChiclets = true;
bool ALCinematicMode::_restoreMovement = false;

// static
void ALCinematicMode::toggle()
{
    if (gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK) return;

    LL_INFOS() << "Toggling Cinematic Mode" << LL_ENDL;
    if (!_enabled)
    {
        // The current UI owner handles both navigation-bar styles and floaters.
        // Restore prior visibility on exit instead of enabling hidden controls.
        _restoreUI = gViewerWindow->getUIVisibility();
        _restoreChiclets = LLChicletBar::getInstance()->getVisible();
        _restoreMovement = LLPanelStandStopFlying::getInstance()->getVisible();
        _enabled = true;
        gViewerWindow->setUIVisibility(false);
        LLChicletBar::getInstance()->setVisible(false);
        LLPanelStandStopFlying::getInstance()->setVisible(false);
    }
    else
    {
        _enabled = false;
        gViewerWindow->setUIVisibility(_restoreUI);
        LLChicletBar::getInstance()->setVisible(_restoreChiclets);
        LLPanelStandStopFlying::getInstance()->setVisible(_restoreMovement);
    }
    LLHUDText::refreshAllObjectText();
}
