// PPD481: actual whole receive/request/error bodies plus exact idle drain/cases.
// Controlled boundaries deliberately exclude IPC, CEF and listener destruction.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Log { template<class T> Log& operator<<(const T&) { return *this; } };
inline Log log_stream;
inline int warnings = 0;
#define LL_DEBUGS(...) log_stream
#define LL_INFOS(...) log_stream
#define LL_WARNS(...) (++warnings, log_stream)
#define LL_ENDL 0
#define llassert_always(x) do { if (!(x)) std::abort(); } while (false)
inline const std::string LLPLUGIN_MESSAGE_CLASS_INTERNAL = "internal";
struct LLSD {
    struct Value { std::string value; std::string asString() const { return value; } };
    using map_type = std::map<std::string, Value>;
    using map_iterator = map_type::iterator;
    map_type values;
    map_iterator beginMap() { return values.begin(); }
    map_iterator endMap() { return values.end(); }
};
struct LLPluginMessage {
    std::string klass, name;
    std::map<std::string, std::string> values;
    LLSD versions;
    LLPluginMessage(std::string c, std::string n): klass(std::move(c)), name(std::move(n)) {}
    std::string getClass() const { return klass; }
    std::string getName() const { return name; }
    std::string getValue(const std::string& key) const { auto i=values.find(key); return i==values.end()?"":i->second; }
    LLSD getValueLLSD(const std::string&) const { return versions; }
    double getValueReal(const std::string&) const { return 0.; }
    void setValue(const std::string& key, const std::string& value) { values[key]=value; }
    void setValueReal(const std::string&, double) {}
};
struct Owner {
    int received=0, died=0, launch_failed=0;
    void receivePluginMessage(const LLPluginMessage&) { ++received; }
    bool receivePluginMessageEarly(const LLPluginMessage&) { return false; }
    void pluginDied() { ++died; }
    void pluginLaunchFailed() { ++launch_failed; }
};
struct Mutex { bool trylock() { return true; } void unlock() {} };
struct Heartbeat {
    bool expired=false; double expiry=0;
    void setTimerExpirySec(double seconds) { expiry=seconds; }
    bool hasExpired() const { return expired; }
    void start() {} void reset() {} double getElapsedTimeF64() const { return 1.; }
};
struct SharedRegion { void destroy() {} };
struct LLApp { static inline bool quitting=false; static bool isQuitting() { return quitting; } };
struct EventPump { template<class T> int listen(const std::string&, T) { return 1; } };
struct LLEventPumps { static LLEventPumps& instance() { static LLEventPumps p; return p; } EventPump obtain(const std::string&) { return {}; } };
namespace boost { template<class F, class T> int bind(F, T*) { return 0; } }
struct LLProcess { static inline int killed=0; static inline bool running=true; static void kill(int) { ++killed; running=false; } static bool isRunning(int) { return running; } };
inline constexpr int APR_SUCCESS=0;
#define APR_STATUS_IS_EOF(x) ((x)==1)
inline constexpr double GOODBYE_SECONDS=12., HEARTBEAT_SECONDS=1., PLUGIN_IDLE_SECONDS=.01;
struct ListenSocket { void reset() {} };

class LLPluginProcessParent {
public:
@states@
    EState mState=STATE_LOADING;
    Owner* mOwner=nullptr;
    Mutex mIncomingQueueMutex;
    std::queue<LLPluginMessage> mIncomingQueue;
    int mPolling=0, mProcess=0, mSocketError=APR_SUCCESS;
    bool mUseDaemon=true, mMessagePipe=true, timed_out=false;
    bool accepted=true;
    ListenSocket mListenSocket;
    int socket_kills=0, dirty_polls=0, sleep_sends=0, removed=0;
    std::vector<std::string> sent;
    std::string mPluginVersionString, mPluginFile="plugin", mPluginDir="directory";
    LLSD mMessageClassVersions;
    double mSleepTime=.01, mPluginLockupTimeout=10, mCPUUsage=0;
    Heartbeat mHeartbeat;
    using sharedMemoryRegionsType=std::map<std::string, SharedRegion*>;
    sharedMemoryRegionsType mSharedMemoryRegions;
    void setState(EState state) { mState=state; }
    void setSleepTime(double, bool) { ++sleep_sends; }
    void sendMessage(const LLPluginMessage& message) { sent.push_back(message.name); }
    bool pluginLockedUp() { return timed_out; }
    bool pluginLockedUpOrQuit() { return timed_out; }
    bool accept() { return accepted; }
    void killSockets() { ++socket_kills; mMessagePipe=false; }
    void dirtyPollSet() { ++dirty_polls; }
    void removeFromProcessing() { ++removed; }
    bool pollTick() { return false; }
    void requestShutdown();
    void receiveMessageEarly(const LLPluginMessage&);
    void receiveMessage(const LLPluginMessage&);
    void errorState();
    void idle() {
@queue_drain@
        switch(mState) {
@parent_cases@
        default: break;
        }
    }
};
@requestShutdown@
@receiveMessageEarly@
@receiveMessage@
@errorState@

