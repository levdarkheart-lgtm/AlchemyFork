// Real native classification, availability, signal hookups and destruction bodies.
#include <boost/bind/bind.hpp>
#include <boost/signals2.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
using F32 = float;
using LLSD = int;
using boost::placeholders::_2;
#define LL_INFOS(...) std::ostringstream()
#define LL_WARNS(...) std::ostringstream()
#define LL_ENDL std::endl
struct LLControlVariable
{
    boost::signals2::signal<void(LLControlVariable*, const LLSD&, const LLSD&)> signal;
    auto* getSignal() { return &signal; }
};
struct Control
{
    bool enabled = true, visible = true;
    int value = 1;
    std::set<std::string> items{"2x", "4x", "8x", "16x"};
    void setEnabled(bool state) { enabled = state; }
    void setVisible(bool state) { visible = state; }
    void setValue(int state) { value = state; }
    void remove(const std::string& name) { items.erase(name); }
};
using LLCheckBoxCtrl = Control;
using LLComboBox = Control;
using LLTextBox = Control;
using LLSliderCtrl = Control;
struct Settings
{
    std::map<std::string, bool> values;
    LLControlVariable impostors;
    mutable unsigned reads = 0;
    bool getBOOL(const std::string& name) const { ++reads; return values.at(name); }
    LLControlVariable* getControl(const std::string& name)
    {
        ++reads;
        if (name != "RenderAvatarMaxNonImpostors") throw std::runtime_error("unexpected setting");
        return &impostors;
    }
} gSavedSettings;
template<typename T> struct LLCachedControl
{
    Settings& settings;
    std::string name;
    LLCachedControl(Settings& store, const std::string& key) : settings(store), name(key) {}
    operator T() const { ++settings.reads; return settings.values.at(name); }
};
// @GPU_CLASS_ENUM@
struct LLFeatureManager
{
    std::map<std::string, bool> available;
    EGPUClass mGPUClass = GPU_CLASS_UNKNOWN;
    F32 mGPUMemoryBandwidth = -1.f;
    std::string mGPUString;
    static LLFeatureManager& instance() { static LLFeatureManager value; return value; }
    static LLFeatureManager* getInstance() { return &instance(); }
    bool isFeatureAvailable(const std::string& name) const { return available.at(name); }
    bool loadGPUClass();
    // @FEATURE_ACCESSORS@
};
struct Capabilities
{
    bool anisotropic = true;
    F32 max_anisotropy = 16.f;
    bool initialized = true;
    bool metal = true;
    std::string renderer_name = "Apple M1 Max";
    bool isInitialized() const { return initialized; }
    bool isMetal() const { return initialized && metal; }
    const std::string& renderer() const { return renderer_name; }
    bool hasAnisotropicFiltering() const { return anisotropic; }
    F32 maxAnisotropy() const { return max_anisotropy; }
} gRenderCapabilities;
struct { F32 mMaxAnisotropy = 16.f; } gGLManager;
class LLFloaterPreferenceGraphicsAdvanced
{
public:
    std::map<std::string, Control> controls{
        // @PACKAGED_NAMES@
    };
    template<typename T> T* getChild(const std::string& name) { return &controls.at(name); }
    Control* getChildView(const std::string& name) { return &controls.at(name); }
    boost::signals2::connection mImpostorsChangedSignal, mComplexityChangedSignal,
        mComplexityModeChangedSignal, mLODFactorChangedSignal, mNumImpostorsChangedSignal;
    unsigned callbacks = 0;
    void updateIndirectMaxNonImpostors(const LLSD&) { ++callbacks; }
    void connectNativeCallbacks()
    {
        // @PRODUCTION_HOOKUPS@
    }
    ~LLFloaterPreferenceGraphicsAdvanced();
    void refreshEnabledState();
    void disableUnavailableSettings(); // Used only by historical discrimination controls.
};
class LLFloaterPerformance
{
public:
    unsigned prompts = 0;
    std::string notification;
    void changeQualityLevel(const std::string& name) { ++prompts; notification = name; }
    void onClickShadows();
};
// @PRODUCTION_METHODS@
unsigned checks = 0, failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; std::cerr << __LINE__ << ": " #condition "\n"; } } while (false)
int main()
{
    // Startup classification uses the initialized native identity, not an
    // automatic bandwidth workload or saved benchmark/workaround state.
    auto& manager = LLFeatureManager::instance();
    gSavedSettings.values = {{"SkipBenchmark", true}, {"UseOcclusion", false}};
    const auto saved = gSavedSettings.values;
    const auto reads = gSavedSettings.reads;
    for (const auto* name : {"Apple M1 Max", "Apple M4"})
    {
        gRenderCapabilities = {};
        gRenderCapabilities.renderer_name = name;
        CHECK(manager.loadGPUClass());
        CHECK(manager.getGPUClass() == GPU_CLASS_3);
        CHECK(manager.getGPUString() == name);
        CHECK(manager.getGPUMemoryBandwidth() == -1.f);
        CHECK(gSavedSettings.reads == reads);
        CHECK(gSavedSettings.values == saved);
    }
    for (bool initialized : {false, true})
    {
        gRenderCapabilities = {};
        gRenderCapabilities.initialized = initialized;
        gRenderCapabilities.metal = !initialized;
        manager.mGPUClass = GPU_CLASS_5;
        manager.mGPUString = "stale renderer";
        manager.mGPUMemoryBandwidth = 42.f;
        CHECK(!manager.loadGPUClass());
        CHECK(manager.getGPUClass() == GPU_CLASS_UNKNOWN);
        CHECK(manager.getGPUString().empty());
        CHECK(manager.getGPUMemoryBandwidth() == -1.f);
        CHECK(gSavedSettings.reads == reads);
        CHECK(gSavedSettings.values == saved);
    }
    for (bool ssao : {false, true})
    for (bool shadows : {false, true})
    for (bool modern : {false, true})
    for (bool old_shaders : {false, true})
    {
        auto& features = LLFeatureManager::instance().available;
        features = {{"RenderDeferredSSAO", ssao}, {"RenderShadowDetail", shadows},
            {"RenderAnisotropicLevel", true}, {"WindLightUseAtmosShaders", old_shaders},
            {"RenderDeferred", old_shaders}, {"RenderVBOEnable", true}, {"RenderObjectBump", old_shaders}};
        gSavedSettings.values = {{"RenderDisableVintageMode", modern}, {"RenderObjectBump", old_shaders},
            {"WindLightUseAtmosShaders", old_shaders}, {"RenderDeferred", old_shaders}};
        gRenderCapabilities = {};
        LLFloaterPreferenceGraphicsAdvanced floater;
        floater.refreshEnabledState();
        CHECK(floater.controls.at("UseSSAO").enabled == ssao);
        CHECK(floater.controls.at("UseDoF").enabled);
        CHECK(floater.controls.at("UseDoF").value == 1);
        CHECK(floater.controls.at("ShadowDetail").enabled == shadows);
        CHECK(floater.controls.at("RenderShadowDetailText").enabled == shadows);
        CHECK(floater.controls.at("UseSSAO").value == (ssao ? 1 : 0));
        CHECK(floater.controls.at("ShadowDetail").value == (shadows ? 1 : 0));
        for (const auto& name : {"TonemapType", "TonemapTypeText", "TonemapMix", "RenderExposure", "RenderSharpness"})
            CHECK(floater.controls.at(name).enabled == modern);
        LLFloaterPerformance performance;
        performance.onClickShadows();
        CHECK(performance.prompts == (shadows ? 0u : 1u));
        if (!shadows) CHECK(performance.notification == "ShadowsConfirm");
    }
    for (F32 maximum : {1.f, 2.f, 4.f, 8.f, 16.f})
    for (bool hardware : {false, true})
    for (bool feature : {false, true})
    {
        LLFeatureManager::instance().available = {{"RenderDeferredSSAO", true},
            {"RenderShadowDetail", true}, {"RenderAnisotropicLevel", feature},
            {"WindLightUseAtmosShaders", true}, {"RenderDeferred", true}, {"RenderVBOEnable", true}};
        gRenderCapabilities = {hardware, maximum};
        gGLManager.mMaxAnisotropy = maximum;
        LLFloaterPreferenceGraphicsAdvanced floater;
        floater.refreshEnabledState();
        const auto& af = floater.controls.at("anisotropic_filter");
        CHECK(af.enabled == (hardware && feature && maximum >= 2.f));
        for (unsigned level : {2u, 4u, 8u, 16u})
            CHECK(af.items.contains(std::to_string(level) + "x") == (maximum >= level));
    }
    auto* control = gSavedSettings.getControl("RenderAvatarMaxNonImpostors");
    CHECK(control->signal.num_slots() == 0);
    auto owned = std::make_unique<LLFloaterPreferenceGraphicsAdvanced>();
    owned->connectNativeCallbacks();
    CHECK(control->signal.num_slots() == 2);
    control->signal(control, 1, 0);
    CHECK(owned->callbacks == 2);
    owned.reset();
    CHECK(control->signal.num_slots() == 0);
    control->signal(control, 2, 1); // Must not visit the destroyed floater.
    std::cout << "native graphics preferences: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
