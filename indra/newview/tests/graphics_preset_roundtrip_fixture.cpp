// Actual production preset-manager method bodies, real LLControlGroup and XML.
// Only app UI/translation/hardware-default services and the directory root are
// substituted; no copied preset saving/loading/control implementation is used.
#include "linden_common.h"
#include "llcontrol.h"
#include "llfile.h"
#include "llsdserialize.h"
#include "lluri.h"
#include <boost/bind/bind.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

// @PRESET_CONSTANTS@
// @CAMERA_PRESET_ENUM@

LLControlGroup gSavedSettings("PresetRoundtripGlobal");
LLControlGroup gSavedPerAccountSettings("PresetRoundtripAccount");

namespace
{
constexpr const char* MODE = "RenderMetalMainResolutionMode";
struct Directory
{
    std::filesystem::path root;
    std::string getExpandedFilename(int, const std::string& part) const
    { return (root / part).string(); }
    std::string add(const std::string& parent, const std::string& child) const
    { return (std::filesystem::path(parent) / child).string(); }
    bool fileExists(const std::string& path) const { return std::filesystem::exists(path); }
    std::string getDirDelimiter() const { return "/"; }
} directory;
// Avoid defining the application's real LLDir global in a library-linked test.
#define gDirUtilp presetTestDirectory
Directory* presetTestDirectory = &directory;
constexpr int LL_PATH_USER_SETTINGS = 1;
}

struct LLTrans
{
    static std::string getString(const std::string& name) { return name; }
};
struct LLFloaterPreference { void refreshEnabledGraphics() {} };
struct LLFloaterReg
{
    template<typename T> static T* findTypedInstance(const std::string&) { return nullptr; }
};
struct LLFeatureManager
{
    static LLFeatureManager* getInstance() { static LLFeatureManager instance; return &instance; }
    LLSD getRecommendedSettingsMap() { return LLSD::emptyMap(); }
};

class LLPresetsManager
{
public:
    LLPresetsManager() { startWatching(PRESETS_GRAPHIC); }
    ~LLPresetsManager();
    static std::string getPresetsDir(const std::string& subdirectory);
    void getGraphicsControlNames(std::vector<std::string>& names);
    void startWatching(const std::string& subdirectory);
    void graphicsSettingChanged();
    bool savePreset(const std::string& subdirectory, std::string name, bool createDefault = false);
    void loadPreset(const std::string& subdirectory, std::string name);
    bool isDefaultCameraPreset(std::string name);
    bool isTemplateCameraPreset(std::string name);
    void getCameraControlNames(std::vector<std::string>& names) { names = {"CameraAngle"}; }
    void getLooksControlNames(std::vector<std::string>& names) { names.clear(); }
    void cameraSettingChanged() {}
    void looksSettingChanged() {}
    void triggerChangeSignal() { ++graphics_notifications; }
    void triggerChangeCameraSignal() { ++camera_notifications; }
    void triggerChangeLooksSignal() {}
    unsigned graphics_notifications = 0, camera_notifications = 0;
    bool mIgnoreChangedSignal = false;
    std::vector<boost::signals2::connection> mGraphicsChangedSignals, mCameraChangedSignals, mLooksChangedSignals;
private:
    LOG_CLASS(LLPresetsManager);
};

// @PRODUCTION_METHODS@

namespace
{
unsigned checks = 0;
void check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition) throw std::runtime_error(std::to_string(line) + ": " + expression);
}
#define CHECK(condition) check(bool(condition), #condition, __LINE__)

LLSD setting(const char* type, LLSD value)
{
    LLSD result;
    result["Comment"] = "Hermetic preset fixture control";
    result["Persist"] = 1;
    result["Type"] = type;
    result["Value"] = value;
    return result;
}

std::string path(const std::string& kind, const std::string& name)
{
    return directory.add(LLPresetsManager::getPresetsDir(kind), LLURI::escape(name) + ".xml");
}

