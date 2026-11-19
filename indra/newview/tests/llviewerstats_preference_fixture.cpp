// PPD491: actual preference-to-log/XML/HTTP tail, real LLSD/serialization,
// controlled settings, recording and sinks. All identities/data are synthetic.
#include "linden_common.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "lluuid.h"
#include "lluri.h"
#include "alsafepreferencetelemetry.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Sinks
{
    std::vector<std::string> info, xml;
    LLSD post;
    int posts=0, resumes=0;
} sinks;
struct LogEnd {};
LogEnd logEnd() { return {}; }
std::ostream& operator<<(std::ostream& out, LogEnd) { return out; }
struct LogLine : std::ostringstream
{
    ~LogLine() { sinks.info.push_back(str()); }
};
struct CapturedOutput : std::ostringstream
{
    explicit CapturedOutput(const char*) {}
    ~CapturedOutput() { sinks.xml.push_back(str()); }
};
#undef LL_INFOS
#undef LL_DEBUGS
#undef LL_ENDL
#define LL_INFOS(...) LogLine()
#define LL_DEBUGS(...) (void)0
#define LL_ENDL logEnd()
#define llofstream CapturedOutput

enum { TYPE_STRING, TYPE_LLSD };
struct Control
{
    int type=TYPE_LLSD;
    bool is_default=false;
    bool isType(int candidate) const { return type==candidate; }
    bool isDefault() const { return is_default; }
};
struct LLControlGroup
{
    std::map<std::string,std::shared_ptr<Control>> mNameTable;
    LLSD values;
    LLSD getLLSD(const std::string& name) { return values[name]; }
    LLSD asLLSD(bool diffs_only);
};
// @AS_LLSD@

struct Settings
{
    LLSD diffs=LLSD::emptyMap();
    int reads=0;
    bool diffs_only=false;
    LLSD asLLSD(bool only) { ++reads; diffs_only=only; return diffs; }
} gSavedSettings, gSavedPerAccountSettings;
LLUUID gAgentSessionID("11111111-2222-3333-4444-555555555555");
namespace LLStatViewer { enum { ENABLE_VBO, AGENT_POSITION_SNAP }; }
struct Number
{
    operator double() const { return 2.; }
    double value() const { return 2.; }
};
struct LLViewerStats
{
    struct Recording
    {
        Number getMean(int) const { return {}; }
        Number getSum(int) const { return {}; }
        Number getStandardDeviation(int) const { return {}; }
        int getSampleCount(int) const { return 3; }
        void resume() { ++sinks.resumes; }
    } recording;
    static LLViewerStats& instance() { static LLViewerStats stats; return stats; }
    static LLViewerStats* getInstance() { return &instance(); }
    Recording& getRecording() { return recording; }
    void addToMessage(LLSD& body);
};
namespace LLCoreHttpUtil
{
struct HttpCoroutineAdapter
{
    static void messageHttpPost(const std::string&, const LLSD& body, const char*, const char*)
    { ++sinks.posts; sinks.post=body; }
};
}

// @ADD_TO_MESSAGE@

void dispatch(bool include_preferences)
{
    const std::string url="synthetic://statistics";
    LLSD body;
    body["unrelated_numeric_stat"]=37;
    // @PRODUCTION_TAIL@
}

