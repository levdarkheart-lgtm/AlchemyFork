/**
 * @file llfloaterimagepreview.cpp
 * @brief LLFloaterImagePreview class implementation
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

#include "llfloaterimagepreview.h"

#include "llimagebmp.h"
#include "llimagetga.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagej2c.h"

#include "llagent.h"
#include "llagentbenefits.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldrawable.h"
#include "llrender.h"
#include "llface.h"
#include "llfocusmgr.h"
#include "llfilesystem.h"
#include "llfloaterperms.h"
#include "llnotificationsutil.h"
#include "llstatusbar.h"    // can_afford_transaction()
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "llviewermenufile.h"   // upload_new_resource()
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llstring.h"

#include "llendianswizzle.h"

#include "llviewercontrol.h"
#include "lltrans.h"
#include "llimagedimensionsinfo.h"

#if LL_DARWIN
#include "../almetal/almetalsculptpreview.h"
#endif

#include <algorithm>
#include <array>

namespace
{
#if LL_DARWIN
class LLScopedImagePreviewCameraState final
{
public:
    LLScopedImagePreviewCameraState()
    : mCamera(*LLViewerCamera::getInstance()),
      mCurrent(LLViewerCamera::getCurrent()),
      mViewport{{
          gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3],
      }}
    {
    }

    ~LLScopedImagePreviewCameraState()
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

#include "image_preview_native.inl"

const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
const S32 PREVIEW_VPAD = -24 + 35;  // yuk, hard coded
const S32 PREF_BUTTON_HEIGHT = 16 + 7 + 16 + 35;
const S32 PREVIEW_TEXTURE_HEIGHT = 320;

//-----------------------------------------------------------------------------
// LLFloaterImagePreview()
//-----------------------------------------------------------------------------
LLFloaterImagePreview::LLFloaterImagePreview(const LLSD& args) :
    LLFloaterNameDesc(args),

    mAvatarPreview(NULL),
    mSculptedPreview(NULL),
    mLastMouseX(0),
    mLastMouseY(0),
    mImagep(NULL)
{
    loadImage(mFilenameAndPath);
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
bool LLFloaterImagePreview::postBuild()
{
    if (!LLFloaterNameDesc::postBuild())
    {
        return false;
    }

    LLCtrlSelectionInterface* iface = childGetSelectionInterface("clothing_type_combo");
    if (iface)
    {
        iface->selectFirstItem();
    }
    childSetCommitCallback("clothing_type_combo", onPreviewTypeCommit, this);

    mPreviewRect.set(PREVIEW_HPAD,
        PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD,
        getRect().getWidth() - PREVIEW_HPAD,
        PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
    mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);

    getChildView("bad_image_text")->setVisible(false);

    if (mRawImagep.notNull() && gAgent.getRegion() != NULL)
    {
        mAvatarPreview = new LLImagePreviewAvatar(256, 256);
        mAvatarPreview->setPreviewTarget("mPelvis", "mUpperBodyMesh0", mRawImagep, 2.f, false);

        mSculptedPreview = new LLImagePreviewSculpted(256, 256);
        mSculptedPreview->setPreviewTarget(mRawImagep, 2.0f);

        if (mRawImagep->getWidth() * mRawImagep->getHeight() <= LL_IMAGE_REZ_LOSSLESS_CUTOFF * LL_IMAGE_REZ_LOSSLESS_CUTOFF)
        {
            // We want "lossless_check" to be unchecked when it is disabled, regardless of
            // LosslessJ2CUpload state, so only assign control when enabling checkbox
            LLCheckBoxCtrl* check_box = getChild<LLCheckBoxCtrl>("lossless_check");
            check_box->setEnabled(true);
            check_box->setControlVariable(gSavedSettings.getControl("LosslessJ2CUpload"));
        }
    }
    else
    {
        mAvatarPreview = NULL;
        mSculptedPreview = NULL;
        getChildView("bad_image_text")->setVisible(true);
        getChildView("clothing_type_combo")->setEnabled(false);
        getChildView("ok_btn")->setEnabled(false);

        if(!mImageLoadError.empty())
        {
            getChild<LLUICtrl>("bad_image_text")->setValue(mImageLoadError.c_str());
        }
    }

    getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterImagePreview::onBtnOK, this));

    return true;
}


//-----------------------------------------------------------------------------
// getExpectedUploadCost()
//-----------------------------------------------------------------------------
S32 LLFloaterImagePreview::getExpectedUploadCost() const
{
    return LLAgentBenefitsMgr::current().getTextureUploadCost(mRawImagep);
}

//-----------------------------------------------------------------------------
// LLFloaterImagePreview()
//-----------------------------------------------------------------------------
LLFloaterImagePreview::~LLFloaterImagePreview()
{
    clearAllPreviewTextures();

    mRawImagep = NULL;
    mImagep = NULL ;
}

//static
//-----------------------------------------------------------------------------
// onPreviewTypeCommit()
//-----------------------------------------------------------------------------
void    LLFloaterImagePreview::onPreviewTypeCommit(LLUICtrl* ctrl, void* userdata)
{
    LLFloaterImagePreview *fp =(LLFloaterImagePreview *)userdata;

    if (!fp->mAvatarPreview || !fp->mSculptedPreview)
    {
        return;
    }

    S32 which_mode = 0;

    LLCtrlSelectionInterface* iface = fp->childGetSelectionInterface("clothing_type_combo");
    if (iface)
    {
        which_mode = iface->getFirstSelectedIndex();
    }

    switch(which_mode)
    {
    case 0:
        break;
    case 1:
        fp->mAvatarPreview->setPreviewTarget("mSkull", "mHairMesh0", fp->mRawImagep, 0.4f, false);
        break;
    case 2:
        fp->mAvatarPreview->setPreviewTarget("mSkull", "mHeadMesh0", fp->mRawImagep, 0.4f, false);
        break;
    case 3:
        fp->mAvatarPreview->setPreviewTarget("mChest", "mUpperBodyMesh0", fp->mRawImagep, 1.0f, false);
        break;
    case 4:
        fp->mAvatarPreview->setPreviewTarget("mKneeLeft", "mLowerBodyMesh0", fp->mRawImagep, 1.2f, false);
        break;
    case 5:
        fp->mAvatarPreview->setPreviewTarget("mSkull", "mHeadMesh0", fp->mRawImagep, 0.4f, true);
        break;
    case 6:
        fp->mAvatarPreview->setPreviewTarget("mChest", "mUpperBodyMesh0", fp->mRawImagep, 1.2f, true);
        break;
    case 7:
        fp->mAvatarPreview->setPreviewTarget("mKneeLeft", "mLowerBodyMesh0", fp->mRawImagep, 1.2f, true);
        break;
    case 8:
        fp->mAvatarPreview->setPreviewTarget("mKneeLeft", "mSkirtMesh0", fp->mRawImagep, 1.3f, false);
        break;
    case 9:
        fp->mSculptedPreview->setPreviewTarget(fp->mRawImagep, 2.0f);
        break;
    default:
        break;
    }

    fp->mAvatarPreview->refresh();
    fp->mSculptedPreview->refresh();
}


//-----------------------------------------------------------------------------
// clearAllPreviewTextures()
//-----------------------------------------------------------------------------
void LLFloaterImagePreview::clearAllPreviewTextures()
{
    if (mAvatarPreview)
    {
        mAvatarPreview->setTexture(nullptr);
    }
}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterImagePreview::onBtnOK()
{
    getChildView("ok_btn")->setEnabled(false); // don't allow inadvertent extra uploads

    S32 expected_upload_cost = getExpectedUploadCost();
    if (can_afford_transaction(expected_upload_cost))
    {
        LL_INFOS() << "saving texture: " << mRawImagep->getWidth() << "x" << mRawImagep->getHeight() << LL_ENDL;
        // gen a new uuid for this asset
        LLTransactionID tid;
        tid.generate();
        LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

        LLPointer<LLImageJ2C> formatted = new LLImageJ2C;

        if (mRawImagep->getWidth() * mRawImagep->getHeight() <= LL_IMAGE_REZ_LOSSLESS_CUTOFF * LL_IMAGE_REZ_LOSSLESS_CUTOFF)
        {
            if (gSavedSettings.getBOOL("LosslessJ2CUpload"))
            {
                formatted->setReversible(true);
            }
        }

        if (formatted->encode(mRawImagep, 0.0f))
        {
            LLFileSystem fmt_file(new_asset_id, LLAssetType::AT_TEXTURE, LLFileSystem::WRITE);
            fmt_file.write(formatted->getData(), formatted->getDataSize());

            LLResourceUploadInfo::ptr_t assetUploadInfo = std::make_shared<LLResourceUploadInfo>(
                tid, LLAssetType::AT_TEXTURE,
                getChild<LLUICtrl>("name_form")->getValue().asString(),
                getChild<LLUICtrl>("description_form")->getValue().asString(),
                0,
                LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
                LLFloaterPerms::getNextOwnerPerms("Uploads"),
                LLFloaterPerms::getGroupPerms("Uploads"),
                LLFloaterPerms::getEveryonePerms("Uploads"),
                expected_upload_cost,
                mDestinationFolderId
            );

            upload_new_resource(assetUploadInfo);
        }
        else
        {
            LLSD args;
            args["REASON"] = LLImage::getLastThreadError();
            LLNotificationsUtil::add("ErrorEncodingImage", args);
            LL_WARNS() << "Error encoding image" << LL_ENDL;
        }
    }
    else
    {
        LLSD args;
        args["COST"] = llformat("%d", expected_upload_cost);
        LLNotificationsUtil::add("ErrorCannotAffordUpload", args);
    }

    closeFloater(false);
}

//-----------------------------------------------------------------------------
// draw()
//-----------------------------------------------------------------------------
void LLFloaterImagePreview::draw()
{
    LLFloater::draw();
    LLRect r = getRect();

    if (mRawImagep.notNull())
    {
        LLCtrlSelectionInterface* iface = childGetSelectionInterface("clothing_type_combo");
        U32 selected = 0;
        if (iface)
            selected = iface->getFirstSelectedIndex();

        if (selected <= 0)
        {
            gl_rect_2d_checkerboard(mPreviewRect);

            if(mImagep.notNull())
            {
                gGL.getTextureSlot(0)->bindSampled(mImagep.get(),
                                                   ALSamplers::BilinearClamp);
            }
            else
            {
                mImagep = LLViewerTextureManager::getLocalTexture(mRawImagep.get(), false) ;

                gGL.getTextureSlot(0)->unbind() ;
                gGL.getTextureSlot(0)->bindSampled(mImagep.get(),
                                                   ALSamplers::BilinearClamp);
                stop_glerror();
                if (mAvatarPreview)
                {
                    mAvatarPreview->setTexture(mImagep.get());
                }
            }

            gGL.color3f(1.f, 1.f, 1.f);
            gGL.begin(LLRender::TRIANGLES);
            {
                gGL.texCoord2f(mPreviewImageRect.mLeft, mPreviewImageRect.mTop);
                gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
                gGL.texCoord2f(mPreviewImageRect.mLeft, mPreviewImageRect.mBottom);
                gGL.vertex2i(PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
                gGL.texCoord2f(mPreviewImageRect.mRight, mPreviewImageRect.mBottom);
                gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);

                gGL.texCoord2f(mPreviewImageRect.mRight, mPreviewImageRect.mBottom);
                gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
                gGL.texCoord2f(mPreviewImageRect.mLeft, mPreviewImageRect.mTop);
                gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
                gGL.texCoord2f(mPreviewImageRect.mRight, mPreviewImageRect.mTop);
                gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
            }
            gGL.end();

            gGL.getTextureSlot(0)->unbind();

            stop_glerror();
        }
        else
        {
            if ((mAvatarPreview) && (mSculptedPreview))
            {
                gGL.color3f(1.f, 1.f, 1.f);

                if (selected == 9)
                {
                    gGL.getTextureSlot(0)->bindSampled(mSculptedPreview, ALSamplers::AnisoWrap);
                }
                else
                {
                    gGL.getTextureSlot(0)->bindSampled(mAvatarPreview, ALSamplers::AnisoWrap);
                }

                gGL.begin(LLRender::TRIANGLES);
                {
                    gGL.texCoord2f(0.f, 1.f);
                    gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
                    gGL.texCoord2f(0.f, 0.f);
                    gGL.vertex2i(PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
                    gGL.texCoord2f(1.f, 0.f);
                    gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);

                    gGL.texCoord2f(1.f, 0.f);
                    gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
                    gGL.texCoord2f(0.f, 1.f);
                    gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
                    gGL.texCoord2f(1.f, 1.f);
                    gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
                }
                gGL.end();

                gGL.getTextureSlot(0)->unbind();
            }
        }
    }
}


//-----------------------------------------------------------------------------
// loadImage()
//-----------------------------------------------------------------------------
bool LLFloaterImagePreview::loadImage(const std::string& src_filename)
{
    try
    {
    std::string exten = gDirUtilp->getExtension(src_filename);
    U32 codec = LLImageBase::getCodecFromExtension(exten);

    LLImageDimensionsInfo image_info;
    if (!image_info.load(src_filename,codec))
    {
        mImageLoadError = image_info.getLastError();
        return false;
    }

    // raw image is limited to 256MB so need at least some upper limit that fits into that
    constexpr S32 MAX_IMAGE_AREA = 8096 * 8096;

    if (image_info.getWidth() * image_info.getHeight() > MAX_IMAGE_AREA)
    {
        LLStringUtil::format_map_t args;
        args["PIXELS"] = llformat("%dM", (S32)(MAX_IMAGE_AREA / 1000000));

        mImageLoadError = LLTrans::getString("texture_load_area_error", args);
        return false;
    }

    // Load the image
    LLPointer<LLImageFormatted> image = LLImageFormatted::createFromType(codec);
    if (image.isNull())
    {
        return false;
    }
    if (!image->load(src_filename))
    {
        return false;
    }
    // Decompress or expand it in a raw image structure
    LLPointer<LLImageRaw> raw_image = new LLImageRaw;
    if (!image->decode(raw_image, 0.0f))
    {
        return false;
    }
    // Check the image constraints
    if ((image->getComponents() != 3) && (image->getComponents() != 4))
    {
        image->setLastError("Image files with less than 3 or more than 4 components are not supported.");
        return false;
    }
    // Downscale images to fit the max_texture_dimensions_*
    S32 max_width  = gSavedSettings.getS32("max_texture_dimension_X");
    S32 max_height = gSavedSettings.getS32("max_texture_dimension_Y");

    S32 orig_width  = raw_image->getWidth();
    S32 orig_height = raw_image->getHeight();

    if (orig_width > max_width || orig_height > max_height)
    {
        // Calculate scale factors
        F32 width_scale  = (F32)max_width / (F32)orig_width;
        F32 height_scale = (F32)max_height / (F32)orig_height;
        F32 scale        = llmin(width_scale, height_scale);

        // Calculate new dimensions, preserving aspect ratio
        S32 new_width  = LLImageRaw::contractDimToPowerOfTwo(
            llclamp((S32)llroundf(orig_width * scale), 4, max_width)
        );
        S32 new_height = LLImageRaw::contractDimToPowerOfTwo(
            llclamp((S32)llroundf(orig_height * scale), 4, max_height)
        );

        if (!raw_image->scale(new_width, new_height))
        {
            LL_WARNS() << "Failed to scale image from "
                       << orig_width << "x" << orig_height
                       << " to " << new_width << "x" << new_height << LL_ENDL;
            return false;
        }

        // Inform the resident about the resized image
        LLSD subs;
        subs["[ORIGINAL_WIDTH]"]  = orig_width;
        subs["[ORIGINAL_HEIGHT]"] = orig_height;
        subs["[NEW_WIDTH]"]       = new_width;
        subs["[NEW_HEIGHT]"]      = new_height;
        subs["[MAX_WIDTH]"]       = max_width;
        subs["[MAX_HEIGHT]"]      = max_height;
        LLNotificationsUtil::add("ImageUploadResized", subs);
    }

    raw_image->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
    mRawImagep = raw_image;
    }
    catch (...)
    {
        LOG_UNHANDLED_EXCEPTION("");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
bool LLFloaterImagePreview::handleMouseDown(S32 x, S32 y, MASK mask)
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
bool LLFloaterImagePreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
    gFocusMgr.setMouseCapture(nullptr);
    gViewerWindow->showCursor();
    return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
bool LLFloaterImagePreview::handleHover(S32 x, S32 y, MASK mask)
{
    MASK local_mask = mask & ~MASK_ALT;

    if (mAvatarPreview && hasMouseCapture())
    {
        if (local_mask == MASK_PAN)
        {
            // pan here
            LLCtrlSelectionInterface* iface = childGetSelectionInterface("clothing_type_combo");
            if (iface && iface->getFirstSelectedIndex() <= 0)
            {
                mPreviewImageRect.translate((F32)(x - mLastMouseX) * -0.005f * mPreviewImageRect.getWidth(),
                    (F32)(y - mLastMouseY) * -0.005f * mPreviewImageRect.getHeight());
            }
            else
            {
                mAvatarPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
                mSculptedPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
            }
        }
        else if (local_mask == MASK_ORBIT)
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

            mAvatarPreview->rotate(yaw_radians, pitch_radians);
            mSculptedPreview->rotate(yaw_radians, pitch_radians);
        }
        else
        {
            LLCtrlSelectionInterface* iface = childGetSelectionInterface("clothing_type_combo");
            if (iface && iface->getFirstSelectedIndex() <= 0)
            {
                F32 zoom_amt = (F32)(y - mLastMouseY) * -0.002f;
                mPreviewImageRect.stretch(zoom_amt);
            }
            else
            {
                F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
                F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

                mAvatarPreview->rotate(yaw_radians, 0.f);
                mAvatarPreview->zoom(zoom_amt);
                mSculptedPreview->rotate(yaw_radians, 0.f);
                mSculptedPreview->zoom(zoom_amt);
            }
        }

        LLCtrlSelectionInterface* iface = childGetSelectionInterface("clothing_type_combo");
        if (iface && iface->getFirstSelectedIndex() <= 0)
        {
            if (mPreviewImageRect.getWidth() > 1.f)
            {
                mPreviewImageRect.stretch((1.f - mPreviewImageRect.getWidth()) * 0.5f);
            }
            else if (mPreviewImageRect.getWidth() < 0.1f)
            {
                mPreviewImageRect.stretch((0.1f - mPreviewImageRect.getWidth()) * 0.5f);
            }

            if (mPreviewImageRect.getHeight() > 1.f)
            {
                mPreviewImageRect.stretch((1.f - mPreviewImageRect.getHeight()) * 0.5f);
            }
            else if (mPreviewImageRect.getHeight() < 0.1f)
            {
                mPreviewImageRect.stretch((0.1f - mPreviewImageRect.getHeight()) * 0.5f);
            }

            if (mPreviewImageRect.mLeft < 0.f)
            {
                mPreviewImageRect.translate(-mPreviewImageRect.mLeft, 0.f);
            }
            else if (mPreviewImageRect.mRight > 1.f)
            {
                mPreviewImageRect.translate(1.f - mPreviewImageRect.mRight, 0.f);
            }

            if (mPreviewImageRect.mBottom < 0.f)
            {
                mPreviewImageRect.translate(0.f, -mPreviewImageRect.mBottom);
            }
            else if (mPreviewImageRect.mTop > 1.f)
            {
                mPreviewImageRect.translate(0.f, 1.f - mPreviewImageRect.mTop);
            }
        }
        else
        {
            mAvatarPreview->refresh();
            mSculptedPreview->refresh();
        }

        LLUI::getInstance()->setMousePositionLocal(this, mLastMouseX, mLastMouseY);
    }

    if (!mPreviewRect.pointInRect(x, y) || !mAvatarPreview || !mSculptedPreview)
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
bool LLFloaterImagePreview::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (mPreviewRect.pointInRect(x, y) && mAvatarPreview)
    {
        mAvatarPreview->zoom((F32)delta.mPrecise * -0.2f);
        mAvatarPreview->refresh();

        mSculptedPreview->zoom((F32)delta.mPrecise * -0.2f);
        mSculptedPreview->refresh();
    }

    return true;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
// static
void LLFloaterImagePreview::onMouseCaptureLostImagePreview(LLMouseHandler* handler)
{
    gViewerWindow->showCursor();
}


//-----------------------------------------------------------------------------
// LLImagePreviewAvatar
//-----------------------------------------------------------------------------
LLImagePreviewAvatar::LLImagePreviewAvatar(S32 width, S32 height)
: LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE)
{
    mNeedsUpdate = true;
    mTargetJoint = NULL;
    mTargetMesh = NULL;
    mCameraDistance = 0.f;
    mCameraYaw = 0.f;
    mCameraPitch = 0.f;
    mCameraZoom = 1.f;

    mDummyAvatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, gAgent.getRegion(), LLViewerObject::CO_FLAG_UI_AVATAR);
    mDummyAvatar->mSpecialRenderMode = 2;

}


LLImagePreviewAvatar::~LLImagePreviewAvatar()
{
    mDummyAvatar->markDead();
}

//virtual
S8 LLImagePreviewAvatar::getType() const
{
    return LLViewerDynamicTexture::LL_IMAGE_PREVIEW_AVATAR ;
}

void LLImagePreviewAvatar::setTexture(LLViewerTexture* texture)
{
    mPreviewTexture = texture;
    mNeedsUpdate = true;
}

void LLImagePreviewAvatar::setPreviewTarget(const std::string& joint_name, const std::string& mesh_name, LLImageRaw* imagep, F32 distance, bool male)
{
    mTargetJoint = mDummyAvatar->mRoot->findJoint(joint_name);
    if (male)
    {
        mDummyAvatar->setVisualParamWeight( "male", 1.f );
        mDummyAvatar->updateVisualParams();
        mDummyAvatar->updateGeometry(mDummyAvatar->mDrawable);
    }
    else
    {
        mDummyAvatar->setVisualParamWeight( "male", 0.f );
        mDummyAvatar->updateVisualParams();
        mDummyAvatar->updateGeometry(mDummyAvatar->mDrawable);
    }
    mDummyAvatar->mRoot->setVisible(false, true);

    mTargetMesh = dynamic_cast<LLViewerJointMesh*>(mDummyAvatar->mRoot->findJoint(mesh_name));
    mTargetMesh->setVisible(true, false);
    mCameraDistance = distance;
    mCameraZoom = 1.f;
    mCameraPitch = 0.f;
    mCameraYaw = 0.f;
    mCameraOffset.clearVec();
    mNeedsUpdate = true;
}



//-----------------------------------------------------------------------------
// update()
//-----------------------------------------------------------------------------
bool LLImagePreviewAvatar::render()
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

    LLVOAvatar* avatar = mDummyAvatar.get();
    if (!avatar || !mTargetJoint || !mTargetMesh ||
        !mMetalPreviewDescriptor || mPreviewTexture.isNull())
    {
        return false;
    }

    const LLVector3 target_pos = mTargetJoint->getWorldPosition();
    const LLQuaternion camera_rot =
        LLQuaternion(mCameraPitch, LLVector3::y_axis) *
        LLQuaternion(mCameraYaw, LLVector3::z_axis);
    const LLQuaternion avatar_rot =
        avatar->mPelvisp->getWorldRotation() * camera_rot;

    LLViewerCamera* camera = LLViewerCamera::getInstance();
    camera->lookAt(
        target_pos + ((LLVector3(mCameraDistance, 0.f, 0.f) +
                       mCameraOffset) * avatar_rot),
        target_pos + (mCameraOffset * avatar_rot),
        LLVector3::z_axis);
    camera->setAspect(static_cast<F32>(mFullWidth) / mFullHeight);
    camera->setView(camera->getDefaultFOV() / mCameraZoom);
    camera->setPerspectiveWithoutRenderState(
        mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);

    // Preserve the original preview's final mesh refresh before native
    // extraction, including ordinary priority-one morph changes.
    updateImagePreviewAvatarGeometry(avatar);

    LLFace* face = nullptr;
    LLPolyMesh* mesh = nullptr;
    LLVertexBuffer* buffer = nullptr;
    if (mTargetMesh->getPreviewGeometry(face, mesh) && face)
    {
        buffer = face->getVertexBuffer();
    }
    LLImageGL* preview_image = mPreviewTexture->getGLTexture();
    const ALMetalTexture* preview_texture = preview_image
        ? preview_image->getMetalTexture() : nullptr;
    ALMetalDevice* metal_device = getViewerMetalDevice();
    if (!metal_device || !buffer || !mesh || !preview_texture ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT) ||
        !buffer->getMetalVertexBuffer() || !buffer->getMetalIndexBuffer())
    {
        LL_DEBUGS("Metal")
            << "Avatar image preview is pending because its native geometry, "
               "texture, device or destination is incomplete"
            << LL_ENDL;
        return false;
    }

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
        renderer = ALMetalLitPreviewRenderer::create(*metal_device, &error);
    }

    constexpr std::size_t AVATAR_PALETTE_FLOATS =
        LL_CHARACTER_MAX_JOINTS_PER_MESH * 3 * 4;
    std::array<F32, AVATAR_PALETTE_FLOATS> palette{};
    if (!renderer || !mTargetMesh->buildJointMatrixPalette(
            palette.data(), palette.size()))
    {
        if (error.empty())
        {
            error = "avatar joint palette is unavailable";
        }
        LL_DEBUGS("Metal")
            << "Avatar image preview native production is pending: "
            << error << LL_ENDL;
        return false;
    }

    ALMetalLitPreviewBatch batch;
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    batch.normal_offset = buffer->getOffset(LLVertexBuffer::TYPE_NORMAL);
    batch.texcoord_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_TEXCOORD0);
    batch.weight_offset = buffer->getOffset(LLVertexBuffer::TYPE_WEIGHT);
    batch.skin_palette = palette.data();
    batch.skin_palette_float_count = palette.size();
    batch.skin_joint_count = LL_CHARACTER_MAX_JOINTS_PER_MESH;
    batch.avatar_skinning = true;
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_offset = static_cast<std::size_t>(mesh->mFaceIndexOffset) *
                         buffer->getIndexStride();
    batch.index_count = mesh->mFaceIndexCount;
    batch.texture = preview_texture;
    std::memcpy(batch.modelview_projection,
                camera->getProjection().getF32ptr(),
                sizeof(batch.modelview_projection));
    for (U32 axis = 0; axis < 3; ++axis)
    {
        batch.normal_matrix[axis][axis] = 1.f;
    }

    LLVector4 light(LLPipeline::PreviewDirection0, 0.f);
    LLVector4a source;
    source.loadua(light.mV);
    LLVector4a transformed;
    camera->getModelview().transform4(source, transformed);
    LLVector3 direction(transformed.getF32ptr());
    direction.normVec();
    std::memcpy(batch.light_direction[0], direction.mV, sizeof(F32) * 3);
    std::memcpy(batch.light_diffuse[0], LLPipeline::PreviewDiffuse0.mV,
                sizeof(F32) * 3);
    const LLColor4 preview_color = mTargetMesh->isTransparent()
        ? LLColor4::white : LLColor4(0.7f, 0.6f, 0.3f, 1.f);
    std::memcpy(batch.diffuse_color, preview_color.mV,
                sizeof(batch.diffuse_color));

    ALMetalLitPreviewScene scene;
    scene.batches = &batch;
    scene.batch_count = 1;
    scene.destination_descriptor = mMetalPreviewDescriptor.get();
    // The retained preview wrote encoded color with FRAMEBUFFER_SRGB off.
    // Keep the sRGB sampling descriptor while preserving those exact bytes.
    scene.linear_storage_view = true;
    mMetalPreview = renderer->render(scene, &error);
    if (!mMetalPreview)
    {
        LL_DEBUGS("Metal")
            << "Avatar image preview native production is pending: "
            << error << LL_ENDL;
    }
    return mMetalPreview != nullptr;
#endif
}

//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLImagePreviewAvatar::refresh()
{
    mNeedsUpdate = true;
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLImagePreviewAvatar::rotate(F32 yaw_radians, F32 pitch_radians)
{
    mCameraYaw = mCameraYaw + yaw_radians;

    mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLImagePreviewAvatar::zoom(F32 zoom_amt)
{
    mCameraZoom = llclamp(mCameraZoom + zoom_amt, 1.f, 10.f);
}

void LLImagePreviewAvatar::pan(F32 right, F32 up)
{
    mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom, -1.f, 1.f);
    mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom, -1.f, 1.f);
}


//-----------------------------------------------------------------------------
// LLImagePreviewSculpted
//-----------------------------------------------------------------------------

LLImagePreviewSculpted::LLImagePreviewSculpted(S32 width, S32 height)
: LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE)
{
    mNeedsUpdate = true;
    mCameraDistance = 0.f;
    mCameraYaw = 0.f;
    mCameraPitch = 0.f;
    mCameraZoom = 1.f;
    LLVolumeParams volume_params;
    volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_CIRCLE);
    volume_params.setSculptID(LLUUID::null, LL_SCULPT_TYPE_SPHERE);

    F32 const HIGHEST_LOD = 4.0f;
    mVolume = new LLVolume(volume_params,  HIGHEST_LOD);
}


LLImagePreviewSculpted::~LLImagePreviewSculpted()
{
}

//virtual
S8 LLImagePreviewSculpted::getType() const
{
    return LLViewerDynamicTexture::LL_IMAGE_PREVIEW_SCULPTED ;
}

void LLImagePreviewSculpted::setPreviewTarget(LLImageRaw* imagep, F32 distance)
{
    mCameraDistance = distance;
    mCameraZoom = 1.f;
    mCameraPitch = 0.f;
    mCameraYaw = 0.f;
    mCameraOffset.clearVec();

    if (imagep)
    {
        LLImageDataSharedLock lock(imagep);
        mVolume->sculpt(imagep->getWidth(), imagep->getHeight(), imagep->getComponents(), imagep->getData(), 0, false);
    }

    const LLVolumeFace &vf = mVolume->getVolumeFace(0);
    U32 num_indices = vf.mNumIndices;
    U32 num_vertices = vf.mNumVertices;

    mVertexBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0);
    if (!mVertexBuffer->allocateBuffer(num_vertices, num_indices))
    {
        LL_WARNS() << "Failed to allocate Vertex Buffer for image preview to"
            << num_vertices << " vertices and "
            << num_indices << " indices" << LL_ENDL;
        // We are likely to crash on getTexCoord0Strider()
    }

    LLStrider<LLVector3> vertex_strider;
    LLStrider<LLVector3> normal_strider;
    LLStrider<LLVector2> tc_strider;
    LLStrider<U16> index_strider;

    mVertexBuffer->getVertexStrider(vertex_strider);
    mVertexBuffer->getNormalStrider(normal_strider);
    mVertexBuffer->getTexCoord0Strider(tc_strider);
    mVertexBuffer->getIndexStrider(index_strider);

    // build vertices and normals
    LLStrider<LLVector3> pos;
    pos = (LLVector3*) vf.mPositions; pos.setStride(16);
    LLStrider<LLVector3> norm;
    norm = (LLVector3*) vf.mNormals; norm.setStride(16);
    LLStrider<LLVector2> tc;
    tc = (LLVector2*) vf.mTexCoords; tc.setStride(8);

    for (U32 i = 0; i < num_vertices; i++)
    {
        *(vertex_strider++) = *pos++;
        LLVector3 normal = *norm++;
        normal.normalize();
        *(normal_strider++) = normal;
        *(tc_strider++) = *tc++;
    }

    // build indices
    for (U32 i = 0; i < num_indices; i++)
    {
        *(index_strider++) = vf.mIndices[i];
    }

    mVertexBuffer->unmapBuffer();
    mNeedsUpdate = true;
}




//-----------------------------------------------------------------------------
// render()
//-----------------------------------------------------------------------------
bool LLImagePreviewSculpted::render()
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

    if (!mMetalPreviewDescriptor || mVertexBuffer.isNull() || mVolume.isNull())
    {
        return false;
    }

    const LLVector3 target_pos(0.f, 0.f, 0.f);

    const LLQuaternion camera_rot =
        LLQuaternion(mCameraPitch, LLVector3::y_axis) *
        LLQuaternion(mCameraYaw, LLVector3::z_axis);

    LLViewerCamera* camera = LLViewerCamera::getInstance();
    camera->lookAt(
        target_pos + ((LLVector3(mCameraDistance, 0.f, 0.f) +
                       mCameraOffset) * camera_rot),
        target_pos + (mCameraOffset * camera_rot),
        LLVector3::z_axis);
    camera->setAspect(static_cast<F32>(mFullWidth) / mFullHeight);
    camera->setView(camera->getDefaultFOV() / mCameraZoom);
    camera->setPerspectiveWithoutRenderState(
        mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);

    const LLVolumeFace &vf = mVolume->getVolumeFace(0);
    const U32 num_indices = vf.mNumIndices;
    const F32 SCALE = 1.25f;
    const F32 BRIGHTNESS = 0.9f;

    ALMetalDevice* metal_device = getViewerMetalDevice();
    ALMetalBuffer* metal_vertices = mVertexBuffer.notNull()
        ? mVertexBuffer->getMetalVertexBuffer() : nullptr;
    ALMetalBuffer* metal_indices = mVertexBuffer.notNull()
        ? mVertexBuffer->getMetalIndexBuffer() : nullptr;
    if (!metal_device || !metal_vertices || !metal_indices)
    {
        LL_DEBUGS("Metal")
            << "Sculpt image preview is pending because its native geometry "
               "or device is incomplete"
            << LL_ENDL;
        return false;
    }

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
        renderer = ALMetalLitPreviewRenderer::create(*metal_device, &error);
    }
    if (!renderer)
    {
        LL_DEBUGS("Metal")
            << "Sculpt image preview native production is pending: "
            << error << LL_ENDL;
        return false;
    }

    LLMatrix4a modelview;
    modelview.setMul(LLMatrix4a::scaling(SCALE, SCALE, SCALE),
                     camera->getModelview());
    LLMatrix4a modelview_projection;
    modelview_projection.setMul(modelview, camera->getProjection());
    LLMatrix4a inverse_modelview;
    if (!inverse_modelview.setAffineInverse(modelview))
    {
        LL_DEBUGS("Metal")
            << "Sculpt image preview native production is pending: "
               "modelview matrix is singular"
            << LL_ENDL;
        return false;
    }

    ALMetalLitPreviewBatch batch;
    batch.vertices = metal_vertices;
    batch.position_offset = mVertexBuffer->getOffset(
        LLVertexBuffer::TYPE_VERTEX);
    batch.normal_offset = mVertexBuffer->getOffset(
        LLVertexBuffer::TYPE_NORMAL);
    batch.vertex_count = vf.mNumVertices;
    batch.indices = metal_indices;
    batch.index_type = mVertexBuffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = num_indices;
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));

    const F32* inverse = inverse_modelview.getF32ptr();
    for (U32 column = 0; column < 3; ++column)
    {
        batch.normal_matrix[column][0] = inverse[0 * 4 + column];
        batch.normal_matrix[column][1] = inverse[1 * 4 + column];
        batch.normal_matrix[column][2] = inverse[2 * 4 + column];
    }

    const LLVector3 light_directions[3] = {
        LLPipeline::PreviewDirection0,
        LLPipeline::PreviewDirection1,
        LLPipeline::PreviewDirection2,
    };
    const LLColor4 light_diffuse[3] = {
        LLPipeline::PreviewDiffuse0,
        LLPipeline::PreviewDiffuse1,
        LLPipeline::PreviewDiffuse2,
    };
    for (U32 light_index = 0; light_index < 3; ++light_index)
    {
        LLVector4 light(light_directions[light_index], 0.f);
        LLVector4a source;
        source.loadua(light.mV);
        LLVector4a transformed;
        modelview.transform4(source, transformed);
        LLVector3 direction(transformed.getF32ptr());
        direction.normVec();
        std::memcpy(batch.light_direction[light_index], direction.mV,
                    sizeof(F32) * 3);
        std::memcpy(batch.light_diffuse[light_index],
                    light_diffuse[light_index].mV, sizeof(F32) * 3);
    }
    batch.diffuse_color[0] = BRIGHTNESS;
    batch.diffuse_color[1] = BRIGHTNESS;
    batch.diffuse_color[2] = BRIGHTNESS;
    batch.diffuse_color[3] = 1.f;

    ALMetalLitPreviewScene scene;
    scene.batches = &batch;
    scene.batch_count = 1;
    scene.destination_descriptor = mMetalPreviewDescriptor.get();
    scene.linear_storage_view = true;
    mMetalPreview = renderer->render(scene, &error);
    if (!mMetalPreview)
    {
        LL_DEBUGS("Metal")
            << "Sculpt image preview native production is pending: "
            << error << LL_ENDL;
    }
    return mMetalPreview != nullptr;
#endif
}

//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLImagePreviewSculpted::refresh()
{
    mNeedsUpdate = true;
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLImagePreviewSculpted::rotate(F32 yaw_radians, F32 pitch_radians)
{
    mCameraYaw = mCameraYaw + yaw_radians;

    mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLImagePreviewSculpted::zoom(F32 zoom_amt)
{
    mCameraZoom = llclamp(mCameraZoom + zoom_amt, 1.f, 10.f);
}

void LLImagePreviewSculpted::pan(F32 right, F32 up)
{
    mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom, -1.f, 1.f);
    mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom, -1.f, 1.f);
}
