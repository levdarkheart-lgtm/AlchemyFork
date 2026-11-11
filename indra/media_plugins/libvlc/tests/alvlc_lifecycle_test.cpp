/**
 * @file alvlc_lifecycle_test.cpp
 * @brief Production ownership bodies against explicitly named libVLC doubles.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "../../base/media_plugin_base.h"
#include <vlc/vlc.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

// No libVLC library is called or linked by this fixture. Its backend models
// libVLC3's documented retained media/player/instance references, and records
// calls, failures and stop-time observations. The real LLPluginMessage and
// actual Darwin lifecycle method bodies below remain the code under test.
namespace vlc_lifecycle_backend
{
namespace fs=std::filesystem;
struct Instance { int id, refs=1; };
struct Media { int id, refs=1; Instance* instance; std::string uri; };
struct Player;
struct Events { Player* owner; };
struct Player
{
    int id, refs=1; Media* media; Events events{this}; bool stopped=true;
    void* opaque=nullptr;
    libvlc_video_lock_cb lock=nullptr;
    libvlc_video_unlock_cb unlock=nullptr;
    libvlc_video_display_cb display=nullptr;
    unsigned width=0,height=0,pitch=0;
    std::string chroma;
    std::vector<libvlc_event_e> attached;
    std::vector<libvlc_callback_t> handlers;std::vector<void*> event_owners;
};
using libvlc_instance_t=Instance;
using libvlc_media_t=Media;
using libvlc_media_player_t=Player;
using libvlc_event_manager_t=Events;
struct Backend
{
    std::vector<std::unique_ptr<Instance>> instances;
    std::vector<std::unique_ptr<Media>> media;
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::string> trace,args,options;
    std::string executable,environment_name,environment_value;
    bool fail_instance=false,fail_media=false,fail_player=false,fail_play=false;
    bool fail_path=false,fail_environment=false,missing_events=false;
    libvlc_time_t seek=0,reported_time=12500;
    std::function<void(Player*)> on_stop,on_play;
    int live_instances() const {return std::count_if(instances.begin(),instances.end(),[](auto& p){return p->refs>0;});}
    int live_media() const {return std::count_if(media.begin(),media.end(),[](auto& p){return p->refs>0;});}
    int live_players() const {return std::count_if(players.begin(),players.end(),[](auto& p){return p->refs>0;});}
};
Backend* backend=nullptr;
int _NSGetExecutablePath(char* buffer,std::uint32_t* size)
{
    if(backend->fail_path){*size=0;return -1;}
    const auto length=backend->executable.size()+1;
    if(!buffer||*size<length){*size=length;return -1;}
    std::memcpy(buffer,backend->executable.c_str(),length);return 0;
}
int setenv(const char* name,const char* value,int replace)
{
    CHECK(replace==1);backend->environment_name=name;backend->environment_value=value;
    return backend->fail_environment?1:0;
}
Instance* libvlc_new(int count,const char* const* args)
{
    backend->trace.push_back("new-instance");backend->args.assign(args,args+count);
    if(backend->fail_instance)return nullptr;
    auto value=std::make_unique<Instance>();value->id=backend->instances.size()+1;
    auto* result=value.get();backend->instances.push_back(std::move(value));return result;
}
void libvlc_release(Instance* value)
{
    REQUIRE(value);REQUIRE(value->refs>0);backend->trace.push_back("release-instance");--value->refs;
}
Media* libvlc_media_new_location(Instance* instance,const char* uri)
{
    REQUIRE(instance);REQUIRE(instance->refs>0);backend->trace.push_back("new-media");
    if(backend->fail_media)return nullptr;
    auto value=std::make_unique<Media>();value->id=backend->media.size()+1;value->instance=instance;value->uri=uri;++instance->refs;
    auto* result=value.get();backend->media.push_back(std::move(value));return result;
}
void libvlc_media_release(Media* value)
{
    REQUIRE(value);REQUIRE(value->refs>0);backend->trace.push_back("release-media");
    if(--value->refs==0)libvlc_release(value->instance);
}
Player* libvlc_media_player_new_from_media(Media* media)
{
    REQUIRE(media);REQUIRE(media->refs>0);backend->trace.push_back("new-player");
    if(backend->fail_player)return nullptr;
    auto value=std::make_unique<Player>();value->id=backend->players.size()+1;value->media=media;++media->refs;++media->instance->refs;
    auto* result=value.get();backend->players.push_back(std::move(value));return result;
}
void libvlc_media_player_stop(Player* player)
{
    REQUIRE(player);REQUIRE(player->refs>0);backend->trace.push_back("stop");
    if(backend->on_stop)backend->on_stop(player);player->stopped=true;
}
void libvlc_media_player_release(Player* player)
{
    REQUIRE(player);REQUIRE(player->refs>0);CHECK(player->stopped);backend->trace.push_back("release-player");
    if(--player->refs==0){libvlc_media_release(player->media);libvlc_release(player->media->instance);}
}
Events* libvlc_media_player_event_manager(Player* player)
{return backend->missing_events?nullptr:&player->events;}
int libvlc_event_attach(Events* events,libvlc_event_e type,libvlc_callback_t handler,void* opaque)
{
    REQUIRE(events);REQUIRE(opaque);events->owner->attached.push_back(type);events->owner->handlers.push_back(handler);events->owner->event_owners.push_back(opaque);return 0;
}
void libvlc_video_set_callbacks(Player* player,libvlc_video_lock_cb lock,libvlc_video_unlock_cb unlock,libvlc_video_display_cb display,void* opaque)
{player->lock=lock;player->unlock=unlock;player->display=display;player->opaque=opaque;backend->trace.push_back("callbacks");}
void libvlc_video_set_format(Player* player,const char* chroma,unsigned width,unsigned height,unsigned pitch)
{player->chroma=chroma;player->width=width;player->height=height;player->pitch=pitch;backend->trace.push_back("format");}
void libvlc_media_add_option(Media*,const char* value){backend->options.push_back(value);backend->trace.push_back("repeat");}
int libvlc_media_player_play(Player* player)
{
    backend->trace.push_back("play");if(backend->on_play)backend->on_play(player);
    player->stopped=backend->fail_play;return backend->fail_play?-1:0;
}
void libvlc_media_player_set_time(Player*,libvlc_time_t time){backend->seek=time;backend->trace.push_back("seek");}
libvlc_time_t libvlc_media_player_get_time(Player*){return backend->reported_time;}

// Expose only the real status vocabulary, without constructing a plugin base.
struct StatusVocabulary:MediaPluginBase
{
    using MediaPluginBase::EStatus;
    using MediaPluginBase::STATUS_NONE;using MediaPluginBase::STATUS_LOADING;
    using MediaPluginBase::STATUS_LOADED;using MediaPluginBase::STATUS_ERROR;
    using MediaPluginBase::STATUS_PLAYING;using MediaPluginBase::STATUS_PAUSED;
    using MediaPluginBase::STATUS_DONE;
};
using EStatus=StatusVocabulary::EStatus;
constexpr auto STATUS_NONE=StatusVocabulary::STATUS_NONE;
constexpr auto STATUS_LOADED=StatusVocabulary::STATUS_LOADED;
constexpr auto STATUS_ERROR=StatusVocabulary::STATUS_ERROR;
constexpr auto STATUS_DONE=StatusVocabulary::STATUS_DONE;
class MediaPluginLibVLC
{
public:
    Instance* mLibVLC=nullptr;Media* mLibVLCMedia=nullptr;Player* mLibVLCMediaPlayer=nullptr;
    struct mLibVLCContext {unsigned char* texture_pixels;Player* mp;MediaPluginLibVLC* parent;};
    mLibVLCContext mLibVLCCallbackContext{};
    std::string mURL="file:///owned-test-media";
    F64 mCurVolume=.375,mCurTime=12.5;bool mIsLooping=false;
    EStatus mVlcStatus=STATUS_NONE;
    unsigned char* mPixels=nullptr;S32 mWidth=32,mHeight=24,mDepth=4,mTextureWidth=32,mTextureHeight=24;
    std::string mTextureSegmentName;
    struct SharedSegmentInfo{void* mAddress=nullptr;size_t mSize=0;};
    using SharedSegmentMap=std::map<std::string,SharedSegmentInfo>;
    SharedSegmentMap mSharedSegments;
    std::vector<LLPluginMessage> messages;std::vector<EStatus> statuses;
    ~MediaPluginLibVLC();
    void initVLC();void resetVLC();void playMedia();void releaseMedia();
    void handleSizeChange(const LLPluginMessage& message_in);
    void handleSharedMemoryRemove(const LLPluginMessage& message_in);
    void setStatus(EStatus status){statuses.push_back(status);backend->trace.push_back("status:"+std::to_string(status));}
    void setVolume(F64 volume){CHECK(volume==mCurVolume);backend->trace.push_back("volume");}
    void sendMessage(const LLPluginMessage& message){messages.push_back(message);backend->trace.push_back(message.getName());}
    static void* lock(void*,void**){return nullptr;}
    static void unlock(void*,void*,void*const*){}
    static void display(void*,void*){}
    static void eventCallbacks(const libvlc_event_t*,void*){}
};
#include "../media_plugin_libvlc_lifecycle.inl"

struct Fixture
{
    Backend state;
    std::unique_ptr<MediaPluginLibVLC> plugin;
    std::array<unsigned char,32*24*4> old_pixels{};
    std::array<unsigned char,48*16*4> new_pixels{};
    fs::path directory;
    Fixture()
    {
        static unsigned sequence=0;backend=&state;
        directory=fs::path("/private/tmp")/("alchemy-vlc-lifecycle-"+std::to_string(getpid())+"-"+std::to_string(++sequence));
        const auto executable=directory/"Fixture.app/Contents/MacOS/host-path-double";
        fs::create_directories(executable.parent_path());
        fs::create_directories(directory/"Fixture.app/Contents/Frameworks/plugins");
        std::ofstream(executable)<<"Test path only; never executed.";
        state.executable=executable.string();plugin=std::make_unique<MediaPluginLibVLC>();plugin->mPixels=old_pixels.data();
    }
    ~Fixture(){plugin.reset();std::error_code error;fs::remove_all(directory,error);backend=nullptr;}
    void init(){plugin->initVLC();REQUIRE(plugin->mLibVLC);}
    void play(){plugin->playMedia();REQUIRE(plugin->mLibVLCMediaPlayer);}
    void empty(){CHECK(state.live_players()==0);CHECK(state.live_media()==0);CHECK(state.live_instances()==0);}
    LLPluginMessage size_message(const std::string& name="new")
    {
        LLPluginMessage result("media","size_change");result.setValue("name",name);
        result.setValueS32("width",48);result.setValueS32("height",16);
        result.setValueS32("texture_width",64);result.setValueS32("texture_height",32);return result;
    }
};
size_t position(const std::vector<std::string>& trace,const std::string& name)
{auto found=std::find(trace.begin(),trace.end(),name);REQUIRE(found!=trace.end());return found-trace.begin();}
}
using namespace vlc_lifecycle_backend;

TEST_CASE("VLC production replay releases the preceding media player and references", "[vlc-lifecycle]")
{
    Fixture f;f.init();
    for(unsigned index=0;index<8;++index)
    {
        f.plugin->mURL="file:///owned-test-"+std::to_string(index);f.play();
        CHECK(f.state.live_media()==1);CHECK(f.state.live_players()==1);CHECK(f.state.instances[0]->refs==3);
        CHECK(f.plugin->mLibVLCMedia->uri==f.plugin->mURL);
    }
    CHECK(f.state.options.empty());f.plugin->resetVLC();f.empty();const auto calls=f.state.trace;f.plugin->resetVLC();CHECK(f.state.trace==calls);
}
TEST_CASE("VLC production resize stops with old pixels then restores the saved seek", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();f.plugin->mSharedSegments["new"]={f.new_pixels.data(),f.new_pixels.size()};
    f.state.on_stop=[&](Player*){CHECK(f.plugin->mPixels==f.old_pixels.data());CHECK(f.plugin->mWidth==32);CHECK(f.plugin->mHeight==24);f.plugin->mCurTime=99.;};
    f.state.trace.clear();f.plugin->handleSizeChange(f.size_message());f.state.on_stop={};
    CHECK(f.plugin->mPixels==f.new_pixels.data());CHECK(f.plugin->mWidth==48);CHECK(f.plugin->mHeight==16);
    CHECK(f.plugin->mTextureWidth==64);CHECK(f.plugin->mTextureHeight==32);
    CHECK(f.state.seek==12500);CHECK(f.plugin->mCurTime==12.5);
    CHECK(position(f.state.trace,"stop")<position(f.state.trace,"new-media"));
    CHECK(f.state.live_media()==1);CHECK(f.state.live_players()==1);
    CHECK(f.plugin->messages.back().getName()=="size_change_response");
    CHECK(f.plugin->messages.back().getValueS32("texture_width")==64);
    f.state.reported_time=-1;f.plugin->handleSizeChange(f.size_message());CHECK(f.plugin->mCurTime==0);
    f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production invalid resize leaves playback and still responds", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();auto* player=f.plugin->mLibVLCMediaPlayer;const auto calls=f.state.trace;
    f.plugin->handleSizeChange(f.size_message("missing"));
    CHECK(f.plugin->mLibVLCMediaPlayer==player);CHECK(f.plugin->mPixels==f.old_pixels.data());
    CHECK(f.state.trace.size()==calls.size()+1);CHECK(f.plugin->messages.back().getName()=="size_change_response");
    CHECK(f.plugin->messages.back().getValue("name")=="missing");f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production shared memory removal retires media before its buffer", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();f.plugin->mSharedSegments["old"]={f.old_pixels.data(),f.old_pixels.size()};
    f.state.on_stop=[&](Player*){CHECK(f.plugin->mPixels==f.old_pixels.data());CHECK(f.plugin->mSharedSegments.contains("old"));};
    LLPluginMessage remove("base","shm_remove");remove.setValue("name","old");f.plugin->handleSharedMemoryRemove(remove);f.state.on_stop={};
    CHECK(f.plugin->mPixels==nullptr);CHECK(f.plugin->mLibVLCMediaPlayer==nullptr);CHECK(f.plugin->mLibVLCMedia==nullptr);
    CHECK(f.state.live_media()==0);CHECK(f.state.live_players()==0);CHECK(f.state.instances[0]->refs==1);
    CHECK_FALSE(f.plugin->mSharedSegments.contains("old"));CHECK(f.plugin->messages.back().getName()=="shm_remove_response");
    f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production factory and play failures release every partial owner", "[vlc-lifecycle]")
{
    const auto failure=GENERATE(0,1,2);Fixture f;f.init();f.play();
    f.state.fail_media=failure==0;f.state.fail_player=failure==1;f.state.fail_play=failure==2;
    f.state.on_stop=[&](Player*){f.plugin->mVlcStatus=STATUS_DONE;};
    f.plugin->messages.clear();f.plugin->playMedia();
    CHECK(f.plugin->mVlcStatus==STATUS_ERROR);CHECK(f.plugin->mLibVLCMediaPlayer==nullptr);CHECK(f.plugin->mLibVLCMedia==nullptr);
    CHECK(f.state.live_players()==0);CHECK(f.state.live_media()==0);CHECK(f.state.instances[0]->refs==1);
    CHECK(std::none_of(f.plugin->messages.begin(),f.plugin->messages.end(),[](auto& m){return m.getName()=="navigate_complete";}));
    if(failure==2){REQUIRE(f.plugin->messages.size()==1);CHECK(f.plugin->messages[0].getName()=="navigate_begin");}
    f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production destruction and repeated init balance instance references", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();auto* old=f.plugin->mLibVLC;f.plugin->initVLC();
    CHECK(old->refs==0);CHECK(f.plugin->mLibVLC!=old);CHECK(f.plugin->mLibVLCMediaPlayer==nullptr);CHECK(f.plugin->mLibVLCMedia==nullptr);
    f.play();f.plugin.reset();f.empty();
}
TEST_CASE("VLC production failed instance replacement cleans old owners", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();f.state.fail_instance=true;f.plugin->initVLC();
    CHECK(f.plugin->mVlcStatus==STATUS_ERROR);CHECK(f.plugin->mLibVLC==nullptr);
    CHECK(f.plugin->mLibVLCMediaPlayer==nullptr);CHECK(f.plugin->mLibVLCMedia==nullptr);f.empty();
}
TEST_CASE("VLC production early guards preserve empty URI and path failure behavior", "[vlc-lifecycle]")
{
    Fixture f;f.plugin->playMedia();CHECK(f.plugin->mVlcStatus==STATUS_ERROR);CHECK(f.state.trace.size()==1);
    f.init();f.play();auto* player=f.plugin->mLibVLCMediaPlayer;auto* instance=f.plugin->mLibVLC;
    f.plugin->mURL.clear();const auto calls=f.state.trace;f.plugin->playMedia();CHECK(f.state.trace==calls);CHECK(f.plugin->mLibVLCMediaPlayer==player);
    f.state.fail_path=true;f.plugin->initVLC();CHECK(f.plugin->mLibVLC==instance);CHECK(f.plugin->mLibVLCMediaPlayer==player);CHECK(f.plugin->mVlcStatus==STATUS_ERROR);
    f.state.fail_path=false;f.state.fail_environment=true;f.plugin->initVLC();
    CHECK(f.plugin->mLibVLC==instance);CHECK(f.plugin->mLibVLCMediaPlayer==player);CHECK(f.plugin->mVlcStatus==STATUS_ERROR);
    f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production playback preserves callback messages repeat and volume order", "[vlc-lifecycle]")
{
    const bool events=GENERATE(false,true);Fixture f;f.state.missing_events=!events;f.init();f.plugin->mIsLooping=true;
    f.state.on_play=[&](Player* p)
    {
        CHECK(p->chroma=="RV32");CHECK(p->width==32);CHECK(p->height==24);CHECK(p->pitch==128);
        CHECK(p->opaque==&f.plugin->mLibVLCCallbackContext);CHECK(p->lock==MediaPluginLibVLC::lock);
        CHECK(p->unlock==MediaPluginLibVLC::unlock);CHECK(p->display==MediaPluginLibVLC::display);
        CHECK(f.plugin->mLibVLCCallbackContext.texture_pixels==f.old_pixels.data());
        CHECK(f.plugin->mLibVLCCallbackContext.mp==p);CHECK(f.plugin->mLibVLCCallbackContext.parent==f.plugin.get());
        const std::vector<libvlc_event_e> expected={libvlc_MediaPlayerOpening,libvlc_MediaPlayerPlaying,
            libvlc_MediaPlayerPaused,libvlc_MediaPlayerStopped,libvlc_MediaPlayerEndReached,
            libvlc_MediaPlayerEncounteredError,libvlc_MediaPlayerTimeChanged,libvlc_MediaPlayerPositionChanged,
            libvlc_MediaPlayerLengthChanged,libvlc_MediaPlayerTitleChanged};
        CHECK(p->attached==(events?expected:std::vector<libvlc_event_e>{}));
        CHECK(std::all_of(p->handlers.begin(),p->handlers.end(),[](auto callback){return callback==MediaPluginLibVLC::eventCallbacks;}));
        CHECK(std::all_of(p->event_owners.begin(),p->event_owners.end(),[&](auto* owner){return owner==f.plugin.get();}));
        REQUIRE_FALSE(f.plugin->statuses.empty());CHECK(f.plugin->statuses.back()==STATUS_LOADED);
    };
    f.play();CHECK(f.state.args==std::vector<std::string>{"--no-xlib","--video-filter=transform{type=vflip}"});
    CHECK(f.state.environment_name=="VLC_PLUGIN_PATH");CHECK(fs::is_directory(f.state.environment_value));
    CHECK(f.state.options==std::vector<std::string>{"input-repeat=65535"});
    REQUIRE(f.plugin->messages.size()==3);
    CHECK(f.plugin->messages[0].getClass()==LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER);CHECK(f.plugin->messages[0].getName()=="navigate_begin");CHECK_FALSE(f.plugin->messages[0].getValueBoolean("history_back_available"));
    CHECK(f.plugin->messages[1].getName()=="location_changed");CHECK(f.plugin->messages[1].getValue("uri")==f.plugin->mURL);
    CHECK(f.plugin->messages[2].getName()=="navigate_complete");CHECK(f.plugin->messages[2].getValueS32("result_code")==200);CHECK(f.plugin->messages[2].getValue("result_string")=="OK");
    CHECK(position(f.state.trace,"navigate_begin")<position(f.state.trace,"volume"));
    CHECK(position(f.state.trace,"volume")<position(f.state.trace,"play"));CHECK(position(f.state.trace,"repeat")<position(f.state.trace,"play"));
    CHECK(f.plugin->mVlcStatus==STATUS_NONE); // setStatus(LOADED) does not rewrite the asynchronous status field.
    f.plugin->resetVLC();f.empty();
}

TEST_CASE("VLC production resize retires active callbacks even after an empty URI", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();f.plugin->mURL.clear();
    f.plugin->mSharedSegments["new"]={f.new_pixels.data(),f.new_pixels.size()};
    f.state.on_stop=[&](Player*){CHECK(f.plugin->mPixels==f.old_pixels.data());CHECK(f.plugin->mWidth==32);CHECK(f.plugin->mHeight==24);};
    const auto factories=std::count(f.state.trace.begin(),f.state.trace.end(),"new-media");
    f.plugin->handleSizeChange(f.size_message());f.state.on_stop={};
    CHECK(f.plugin->mPixels==f.new_pixels.data());CHECK(f.plugin->mLibVLCMediaPlayer==nullptr);CHECK(f.plugin->mLibVLCMedia==nullptr);
    CHECK(std::count(f.state.trace.begin(),f.state.trace.end(),"new-media")==factories);
    CHECK(f.state.live_media()==0);CHECK(f.state.live_players()==0);
    f.plugin->resetVLC();f.empty();
}
TEST_CASE("VLC production unrelated shared memory removal preserves active playback", "[vlc-lifecycle]")
{
    Fixture f;f.init();f.play();auto* player=f.plugin->mLibVLCMediaPlayer;
    f.plugin->mSharedSegments["unrelated"]={f.new_pixels.data(),f.new_pixels.size()};
    LLPluginMessage remove("base","shm_remove");remove.setValue("name","unrelated");
    f.plugin->handleSharedMemoryRemove(remove);CHECK(f.plugin->mLibVLCMediaPlayer==player);CHECK_FALSE(player->stopped);
    CHECK(f.plugin->mPixels==f.old_pixels.data());CHECK_FALSE(f.plugin->mSharedSegments.contains("unrelated"));
    remove.setValue("name","unknown");f.plugin->handleSharedMemoryRemove(remove);CHECK(f.plugin->mLibVLCMediaPlayer==player);
    CHECK(f.plugin->messages.back().getName()=="shm_remove_response");CHECK(f.plugin->messages.back().getValue("name")=="unknown");
    f.plugin->resetVLC();f.empty();
}
