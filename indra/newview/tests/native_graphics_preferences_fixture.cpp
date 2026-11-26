// Real native classification, availability, signal hookups and destruction bodies.
#include "alrenderresourceinputs.h"
#include <boost/bind/bind.hpp>
#include <boost/signals2.hpp>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
using F32 = float;
using S32 = int;
using LLSD = int;
template<typename T> T llclamp(T value, T low, T high) { return std::clamp(value, low, high); }
namespace LLStringUtil { using format_map_t = std::map<std::string, std::string>; }
std::string llformat(const char* format, double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}
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
    std::string text, tooltip;
    unsigned text_writes = 0;
    void setEnabled(bool state) { enabled = state; }
    void setVisible(bool state) { visible = state; }
    void setValue(int state) { value = state; }
    void remove(const std::string& name) { items.erase(name); }
    const std::string& getText() const { return text; }
    void setText(const std::string& value) { text = value; ++text_writes; }
    void setToolTip(const std::string& value) { tooltip = value; }
    const std::string& getToolTip() const { return tooltip; }
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
    S32 resolution_mode = 0;
    bool getBOOL(const std::string& name) const { ++reads; return values.at(name); }
    S32 getS32(const std::string& name) const
    {
        ++reads;
        if (name != "RenderMetalMainResolutionMode") throw std::runtime_error("unexpected integer setting");
        return resolution_mode;
    }
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
struct
{
    struct Status
    {
        S32 requested = 0, effective = 0;
        bool known = false;
        std::string reason;
        U32 scene_width = 0, scene_height = 0, output_width = 0, output_height = 0;
        float material_mip_bias = 0.f;
    } status;
    const Status& getMetalMainResolutionStatus() const { return status; }
} gPipeline;
class LLFloater
{
public:
    unsigned draws = 0;
    void draw() { ++draws; }
};
class LLFloaterPreferenceGraphicsAdvanced : public LLFloater
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
    LLTextBox* mMainResolutionStatus = nullptr;
    std::string getString(const std::string& name, const LLStringUtil::format_map_t& args = {}) const
    {
        const std::map<std::string, std::string> strings{
            // @STATUS_STRINGS@
        };
        auto result = strings.at(name);
        for (const auto& [key, value] : args)
        {
            for (auto at = result.find(key); at != std::string::npos; at = result.find(key, at + value.size()))
                result.replace(at, key.size(), value);
        }
        return result;
    }
    void updateIndirectMaxNonImpostors(const LLSD&) { ++callbacks; }
    void connectNativeCallbacks()
    {
        // @PRODUCTION_HOOKUPS@
    }
    ~LLFloaterPreferenceGraphicsAdvanced();
    void refreshEnabledState();
    void refreshMainResolutionStatus();
    void draw();
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
#if HAS_RESOLUTION_STATUS
void checkResolutionStatus()
{
    LLFloaterPreferenceGraphicsAdvanced floater;
    auto& text = floater.controls.at("main resolution status");
    floater.mMainResolutionStatus = &text;
    const auto saved = gSavedSettings.values;
    gSavedSettings.resolution_mode = 2;
    gPipeline.status = {2, 0, false, ""};
    floater.draw();
    CHECK(text.text == "Pending next world frame");
    CHECK(floater.draws == 1);
    const auto writes = text.text_writes;
    floater.draw();
    CHECK(text.text_writes == writes);
    CHECK(floater.draws == 2);
    gPipeline.status = {0, 0, true, ""};
    floater.draw(); // A newly selected mode must not show the previous mode as active.
    CHECK(text.text == "Pending next world frame");
    for (const auto& [mode, expected] : std::map<S32, std::string>{
        {0, "Active: Standard"}, {1, "Active: 50% Linear"}, {2, "Active: 50% MetalFX Spatial"}})
    {
        gSavedSettings.resolution_mode = mode;
        gPipeline.status = {mode, mode, true, "Old fallback reason"};
        floater.draw();
        CHECK(text.text == expected);
        CHECK(text.tooltip == expected);
    }
    gSavedSettings.resolution_mode = 2;
    for (const auto& reason : {"Antialiasing is off", "MetalFX preparation failed", "This view uses linear scaling"})
    {
        gPipeline.status = {2, 1, true, reason};
        floater.draw();
        CHECK(text.text == std::string("Active: 50% Linear — ") + reason);
        CHECK(text.tooltip == text.text);
    }
    gPipeline.status = {2, 1, true, ""};
    floater.draw();
    CHECK(text.text == "Active: 50% Linear — Reconstruction is unavailable for this view");
    gPipeline.status = {2, 9, true, ""};
    floater.draw();
    CHECK(text.text == "Pending next world frame");
    gPipeline.status = {2, 2, true, ""};
    floater.draw();
    CHECK(text.text == "Active: 50% MetalFX Spatial");
    CHECK(text.tooltip == text.text);
#if HAS_SCALED_RESOLUTION
    for (const auto& [choice, percentage] : std::map<S32, std::string>{{3, "67"}, {4, "75"}})
    {
        gSavedSettings.resolution_mode = choice;
        floater.draw();
        CHECK(text.text == "Pending next world frame");
        gPipeline.status = {choice, 2, true, ""};
        floater.draw();
        CHECK(text.text == "Active: " + percentage + "% MetalFX Spatial");
        gPipeline.status = {choice, 1, true, "Antialiasing is off"};
        floater.draw();
        CHECK(text.text == "Active: " + percentage + "% Linear — Antialiasing is off");
    }
    gSavedSettings.resolution_mode = 3;
    gPipeline.status = {3, 2, true, "", 2305, 1446, 3457, 2169, -0.58f};
    floater.draw();
    CHECK(text.text == "Active: 67% MetalFX Spatial\nScene 2305×1446 → 3457×2169");
    CHECK(text.tooltip == text.text + "\nWorld material mip adjustment: -0.58");
    const auto dimension_writes = text.text_writes;
    gPipeline.status.material_mip_bias = -0.42f;
    floater.draw();
    CHECK(text.text_writes == dimension_writes);
    CHECK(text.tooltip == text.text + "\nWorld material mip adjustment: -0.42");
    gSavedSettings.resolution_mode = 4;
    gPipeline.status = {4, 2, true, "", 2593, 1627, 3457, 2169};
    floater.draw();
    CHECK(text.text == "Active: 75% MetalFX Spatial\nScene 2593×1627 → 3457×2169");
#if HAS_TEMPORAL_RESOLUTION
    for (const auto& [choice, percentage] : std::map<S32, std::string>{{5, "50"}, {6, "67"}, {7, "75"}})
    {
        gSavedSettings.resolution_mode = choice;
        floater.draw();
        CHECK(text.text == "Pending next world frame");
        gPipeline.status = {choice, 3, true, ""};
        floater.draw();
        CHECK(text.text == "Active: " + percentage + "% MetalFX Temporal");
        gPipeline.status = {choice, 1, true, "Temporal is unavailable on this device"};
        floater.draw();
        CHECK(text.text == "Active: " + percentage + "% Linear — Temporal is unavailable on this device");
        gPipeline.status = {choice, 2, true, "This view uses Spatial reconstruction"};
        floater.draw();
        CHECK(text.text == "Active: " + percentage + "% MetalFX Spatial — This view uses Spatial reconstruction");
    }
    gSavedSettings.resolution_mode = 6;
    gPipeline.status = {6, 3, true, "", 2305, 1446, 3457, 2169, -1.58f};
    floater.draw();
    CHECK(text.text == "Active: 67% MetalFX Temporal\nScene 2305×1446 → 3457×2169");

#if HAS_AURORA_RESOLUTION
    gSavedSettings.resolution_mode = 8;
    gPipeline.status = {8, 4, true, "", 2305, 1446, 3457, 2169, -1.5845f};
    floater.refreshMainResolutionStatus();
    CHECK(text.text == "Active: 67% Aurora Temporal (Experimental)\nScene 2305×1446 → 3457×2169");
    gPipeline.status.effective = 1;
    gPipeline.status.reason = "Aurora preparation failed";
    floater.refreshMainResolutionStatus();
    CHECK(text.text == "Active: 67% Linear — Aurora preparation failed\nScene 2305×1446 → 3457×2169");
#endif
    CHECK(text.tooltip == text.text + "\nWorld material mip adjustment: -1.58");
#endif
    for (S32 invalid : {-1, 9, 99})
    {
        gSavedSettings.resolution_mode = invalid;
        gPipeline.status = {0, 0, true, ""};
        floater.draw();
        CHECK(text.text == "Active: Standard");
        CHECK(text.tooltip == text.text);
    }
    gSavedSettings.resolution_mode = 2;
#endif
    CHECK(gSavedSettings.resolution_mode == 2);
    CHECK(gSavedSettings.values == saved);
}
#endif
int main()
{
    // @RESOLUTION_STATUS_TESTS@
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
