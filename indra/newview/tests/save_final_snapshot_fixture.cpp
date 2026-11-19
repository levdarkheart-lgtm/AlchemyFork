// PPD493: complete actual saveFinalSnapshot body, synthetic CPU boundaries.
// No viewer, native renderer, account directory or real image file is used.
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct LLVector3d
{
    double x=0,y=0,z=0;
    bool operator==(const LLVector3d&) const = default;
};
double dist_vec(const LLVector3d& a,const LLVector3d& b)
{ return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
constexpr int UI_CURSOR_WAIT=1;
struct LLSnapshotModel { enum { SNAPSHOT_TYPE_COLOR=2,SNAPSHOT_FORMAT_PNG=3 }; };
struct Input
{
    bool snapshot_success=true;
    int idle_calls=0,copies=0,exists_queries=0,home_queries=0;
    std::map<std::string,std::string> files;
    std::vector<std::string> events;
} input;
struct Settings
{
    bool hud=true,parcel_owners=true;
    std::map<std::string,LLVector3d> positions;
    void setVector3d(const std::string& name,const LLVector3d& value) { positions[name]=value; }
    void setBOOL(const std::string& name,bool value)
    { if(name=="ShowParcelOwners")parcel_owners=value; }
    bool getBOOL(const std::string& name) const { return name=="RenderHUDInSnapshot"&&hud; }
} gSavedSettings;
struct Camera
{
    int changes=0;bool animated=true;
    LLVector3d calcFocusPositionTargetGlobal()const { return {1,2,3}; }
    LLVector3d calcCameraPositionTargetGlobal()const { return {4,5,6}; }
    void changeCameraToThirdPerson(bool animate) { ++changes;animated=animate; }
} gAgentCamera;
struct Agent
{
    bool home_region=true,home_known=true;LLVector3d home,position;
    bool isInHomeRegion()const { ++input.home_queries;return home_region; }
    bool getHomePosGlobal(LLVector3d* value)const { *value=home;return home_known; }
    LLVector3d getPositionGlobal()const { return position; }
} gAgent;
struct Dir
{
    std::string getLindenUserDir()const { return "/synthetic-profile"; }
    std::string getDirDelimiter()const { return "/"; }
    bool fileExists(const std::string& name)const { ++input.exists_queries;return input.files.contains(name); }
} directory;
Dir* gDirUtilp=&directory;
struct LLStartUp
{
    static inline std::string grid_suffix;
    static std::string getScreenLastFilename() { return "screen_last"+grid_suffix+".png"; }
    static std::string getScreenHomeFilename() { return "screen_home"+grid_suffix+".png"; }
};
struct LLFile
{
    static bool copy(const std::string& from,const std::string& to)
    {
        ++input.copies;input.events.push_back("copy");
        auto found=input.files.find(from);
        if(found==input.files.end())return false;
        input.files[to]=found->second;return true;
    }
};
struct Capture
{
    std::string path;int width=0,height=0,type=0,format=0;
    bool ui=false,hud=false,rebuild=false,balance=false;
};
struct ViewerWindow
{
    std::vector<Capture> captures;int cursor_calls=0;
    void setCursor(int) { ++cursor_calls; }
    int getWindowWidthRaw()const { return 1280; }
    int getWindowHeightRaw()const { return 900; }
    bool saveSnapshot(const std::string& path,int width,int height,bool ui,bool hud,
                      bool rebuild,bool balance,int type,int format)
    {
        input.events.push_back("capture");
        captures.push_back({path,width,height,type,format,ui,hud,rebuild,balance});
        // Actual saveSnapshot does not encode/write when rawSnapshot fails.
        if(input.snapshot_success)input.files[path]="synthetic-fresh-capture";
        return input.snapshot_success;
    }
} window;
ViewerWindow* gViewerWindow=&window;
struct LLAppViewer
{
    bool mSavedFinalSnapshot=false;
    void idle() { ++input.idle_calls;input.events.push_back("idle"); }
    void saveFinalSnapshot();
};
// @PRODUCTION@

int cases=0,checks=0,failures=0;
void check(bool pass,const char* expression,int line)
{ ++checks;if(!pass){++failures;std::cerr<<"FAIL line "<<line<<": "<<expression<<'\n';} }
#define CHECK(x) check(bool(x),#x,__LINE__)
std::string last() { return directory.getLindenUserDir()+"/"+LLStartUp::getScreenLastFilename(); }
std::string home() { return directory.getLindenUserDir()+"/"+LLStartUp::getScreenHomeFilename(); }
LLAppViewer reset()
{
    ++cases;input={};gSavedSettings={};gAgentCamera={};gAgent={};window={};LLStartUp::grid_suffix.clear();
    return {};
}
void checkCaptureContract()
{
    CHECK(window.captures.size()==1);
    const Capture& c=window.captures.front();
    CHECK(c.path==last());CHECK(c.width==1280&&c.height==900);
    CHECK(!c.ui&&c.hud==gSavedSettings.hud&&c.rebuild&&!c.balance);
    CHECK(c.type==LLSnapshotModel::SNAPSHOT_TYPE_COLOR&&c.format==LLSnapshotModel::SNAPSHOT_FORMAT_PNG);
    CHECK(gAgentCamera.changes==1&&!gAgentCamera.animated);
    CHECK(input.idle_calls==1&&window.cursor_calls==1);
    CHECK(input.events.size()>=2&&input.events[0]=="idle"&&input.events[1]=="capture");
    CHECK(!gSavedSettings.parcel_owners);
    CHECK(gSavedSettings.positions["FocusPosOnLogout"]==gAgentCamera.calcFocusPositionTargetGlobal());
    CHECK(gSavedSettings.positions["CameraPosOnLogout"]==gAgentCamera.calcCameraPositionTargetGlobal());
}
int main()
{
    // Failed new capture must not promote an old last image into absent Home.
    auto app=reset();input.snapshot_success=false;input.files[last()]="synthetic-old-last";
    app.saveFinalSnapshot();checkCaptureContract();
    CHECK(!app.mSavedFinalSnapshot);CHECK(input.copies==0);CHECK(!input.files.contains(home()));
    CHECK(input.files[last()]=="synthetic-old-last");
    CHECK(input.home_queries==0&&input.exists_queries==0);

    app=reset();input.snapshot_success=false;app.saveFinalSnapshot();
    CHECK(!app.mSavedFinalSnapshot);CHECK(!input.files.contains(last()));
    CHECK(!input.files.contains(home()));CHECK(input.copies==0&&input.home_queries==0);

    app=reset();input.snapshot_success=false;input.files[last()]="synthetic-old-last";
    input.files[home()]="synthetic-existing-home";app.saveFinalSnapshot();
    CHECK(!app.mSavedFinalSnapshot);CHECK(input.files[home()]=="synthetic-existing-home");
    CHECK(input.files[last()]=="synthetic-old-last");CHECK(input.copies==0);

    // No loop is introduced: an external later call may try again after failure.
    app=reset();input.snapshot_success=false;app.saveFinalSnapshot();
    CHECK(window.captures.size()==1&&!app.mSavedFinalSnapshot);
    input.snapshot_success=true;app.saveFinalSnapshot();
    CHECK(window.captures.size()==2&&app.mSavedFinalSnapshot);
    CHECK(input.files[last()]=="synthetic-fresh-capture");
    CHECK(input.files[home()]=="synthetic-fresh-capture"&&input.copies==1);
    app.saveFinalSnapshot();CHECK(window.captures.size()==2&&input.copies==1);

    app=reset();app.saveFinalSnapshot();checkCaptureContract();
    CHECK(app.mSavedFinalSnapshot&&input.copies==1);
    CHECK(input.files[last()]=="synthetic-fresh-capture"&&input.files[home()]=="synthetic-fresh-capture");
    CHECK(input.events.back()=="copy");
    app.saveFinalSnapshot();CHECK(window.captures.size()==1&&input.copies==1);

    app=reset();app.mSavedFinalSnapshot=true;app.saveFinalSnapshot();
    CHECK(window.captures.empty()&&input.copies==0&&input.idle_calls==0);
    CHECK(gSavedSettings.positions.empty()&&gSavedSettings.parcel_owners);
    CHECK(gAgentCamera.changes==0&&window.cursor_calls==0&&input.home_queries==0);

    app=reset();input.files[home()]="synthetic-existing-home";app.saveFinalSnapshot();
    CHECK(app.mSavedFinalSnapshot&&input.copies==0);
    CHECK(input.files[home()]=="synthetic-existing-home");

    app=reset();gAgent.home_region=false;app.saveFinalSnapshot();
    CHECK(app.mSavedFinalSnapshot&&input.copies==0&&!input.files.contains(home()));
    CHECK(input.files[last()]=="synthetic-fresh-capture");

    app=reset();gAgent.home_known=false;app.saveFinalSnapshot();
    CHECK(app.mSavedFinalSnapshot&&input.copies==0&&!input.files.contains(home()));

    app=reset();gAgent.position={10,0,0};app.saveFinalSnapshot();
    CHECK(app.mSavedFinalSnapshot&&input.copies==0&&!input.files.contains(home()));

    app=reset();gAgent.position={9.99,0,0};app.saveFinalSnapshot();
    CHECK(app.mSavedFinalSnapshot&&input.copies==1&&input.files.contains(home()));

    app=reset();LLStartUp::grid_suffix="_synthetic-grid";gSavedSettings.hud=false;
    app.saveFinalSnapshot();checkCaptureContract();
    CHECK(input.files.contains(last())&&input.files.contains(home()));
    CHECK(!input.files.contains("/synthetic-profile/screen_last.png"));
    CHECK(!input.files.contains("/synthetic-profile/screen_home.png"));
    std::cout<<cases<<" cases, "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
