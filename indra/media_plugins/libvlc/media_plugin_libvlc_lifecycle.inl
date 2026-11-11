/**
* @file media_plugin_libvlc_lifecycle.inl
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

// Actual ownership methods shared with the bounded backend contract fixture.

MediaPluginLibVLC::~MediaPluginLibVLC()
{
    resetVLC();
}

void MediaPluginLibVLC::initVLC()
{
    char const* vlc_argv[] =
    {
        "--no-xlib",
        "--video-filter=transform{type=vflip}",  // MAINT-6578 Y flip textures in plugin vs client
    };

#if LL_DARWIN
    // The host is launched from the viewer's Resources directory, while
    // ViewerInstall packages its modules inside this host's Frameworks.
    // Resolve the native executable, not the working directory or a shell
    // command, so relocated bundles and paths containing spaces work alike.
    std::uint32_t path_size = 0;
    _NSGetExecutablePath(nullptr, &path_size);
    std::vector<char> executable_path(path_size);
    if (path_size == 0 ||
        _NSGetExecutablePath(executable_path.data(), &path_size) != 0)
    {
        LL_WARNS("Media") << "LibVLC cannot resolve its native host executable" << LL_ENDL;
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }
    std::error_code path_error;
    const auto executable = std::filesystem::canonical(executable_path.data(), path_error);
    const auto plugins = executable.parent_path().parent_path() / "Frameworks" / "plugins";
    if (path_error || !std::filesystem::is_directory(plugins, path_error) ||
        path_error || setenv("VLC_PLUGIN_PATH", plugins.c_str(), 1) != 0)
    {
        LL_WARNS("Media") << "LibVLC bundled module directory is unavailable" << LL_ENDL;
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }
#endif

    int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);
    resetVLC();
    mLibVLC = libvlc_new(vlc_argc, vlc_argv);

    if (!mLibVLC)
    {
        LL_WARNS("Media") << "LibVLC initialization failed before media playback" << LL_ENDL;
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
    }
}

void MediaPluginLibVLC::releaseMedia()
{
    if (mLibVLCMediaPlayer)
    {
        libvlc_media_player_stop(mLibVLCMediaPlayer);
        libvlc_media_player_release(mLibVLCMediaPlayer);
        mLibVLCMediaPlayer = nullptr;
    }
    if (mLibVLCMedia)
    {
        libvlc_media_release(mLibVLCMedia);
        mLibVLCMedia = nullptr;
    }
}

void MediaPluginLibVLC::resetVLC()
{
    releaseMedia();
    if (mLibVLC)
    {
        libvlc_release(mLibVLC);
        mLibVLC = nullptr;
    }
}

void MediaPluginLibVLC::playMedia()
{
    if (!mLibVLC)
    {
        LL_WARNS("Media") << "LibVLC cannot play media without a valid instance" << LL_ENDL;
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }
    if (mURL.length() == 0)
    {
        return;
    }

    // Retire both old handles before replacing them. The resize handler also
    // retires playback before changing its shared pixel layout.
    releaseMedia();

    mLibVLCMedia = libvlc_media_new_location(mLibVLC, mURL.c_str());
    if (!mLibVLCMedia)
    {
        mLibVLCMediaPlayer = 0;
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }

    mLibVLCMediaPlayer = libvlc_media_player_new_from_media(mLibVLCMedia);
    if (!mLibVLCMediaPlayer)
    {
        releaseMedia();
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }

    // listen to events
    libvlc_event_manager_t* em = libvlc_media_player_event_manager(mLibVLCMediaPlayer);
    if (em)
    {
        libvlc_event_attach(em, libvlc_MediaPlayerOpening, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPlaying, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPaused, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerStopped, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerEndReached, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, eventCallbacks, this);
    }

    libvlc_video_set_callbacks(mLibVLCMediaPlayer, lock, unlock, display, &mLibVLCCallbackContext);
    libvlc_video_set_format(mLibVLCMediaPlayer, "RV32", mWidth, mHeight, mWidth * mDepth);

    mLibVLCCallbackContext.parent = this;
    mLibVLCCallbackContext.texture_pixels = mPixels;
    mLibVLCCallbackContext.mp = mLibVLCMediaPlayer;

    // Send a "navigate begin" event.
    // This is really a browser message but the QuickTime plugin did it and
    // the media system relies on this message to update internal state so we must send it too
    // Note: see "navigate_complete" message below too
    // https://jira.secondlife.com/browse/MAINT-6528
    LLPluginMessage message_begin(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "navigate_begin");
    message_begin.setValue("uri", mURL);
    message_begin.setValueBoolean("history_back_available", false);
    message_begin.setValueBoolean("history_forward_available", false);
    sendMessage(message_begin);

    // volume level gets set before VLC is initialized (thanks media system) so we have to record
    // it in mCurVolume and set it again here so that volume levels are correctly initialized
    setVolume(mCurVolume);

    setStatus(STATUS_LOADED);

    // note this relies on the "set_loop" message arriving before the "start" (play) one
    // but that appears to always be the case
    if (mIsLooping)
    {
        libvlc_media_add_option(mLibVLCMedia, "input-repeat=65535");
    }

    if (libvlc_media_player_play(mLibVLCMediaPlayer) != 0)
    {
        LL_WARNS("Media") << "LibVLC could not start its media player; media address omitted" << LL_ENDL;
        releaseMedia();
        mVlcStatus = STATUS_ERROR;
        setStatus(mVlcStatus);
        return;
    }

    // send a "location_changed" message - this informs the media system
    // that a new URL is the 'current' one and is used extensively.
    // Again, this is really a browser message but we will use it here.
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "location_changed");
    message.setValue("uri", mURL);
    sendMessage(message);

    // Send a "navigate complete" event.
    // This is really a browser message but the QuickTime plugin did it and
    // the media system relies on this message to update internal state so we must send it too
    // Note: see "navigate_begin" message above too
    // https://jira.secondlife.com/browse/MAINT-6528
    LLPluginMessage message_complete(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "navigate_complete");
    message_complete.setValue("uri", mURL);
    message_complete.setValueS32("result_code", 200);
    message_complete.setValue("result_string", "OK");
    sendMessage(message_complete);
}

void MediaPluginLibVLC::handleSizeChange(const LLPluginMessage& message_in)
{
    std::string name = message_in.getValue("name");
    S32 width = message_in.getValueS32("width");
    S32 height = message_in.getValueS32("height");
    S32 texture_width = message_in.getValueS32("texture_width");
    S32 texture_height = message_in.getValueS32("texture_height");

    if (!name.empty())
    {
        // Find the shared memory region with this name
        SharedSegmentMap::iterator iter = mSharedSegments.find(name);
        if (iter != mSharedSegments.end())
        {
            // Preserve the requested seek before stop can deliver time events.
            libvlc_time_t time = (libvlc_time_t)(1000.0 * mCurTime);
            releaseMedia();
            mPixels = (unsigned char*)iter->second.mAddress;
            mWidth = width;
            mHeight = height;
            mTextureWidth = texture_width;
            mTextureHeight = texture_height;

            playMedia();

            if (mLibVLCMediaPlayer)
            {
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
            }
        };
    };

    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "size_change_response");
    message.setValue("name", name);
    message.setValueS32("width", width);
    message.setValueS32("height", height);
    message.setValueS32("texture_width", texture_width);
    message.setValueS32("texture_height", texture_height);
    sendMessage(message);
}

void MediaPluginLibVLC::handleSharedMemoryRemove(const LLPluginMessage& message_in)
{
    std::string name = message_in.getValue("name");

    SharedSegmentMap::iterator iter = mSharedSegments.find(name);
    if (iter != mSharedSegments.end())
    {
        if (mPixels == iter->second.mAddress)
        {
            releaseMedia();

            mPixels = NULL;
            mTextureSegmentName.clear();
        }
        mSharedSegments.erase(iter);
    }
    else
    {
        //std::cerr << "MediaPluginWebKit::receiveMessage: unknown shared memory region!" << std::endl;
    }

    // Send the response so it can be cleaned up.
    LLPluginMessage message("base", "shm_remove_response");
    message.setValue("name", name);
    sendMessage(message);
}
