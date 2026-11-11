/**
* @file media_plugin_libvlc.cpp
* @brief LibVLC plugin for LLMedia API plugin system
*
* @cond
* $LicenseInfo:firstyear=2008&license=viewerlgpl$
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
* @endcond
*/

#include "linden_common.h"

#include "llgl.h"
#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "media_plugin_base.h"

#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "vlc/vlc.h"
#include "vlc/libvlc_version.h"

#if LL_DARWIN
#include <mach-o/dyld.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#endif

#if LL_WINDOWS
// needed for waveOut call - see below for description
#include <mmsystem.h>
#endif

////////////////////////////////////////////////////////////////////////////////
//
class MediaPluginLibVLC :
    public MediaPluginBase
{
public:
    MediaPluginLibVLC(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data);
    ~MediaPluginLibVLC();

    /*virtual*/ void receiveMessage(const std::string& message_string);

private:
    bool init();

    void initVLC();
    void playMedia();
    void releaseMedia();
    void handleSizeChange(const LLPluginMessage& message_in);
    void handleSharedMemoryRemove(const LLPluginMessage& message_in);
    void resetVLC();
    void setVolume(const F64 volume);
    void setVolumeVLC();
    void updateTitle(const char* title);

    static void* lock(void* data, void** p_pixels);
    static void unlock(void* data, void* id, void* const* raw_pixels);
    static void display(void* data, void* id);

    /*virtual*/ void setDirty(int left, int top, int right, int bottom) /* override, but that is not supported in gcc 4.6 */;
    void setDurationDirty();

    static void eventCallbacks(const libvlc_event_t* event, void* ptr);

    libvlc_instance_t* mLibVLC;
    libvlc_media_t* mLibVLCMedia;
    libvlc_media_player_t* mLibVLCMediaPlayer;

    struct mLibVLCContext
    {
        unsigned char* texture_pixels;
        libvlc_media_player_t* mp;
        MediaPluginLibVLC* parent;
    };
    struct mLibVLCContext mLibVLCCallbackContext;

    std::string mURL;
    F64 mCurVolume;

    bool mIsLooping;

    F64 mCurTime;
    F64 mDuration;
    EStatus mVlcStatus;
};

////////////////////////////////////////////////////////////////////////////////
//
MediaPluginLibVLC::MediaPluginLibVLC(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data) :
MediaPluginBase(host_send_func, host_user_data)
{
    mTextureWidth = 0;
    mTextureHeight = 0;
    mWidth = 0;
    mHeight = 0;
    mDepth = 4;
    mPixels = 0;

    mLibVLC = 0;
    mLibVLCMedia = 0;
    mLibVLCMediaPlayer = 0;

    mCurVolume = 0.0;

    mIsLooping = false;

    mCurTime = 0.0;
    mDuration = 0.0;

    mURL = std::string();

    mVlcStatus = STATUS_NONE;
    setStatus(STATUS_NONE);
}

#include "media_plugin_libvlc_lifecycle.inl"