struct ChildSocket { bool blockingConnect(int) { return true; } };
class LLPluginProcessChild;
struct LLPluginMessagePipe { LLPluginMessagePipe(LLPluginProcessChild*, const std::shared_ptr<ChildSocket>&); };
struct LLPluginInstance {
    explicit LLPluginInstance(LLPluginProcessChild*) {}
    int load(const std::string&, const std::string&) { return 0; }
    void idle() {}
};
class LLPluginProcessChild {
public:
@child_states@
    EState mState=STATE_CONNECTED;
    int mSocketError=APR_SUCCESS, mLauncherHost=0, socket_kills=0;
    bool mDaemonMode=true;
    LLPluginMessagePipe* mMessagePipe=nullptr;
    std::shared_ptr<ChildSocket> mSocket=std::make_shared<ChildSocket>();
    LLPluginInstance* mInstance=nullptr;
    std::string mPluginFile, mPluginDir;
    Heartbeat mHeartbeat, mWaitGoodbye;
    double mCPUElapsed=0.;
    std::vector<std::string> parent_messages, plugin_messages;
    void setState(EState state) { mState=state; }
    void sendMessageToParent(const LLPluginMessage& message) { parent_messages.push_back(message.name); }
    void sendMessageToPlugin(const LLPluginMessage& message) { plugin_messages.push_back(message.name); }
    void killSockets() { ++socket_kills; delete mMessagePipe; mMessagePipe=nullptr; }
    void receiveShutdown()
@child_shutdown_dispatch@
    void idle();
    ~LLPluginProcessChild() { delete mMessagePipe; delete mInstance; }
};
LLPluginMessagePipe::LLPluginMessagePipe(LLPluginProcessChild* child, const std::shared_ptr<ChildSocket>&) { child->mMessagePipe=this; }
@child_idle@

