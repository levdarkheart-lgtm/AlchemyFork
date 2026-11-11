/**
 * @file llfloaterbvhpreview.cpp
 * @brief LLFloaterBvhPreview class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#include "llfloaterbvhpreview.h"

#include "llbvhloader.h"
#include "llbvhconsts.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llnotificationsutil.h"
#include "llfilesystem.h"
#include "llstring.h"

#include "llagent.h"
#include "llagentbenefits.h"
#include "llanimationstates.h"
#include "llbbox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llrender.h"
#include "llface.h"
#include "llfocusmgr.h"
#include "llkeyframemotion.h"
#include "lllineeditor.h"
#include "llfloaterperms.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerjointmesh.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llviewermenufile.h"   // upload_new_resource()
#include "llvoavatar.h"
#include "noise.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "lltrans.h"

#if LL_DARWIN
#include "../almetal/almetalsculptpreview.h"
#endif

#include <algorithm>
#include <array>
#include <vector>

namespace
{
#if LL_DARWIN
class LLScopedAnimationPreviewCameraState final
{
public:
    LLScopedAnimationPreviewCameraState()
    : mCamera(*LLViewerCamera::getInstance()),
      mCurrent(LLViewerCamera::getCurrent()),
      mViewport{{
          gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3],
      }}
    {
    }

    ~LLScopedAnimationPreviewCameraState()
    {
        LLViewerCamera* camera = LLViewerCamera::getInstance();
        camera->setOrigin(mCamera);
        camera->setAxes(mCamera);
        camera->setAspect(mCamera.getAspect());
        camera->setViewNoBroadcast(mCamera.getView());
        camera->setNear(mCamera.getNear());
        camera->setFar(mCamera.getFar());
        camera->setModelview(mCamera.getModelview());
        camera->setProjection(mCamera.getProjection());
        LLViewerCamera::updateFrustumPlanes(*camera);
        LLViewerCamera::setCurrent(mCurrent);
        std::copy(mViewport.begin(), mViewport.end(), gGLViewport);
    }

private:
    LLCamera mCamera;
    LLCamera mCurrent;
    std::array<S32, 4> mViewport;
};
#endif
}

const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
const S32 PREVIEW_VPAD = 35;
const S32 PREF_BUTTON_HEIGHT = 16 + 35;
const S32 PREVIEW_TEXTURE_HEIGHT = 300;

const F32 PREVIEW_CAMERA_DISTANCE = 4.f;

const F32 MIN_CAMERA_ZOOM = 0.5f;
const F32 MAX_CAMERA_ZOOM = 10.f;

const F32 BASE_ANIM_TIME_OFFSET = 5.f;

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::LLFloaterBvhPreview(const LLSD& args) :
    LLFloaterNameDesc(args)
{
    mLastMouseX = 0;
    mLastMouseY = 0;

    mIDList["Standing"] = ANIM_AGENT_STAND;
    mIDList["Walking"] = ANIM_AGENT_FEMALE_WALK;
    mIDList["Sitting"] = ANIM_AGENT_SIT_FEMALE;
    mIDList["Flying"] = ANIM_AGENT_HOVER;

    mIDList["[None]"] = LLUUID::null;
    mIDList["Aaaaah"] = ANIM_AGENT_EXPRESS_OPEN_MOUTH;
    mIDList["Afraid"] = ANIM_AGENT_EXPRESS_AFRAID;
    mIDList["Angry"] = ANIM_AGENT_EXPRESS_ANGER;
    mIDList["Big Smile"] = ANIM_AGENT_EXPRESS_TOOTHSMILE;
    mIDList["Bored"] = ANIM_AGENT_EXPRESS_BORED;
    mIDList["Cry"] = ANIM_AGENT_EXPRESS_CRY;
    mIDList["Disdain"] = ANIM_AGENT_EXPRESS_DISDAIN;
    mIDList["Embarrassed"] = ANIM_AGENT_EXPRESS_EMBARRASSED;
    mIDList["Frown"] = ANIM_AGENT_EXPRESS_FROWN;
    mIDList["Kiss"] = ANIM_AGENT_EXPRESS_KISS;
    mIDList["Laugh"] = ANIM_AGENT_EXPRESS_LAUGH;
    mIDList["Plllppt"] = ANIM_AGENT_EXPRESS_TONGUE_OUT;
    mIDList["Repulsed"] = ANIM_AGENT_EXPRESS_REPULSED;
    mIDList["Sad"] = ANIM_AGENT_EXPRESS_SAD;
    mIDList["Shrug"] = ANIM_AGENT_EXPRESS_SHRUG;
    mIDList["Smile"] = ANIM_AGENT_EXPRESS_SMILE;
    mIDList["Surprise"] = ANIM_AGENT_EXPRESS_SURPRISE;
    mIDList["Wink"] = ANIM_AGENT_EXPRESS_WINK;
    mIDList["Worry"] = ANIM_AGENT_EXPRESS_WORRY;
}

//-----------------------------------------------------------------------------
// setAnimCallbacks()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::setAnimCallbacks()
{
    getChild<LLUICtrl>("playback_slider")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onSliderMove, this));

    getChild<LLUICtrl>("preview_base_anim")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitBaseAnim, this));
    getChild<LLUICtrl>("preview_base_anim")->setValue("Standing");

    getChild<LLUICtrl>("priority")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitPriority, this));
    getChild<LLUICtrl>("loop_check")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoop, this));
    getChild<LLUICtrl>("loop_in_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopIn, this));
    getChild<LLUICtrl>("loop_in_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopIn, this, _1));
    getChild<LLUICtrl>("loop_out_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopOut, this));
    getChild<LLUICtrl>("loop_out_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopOut, this, _1));

    getChild<LLUICtrl>("hand_pose_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitHandPose, this));

    getChild<LLUICtrl>("emote_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEmote, this));
    getChild<LLUICtrl>("emote_combo")->setValue("[None]");

    getChild<LLUICtrl>("ease_in_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseIn, this));
    getChild<LLUICtrl>("ease_in_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseIn, this, _1));
    getChild<LLUICtrl>("ease_out_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseOut, this));
    getChild<LLUICtrl>("ease_out_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseOut, this, _1));
}

std::map<std::string, std::string, std::less<>> LLFloaterBvhPreview::getJointAliases()
{
    LLPointer<LLVOAvatar> av = (LLVOAvatar*)mAnimPreview->getDummyAvatar();
    return av->getJointAliases();
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::postBuild()
{
    LLKeyframeMotion* motionp = NULL;
    LLBVHLoader* loaderp = NULL;

    if (!LLFloaterNameDesc::postBuild())
    {
        return false;
    }

    getChild<LLUICtrl>("name_form")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitName, this));

    childSetAction("ok_btn", onBtnOK, this);
    setDefaultBtn();

    mPreviewRect.set(PREVIEW_HPAD,
        PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD,
        getRect().getWidth() - PREVIEW_HPAD,
        PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
    mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);

    mPlayButton = getChild<LLButton>( "play_btn");
    mPlayButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnPlay, this));
    mPlayButton->setVisible(true);

    mPauseButton = getChild<LLButton>( "pause_btn");
    mPauseButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnPause, this));
    mPauseButton->setVisible(false);

    mStopButton = getChild<LLButton>( "stop_btn");
    mStopButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnStop, this));

    getChildView("bad_animation_text")->setVisible(false);

    mAnimPreview = new LLPreviewAnimation(256, 256);

    std::string exten = gDirUtilp->getExtension(mFilename);
    if (exten == "bvh")
    {
        // loading a bvh file

        // now load bvh file
        S64 file_size;

        std::error_code ec;
        LLFile infile ;
        infile.open(mFilenameAndPath, LLFile::in|LLFile::binary, ec);

        if (!infile || ec)
        {
            LL_WARNS() << "Can't open BVH file:" << mFilename << LL_ENDL;
        }
        else
        {
            file_size = infile.size(ec);
            if (file_size > 0 )
            {
                std::unique_ptr<char[]> file_buffer = std::make_unique<char[]>(file_size + 1);
                if (file_size == infile.read(file_buffer.get(), file_size, ec) && !ec)
                {
                    file_buffer[file_size] = '\0';
                    LL_INFOS() << "Loading BVH file " << mFilename << LL_ENDL;
                    ELoadStatus load_status = E_ST_OK;
                    S32 line_number = 0;

                    auto joint_alias_map = getJointAliases();

                    loaderp = new LLBVHLoader(file_buffer.get(), load_status, line_number, joint_alias_map);
                    std::string status = getString(BVHSTATUS[load_status]);

                    if (load_status == E_ST_NO_XLT_FILE)
                    {
                        LL_WARNS() << "NOTE: No translation table found." << LL_ENDL;
                    }
                    else
                    {
                        LL_WARNS() << "ERROR: [line: " << line_number << "] " << status << LL_ENDL;
                    }
                }
            }

            infile.close();
        }
    }

    if (loaderp && loaderp->isInitialized() && loaderp->getDuration() <= MAX_ANIM_DURATION)
    {
        // generate unique id for this motion
        mTransactionID.generate();
        mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

        // motion will be returned, but it will be in a load-pending state, as this is a new motion
        // this motion will not request an asset transfer until next update, so we have a chance to
        // load the keyframe data locally
        motionp = (LLKeyframeMotion*)mAnimPreview->getDummyAvatar()->createMotion(mMotionID);

        // create data buffer for keyframe initialization
        S32 buffer_size = loaderp->getOutputSize();
        U8* buffer = new(std::nothrow) U8[buffer_size];
        if (!buffer)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            LL_ERRS() << "Bad memory allocation for buffer, size: " << buffer_size << LL_ENDL;
        }

        LLDataPackerBinaryBuffer dp(buffer, buffer_size);

        // pass animation data through memory buffer
        LL_INFOS("BVH") << "Serializing loaderp" << LL_ENDL;
        loaderp->serialize(dp);
        dp.reset();
        LL_INFOS("BVH") << "Deserializing motionp" << LL_ENDL;
        bool success = motionp && motionp->deserialize(dp, mMotionID, false);
        LL_INFOS("BVH") << "Done" << LL_ENDL;

        delete []buffer;

        if (success)
        {
            setAnimCallbacks() ;

            const LLBBoxLocal &pelvis_bbox = motionp->getPelvisBBox();

            LLVector3 temp = pelvis_bbox.getCenter();
            // only consider XY?
            //temp.mV[VZ] = 0.f;
            F32 pelvis_offset = temp.magVec();

            temp = pelvis_bbox.getExtent();
            //temp.mV[VZ] = 0.f;
            F32 pelvis_max_displacement = pelvis_offset + (temp.magVec() * 0.5f) + 1.f;

            F32 camera_zoom = LLViewerCamera::getInstance()->getDefaultFOV() / (2.f * atan(pelvis_max_displacement / PREVIEW_CAMERA_DISTANCE));

            mAnimPreview->setZoom(camera_zoom);

            motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
            mAnimPreview->getDummyAvatar()->startMotion(mMotionID);

            getChild<LLSlider>("playback_slider")->setMinValue(0.0);
            getChild<LLSlider>("playback_slider")->setMaxValue(1.0);

            getChild<LLUICtrl>("loop_check")->setValue(LLSD(motionp->getLoop()));
            getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(motionp->getLoopIn() / motionp->getDuration() * 100.f));
            getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(motionp->getLoopOut() / motionp->getDuration() * 100.f));
            getChild<LLUICtrl>("priority")->setValue(LLSD((F32)motionp->getPriority()));
            getChild<LLUICtrl>("hand_pose_combo")->setValue(LLHandMotion::getHandPoseName(motionp->getHandPose()));
            getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(motionp->getEaseInDuration()));
            getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(motionp->getEaseOutDuration()));
            setEnabled(true);
            std::string seconds_string;
            seconds_string = llformat(" - %.2f seconds", motionp->getDuration());

            setTitle(mFilename + std::string(seconds_string));
        }
        else
        {
            mAnimPreview = NULL;
            mMotionID.setNull();
            getChild<LLUICtrl>("bad_animation_text")->setValue(getString("failed_to_initialize"));
        }
    }
    else
    {
        if ( loaderp )
        {
            if (loaderp->getDuration() > MAX_ANIM_DURATION)
            {
                LLUIString out_str = getString("anim_too_long");
                out_str.setArg("[LENGTH]", llformat("%.1f", loaderp->getDuration()));
                out_str.setArg("[MAX_LENGTH]", llformat("%.1f", MAX_ANIM_DURATION));
                getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
            }
            else
            {
                LLUIString out_str = getString("failed_file_read");
                out_str.setArg("[STATUS]", getString(BVHSTATUS[loaderp->getStatus()]));
                getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
            }
        }

        //setEnabled(false);
        mMotionID.setNull();
        mAnimPreview = NULL;
    }

    refresh();

    delete loaderp;

    return true;
}

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::~LLFloaterBvhPreview()
{
    mAnimPreview = NULL;

    setEnabled(false);
}

//-----------------------------------------------------------------------------
// draw()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::draw()
{
    LLFloater::draw();
    LLRect r = getRect();

    refresh();

    if (mMotionID.notNull() && mAnimPreview)
    {
        gGL.color3f(1.f, 1.f, 1.f);

        gGL.getTextureSlot(0)->bindSampled(mAnimPreview, ALSamplers::AnisoWrap);

        gGL.begin(LLRender::TRIANGLES);
        {
            gGL.texCoord2f(0.f, 1.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
            gGL.texCoord2f(0.f, 0.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
            gGL.texCoord2f(1.f, 0.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);

            gGL.texCoord2f(0.f, 1.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
            gGL.texCoord2f(1.f, 0.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
            gGL.texCoord2f(1.f, 1.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
        }
        gGL.end();

        gGL.getTextureSlot(0)->unbind();

        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        if (!avatarp->areAnimationsPaused())
        {
            mAnimPreview->requestUpdate();
        }
    }
}

//-----------------------------------------------------------------------------
// resetMotion()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::resetMotion()
{
    if (!mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    bool paused = avatarp->areAnimationsPaused();

    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    if( motionp )
    {
        // Set emotion
        std::string emote = getChild<LLUICtrl>("emote_combo")->getValue().asString();
        motionp->setEmote(mIDList[emote]);
    }

    LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
    avatarp->deactivateAllMotions();
    avatarp->startMotion(mMotionID, 0.0f);
    avatarp->startMotion(base_id, BASE_ANIM_TIME_OFFSET);
    getChild<LLUICtrl>("playback_slider")->setValue(0.0f);

    // Set pose
    std::string handpose = getChild<LLUICtrl>("hand_pose_combo")->getValue().asString();
    avatarp->startMotion( ANIM_AGENT_HAND_MOTION, 0.0f );

    if( motionp )
    {
        motionp->setHandPose(LLHandMotion::getHandPose(handpose));
    }

    if (paused)
    {
        mPauseRequest = avatarp->requestPause();
    }
    else
    {
        mPauseRequest = NULL;
    }
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (mPreviewRect.pointInRect(x, y))
    {
        bringToFront( x, y );
        gFocusMgr.setMouseCapture(this);
        gViewerWindow->hideCursor();
        mLastMouseX = x;
        mLastMouseY = y;
        return true;
    }

    return LLFloater::handleMouseDown(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleMouseUp()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
    gFocusMgr.setMouseCapture(nullptr);
    gViewerWindow->showCursor();
    return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleHover(S32 x, S32 y, MASK mask)
{
    MASK local_mask = mask & ~MASK_ALT;

    if (mAnimPreview && hasMouseCapture())
    {
        if (local_mask == MASK_PAN)
        {
            // pan here
            mAnimPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
        }
        else if (local_mask == MASK_ORBIT)
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

            mAnimPreview->rotate(yaw_radians, pitch_radians);
        }
        else
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

            mAnimPreview->rotate(yaw_radians, 0.f);
            mAnimPreview->zoom(zoom_amt);
        }

        mAnimPreview->requestUpdate();

        LLUI::getInstance()->setMousePositionLocal(this, mLastMouseX, mLastMouseY);
    }

    if (!mPreviewRect.pointInRect(x, y) || !mAnimPreview)
    {
        return LLFloater::handleHover(x, y, mask);
    }
    else if (local_mask == MASK_ORBIT)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
    }
    else if (local_mask == MASK_PAN)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
    }
    else
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
    }

    return true;
}

//-----------------------------------------------------------------------------
// handleScrollWheel()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (!mAnimPreview)
        return false;

    mAnimPreview->zoom((F32)delta.mPrecise * -0.2f);
    mAnimPreview->requestUpdate();

    return true;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onMouseCaptureLost()
{
    gViewerWindow->showCursor();
}

//-----------------------------------------------------------------------------
// onBtnPlay()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnPlay()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();

        if (!avatarp->isMotionActive(mMotionID))
        {
            resetMotion();
            mPauseRequest = NULL;
        }
        else if (avatarp->areAnimationsPaused())
        {
            mPauseRequest = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// onBtnPause()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnPause()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();

        if (avatarp->isMotionActive(mMotionID))
        {
            if (!avatarp->areAnimationsPaused())
            {
                mPauseRequest = avatarp->requestPause();
            }
        }
    }
}

//-----------------------------------------------------------------------------
// onBtnStop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnStop()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        resetMotion();
        mPauseRequest = avatarp->requestPause();
    }
}

//-----------------------------------------------------------------------------
// onSliderMove()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onSliderMove()
{
    if (!getEnabled())
        return;

    if (mAnimPreview)
    {
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        F32 slider_value = (F32)getChild<LLUICtrl>("playback_slider")->getValue().asReal();
        LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
        LLMotion* motionp = avatarp->findMotion(mMotionID);
        F32 duration = motionp->getDuration();// + motionp->getEaseOutDuration();
        F32 delta_time = duration * slider_value;
        avatarp->deactivateAllMotions();
        avatarp->startMotion(base_id, delta_time + BASE_ANIM_TIME_OFFSET);
        avatarp->startMotion(mMotionID, delta_time);
        mPauseRequest = avatarp->requestPause();
        refresh();
    }

}

//-----------------------------------------------------------------------------
// onCommitBaseAnim()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitBaseAnim()
{
    if (!getEnabled())
        return;

    if (mAnimPreview)
    {
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();

        bool paused = avatarp->areAnimationsPaused();

        // stop all other possible base motions
        avatarp->stopMotion(mIDList["Standing"], true);
        avatarp->stopMotion(mIDList["Walking"], true);
        avatarp->stopMotion(mIDList["Sitting"], true);
        avatarp->stopMotion(mIDList["Flying"], true);

        resetMotion();

        if (!paused)
        {
            mPauseRequest = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// onCommitLoop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoop()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (motionp)
    {
        motionp->setLoop(getChild<LLUICtrl>("loop_check")->getValue().asBoolean());
        motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() * 0.01f * motionp->getDuration());
        motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
    }
}

//-----------------------------------------------------------------------------
// onCommitLoopIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopIn()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (motionp)
    {
        motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f);
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        onCommitLoop();
    }
}

//-----------------------------------------------------------------------------
// onCommitLoopOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopOut()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (motionp)
    {
        motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        onCommitLoop();
    }
}

//-----------------------------------------------------------------------------
// onCommitName()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitName()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (motionp)
    {
        motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
    }

    doCommit();
}

//-----------------------------------------------------------------------------
// onCommitHandPose()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitHandPose()
{
    if (!getEnabled())
        return;

    resetMotion(); // sets hand pose
}

//-----------------------------------------------------------------------------
// onCommitEmote()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEmote()
{
    if (!getEnabled())
        return;

    resetMotion(); // ssts emote
}

//-----------------------------------------------------------------------------
// onCommitPriority()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitPriority()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    motionp->setPriority(llfloor((F32)getChild<LLUICtrl>("priority")->getValue().asReal()));
}

//-----------------------------------------------------------------------------
// onCommitEaseIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseIn()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    motionp->setEaseIn((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal());
    resetMotion();
}

//-----------------------------------------------------------------------------
// onCommitEaseOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseOut()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    motionp->setEaseOut((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal());
    resetMotion();
}

//-----------------------------------------------------------------------------
// validateEaseIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseIn(const LLSD& data)
{
    if (!getEnabled() || !mAnimPreview)
        return false;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (!motionp->getLoop())
    {
        F32 new_ease_in = llclamp((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseOutDuration());
        getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(new_ease_in));
    }

    return true;
}

//-----------------------------------------------------------------------------
// validateEaseOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseOut(const LLSD& data)
{
    if (!getEnabled() || !mAnimPreview)
        return false;

    LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

    if (!motionp->getLoop())
    {
        F32 new_ease_out = llclamp((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseInDuration());
        getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(new_ease_out));
    }

    return true;
}

//-----------------------------------------------------------------------------
// validateLoopIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopIn(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();
    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();

    if (loop_in_value < 0.f)
    {
        loop_in_value = 0.f;
    }
    else if (loop_in_value > 100.f)
    {
        loop_in_value = 100.f;
    }
    else if (loop_in_value > loop_out_value)
    {
        loop_in_value = loop_out_value;
    }

    getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(loop_in_value));
    return true;
}

//-----------------------------------------------------------------------------
// validateLoopOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopOut(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();
    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();

    if (loop_out_value < 0.f)
    {
        loop_out_value = 0.f;
    }
    else if (loop_out_value > 100.f)
    {
        loop_out_value = 100.f;
    }
    else if (loop_out_value < loop_in_value)
    {
        loop_out_value = loop_in_value;
    }

    getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(loop_out_value));
    return true;
}


//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::refresh()
{
    // Are we showing the play button (default) or the pause button?
    bool show_play = true;
    if (!mAnimPreview)
    {
        getChildView("bad_animation_text")->setVisible(true);
        // play button visible but disabled
        mPlayButton->setEnabled(false);
        mStopButton->setEnabled(false);
        getChildView("ok_btn")->setEnabled(false);
    }
    else
    {
        getChildView("bad_animation_text")->setVisible(false);
        // re-enabled in case previous animation was bad
        mPlayButton->setEnabled(true);
        mStopButton->setEnabled(true);
        LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        if (avatarp->isMotionActive(mMotionID))
        {
            mStopButton->setEnabled(true);
            LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
            if (!avatarp->areAnimationsPaused())
            {
                // animation is playing
                if (motionp)
                {
                    F32 fraction_complete = motionp->getLastUpdateTime() / motionp->getDuration();
                    getChild<LLUICtrl>("playback_slider")->setValue(fraction_complete);
                }
                show_play = false;
            }
        }
        else
        {
            // Motion just finished playing
            mPauseRequest = avatarp->requestPause();
        }
        getChildView("ok_btn")->setEnabled(true);
        mAnimPreview->requestUpdate();
    }
    mPlayButton->setVisible(show_play);
    mPauseButton->setVisible(!show_play);
}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnOK(void* userdata)
{
    LLFloaterBvhPreview* floaterp = (LLFloaterBvhPreview*)userdata;
    if (!floaterp->getEnabled()) return;

    if (floaterp->mAnimPreview)
    {
        LLKeyframeMotion* motionp = (LLKeyframeMotion*)floaterp->mAnimPreview->getDummyAvatar()->findMotion(floaterp->mMotionID);

        S32 file_size = motionp->getFileSize();
        U8* buffer = new(std::nothrow) U8[file_size];
        if (!buffer)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            LL_ERRS() << "Bad memory allocation for buffer, size: " << file_size << LL_ENDL;
        }

        LLDataPackerBinaryBuffer dp(buffer, file_size);
        if (motionp->serialize(dp))
        {
            LLFileSystem file(motionp->getID(), LLAssetType::AT_ANIMATION, LLFileSystem::APPEND);

            S32 size = dp.getCurrentSize();
            if (file.write((U8*)buffer, size))
            {
                std::string name = floaterp->getChild<LLUICtrl>("name_form")->getValue().asString();
                std::string desc = floaterp->getChild<LLUICtrl>("description_form")->getValue().asString();
                S32 expected_upload_cost = LLAgentBenefitsMgr::current().getAnimationUploadCost();

                LLResourceUploadInfo::ptr_t assetUploadInfo = std::make_shared<LLResourceUploadInfo>(
                    floaterp->mTransactionID, LLAssetType::AT_ANIMATION,
                    name, desc, 0,
                    LLFolderType::FT_NONE, LLInventoryType::IT_ANIMATION,
                    LLFloaterPerms::getNextOwnerPerms("Uploads"),
                    LLFloaterPerms::getGroupPerms("Uploads"),
                    LLFloaterPerms::getEveryonePerms("Uploads"),
                    expected_upload_cost,
                    floaterp->mDestinationFolderId);

                upload_new_resource(assetUploadInfo);
            }
            else
            {
                LL_WARNS() << "Failure writing animation data." << LL_ENDL;
                LLNotificationsUtil::add("WriteAnimationFail");
            }
        }

        delete [] buffer;
        // clear out cache for motion data
        floaterp->mAnimPreview->getDummyAvatar()->removeMotion(floaterp->mMotionID);
        LLKeyframeDataCache::removeKeyframeData(floaterp->mMotionID);
    }

    floaterp->closeFloater(false);
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation
//-----------------------------------------------------------------------------
LLPreviewAnimation::LLPreviewAnimation(S32 width, S32 height)
: LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE)
{
    mNeedsUpdate = true;
    mCameraDistance = PREVIEW_CAMERA_DISTANCE;
    mCameraYaw = 0.f;
    mCameraPitch = 0.f;
    mCameraZoom = 1.f;

    mDummyAvatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, gAgent.getRegion(), LLViewerObject::CO_FLAG_UI_AVATAR);
    mDummyAvatar->mSpecialRenderMode = 1;
    mDummyAvatar->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);

    // on idle overall apperance update will set skirt to visible, so either
    // call early or account for mSpecialRenderMode in updateMeshVisibility
    mDummyAvatar->updateOverallAppearance();
    mDummyAvatar->hideHair();
    mDummyAvatar->hideSkirt();

    // stop extraneous animations
    mDummyAvatar->stopMotion( ANIM_AGENT_HEAD_ROT, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_EYE, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_BODY_NOISE, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_BREATHE_ROT, true );
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation()
//-----------------------------------------------------------------------------
LLPreviewAnimation::~LLPreviewAnimation()
{
    mDummyAvatar->markDead();
}

//virtual
S8 LLPreviewAnimation::getType() const
{
    return LLViewerDynamicTexture::LL_PREVIEW_ANIMATION ;
}

#include "animation_preview_native.inl"

//-----------------------------------------------------------------------------
// update()
//-----------------------------------------------------------------------------
bool    LLPreviewAnimation::render()
{
#if !LL_DARWIN
    return false;
#else
    mMetalPreview.reset();
    mMetalPreviewDescriptor.reset();
    if (mGLTexturep.notNull() && mGLTexturep->getMetalTexture())
    {
        mMetalPreviewDescriptor = std::make_unique<ALMetalTextureDescriptor>(
            mGLTexturep->getMetalTexture()->descriptor());
    }
    LLVOAvatar* avatarp = mDummyAvatar;
    if (!avatarp || !mMetalPreviewDescriptor)
    {
        return false;
    }

    LLVector3 target_pos = avatarp->mRoot->getWorldPosition();

    LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
        LLQuaternion(mCameraYaw, LLVector3::z_axis);

    LLViewerCamera* camera = LLViewerCamera::getInstance();

    LLQuaternion av_rot = avatarp->mRoot->getWorldRotation() * camera_rot;
    camera->lookAt(
        target_pos + ((LLVector3(mCameraDistance, 0.f, 0.f) + mCameraOffset) * av_rot),     // camera
        target_pos + (mCameraOffset  * av_rot),                                             // point of interest
        LLVector3::z_axis);                                                                 // up

    camera->setViewNoBroadcast(LLViewerCamera::getInstance()->getDefaultFOV() / mCameraZoom);
    camera->setAspect((F32) mFullWidth / (F32) mFullHeight);
    camera->setPerspectiveWithoutRenderState(
        mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);

    //SJB: Animation is updated in LLVOAvatar::updateCharacter

    if (avatarp->mDrawable.notNull())
    {
        // Animation advances in LLVOAvatar::updateCharacter. Rebuild the
        // corresponding dynamic mesh data before native extraction, matching
        // the work the former renderAvatars() call performed every frame.
        avatarp->updateLOD();
        if (!avatarp->isBuilt())
        {
            return false;
        }
        avatarp->dirtyMesh();
        avatarp->updateMeshData();

        LLFace* face = avatarp->mDrawable->getFace(0);
        if (face)
        {
            ALMetalDevice* metal_device = getViewerMetalDevice();
            LLPointer<LLViewerFetchedTexture> default_texture =
                LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT);
            LLImageGL* default_image = default_texture.notNull()
                ? default_texture->getGLTexture() : nullptr;
            const ALMetalTexture* metal_texture = default_image
                ? default_image->getMetalTexture() : nullptr;
            if (metal_device && mMetalPreviewDescriptor && metal_texture)
            {
                static ALMetalDevice* renderer_device = nullptr;
                static std::unique_ptr<ALMetalLitPreviewRenderer> renderer;
                std::string error;
                if (renderer_device != metal_device)
                {
                    renderer.reset();
                    renderer_device = metal_device;
                }
                if (!renderer)
                {
                    renderer = ALMetalLitPreviewRenderer::create(
                        *metal_device, &error);
                }

                using namespace LLAvatarAppearanceDefines;
                const std::array<EMeshIndex, 6> required_meshes{{
                    MESH_ID_EYEBALL_LEFT,
                    MESH_ID_EYEBALL_RIGHT,
                    MESH_ID_HEAD,
                    MESH_ID_UPPER_BODY,
                    MESH_ID_LOWER_BODY,
                    MESH_ID_EYELASH,
                }};
                std::vector<LLViewerJointMesh*> preview_meshes;
                preview_meshes.reserve(required_meshes.size());
                bool complete = renderer != nullptr;
                for (EMeshIndex mesh_index : required_meshes)
                {
                    if (mesh_index == MESH_ID_EYELASH &&
                        !avatarp->isTextureVisible(TEX_HEAD_BAKED))
                    {
                        continue;
                    }
                    const auto* entry = LLAvatarAppearance::getDictionary()
                        ->getMeshEntry(mesh_index);
                    auto* joint = entry
                        ? dynamic_cast<LLAvatarJoint*>(
                            avatarp->mRoot->findJoint(std::string(entry->mName)))
                        : nullptr;
                    std::size_t active_parts = 0;
                    if (joint)
                    {
                        for (LLAvatarJointMesh* part : joint->mMeshParts)
                        {
                            auto* viewer_mesh =
                                dynamic_cast<LLViewerJointMesh*>(part);
                            LLFace* preview_face = nullptr;
                            LLPolyMesh* preview_mesh = nullptr;
                            if (viewer_mesh && viewer_mesh->getPreviewGeometry(
                                                   preview_face, preview_mesh))
                            {
                                preview_meshes.push_back(viewer_mesh);
                                ++active_parts;
                            }
                        }
                    }
                    complete = complete && active_parts == 1;
                }

                constexpr std::size_t AVATAR_PALETTE_FLOATS =
                    LL_CHARACTER_MAX_JOINTS_PER_MESH * 3 * 4;
                std::vector<std::array<F32, AVATAR_PALETTE_FLOATS>> palettes;
                std::vector<ALMetalLitPreviewBatch> batches;
                palettes.reserve(preview_meshes.size());
                batches.reserve(preview_meshes.size());

                const LLMatrix4a modelview = camera->getModelview();
                const LLMatrix4a projection = camera->getProjection();
                LLVector4 light(LLPipeline::PreviewDirection0, 0.f);
                LLVector4a source;
                source.loadua(light.mV);
                LLVector4a transformed;
                modelview.transform4(source, transformed);
                LLVector3 light_direction(transformed.getF32ptr());
                light_direction.normVec();
                const LLColor4 dummy_color = LLVOAvatar::getDummyColor();
                const bool avatar_cloth =
                    gPipeline.shaderInputs().mLocal.mAvatarCloth;
                LLVector4 cloth_wind;
                LLVector4 cloth_sin_wave;
                LLVector4 cloth_gravity;
                LLVector4 cloth_depth(0.f, 0.f, 1.f, 0.f);
                if (avatar_cloth)
                {
                    // Match LLDrawPoolAvatar::avatar_cloth_uniforms(), which
                    // supplied the retained animation-preview draw.
                    const LLMatrix4& cloth_view =
                        LLDrawPoolAvatar::getModelView();
                    cloth_wind.setVec(avatarp->mWindVec);
                    cloth_wind.mV[VW] = 0.f;
                    cloth_wind = cloth_wind * cloth_view;
                    cloth_wind.mV[VW] = avatarp->mWindVec.mV[VW];

                    const F32 phase = -avatarp->mRipplePhase;
                    const F32 frequency =
                        7.f + noise1(avatarp->mRipplePhase) * 2.f;
                    cloth_sin_wave.set(
                        frequency, frequency, frequency, phase);

                    cloth_gravity.set(0.f, 0.f, -0.7f, 0.f);
                    cloth_gravity = cloth_gravity * cloth_view;

                    LLViewerCamera* main_camera =
                        LLViewerCamera::getInstance();
                    if (&LLViewerCamera::getCurrent() != main_camera)
                    {
                        LLMatrix4 main_view;
                        main_camera->getModelview().store(main_view);
                        LLMatrix4 to_main = cloth_view;
                        to_main.invert();
                        to_main *= main_view;
                        cloth_depth.set(
                            to_main.mMatrix[0][2], to_main.mMatrix[1][2],
                            to_main.mMatrix[2][2], to_main.mMatrix[3][2]);
                    }
                }

                for (LLViewerJointMesh* viewer_mesh : preview_meshes)
                {
                    LLFace* preview_face = nullptr;
                    LLPolyMesh* preview_mesh = nullptr;
                    complete = complete && viewer_mesh->getPreviewGeometry(
                        preview_face, preview_mesh);
                    LLVertexBuffer* buffer = preview_face
                        ? preview_face->getVertexBuffer() : nullptr;
                    complete = complete && buffer && preview_mesh &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0);
                    if (complete)
                    {
                        buffer->setBuffer();
                        complete = buffer->hasPublishedMetalVertexBuffer() &&
                            buffer->hasPublishedMetalIndexBuffer();
                    }
                    if (!complete)
                    {
                        break;
                    }

                    ALMetalLitPreviewBatch batch;
                    batch.vertices = buffer->getMetalVertexBuffer();
                    batch.position_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_VERTEX);
                    batch.normal_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_NORMAL);
                    batch.texcoord_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_TEXCOORD0);
                    batch.vertex_count = buffer->getNumVerts();
                    batch.indices = buffer->getMetalIndexBuffer();
                    batch.index_type = buffer->getIndexStride() == 4
                        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
                    batch.index_offset = static_cast<std::size_t>(
                        preview_mesh->mFaceIndexOffset) *
                        buffer->getIndexStride();
                    batch.index_count = preview_mesh->mFaceIndexCount;
                    batch.texture = metal_texture;
                    batch.destination_descriptor =
                        mMetalPreviewDescriptor.get();
                    std::memcpy(batch.light_direction[0],
                                light_direction.mV, sizeof(F32) * 3);
                    std::memcpy(batch.light_diffuse[0],
                                LLPipeline::PreviewDiffuse0.mV,
                                sizeof(F32) * 3);
                    std::memcpy(batch.diffuse_color, dummy_color.mV,
                                sizeof(batch.diffuse_color));

                    if (preview_mesh->hasWeights())
                    {
                        complete = buffer->hasDataType(
                            LLVertexBuffer::TYPE_WEIGHT) &&
                            (!avatar_cloth || buffer->hasDataType(
                                LLVertexBuffer::TYPE_CLOTHWEIGHT));
                        if (!complete)
                        {
                            break;
                        }
                        palettes.emplace_back();
                        complete = viewer_mesh->buildJointMatrixPalette(
                            palettes.back().data(), palettes.back().size());
                        if (!complete)
                        {
                            break;
                        }
                        batch.weight_offset = buffer->getOffset(
                            LLVertexBuffer::TYPE_WEIGHT);
                        batch.skin_palette = palettes.back().data();
                        batch.skin_palette_float_count =
                            palettes.back().size();
                        batch.skin_joint_count =
                            LL_CHARACTER_MAX_JOINTS_PER_MESH;
                        batch.avatar_skinning = true;
                        if (avatar_cloth)
                        {
                            batch.cloth_offset = buffer->getOffset(
                                LLVertexBuffer::TYPE_CLOTHWEIGHT);
                            batch.avatar_cloth = true;
                            std::memcpy(batch.avatar_wind, cloth_wind.mV,
                                        sizeof(batch.avatar_wind));
                            std::memcpy(batch.avatar_sin_wave,
                                        cloth_sin_wave.mV,
                                        sizeof(batch.avatar_sin_wave));
                            std::memcpy(batch.avatar_gravity,
                                        cloth_gravity.mV,
                                        sizeof(batch.avatar_gravity));
                            std::memcpy(batch.avatar_cloth_depth,
                                        cloth_depth.mV,
                                        sizeof(batch.avatar_cloth_depth));
                        }
                        std::memcpy(batch.modelview_projection,
                                    projection.getF32ptr(),
                                    sizeof(batch.modelview_projection));
                    }
                    else
                    {
                        batch.avatar_lighting = true;
                        const LLMatrix4a& joint_world =
                            viewer_mesh->getWorldMatrix();
                        LLMatrix4a rigid_modelview;
                        rigid_modelview.setMul(joint_world, modelview);
                        LLMatrix4a modelview_projection;
                        modelview_projection.setMul(
                            rigid_modelview, projection);
                        std::memcpy(batch.modelview_projection,
                                    modelview_projection.getF32ptr(),
                                    sizeof(batch.modelview_projection));
                        LLMatrix4a inverse_modelview;
                        complete = inverse_modelview.setAffineInverse(
                            rigid_modelview);
                        if (!complete)
                        {
                            break;
                        }
                        const F32* inverse = inverse_modelview.getF32ptr();
                        for (U32 column = 0; column < 3; ++column)
                        {
                            batch.normal_matrix[column][0] =
                                inverse[0 * 4 + column];
                            batch.normal_matrix[column][1] =
                                inverse[1 * 4 + column];
                            batch.normal_matrix[column][2] =
                                inverse[2 * 4 + column];
                        }
                    }
                    batches.push_back(batch);
                }

                if (complete && !batches.empty())
                {
                    ALMetalLitPreviewScene scene;
                    scene.batches = batches.data();
                    scene.batch_count = batches.size();
                    scene.destination_descriptor =
                        mMetalPreviewDescriptor.get();
                    // The retained preview wrote encoded bytes with
                    // FRAMEBUFFER_SRGB disabled into a logical RGB target.
                    scene.linear_storage_view = true;
                    mMetalPreview = renderer->render(scene, &error);
                }
                else if (error.empty())
                {
                    error = "animation avatar geometry is incomplete";
                }
                if (!mMetalPreview)
                {
                    LL_DEBUGS("Metal")
                        << "Animation preview native production is pending: "
                        << error << LL_ENDL;
                }
            }
            else
            {
                LL_DEBUGS("Metal")
                    << "Animation preview is pending because its native "
                       "texture, device or destination is incomplete"
                    << LL_ENDL;
            }
        }
    }

    return mMetalPreview != nullptr;
#endif
}

//-----------------------------------------------------------------------------
// requestUpdate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::requestUpdate()
{
    mNeedsUpdate = true;
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::rotate(F32 yaw_radians, F32 pitch_radians)
{
    mCameraYaw = mCameraYaw + yaw_radians;

    mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::zoom(F32 zoom_delta)
{
    setZoom(mCameraZoom + zoom_delta);
}

//-----------------------------------------------------------------------------
// setZoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::setZoom(F32 zoom_amt)
{
    mCameraZoom = llclamp(zoom_amt, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

//-----------------------------------------------------------------------------
// pan()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::pan(F32 right, F32 up)
{
    mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom, -1.f, 1.f);
    mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom, -1.f, 1.f);
}