/////////////////////////////////////////////////////////////////////////////////
//
void* MediaPluginLibVLC::lock(void* data, void** p_pixels)
{
    struct mLibVLCContext* context = (mLibVLCContext*)data;

    *p_pixels = context->texture_pixels;

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::unlock(void* data, void* id, void* const* raw_pixels)
{
    // nothing to do here for the moment
    // we *could* modify pixels here to, for example, Y flip, but this is done with
    // a VLC video filter transform.
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::display(void* data, void* id)
{
    struct mLibVLCContext* context = (mLibVLCContext*)data;

    context->parent->setDirty(0, 0, context->parent->mWidth, context->parent->mHeight);
}



////////////////////////////////////////////////////////////////////////////////
// *virtual*
void MediaPluginLibVLC::setDirty(int left, int top, int right, int bottom)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

    message.setValueS32("left", left);
    message.setValueS32("top", top);
    message.setValueS32("right", right);
    message.setValueS32("bottom", bottom);

    message.setValueReal("current_time", mCurTime);
    message.setValueReal("duration", mDuration);
    message.setValueReal("current_rate", 1.0f);

    sendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
// *virtual*
void MediaPluginLibVLC::setDurationDirty()
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

    message.setValueReal("current_time", mCurTime);
    message.setValueReal("duration", mDuration);
    message.setValueReal("current_rate", 1.0f);

    sendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::eventCallbacks(const libvlc_event_t* event, void* ptr)
{
    MediaPluginLibVLC* parent = (MediaPluginLibVLC*)ptr;
    if (parent == 0)
    {
        return;
    }

    switch (event->type)
    {
    case libvlc_MediaPlayerOpening:
        parent->mVlcStatus = STATUS_LOADING;
        break;

    case libvlc_MediaPlayerPlaying:
        parent->mDuration = (float)(libvlc_media_get_duration(parent->mLibVLCMedia)) / 1000.0f;
        parent->mVlcStatus = STATUS_PLAYING;
        parent->setVolumeVLC();
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerPaused:
        parent->mVlcStatus = STATUS_PAUSED;
        break;

    case libvlc_MediaPlayerStopped:
        parent->mVlcStatus = STATUS_DONE;
        break;

    case libvlc_MediaPlayerEndReached:
        parent->mVlcStatus = STATUS_DONE;
        parent->mCurTime = parent->mDuration;
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerEncounteredError:
        parent->mVlcStatus = STATUS_ERROR;
        LL_WARNS("Media") << "LibVLC playback failed; media address omitted" << LL_ENDL;
        break;

    case libvlc_MediaPlayerTimeChanged:
        parent->mCurTime = (float)libvlc_media_player_get_time(parent->mLibVLCMediaPlayer) / 1000.0f;
        if (parent->mVlcStatus == STATUS_DONE && libvlc_media_player_is_playing(parent->mLibVLCMediaPlayer))
        {
            parent->mVlcStatus = STATUS_PLAYING;
        }
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerPositionChanged:
        break;

    case libvlc_MediaPlayerLengthChanged:
        parent->mDuration = (float)libvlc_media_get_duration(parent->mLibVLCMedia) / 1000.0f;
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerTitleChanged:
    {
        char* title = libvlc_media_get_meta(parent->mLibVLCMedia, libvlc_meta_Title);
        if (title)
        {
            parent->updateTitle(title);
        }
    }
    break;
    }
}


////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::updateTitle(const char* title)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    message.setValue("name", title);
    sendMessage(message);
}

void MediaPluginLibVLC::setVolumeVLC()
{
    if (mLibVLCMediaPlayer)
    {
        int vlc_vol = (int)(mCurVolume * 100);

        int result = libvlc_audio_set_volume(mLibVLCMediaPlayer, vlc_vol);
        if (result == 0)
        {
            // volume change was accepted by LibVLC
        }
        else
        {
            // volume change was NOT accepted by LibVLC and not actioned
        }

#if LL_WINDOWS
        // https ://jira.secondlife.com/browse/MAINT-8119
        // CEF media plugin uses code in media_plugins/cef/windows_volume_catcher.cpp to
        // set the actual output volume of the plugin process since there is no API in
        // CEF to otherwise do this.
        // There are explicit calls to change the volume in LibVLC but sometimes they
        // are ignored SLPlugin.exe process volume is set to 0 so you never heard audio
        // from the VLC media stream.
        // The right way to solve this is to move the volume catcher stuff out of
        // the CEF plugin and into it's own folder under media_plugins and have it referenced
        // by both CEF and VLC. That's for later. The code there boils down to this so for
                // now, as we approach a release, the less risky option is to do it directly vs
                // calls to volume catcher code.
        DWORD left_channel = (DWORD)(mCurVolume * 65535.0f);
        DWORD right_channel = (DWORD)(mCurVolume * 65535.0f);
        DWORD hw_volume = left_channel << 16 | right_channel;
        waveOutSetVolume(NULL, hw_volume);
#endif
    }
    else
    {
        // volume change was requested but VLC wasn't ready.
        // that's okay though because we saved the value in mCurVolume and
        // the next volume change after the VLC system is initilzied  will set it
    }
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::setVolume(const F64 volume)
{
    mCurVolume = volume;

    setVolumeVLC();
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::receiveMessage(const std::string& message_string)
{
    LLPluginMessage message_in;

    if (message_in.parse(message_string) >= 0)
    {
        std::string message_class = message_in.getClass();
        std::string message_name = message_in.getName();
        if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
        {
            if (message_name == "init")
            {
                initVLC();

                LLPluginMessage message("base", "init_response");
                LLSD versions = LLSD::emptyMap();
                versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
                versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] = LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
                versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME] = LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME_VERSION;
                message.setValueLLSD("versions", versions);

                std::ostringstream s;
                s << "LibVLC plugin ";
                s << LIBVLC_VERSION_MAJOR;
                s << ".";
                s << LIBVLC_VERSION_MINOR;
                s << ".";
                s << LIBVLC_VERSION_REVISION;

                message.setValue("plugin_version", s.str());
                sendMessage(message);
            }
            else if (message_name == "idle")
            {
                setStatus(mVlcStatus);
            }
            else if (message_name == "cleanup")
            {
                resetVLC();
            }
            else if (message_name == "force_exit")
            {
                mDeleteMe = true;
            }
            else if (message_name == "shm_added")
            {
                SharedSegmentInfo info;
                info.mAddress = message_in.getValuePointer("address");
                info.mSize = (size_t)message_in.getValueS32("size");
                std::string name = message_in.getValue("name");

                mSharedSegments.insert(SharedSegmentMap::value_type(name, info));

            }
            else if (message_name == "shm_remove")
            {
                handleSharedMemoryRemove(message_in);
            }
            else
            {
                //std::cerr << "MediaPluginWebKit::receiveMessage: unknown base message: " << message_name << std::endl;
            }
        }
        else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA)
        {
            if (message_name == "init")
            {
                LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "texture_params");
                message.setValueS32("default_width", 1024);
                message.setValueS32("default_height", 1024);
                message.setValueS32("depth", mDepth);
                message.setValueU32("internalformat", GL_RGB8);
                message.setValueU32("format", GL_BGRA);
                message.setValueU32("type", GL_UNSIGNED_BYTE);
                message.setValueBoolean("coords_opengl", true);
                sendMessage(message);
            }
            else if (message_name == "size_change")
            {
                handleSizeChange(message_in);
            }
            else if (message_name == "load_uri")
            {
                mURL = message_in.getValue("uri");
                playMedia();
            }
        }
        else
            if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME)
            {
                if (message_name == "stop")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_media_player_stop(mLibVLCMediaPlayer);
                    }
                }
                else if (message_name == "start")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        if (mVlcStatus == STATUS_DONE && !libvlc_media_player_is_playing(mLibVLCMediaPlayer))
                        {
                            // stop or vlc will ignore 'play', it will just
                            // make an MediaPlayerEndReached event even if
                            // seek was used
                            libvlc_media_player_stop(mLibVLCMediaPlayer);
                        }
                        libvlc_media_player_play(mLibVLCMediaPlayer);
                    }
                }
                else if (message_name == "pause")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_media_player_set_pause(mLibVLCMediaPlayer, 1);
                    }
                }
                else if (message_name == "seek")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_time_t time = (libvlc_time_t)(1000.0 * message_in.getValueReal("time"));
                        libvlc_media_player_set_time(mLibVLCMediaPlayer, time);
                        time = libvlc_media_player_get_time(mLibVLCMediaPlayer);
                        if (time < 0)
                        {
                            // -1 if there is no media
                            mCurTime = 0;
                        }
                        else
                        {
                            mCurTime = (F64)time / 1000.0;
                        }

                        if (!libvlc_media_player_is_playing(mLibVLCMediaPlayer))
                        {
                            // if paused, won't trigger update, update now
                            setDurationDirty();
                        }
                    }
                }
                else if (message_name == "set_loop")
                {
                    bool loop = message_in.getValueBoolean("loop");
                    mIsLooping = loop;
                }
                else if (message_name == "set_volume")
                {
                    // volume comes in 0 -> 1.0
                    F64 volume = message_in.getValueReal("volume");
                    setVolume(volume);
                }
            }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
bool MediaPluginLibVLC::init()
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    message.setValue("name", "LibVLC Plugin");
    sendMessage(message);

    return true;
};

////////////////////////////////////////////////////////////////////////////////
//
int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
    void* host_user_data,
    LLPluginInstance::sendMessageFunction *plugin_send_func,
    void **plugin_user_data)
{
    MediaPluginLibVLC* self = new MediaPluginLibVLC(host_send_func, host_user_data);
    *plugin_send_func = MediaPluginLibVLC::staticReceiveMessage;
    *plugin_user_data = (void*)self;

    return 0;
}
