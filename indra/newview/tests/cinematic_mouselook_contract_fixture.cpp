// PPD483: real transition/visibility bodies; unrelated app/avatar services stubbed.
#include <boost/bind/bind.hpp>
#include <boost/signals2.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "alcinematicmode.h"
using U8 = std::uint8_t;
using U32 = std::uint32_t;
std::ostringstream log_sink;
#define LL_INFOS(...) log_sink
#define LL_ENDL '\n'
constexpr int CAMERA_MODE_THIRD_PERSON = 0, CAMERA_MODE_MOUSELOOK = 1, CAMERA_MODE_CUSTOMIZE_AVATAR = 2;
constexpr int LOOKAT_TARGET_CLEAR = 0, ANIM_REQUEST_STOP = 0, ANIM_REQUEST_START = 1;
constexpr int ANIM_AGENT_AIM_RIFLE_R = 1, ANIM_AGENT_HOLD_RIFLE_R = 2;
constexpr int ANIM_AGENT_AIM_HANDGUN_R = 3, ANIM_AGENT_HOLD_HANDGUN_R = 4;
constexpr int ANIM_AGENT_AIM_BAZOOKA_R = 5, ANIM_AGENT_HOLD_BAZOOKA_R = 6;
constexpr int ANIM_AGENT_AIM_BOW_L = 7, ANIM_AGENT_HOLD_BOW_L = 8;
constexpr int ANIM_AGENT_CUSTOMIZE = 9, ANIM_AGENT_CUSTOMIZE_DONE = 10;
constexpr int AGENT_GUN_AIM_ANIMS = 0, NUM_AGENT_GUN_AIM_ANIMS = 0;
constexpr int AGENT_GUN_HOLD_ANIMS = 0, NUM_AGENT_GUN_HOLD_ANIMS = 0;
constexpr int ANIM_AGENT_BODY_NOISE = 11, ANIM_AGENT_BREATHE_ROT = 12, AGENT_CONTROL_MOUSELOOK = 1, VZ = 2;
constexpr float INITIAL_ZOOM_FRACTION = 1.f, MIN_CAMERA_DISTANCE = .1f;
struct Rotation {};
Rotation operator~(Rotation v) { return v; }
struct LLVector3
{
    static const LLVector3 zero;
    float mV[3]{};
    void clearVec() {}
    void normalize() {}
};
const LLVector3 LLVector3::zero;
LLVector3 operator*(LLVector3 v, Rotation) { return v; }
struct LLView
{
    using child_list_t = std::list<LLView*>;
    using child_list_iter_t = child_list_t::iterator;
    using child_list_const_iter_t = child_list_t::const_iterator;
    bool mVisible = true;
    unsigned changes = 0;
    bool getVisible() const { return mVisible; }
    void setVisible(bool value) { mVisible = value; ++changes; }
    // @ACTUAL_SAVED@
    // @ACTUAL_PUSH@
    // @ACTUAL_POP@
    bool saved() const { return mSavedVisible != SAVED_NOTHING; }
};
struct LLFloater : LLView
{
    struct Handle { LLFloater* value; LLFloater* get() const { return value; } };
    boost::signals2::signal<void()> mCloseSignal;
    Handle getHandle() { return {this}; }
    const char* getTitle() const { return "fixture"; }
};
struct LLFloaterView : LLView
{
    using skip_list_t = std::set<LLView*>;
    using hidden_floaters_t = std::vector<std::pair<LLFloater::Handle, boost::signals2::connection>>;
    child_list_t children;
    hidden_floaters_t mHiddenFloaters;
    child_list_t* getChildList() { return &children; }
    void hiddenFloaterClosed(LLFloater*);
    void hideAllFloaters();
    void showHiddenFloaters();
    void pushVisibleAll(bool, const skip_list_t&);
    void popVisibleAll(const skip_list_t&);
} floater_view;
LLFloaterView* gFloaterView = &floater_view;
struct LLFloaterIMContainer
{
    using floater_list_t = std::vector<LLFloater*>;
    void getDetachedConversationFloaters(floater_list_t&) {}
};
struct LLFloaterReg
{
    static inline bool blocked = false;
    static inline unsigned pushes = 0, pops = 0;
    static inline std::map<std::string, LLFloater*> instances;
    static void blockShowFloaters(bool value) { blocked = value; value ? ++pushes : ++pops; }
    static LLFloater* findInstance(const char* name) { auto it = instances.find(name); return it == instances.end() ? nullptr : it->second; }
    template<class T> static T* getTypedInstance(const char*) { static T value; return &value; }
};
// @ACTUAL_HIDDEN_CLOSED@
// @ACTUAL_HIDE@
// @ACTUAL_SHOW@
// @ACTUAL_PUSH_ALL@
// @ACTUAL_POP_ALL@
struct LLAgentCamera
{
    int mCameraMode = 0, last = 0;
    unsigned updates = 0, third_person = 0;
    float mCameraZoomFraction = 0, mCurrentCameraDistance = 0, mTargetCameraDistance = 0;
    bool mCameraAnimating = false;
    LLVector3 mCameraLag;
    int getCameraMode() const { return mCameraMode; }
    int getLastCameraMode() const { return last; }
    void updateLastCamera() { last = mCameraMode; ++updates; }
    void changeCameraToThirdPerson(bool);
    void startCameraAnimation() { mCameraAnimating = true; }
    void clearCameraLag() {}
    void setLookAt(int) {}
} gAgentCamera;
struct Toolbar : LLView { void setToolBarsVisible(bool v) { setVisible(v); } } toolbar;
Toolbar* gToolBarView = &toolbar;
LLView menu, status_container, morph, console;
LLView* gMenuBarView = &menu;
LLView* gMorphView = &morph;
LLView* gConsole = &console;
struct Status : LLView { void setVisibleForMouselook(bool v) { setVisible(v); } } status;
Status* gStatusBar = &status;
#define SINGLETON_VIEW(name) struct name : LLView { static name* getInstance() { static name v; return &v; } static bool instanceExists() { return true; } }
SINGLETON_VIEW(LLNavigationBar);
SINGLETON_VIEW(LLPanelTopInfoBar);
SINGLETON_VIEW(LLChicletBar);
SINGLETON_VIEW(LLPanelStandStopFlying);
struct Settings { U32 navigation = 2; U32 getU32(const char*) { return navigation; } } gSavedSettings;
struct LLViewerWindow
{
    struct NativeWindow { void resetBusyCount() {} } native_window;
    bool mUIVisible = true;
    LLView* mStatusBarContainer = &status_container;
    bool getUIVisibility() { return mUIVisible; }
    void setUIVisibility(bool);
    void showCursor() {}
    NativeWindow* getWindow() { return &native_window; }
} window;
LLViewerWindow* gViewerWindow = &window;
// @ACTUAL_WINDOW@
struct LLHUDText { static inline unsigned refreshes = 0; static void refreshAllObjectText() { ++refreshes; } };
// @ACTUAL_MODE@
struct LLApp { static inline bool exiting = false; static bool isExiting() { return exiting; } };
struct Focus { void setKeyboardFocus(void*) {} } gFocusMgr;
struct LLToolMgr { static LLToolMgr* getInstance() { static LLToolMgr v; return &v; } void setCurrentToolset(int) {} };
int gBasicToolset = 0, gMouselookToolset = 1, gFaceEditToolset = 2;
struct LLFloaterCamera { static void onLeavingMouseLook() {} static void onAvatarEditingAppearance(bool) {} };
struct LLViewerObject { bool flagCameraDecoupled() { return false; } Rotation getRenderRotation() { return {}; } };
struct Avatar
{
    struct Pelvis { void setPosition(LLVector3) {} } pelvis;
    Pelvis* mPelvisp = &pelvis;
    std::map<int, int> mSignaledAnimations;
    bool isAnyAnimationSignaled(int, int) { return false; }
    bool isSitting() { return false; }
    void startMotion(int) {}
    LLViewerObject* getParent() { return nullptr; }
    LLViewerObject* getRoot() { return nullptr; }
    int requestPause() { return 0; }
    void updateAttachmentVisibility(int) {}
} avatar;
Avatar* gAgentAvatarp = &avatar;
bool isAgentAvatarValid() { return false; }
struct LLViewerCamera { static LLViewerCamera* getInstance() { static LLViewerCamera v; return &v; } LLVector3 getAtAxis() { return {}; } };
struct Tools { void dirty() {} };
Tools* gFloaterTools = nullptr;
struct LLAgent
{
    struct Frame { LLVector3 getAtAxis() { return {}; } } frame;
    bool mViewsPushed = false, mCustomAnim = false;
    int mPauseRequest = 0;
    std::function<void()>* mMouselookModeInSignal = nullptr;
    std::function<void()>* mMouselookModeOutSignal = nullptr;
    void sendAnimationRequest(int, int) {}
    void resetAxes(LLVector3) {}
    void unpauseAnimation() {}
    void clearControlFlags(int) {}
    Frame& getFrameAgent() { return frame; }
    void endAnimationUpdateUI();
} agent;
LLAgent& gAgent = agent;
// @ACTUAL_AGENT@
struct LLViewerJoystick
{
    bool override_camera = false;
    static LLViewerJoystick* getInstance() { static LLViewerJoystick value; return &value; }
    bool getOverrideCamera() { return override_camera; }
};
struct AOEngine { static AOEngine* getInstance() { static AOEngine value; return &value; } void inMouselook(bool) {} };
// @ACTUAL_CAMERA@
unsigned checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Scene
{
    LLFloater open, hidden, minimap;
    Scene()
    {
        gAgentCamera = {}; agent = {}; window.mUIVisible = true;
        LLViewerJoystick::getInstance()->override_camera = false;
        floater_view.mHiddenFloaters.clear();
        floater_view.children = {&open, &hidden, &minimap};
        hidden.setVisible(false);
        LLFloaterReg::instances = {{"mini_map", &minimap}};
        LLFloaterReg::blocked = false;
        LLFloaterReg::pushes = LLFloaterReg::pops = 0;
    }
    ~Scene()
    {
        // Connections refer to these fixture objects; drain them while alive.
        floater_view.showHiddenFloaters();
        floater_view.children.clear();
        LLFloaterReg::instances.clear();
    }
    void enter() { gAgentCamera.mCameraMode = CAMERA_MODE_MOUSELOOK; agent.endAnimationUpdateUI(); }
    void leave() { gAgentCamera.mCameraMode = 0; agent.endAnimationUpdateUI(); }
    void balanced()
    {
        check(!agent.mViewsPushed && !open.saved() && !hidden.saved(), "push/pop save slots consumed");
        check(LLFloaterReg::pushes == LLFloaterReg::pops && !LLFloaterReg::blocked, "push/pop blocking balanced");
    }
};
void normal()
{
    for (bool ui_hidden : {false, true})
    {
        Scene s;
        if (ui_hidden) window.setUIVisibility(false);
        s.enter();
        check(!s.open.getVisible() && !s.hidden.getVisible() && s.minimap.getVisible(), "normal mouselook preserves exceptions and hides ordinary floaters");
        check(agent.mViewsPushed && LLFloaterReg::blocked, "normal entering pushes and blocks");
        const auto pushes = LLFloaterReg::pushes;
        agent.endAnimationUpdateUI();
        check(LLFloaterReg::pushes == pushes, "same camera mode does not push twice");
        ALCinematicMode::toggle();
        check(!ALCinematicMode::isEnabled(), "cannot activate mode while current camera is mouselook");
        s.leave();
        check(s.open.getVisible() && !s.hidden.getVisible() && s.minimap.getVisible(), "normal exit restores exact ordinary visibility");
        check(window.getUIVisibility(), "normal mouselook keeps prior UI-unhide contract");
        s.balanced();
    }
}
void cinematic_first()
{
    for (bool ui_hidden : {false, true})
    {
        Scene s;
        if (ui_hidden) window.setUIVisibility(false);
        ALCinematicMode::toggle();
        const auto saved = floater_view.mHiddenFloaters.size();
        check(ALCinematicMode::isEnabled() && !s.open.getVisible(), "cinematic mode hides and owns initial visible floaters");
        for (unsigned i = 0; i < 3; ++i)
        {
            s.enter();
            check(!window.getUIVisibility(), "entering mouselook does not unhide cinematic UI owner");
            check(floater_view.mHiddenFloaters.size() == saved, "entering mouselook preserves saved-hidden list");
            check(!s.open.getVisible() && !s.hidden.getVisible() && !s.minimap.getVisible(), "cinematic mode also keeps mouselook exception hidden");
            s.leave();
            check(!s.open.getVisible() && !s.hidden.getVisible() && !s.minimap.getVisible(), "leaving mouselook keeps all cinematic floaters hidden");
            check(floater_view.mHiddenFloaters.size() == saved, "repeated transitions retain exact hidden ownership");
            s.balanced();
        }
        ALCinematicMode::toggle();
        check(!ALCinematicMode::isEnabled() && window.getUIVisibility() == !ui_hidden, "cinematic exit restores prior interface visibility");
        check(s.open.getVisible() == !ui_hidden && !s.hidden.getVisible() && s.minimap.getVisible() == !ui_hidden, "cinematic exit restores only originally visible floaters");
        if (ui_hidden) window.setUIVisibility(true);
        check(s.open.getVisible() && !s.hidden.getVisible() && s.minimap.getVisible(), "preexisting manual UI hide can still restore its saved floaters");
    }
}
void cinematic_during_pending_pop()
{
    for (bool deferred : {false, true})
    {
    Scene s;
    s.enter();
    // Camera leaves mouselook before its UI transition runs; a mode action can
    // occur while that old saved-visible slot is still outstanding.
    gAgentCamera.mCameraMode = 0;
    LLViewerJoystick::getInstance()->override_camera = deferred;
    check(agent.mViewsPushed && s.open.saved(), "old mouselook save slot is pending before mode activation");
    ALCinematicMode::toggle();
    check(ALCinematicMode::isEnabled(), "mode activates while prior mouselook UI transition is pending");
    check(agent.mViewsPushed == deferred && s.open.saved() == deferred,
          "actual camera owner performs nested pop unless joystick override defers it");
    check(!s.open.getVisible() && !s.minimap.getVisible(), "pending transition starts hidden");
    LLViewerJoystick::getInstance()->override_camera = false;
    agent.endAnimationUpdateUI();
    check(!s.open.getVisible() && !s.hidden.getVisible() && !s.minimap.getVisible(), "pending pop cannot reveal floaters while cinematic owns UI");
    check(floater_view.mHiddenFloaters.size() == 2, "pop-restored floater joins existing cinematic hidden owner");
    s.balanced();
    ALCinematicMode::toggle();
    check(s.open.getVisible() && !s.hidden.getVisible() && s.minimap.getVisible(), "cinematic exit restores pre-mouselook visible state after pending pop");
    check(floater_view.mHiddenFloaters.empty(), "successful restoration drains saved-hidden list");
    }
}
void guards()
{
    for (unsigned guard = 0; guard < 5; ++guard)
    {
        Scene s;
        gAgentCamera.mCameraMode = CAMERA_MODE_MOUSELOOK;
        if (guard == 0) LLApp::exiting = true;
        if (guard == 1) gViewerWindow = nullptr;
        if (guard == 2) gMenuBarView = nullptr;
        if (guard == 3) gToolBarView = nullptr;
        if (guard == 4) gStatusBar = nullptr;
        agent.endAnimationUpdateUI();
        check(!agent.mViewsPushed && s.open.getVisible() && gAgentCamera.updates == 0, "app/owner guard leaves camera and visibility ownership untouched");
        LLApp::exiting = false; gViewerWindow = &window; gMenuBarView = &menu; gToolBarView = &toolbar; gStatusBar = &status;
    }
    Scene s;
    gAgentCamera.last = CAMERA_MODE_MOUSELOOK;
    gAgentCamera.mCameraMode = 0;
    agent.endAnimationUpdateUI();
    check(LLFloaterReg::pops == 0 && !agent.mViewsPushed, "leaving without a prior push never pops");
    s.balanced();
}
int main()
{
    normal(); cinematic_first(); cinematic_during_pending_pop(); guards();
    check(!ALCinematicMode::isEnabled(), "all scenarios leave mode off");
    std::cout << "PPD483 cinematic/mouselook " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