void write(const std::string& kind, const std::string& name, const LLSD& values)
{
    llofstream file(path(kind, name));
    CHECK(file.is_open());
    LLSDSerialize::toPrettyXML(values, file);
    file.close();
    CHECK(file.good());
}

LLSD read(const std::string& kind, const std::string& name)
{
    llifstream file(path(kind, name));
    CHECK(file.is_open());
    LLSD values;
    CHECK(LLSDSerialize::fromXML(values, file) != LLSDParser::PARSE_FAILURE);
    return values;
}

struct TemporaryRoot
{
    TemporaryRoot()
    {
        directory.root = std::filesystem::temp_directory_path() /
            ("alchemy-preset-roundtrip-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory.root)) throw std::runtime_error("temporary root collision");
    }
    ~TemporaryRoot() { std::error_code error; std::filesystem::remove_all(directory.root, error); }
};

void run()
{
    TemporaryRoot root;
    gSavedSettings.declareS32(MODE, 0, "Optional main-world resolution mode");
    gSavedSettings.declareU32("RenderFSAAType", 0, "AA fixture");
    gSavedSettings.declareF32("RenderCASSharpness", .4f, "CAS fixture");
    gSavedSettings.declareU32("RenderAvatarMaxComplexity", 100000, "Complexity fixture");
    gSavedSettings.declareU32("CameraPresetType", 0, "Camera fixture");
    gSavedSettings.declareF32("CameraAngle", 1.f, "Camera fixture");
    gSavedSettings.declareBOOL("AppearanceCameraMovement", true, "Camera fixture");
    gSavedSettings.declareBOOL("EditCameraMovement", true, "Camera fixture");
    for (const char* name : {"PresetGraphicActive", "PresetCameraActive", "PresetLooksActive", "PresetLooksLastApplied"})
        gSavedSettings.declareString(name, "", "Preset fixture");
    LLPresetsManager manager;

    // Owner request: actual manager saves MetalFX2, Standard0 is selected,
    // and loading the file must restore2 through the real control/XML loader.
    gSavedSettings.setS32(MODE, 2);
    gSavedSettings.setU32("RenderFSAAType", 3);
    CHECK(manager.savePreset(PRESETS_GRAPHIC, "MetalFX roundtrip"));
    LLSD saved = read(PRESETS_GRAPHIC, "MetalFX roundtrip");
    CHECK(saved.has(MODE));
    CHECK(saved[MODE]["Type"].asString() == "S32");
    CHECK(saved[MODE]["Value"].asInteger() == 2);
    gSavedSettings.setS32(MODE, 0);
    CHECK(gSavedSettings.getString("PresetGraphicActive").empty());
    manager.loadPreset(PRESETS_GRAPHIC, "MetalFX roundtrip");
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(gSavedSettings.getU32("RenderFSAAType") == 3);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == "MetalFX roundtrip");
    CHECK(!manager.mIgnoreChangedSignal);

    // Both Half variants are fenced when an older quality preset has no key.
    LLSD older;
    older["RenderFSAAType"] = setting("U32", 1);
    older["RenderCASSharpness"] = setting("F32", .25f);
    write(PRESETS_GRAPHIC, "Older graphics", older);
    for (S32 mode : {1, 2})
    {
        gSavedSettings.setS32(MODE, mode);
        manager.loadPreset(PRESETS_GRAPHIC, "Older graphics");
        CHECK(gSavedSettings.getS32(MODE) == 0);
        CHECK(gSavedSettings.getU32("RenderFSAAType") == 1);
        CHECK(std::abs(gSavedSettings.getF32("RenderCASSharpness") - .25f) < 1.e-6f);
        CHECK(gSavedSettings.getString("PresetGraphicActive") == "Older graphics");
        CHECK(!manager.mIgnoreChangedSignal);
    }

