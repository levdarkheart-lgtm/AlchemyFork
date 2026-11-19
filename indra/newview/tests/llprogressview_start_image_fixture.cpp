// PPD-480: the driver inserts the complete production initStartTexture body.
// File/decode/native creation are controlled CPU boundaries; no viewer is launched.
#include <bit>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using S32=int;using U8=std::uint8_t;
constexpr S32 START_LOCATION_ID_HOME=0,START_LOCATION_ID_LAST=1,START_LOCATION_ID_URL=2;
constexpr U8 IMG_CODEC_PNG=1,IMG_CODEC_BMP=2;
#define LL_INFOS(...) std::ostringstream()
#define LL_DEBUGS(...) std::ostringstream()
#define LL_WARNS(...) std::ostringstream()
#define LL_ENDL std::endl
struct Ref{int refs=0;virtual ~Ref()=default;void ref(){++refs;}void unref(){if(!--refs)delete this;}};
template<class T>struct LLPointer
{
    T*p=nullptr;LLPointer()=default;LLPointer(T*q):p(q){if(p)p->ref();}
    LLPointer(const LLPointer&x):LLPointer(x.p){}~LLPointer(){if(p)p->unref();}
    LLPointer&operator=(LLPointer x){std::swap(p,x.p);return *this;}
    T*operator->()const{return p;}operator T*()const{return p;}T*get()const{return p;}bool notNull()const{return p;}bool isNull()const{return !p;}
};
struct Record{bool present=true,load=true,decode=true;int width=400,height=250;};
struct Inputs
{
    std::map<std::string,Record>files;std::vector<std::pair<std::string,U8>>loads;
    int native_calls=0;bool native_failure=false;
} input;
struct Settings{bool enabled=true;bool getBOOL(const std::string&name)const{assert(name=="UseStartScreen");return enabled;}}gSavedSettings;
struct Dir
{
    std::string getLindenUserDir()const{return "/profile";}std::string getDirDelimiter()const{return "/";}
    bool fileExists(const std::string&name)const{auto it=input.files.find(name);return it!=input.files.end()&&it->second.present;}
} directory;
Dir*gDirUtilp=&directory;
struct LLStartUp
{
    static inline std::string suffix;
    static std::string getScreenHomeFilename(){return "screen_home"+suffix+".png";}
    static std::string getScreenLastFilename(){return "screen_last"+suffix+".png";}
};
struct LLImageRaw:Ref
{
    static inline int alive=0;int width=0,height=0;bool expanded=false;std::string source;
    LLImageRaw(){++alive;}~LLImageRaw(){--alive;}
    void expandToPowerOfTwo(){expanded=true;width=std::bit_ceil(unsigned(width));height=std::bit_ceil(unsigned(height));}
};
struct LLImageFormatted:Ref
{
    static inline int alive=0;U8 codec;Record record;std::string path;
    explicit LLImageFormatted(U8 c):codec(c){++alive;}~LLImageFormatted(){--alive;}
    static LLImageFormatted*createFromType(U8 c){return new LLImageFormatted(c);}
    bool load(const std::string&name){path=name;input.loads.emplace_back(name,codec);auto it=input.files.find(name);if(it==input.files.end())return false;record=it->second;return record.present&&record.load;}
    S32 getWidth()const{return record.width;}S32 getHeight()const{return record.height;}
    bool decode(LLImageRaw*raw,float){if(!record.decode)return false;raw->width=record.width;raw->height=record.height;raw->source=path;return true;}
};
struct LLViewerTexture:Ref
{
    static inline int alive=0,destroyed=0;int width,height;std::string source;bool published=true;
    static LLPointer<LLViewerTexture>sBlackImagep;
    LLViewerTexture(std::string name,int w,int h):width(w),height(h),source(std::move(name)){++alive;}
    ~LLViewerTexture(){--alive;++destroyed;}
    S32 getWidth()const{return width;}S32 getHeight()const{return height;}
};
LLPointer<LLViewerTexture>LLViewerTexture::sBlackImagep=new LLViewerTexture("black",1,1);
LLPointer<LLViewerTexture>gStartTexture;S32 gStartImageWidth=1,gStartImageHeight=1;
struct LLViewerTextureManager
{
    static LLPointer<LLViewerTexture>getLocalTexture(const LLImageRaw*raw,bool mipmaps)
    {
        assert(!mipmaps&&raw&&raw->expanded);++input.native_calls;
        LLPointer<LLViewerTexture>texture=new LLViewerTexture(raw->source,raw->width,raw->height);texture->published=!input.native_failure;return texture;
    }
};
struct LLProgressView{void initStartTexture(S32,bool);};
// @PRODUCTION@
int checks=0;
void check(bool pass,const char*expression,int line){++checks;if(!pass)throw std::runtime_error(std::string(expression)+"@"+std::to_string(line));}
#define CHECK(x)check(bool(x),#x,__LINE__)
void reset(){gStartTexture=nullptr;input={};gSavedSettings.enabled=true;LLStartUp::suffix.clear();gStartImageWidth=gStartImageHeight=1;}
void file(const std::string&name,Record r={}){input.files["/profile/"+name]=r;}
void selected(const std::string&name){CHECK(gStartTexture.notNull());CHECK(gStartTexture->source=="/profile/"+name);}
void black(){CHECK(gStartTexture.get()==LLViewerTexture::sBlackImagep.get());CHECK(gStartImageWidth==1&&gStartImageHeight==1);}
int main()
{
    reset();file("screen_last.png");LLProgressView probe;probe.initStartTexture(START_LOCATION_ID_HOME,true);
    std::cout<<"PROBE home_absent_last_available black="<<(gStartTexture.get()==LLViewerTexture::sBlackImagep.get())<<" last="<<(gStartTexture.notNull()&&gStartTexture->source=="/profile/screen_last.png")<<" attempts="<<input.loads.size()<<'\n';
    using Case=std::pair<const char*,std::function<void()>>;
    std::vector<Case>cases={
        {"missing Home PNG and BMP use available last image",[]{file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.loads.size()==2);CHECK(input.loads[0].first=="/profile/screen_home.bmp");CHECK(input.loads[1].second==IMG_CODEC_PNG);}},
        {"valid Home PNG keeps precedence over BMP and last",[]{file("screen_home.png");file("screen_home.bmp");file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_home.png");CHECK(input.loads.size()==1&&input.loads[0].second==IMG_CODEC_PNG);}},
        {"missing Home PNG keeps legacy production BMP precedence",[]{file("screen_home.bmp");file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_home.bmp");CHECK(input.loads.size()==1&&input.loads[0].second==IMG_CODEC_BMP);}},
        {"unloadable selected Home PNG uses last",[]{file("screen_home.png",{true,false,true});file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.loads.size()==2);}},
        {"failed Home pixel decode uses last",[]{file("screen_home.png",{true,true,false});file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.native_calls==1);}},
        {"failed Home BMP load uses last",[]{file("screen_home.bmp",{true,false,true});file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.loads[0].second==IMG_CODEC_BMP);}},
        {"failed Home BMP decode uses last",[]{file("screen_home.bmp",{true,true,false});file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.native_calls==1);}},
        {"existing corrupt PNG does not change legacy BMP selector",[]{file("screen_home.png",{true,true,false});file("screen_home.bmp");file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(input.loads[0].second==IMG_CODEC_PNG&&input.loads[1].second==IMG_CODEC_PNG);}},
        {"missing all candidates stays black with bounded attempts",[]{LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);black();CHECK(input.loads.size()==2);CHECK(input.native_calls==0);}},
        {"corrupt all chosen candidates stays black",[]{file("screen_home.png",{true,true,false});file("screen_home.bmp",{true,false,false});file("screen_last.png",{true,true,false});LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);black();CHECK(input.loads.size()==2);CHECK(input.native_calls==0);}},
        {"LAST choice never tries Home",[]{file("screen_home.png");file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_LAST,true);selected("screen_last.png");CHECK(input.loads.size()==1);}},
        {"missing LAST choice stays black despite Home image",[]{file("screen_home.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_LAST,true);black();CHECK(input.loads.size()==1&&input.loads[0].first=="/profile/screen_last.png");}},
        {"corrupt LAST choice stays black despite Home image",[]{file("screen_home.png");file("screen_last.png",{true,true,false});LLProgressView p;p.initStartTexture(START_LOCATION_ID_LAST,true);black();CHECK(input.loads.size()==1);}},
        {"nonproduction fallback uses grid-specific last filename",[]{LLStartUp::suffix=".example-grid";file("screen_home.bmp");file("screen_last.example-grid.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,false);selected("screen_last.example-grid.png");CHECK(input.loads[0].first=="/profile/screen_home.example-grid.png");CHECK(input.loads[0].second==IMG_CODEC_PNG);}},
        {"nonproduction Home image keeps grid-specific precedence",[]{LLStartUp::suffix=".example-grid";file("screen_home.example-grid.png");file("screen_last.example-grid.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,false);selected("screen_home.example-grid.png");CHECK(input.loads.size()==1);}},
        {"grid-specific LAST choice stays direct",[]{LLStartUp::suffix=".example-grid";file("screen_last.example-grid.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_LAST,false);selected("screen_last.example-grid.png");CHECK(input.loads.size()==1);}},
        {"URL login retains Home selection with last fallback",[]{file("screen_last.png");LLProgressView p;p.initStartTexture(START_LOCATION_ID_URL,true);selected("screen_last.png");CHECK(input.loads.size()==2);}},
        {"UseStartScreen optout loads and creates nothing",[]{file("screen_home.png");file("screen_last.png");gStartTexture=new LLViewerTexture("previous",2,2);gSavedSettings.enabled=false;LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);CHECK(gStartTexture.isNull());CHECK(input.loads.empty());CHECK(input.native_calls==0);}},
        {"reinitialization releases old texture before replacement",[]{file("screen_last.png");gStartTexture=new LLViewerTexture("previous",2,2);int old=LLViewerTexture::destroyed;LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(LLViewerTexture::destroyed==old+1);CHECK(input.native_calls==1);}},
        {"original image dimensions survive power-of-two expansion",[]{file("screen_last.png",{true,true,true,3456,2168});LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_last.png");CHECK(gStartImageWidth==3456&&gStartImageHeight==2168);CHECK(gStartTexture->getWidth()==4096&&gStartTexture->getHeight()==4096);}},
        {"native publication failure does not replace valid Home",[]{file("screen_home.png");file("screen_last.png");input.native_failure=true;LLProgressView p;p.initStartTexture(START_LOCATION_ID_HOME,true);selected("screen_home.png");CHECK(!gStartTexture->published);CHECK(input.loads.size()==1);CHECK(input.native_calls==1);}}
    };
    int failures=0;for(auto&[name,run]:cases){reset();try{run();CHECK(LLImageRaw::alive==0);CHECK(LLImageFormatted::alive==0);std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
    gStartTexture=nullptr;
    std::cout<<cases.size()<<" cases, "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