int checks=0, failures=0, cases=0;
void check(bool pass, const char* expression, int line)
{
    ++checks;
    if (!pass) { ++failures; std::cerr << "FAIL line " << line << ": " << expression << '\n'; }
}
#define CHECK(x) check(bool(x),#x,__LINE__)
void reset() { sinks={}; gSavedSettings={}; gSavedPerAccountSettings={}; ++cases; }
std::string xml(const LLSD& value) { std::ostringstream out; LLSDSerialize::toPrettyXML(value,out); return out.str(); }
void checkSinks()
{
    CHECK(sinks.posts==1);
    CHECK(sinks.resumes==1);
    CHECK(sinks.xml.size()==1);
    CHECK(sinks.info.size()==2);
    CHECK(sinks.post["session_id"].asUUID()==gAgentSessionID);
    CHECK(sinks.post["unrelated_numeric_stat"].asInteger()==37);
    CHECK(sinks.post["misc"]["Version"].asBoolean());
    CHECK(sinks.post["AgentPositionSnaps"].asReal()==2.);
    CHECK(!sinks.post["MinimalSkin"].asBoolean());
    CHECK(sinks.xml.front().find(gAgentSessionID.asString())==std::string::npos);
    for (const auto& log:sinks.info)
        CHECK(log.find(gAgentSessionID.asString())==std::string::npos);
}
void noSyntheticSecret()
{
    // Never print the value: failure output contains only this assertion.
    const std::string marker="SYNTHETIC_PRIVATE_VALUE_491";
    CHECK(xml(sinks.post).find(marker)==std::string::npos);
    for (const auto& log:sinks.info) CHECK(log.find(marker)==std::string::npos);
    for (const auto& file:sinks.xml) CHECK(file.find(marker)==std::string::npos);
}
int main()
{
    // The actual control-group exporter filters declared String, not strings
    // inside declared LLSD. Only storage/metadata owners are controlled here.
    reset();
    LLControlGroup storage;
    storage.mNameTable["DeclaredString"]=std::make_shared<Control>(Control{TYPE_STRING,false});
    storage.mNameTable["AzureTranslateAPIKey"]=std::make_shared<Control>();
    storage.mNameTable["DefaultOnly"]=std::make_shared<Control>(Control{TYPE_LLSD,true});
    storage.values["DeclaredString"]="SYNTHETIC_PRIVATE_VALUE_491";
    storage.values["AzureTranslateAPIKey"]["key"]="SYNTHETIC_PRIVATE_VALUE_491";
    storage.values["DefaultOnly"]=91;
    const LLSD exported=storage.asLLSD(true);
    CHECK(!exported.has("DeclaredString"));
    CHECK(!exported.has("DefaultOnly"));
    CHECK(exported.has("AzureTranslateAPIKey"));
    CHECK(exported["AzureTranslateAPIKey"].isMap());
    CHECK(storage.asLLSD(false).has("DefaultOnly"));

    // The old asLLSD path accepts strings nested in a declared LLSD control.
    // Both global/per-account sources and every serialized sink are exercised.
    reset();
    auto& globals=gSavedSettings.diffs;
    globals["AzureTranslateAPIKey"]["key"]="SYNTHETIC_PRIVATE_VALUE_491";
    globals["UnknownFutureControl"][0]["nested"]="SYNTHETIC_PRIVATE_VALUE_491";
    globals["RenderFarClip"]=256.;
    gSavedPerAccountSettings.diffs["UnknownAccountControl"]="SYNTHETIC_PRIVATE_VALUE_491";
    dispatch(true); checkSinks(); noSyntheticSecret();
    CHECK(sinks.post["preferences"]["settings"]["RenderFarClip"].asReal()==256.);
    CHECK(!sinks.post["preferences"]["settings"].has("AzureTranslateAPIKey"));
    CHECK(!sinks.post["preferences"]["settings"].has("UnknownFutureControl"));
    CHECK(sinks.post["preferences"]["settings_per_account"].isMap());
    CHECK(sinks.post["preferences"]["settings_per_account"].size()==0);
    CHECK(gSavedSettings.reads==1 && gSavedSettings.diffs_only);
    CHECK(gSavedPerAccountSettings.reads==0);

    reset();
    // @SAFE_VALUES@
    dispatch(true); checkSinks(); noSyntheticSecret();
    // @SAFE_ASSERTIONS@
    CHECK(sinks.post["preferences"]["settings"].size()==24);

    // Accepted primitive representations, without string conversion.
    for (const LLSD& value : std::vector<LLSD>{LLSD(true),LLSD(false),LLSD(0),LLSD(1)})
    {
        reset(); gSavedSettings.diffs["ImageDecodeAdaptive"]=value; dispatch(true);
        CHECK(xml(sinks.post["preferences"]["settings"]["ImageDecodeAdaptive"])==xml(value));
    }
    for (const LLSD& value : std::vector<LLSD>{LLSD(1),LLSD(1.),LLSD(0.),LLSD(-1.)})
    {
        reset(); gSavedSettings.diffs["RenderFarClip"]=value; dispatch(true);
        CHECK(xml(sinks.post["preferences"]["settings"]["RenderFarClip"])==xml(value));
    }
    // Arbitrary text and composite types are rejected even under allowed names.
    LLSD nested; nested["secret"]="SYNTHETIC_PRIVATE_VALUE_491";
    LLSD array; array.append("SYNTHETIC_PRIVATE_VALUE_491");
    std::vector<LLSD> unsafe={LLSD(),LLSD("SYNTHETIC_PRIVATE_VALUE_491"),nested,array,
        LLSD(LLUUID("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")),
        LLSD(LLURI("synthetic://SYNTHETIC_PRIVATE_VALUE_491")),
        LLSD(LLDate(1.)),LLSD(LLSD::Binary{4,9,1})};
    for (const auto& value:unsafe)
    {
        reset();
        gSavedSettings.diffs["ImageDecodeAdaptive"]=value;
        gSavedSettings.diffs["RenderFarClip"]=value;
        gSavedSettings.diffs["RenderShadowDetail"]=value;
        gSavedSettings.diffs["RenderMaxVRAMBudget"]=value;
        dispatch(true); noSyntheticSecret();
        CHECK(sinks.post["preferences"]["settings"].size()==0);
    }
    for (const auto& value:std::vector<LLSD>{LLSD(2),LLSD(-1),LLSD(1.)})
    {
        reset(); gSavedSettings.diffs["ImageDecodeAdaptive"]=value; dispatch(true);
        CHECK(!sinks.post["preferences"]["settings"].has("ImageDecodeAdaptive"));
    }
    for (const auto& value:std::vector<LLSD>{LLSD(std::numeric_limits<double>::infinity()),
        LLSD(-std::numeric_limits<double>::infinity()),LLSD(std::numeric_limits<double>::quiet_NaN())})
    {
        reset(); gSavedSettings.diffs["RenderFarClip"]=value; dispatch(true);
        CHECK(!sinks.post["preferences"]["settings"].has("RenderFarClip"));
    }
    reset(); gSavedSettings.diffs["RenderMaxVRAMBudget"]=-1;
    gSavedSettings.diffs["RenderShadowDetail"]=-1; dispatch(true);
    CHECK(!sinks.post["preferences"]["settings"].has("RenderMaxVRAMBudget"));
    CHECK(sinks.post["preferences"]["settings"]["RenderShadowDetail"].asInteger()==-1);

    reset();
    auto& pools=gSavedSettings.diffs["ThreadPoolSizes"];
    pools["General"]=2; pools["ImageDecode"]=6; pools["LLImageGL"]=1;
    pools["UnknownPool"]=91; pools["SYNTHETIC_PRIVATE_VALUE_491"]=3;
    gSavedPerAccountSettings.diffs["ThreadPoolSizes"]["General"]=99;
    dispatch(true); noSyntheticSecret();
    const auto& safe_pools=sinks.post["preferences"]["settings"]["ThreadPoolSizes"];
    CHECK(safe_pools.size()==3); CHECK(safe_pools["General"].asInteger()==2);
    CHECK(safe_pools["ImageDecode"].asInteger()==6); CHECK(safe_pools["LLImageGL"].asInteger()==1);
    for (const auto& value:unsafe)
    {
        reset(); gSavedSettings.diffs["ThreadPoolSizes"]["General"]=value;
        dispatch(true); noSyntheticSecret();
        CHECK(!sinks.post["preferences"]["settings"].has("ThreadPoolSizes"));
    }
    for (const auto& value:unsafe)
    {
        reset(); gSavedSettings.diffs["ThreadPoolSizes"]=value; dispatch(true); noSyntheticSecret();
        CHECK(!sinks.post["preferences"]["settings"].has("ThreadPoolSizes"));
    }
    for (const auto& value:std::vector<LLSD>{LLSD(0),LLSD(-1),LLSD(2.),LLSD(true)})
    {
        reset(); gSavedSettings.diffs["ThreadPoolSizes"]["General"]=value; dispatch(true);
        CHECK(!sinks.post["preferences"]["settings"].has("ThreadPoolSizes"));
    }
    // Missing/default-only settings and malformed top-level input remain empty.
    for (const auto& value:std::vector<LLSD>{LLSD::emptyMap(),LLSD::emptyArray(),LLSD("SYNTHETIC_PRIVATE_VALUE_491")})
    {
        reset(); gSavedSettings.diffs=value; dispatch(true); noSyntheticSecret();
        CHECK(sinks.post["preferences"]["settings"].isMap());
        CHECK(sinks.post["preferences"]["settings"].size()==0);
    }
    reset(); gSavedSettings.diffs["AzureTranslateAPIKey"]="SYNTHETIC_PRIVATE_VALUE_491";
    gSavedPerAccountSettings.diffs["Text"]="SYNTHETIC_PRIVATE_VALUE_491";
    dispatch(false); checkSinks(); noSyntheticSecret();
    CHECK(!sinks.post.has("preferences"));
    CHECK(gSavedSettings.reads==0 && gSavedPerAccountSettings.reads==0);
    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";
    return failures?1:0;
}