    // Explicit Standard, Half, and higher-resolution MetalFX modes round-trip.
    for (S32 mode : {0, 1, 3, 4})
    {
        gSavedSettings.setS32(MODE, mode);
        const auto name = "Explicit mode " + std::to_string(mode);
        CHECK(manager.savePreset(PRESETS_GRAPHIC, name));
        CHECK(read(PRESETS_GRAPHIC, name)[MODE]["Value"].asInteger() == mode);
        gSavedSettings.setS32(MODE, 2);
        manager.loadPreset(PRESETS_GRAPHIC, name);
        CHECK(gSavedSettings.getS32(MODE) == mode);
    }

    // Generating a missing hardware Default must serialize the mode's
    // declared Standard default, without changing the currently selected FX.
    gSavedSettings.setS32(MODE, 2);
    CHECK(manager.savePreset(PRESETS_GRAPHIC, "MetalFX selected"));
    const auto active_before_default = gSavedSettings.getString("PresetGraphicActive");
    const auto default_notifications = manager.graphics_notifications;
    CHECK(manager.savePreset(PRESETS_GRAPHIC, PRESETS_DEFAULT, true));
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == active_before_default);
    CHECK(manager.graphics_notifications == default_notifications + 1);
    LLSD defaults = read(PRESETS_GRAPHIC, PRESETS_DEFAULT);
    CHECK(defaults[MODE]["Value"].asInteger() == 0);
    CHECK(gSavedSettings.getS32(MODE) == 2);
    manager.loadPreset(PRESETS_GRAPHIC, PRESETS_DEFAULT);
    CHECK(gSavedSettings.getS32(MODE) == 0);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == PRESETS_DEFAULT);

    // Do not replace loadFromFile/Include semantics with a shallow key scan.
    LLSD child;
    child[MODE] = setting("S32", 2);
    write(PRESETS_GRAPHIC, "Included mode", child);
    LLSD including;
    including["Include"] = LLSD::emptyArray();
    including["Include"].append(LLURI::escape("Included mode") + ".xml");
    including["RenderCASSharpness"] = setting("F32", .3f);
    write(PRESETS_GRAPHIC, "Including graphics", including);
    gSavedSettings.setS32(MODE, 1);
    manager.loadPreset(PRESETS_GRAPHIC, "Including graphics");
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == "Including graphics");

    // Failed loads cannot leave the staged Standard baseline behind or clear
    // the active preset through the ordinary graphics-change watcher.
    const auto active = gSavedSettings.getString("PresetGraphicActive");
    const auto notified = manager.graphics_notifications;
    manager.loadPreset(PRESETS_GRAPHIC, "Missing graphics");
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == active);
    CHECK(manager.graphics_notifications == notified);
    CHECK(!manager.mIgnoreChangedSignal);
    write(PRESETS_GRAPHIC, "Empty graphics", LLSD::emptyMap());
    manager.loadPreset(PRESETS_GRAPHIC, "Empty graphics");
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(gSavedSettings.getString("PresetGraphicActive") == active);
    CHECK(!manager.mIgnoreChangedSignal);

    // Camera files retain their existing merge semantics and do not reset a
    // graphics control simply because they predate its declaration.
    LLSD camera;
    camera["CameraAngle"] = setting("F32", .75f);
    camera["AppearanceCameraMovement"] = setting("Boolean", false);
    write(PRESETS_CAMERA, "Camera fixture", camera);
    manager.loadPreset(PRESETS_CAMERA, "Camera fixture");
    CHECK(gSavedSettings.getS32(MODE) == 2);
    CHECK(std::abs(gSavedSettings.getF32("CameraAngle") - .75f) < 1.e-6f);
    CHECK(gSavedSettings.getBOOL("AppearanceCameraMovement"));
    CHECK(gSavedSettings.getString("PresetCameraActive") == "Camera fixture");
    CHECK(!manager.mIgnoreChangedSignal);
}
}

int main()
{
    try { run(); }
    catch (const std::exception& error)
    { std::cerr << "graphics preset roundtrip FAILED: " << error.what() << '\n'; return 1; }
    std::cout << "graphics preset roundtrip: " << checks << " checks passed\n";
    return 0;
}
