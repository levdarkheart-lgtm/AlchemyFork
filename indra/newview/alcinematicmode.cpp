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

// static
void ALCinematicMode::toggle()
{
    if (gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK) return;

    LL_INFOS() << "Toggling Cinematic Mode" << LL_ENDL;
    if (!_enabled)
    {
        _enabled = true;
        gViewerWindow->setUIVisibility(false);
        LLChicletBar::getInstance()->setVisible(false);
        LLPanelStandStopFlying::getInstance()->setVisible(false);
    }
    else
    {
        _enabled = false;
        // Preserve the archived visible-on-exit contract. Movement/IM state
        // can change while hidden; the current owners expose that live state.
        gViewerWindow->setUIVisibility(true);
        LLChicletBar::getInstance()->setVisible(true);
        LLPanelStandStopFlying::getInstance()->setVisible(true);
    }
    LLHUDText::refreshAllObjectText();
}
