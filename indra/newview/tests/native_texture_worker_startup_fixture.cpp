// PPD487: controlled settings and ThreadPool boundaries; production startup bodies below.
#include <iostream>
#include <exception>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
using S32 = int;
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_WARNS(category) std::cerr
#define LL_ENDL '\n'
struct LLWindow {};
namespace LL { struct ThreadPool {
    inline static std::size_t configuredWidth = 0, configuredQueries = 0;
    bool started = false;
    std::size_t mThreadCount;
    static std::size_t getConfiguredWidth(const std::string&, std::size_t dft) {
        ++configuredQueries; return configuredWidth ? configuredWidth : dft;
    }
    ThreadPool(const std::string& name, std::size_t threads, std::size_t = 1024 * 1024,
               bool = true, bool fixed_width = false)
        : mThreadCount(/* @WIDTH@ */) { if (threads != 1) std::terminate(); }
    void start() { started = true; }
}; }
struct LLImageGL {
    inline static bool sSkipAnalyzeAlpha = false, authority = true;
    static bool isMetalTextureAuthority() { return authority; }
    static void initClass(LLWindow*, S32, bool = false, bool = false, bool = false);
};
struct LLImageGLThread : LL::ThreadPool {
    inline static bool sEnabledTextures = false, sEnabledMedia = false;
    static std::unique_ptr<LLImageGLThread> instance;
    explicit LLImageGLThread(LLWindow*);
    static void createInstance(LLWindow* window) { instance.reset(new LLImageGLThread(window)); }
};
std::unique_ptr<LLImageGLThread> LLImageGLThread::instance;
struct Settings {
    std::map<std::string, bool, std::less<>> values;
    bool getBOOL(std::string_view key) const {
        const auto it = values.find(key); return it != values.end() && it->second;
    }
} gSavedSettings;
const std::map<std::string, bool, std::less<>> defaults = {
// @DEFAULTS@
};
// Category is unused by the real initializer; the viewer still supplies its existing enum.
struct LLViewerTexture { static constexpr int MAX_GL_IMAGE_CATEGORY = 31; };
void initializeWindowTextures() {
    LLWindow* mWindow = nullptr;
    // @WINDOW_CALL@
}
// @INITIALIZER@
// @WORKER@
unsigned checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::cerr << __LINE__ << ": " #value "\n"; } } while (false)
void reset() {
    LLImageGLThread::instance.reset();
    LLImageGLThread::sEnabledTextures = LLImageGLThread::sEnabledMedia = false;
    LLImageGL::authority = true;
    LL::ThreadPool::configuredWidth = LL::ThreadPool::configuredQueries = 0;
    gSavedSettings.values = defaults;
}
void state(bool worker, bool textures, bool media) {
    CHECK(bool(LLImageGLThread::instance) == worker);
    if (worker) CHECK(LLImageGLThread::instance &&
        LLImageGLThread::instance->started == (textures || media));
    if (worker) CHECK(LLImageGLThread::instance && LLImageGLThread::instance->mThreadCount == 1);
    CHECK(LLImageGLThread::sEnabledTextures == textures);
    CHECK(LLImageGLThread::sEnabledMedia == media);
}
int main() {
    reset();
    CHECK(gSavedSettings.getBOOL("RenderMetalMultiThreadedTextures"));
    initializeWindowTextures(); state(true, true, false);
    reset(); gSavedSettings.values["RenderMetalMultiThreadedTextures"] = false;
    initializeWindowTextures(); state(false, false, false);
    reset(); gSavedSettings.values["RenderGLMultiThreadedTextures"] = false;
    initializeWindowTextures(); state(true, true, false);
    reset(); gSavedSettings.values["RenderMetalMultiThreadedTextures"] = false;
    gSavedSettings.values["RenderGLMultiThreadedTextures"] = true;
    initializeWindowTextures(); state(false, false, false);
    reset(); gSavedSettings.values["RenderMetalMultiThreadedTextures"] = false;
    gSavedSettings.values["RenderGLMultiThreadedMedia"] = true;
    initializeWindowTextures(); state(true, false, true);
    reset(); gSavedSettings.values["RenderGLMultiThreadedMedia"] = true;
    initializeWindowTextures(); state(true, true, true);
    reset(); LLImageGL::authority = false;
    initializeWindowTextures(); state(true, false, false);
    reset(); LL::ThreadPool::configuredWidth = 6;
    initializeWindowTextures(); state(true, true, false);
    CHECK(LL::ThreadPool::configuredQueries == 0);
    std::cout << "native worker startup: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
