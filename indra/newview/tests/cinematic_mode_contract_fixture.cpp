// PPD474: actual mode transition and world-hover consumer, stopping at UI owners.
#include <iostream>
#include <sstream>
#include <string>
#include "alcinematicmode.h"
std::ostringstream log_sink;
#define LL_INFOS(...) log_sink
#define LL_ENDL '\n'
constexpr int CAMERA_MODE_MOUSELOOK=1;
struct Camera { int mode=0; int getCameraMode()const{return mode;} } gAgentCamera;
struct Window { bool visible=true; bool getUIVisibility()const{return visible;} void setUIVisibility(bool v){visible=v;} } window;
Window* gViewerWindow=&window;
struct LLChicletBar { bool visible=true; static LLChicletBar* getInstance(){static LLChicletBar p;return &p;} bool getVisible()const{return visible;} void setVisible(bool v){visible=v;} };
struct LLPanelStandStopFlying { bool visible=false; static LLPanelStandStopFlying* getInstance(){static LLPanelStandStopFlying p;return &p;} bool getVisible()const{return visible;} void setVisible(bool v){visible=v;} };
struct Settings { bool hide_hover=false; } gSavedSettings;
template<class T> struct LLCachedControl { LLCachedControl(Settings&,const char*,T){} operator bool()const{return gSavedSettings.hide_hover;} };
constexpr bool GL_TRUE=true,GL_FALSE=false;
struct LLGLDepthTest { LLGLDepthTest(bool,bool){} };
struct LLHUDText {
    static inline int refreshes=0;
    static inline bool sDisplayText=true;
    bool mOnHUDAttachment=false;int rendered=0;
    static void refreshAllObjectText(){++refreshes;}
    void renderText(){++rendered;}
    void render();
};
// @ACTUAL_MODE@
// @ACTUAL_HOVER@
int checks=0,failures=0;
void check(bool ok,const char*name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
int main(){
    check(!ALCinematicMode::isEnabled(),"mode starts off");
    for(unsigned mask=0;mask<8;++mask){
        window.visible=mask&1;LLChicletBar::getInstance()->visible=mask&2;LLPanelStandStopFlying::getInstance()->visible=mask&4;
        int before=LLHUDText::refreshes;
        ALCinematicMode::toggle();
        check(ALCinematicMode::isEnabled(),"mode enters");check(!window.visible,"interface hidden");check(!LLChicletBar::getInstance()->visible,"chiclets hidden");check(!LLPanelStandStopFlying::getInstance()->visible,"movement controls hidden");
        check(LLHUDText::refreshes==before+1,"hover refresh on enter");
        for(bool hide:{false,true})for(bool on_hud:{false,true}){
            gSavedSettings.hide_hover=hide;LLHUDText text;text.mOnHUDAttachment=on_hud;text.render();
            check(text.rendered==(!hide&&!on_hud?1:0),"cinematic hover preference actually consumed");
        }
        gAgentCamera.mode=CAMERA_MODE_MOUSELOOK;ALCinematicMode::toggle();
        check(ALCinematicMode::isEnabled(),"mouselook does not change active mode");
        gAgentCamera.mode=0;ALCinematicMode::toggle();
        check(!ALCinematicMode::isEnabled(),"mode exits");check(window.visible==bool(mask&1),"prior interface restored");check(LLChicletBar::getInstance()->visible==bool(mask&2),"prior chiclets restored");check(LLPanelStandStopFlying::getInstance()->visible==bool(mask&4),"prior movement controls restored");
        check(LLHUDText::refreshes==before+2,"hover refresh on exit");
        for(bool hide:{false,true}){
            gSavedSettings.hide_hover=hide;LLHUDText text;text.render();check(text.rendered==1,"setting does not hide hover outside mode");
        }
    }
    gAgentCamera.mode=CAMERA_MODE_MOUSELOOK;int before=LLHUDText::refreshes;ALCinematicMode::toggle();
    check(!ALCinematicMode::isEnabled(),"mouselook cannot enter mode");check(LLHUDText::refreshes==before,"mouselook unchanged does not refresh");
    std::cout<<"PPD474 cinematic "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