static int checks=0, failures=0, cases=0;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; std::cerr << __func__ << ':' << __LINE__ << " failed: " #expr "\n"; } } while(false)
using Parent=LLPluginProcessParent;
LLPluginMessage response() { LLPluginMessage m("internal", "load_plugin_response"); m.values["plugin_version"]="test"; m.versions.values["base"]={"1"}; return m; }
void normal_load() {
    ++cases; warnings=0; Owner owner; Parent p; p.mOwner=&owner;
    p.receiveMessageEarly(response()); CHECK(p.mState==Parent::STATE_LOADING); CHECK(p.mIncomingQueue.size()==1);
    p.idle(); CHECK(p.mState==Parent::STATE_RUNNING); CHECK(p.mPluginVersionString=="test"); CHECK(p.sleep_sends==1); CHECK(warnings==0); CHECK(owner.died==0);
}
void unexpected_active_response() {
    ++cases;
    for (auto state : {Parent::STATE_UNINITIALIZED, Parent::STATE_CONNECTED, Parent::STATE_HELLO, Parent::STATE_RUNNING}) {
        warnings=0; Parent p; p.mState=state; p.receiveMessage(response());
        CHECK(warnings==1); CHECK(p.mState==(state<Parent::STATE_RUNNING ? Parent::STATE_LAUNCH_FAILURE : Parent::STATE_ERROR)); CHECK(p.sleep_sends==0);
    }
}
void queued_then_cancelled(bool quitting) {
    ++cases; warnings=0; LLApp::quitting=quitting; Owner owner; Parent p; p.mOwner=&owner;
    p.mPluginVersionString="unpublished";
    p.receiveMessageEarly(response()); p.requestShutdown();
    if(!quitting) { CHECK(p.mState==Parent::STATE_GOODBYE); p.idle(); }
    CHECK(p.mOwner==nullptr); CHECK(p.mState==Parent::STATE_EXITING); CHECK(p.sent==std::vector<std::string>{"shutdown_plugin"});
    CHECK(warnings==0); CHECK(p.sleep_sends==0); CHECK(p.socket_kills==0); CHECK(owner.died==0); CHECK(owner.launch_failed==0);
    CHECK(p.mPluginVersionString=="unpublished"); CHECK(p.mMessageClassVersions.values.empty());
    if(quitting) CHECK(p.removed==1); else CHECK(p.mPolling==1);
    p.mSocketError=1; p.idle(); CHECK(p.mState==Parent::STATE_CLEANUP); p.idle(); CHECK(p.mState==Parent::STATE_DONE); CHECK(p.socket_kills==1);
    LLApp::quitting=false;
}
void reply_after_shutdown_send() {
    ++cases; warnings=0; Owner owner; Parent p; p.mOwner=&owner;
    p.requestShutdown(); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(p.sent.size()==1);
    p.receiveMessageEarly(response()); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0); CHECK(p.sent.size()==1); CHECK(p.socket_kills==0);
    p.timed_out=true; p.idle(); CHECK(p.mState==Parent::STATE_CLEANUP); CHECK(warnings==1);
}
void duplicate_live_reply_still_fails() {
    ++cases; warnings=0; Owner owner; Parent p; p.mOwner=&owner;
    p.receiveMessageEarly(response()); p.receiveMessageEarly(response()); p.idle();
    CHECK(p.mState==Parent::STATE_CLEANUP); CHECK(warnings==1); CHECK(owner.died==1); CHECK(p.sleep_sends==1);
}
void wrong_terminal_states_still_fail() {
    ++cases;
    for(auto state : {Parent::STATE_LAUNCH_FAILURE,Parent::STATE_ERROR,Parent::STATE_CLEANUP,Parent::STATE_DONE}) {
        warnings=0; Parent p; p.mState=state; p.receiveMessage(response()); CHECK(warnings==1); CHECK(p.mState==Parent::STATE_ERROR);
    }
}
void normal_running_shutdown() {
    ++cases; warnings=0; Parent p; Owner owner; p.mOwner=&owner; p.mState=Parent::STATE_RUNNING;
    p.requestShutdown(); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(p.sent==std::vector<std::string>{"shutdown_plugin"}); CHECK(warnings==0);
}
void standalone_cancelled_load() {
    ++cases; warnings=0; LLProcess::running=true; Parent p; Owner owner;
    p.mUseDaemon=false; p.mProcess=1; p.mOwner=&owner;
    p.receiveMessageEarly(response()); p.requestShutdown(); p.idle();
    CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0); CHECK(p.sent.size()==1);
    p.receiveMessageEarly(response()); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0); CHECK(owner.died==0);
    LLProcess::running=false; p.idle(); CHECK(p.mState==Parent::STATE_CLEANUP); p.idle(); CHECK(p.mState==Parent::STATE_DONE); CHECK(p.socket_kills==1);
}
void multiple_obsolete_load_replies() {
    ++cases; warnings=0; Parent p; p.receiveMessageEarly(response()); p.receiveMessageEarly(response());
    p.requestShutdown(); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0); CHECK(p.sleep_sends==0); CHECK(p.sent.size()==1);
}
void active_hello_guard_unchanged() {
    ++cases; warnings=0; Parent p; p.mState=Parent::STATE_CONNECTED;
    LLPluginMessage hello("internal","hello"); p.receiveMessage(hello); CHECK(p.mState==Parent::STATE_HELLO); CHECK(warnings==0);
    p.receiveMessage(hello); CHECK(p.mState==Parent::STATE_LAUNCH_FAILURE); CHECK(warnings==1);
    for(auto state : {Parent::STATE_UNINITIALIZED,Parent::STATE_INITIALIZED,Parent::STATE_LISTENING,Parent::STATE_LAUNCHED,Parent::STATE_LOADING,Parent::STATE_RUNNING,Parent::STATE_CLEANUP,Parent::STATE_DONE}) {
        warnings=0; Parent other; other.mState=state; other.receiveMessage(hello); CHECK(warnings==1);
        CHECK(other.mState==(state<Parent::STATE_RUNNING ? Parent::STATE_LAUNCH_FAILURE : Parent::STATE_ERROR));
    }
}
void noninternal_reply_still_owned() {
    ++cases; warnings=0; Parent p; Owner owner; p.mOwner=&owner;
    LLPluginMessage media("media","status"); p.receiveMessageEarly(media); p.idle(); CHECK(owner.received==1);
    p.requestShutdown(); p.receiveMessageEarly(media); p.idle(); CHECK(owner.received==1); CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0);
}
void launched_to_running_handshake() {
    ++cases; warnings=0; Parent p; p.mState=Parent::STATE_LAUNCHED;
    p.idle(); CHECK(p.mState==Parent::STATE_CONNECTED);
    p.receiveMessageEarly(LLPluginMessage("internal","hello")); p.idle();
    CHECK(p.mState==Parent::STATE_LOADING); CHECK(p.sent==std::vector<std::string>{"load_plugin"});
    p.receiveMessageEarly(response()); p.idle(); CHECK(p.mState==Parent::STATE_RUNNING); CHECK(warnings==0);
}
void connected_hello_cancelled(bool quitting) {
    ++cases; warnings=0; LLApp::quitting=quitting; Parent p; Owner owner; p.mOwner=&owner; p.mState=Parent::STATE_LAUNCHED;
    p.idle(); CHECK(p.mState==Parent::STATE_CONNECTED);
    p.receiveMessageEarly(LLPluginMessage("internal","hello")); p.requestShutdown();
    if(!quitting) p.idle();
    CHECK(p.mState==Parent::STATE_EXITING); CHECK(p.sent==std::vector<std::string>{"shutdown_plugin"}); CHECK(warnings==0);
    CHECK(p.sleep_sends==0); CHECK(owner.died==0); CHECK(owner.launch_failed==0); CHECK(p.mOwner==nullptr);
    LLApp::quitting=false;
}
void late_hello_during_exit() {
    ++cases; warnings=0; Parent p; p.mState=Parent::STATE_LAUNCHED; p.idle();
    p.requestShutdown(); p.idle(); CHECK(p.mState==Parent::STATE_EXITING);
    p.receiveMessageEarly(LLPluginMessage("internal","hello")); p.idle(); CHECK(p.mState==Parent::STATE_EXITING); CHECK(warnings==0); CHECK(p.sent.size()==1);
}
void child_shutdown_before_plugin_load() {
    ++cases; warnings=0; LLPluginProcessChild child; child.mState=LLPluginProcessChild::STATE_INITIALIZED;
    child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_CONNECTED); CHECK(child.mMessagePipe!=nullptr);
    child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_PLUGIN_LOADING); CHECK(child.parent_messages==std::vector<std::string>{"hello"});
    CHECK(child.mInstance==nullptr); child.receiveShutdown(); CHECK(child.mState==LLPluginProcessChild::STATE_SHUTDOWNREQ);
    child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_UNLOADING); CHECK(child.mWaitGoodbye.expiry==GOODBYE_SECONDS); CHECK(child.plugin_messages.empty());
    child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_UNLOADING); CHECK(child.socket_kills==0);
    child.mWaitGoodbye.expired=true; child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_UNLOADED); CHECK(warnings==1);
    child.idle(); CHECK(child.mState==LLPluginProcessChild::STATE_DONE); CHECK(child.socket_kills==1); CHECK(child.mMessagePipe==nullptr);
}
int main() {
    normal_load(); unexpected_active_response(); queued_then_cancelled(false); queued_then_cancelled(true);
    reply_after_shutdown_send(); duplicate_live_reply_still_fails(); wrong_terminal_states_still_fail(); normal_running_shutdown();
    standalone_cancelled_load(); multiple_obsolete_load_replies(); active_hello_guard_unchanged(); noninternal_reply_still_owned();
    launched_to_running_handshake(); connected_hello_cancelled(false); connected_hello_cancelled(true); late_hello_during_exit(); child_shutdown_before_plugin_load();
    std::cout << cases << " cases / " << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
