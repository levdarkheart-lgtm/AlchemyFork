// PPD-467: controlled queue/storage inputs around verbatim production bodies.
// Run llviewertexture_upload_recovery_test.py; this template is not built alone.
#include <atomic>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#define LL_DARWIN 1
#ifndef LL_IMAGEGL_THREAD_CHECK
#define LL_IMAGEGL_THREAD_CHECK 0
#endif
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_PROFILE_ZONE_SCOPED
#define LL_PROFILE_ZONE_NAMED(...)
#define LL_INFOS(...) std::ostringstream()
#define LL_ENDL std::endl
#define llassert(value) assert(value)
using S8 = signed char;
using S32 = int;
using U32 = unsigned;
using U64 = unsigned long long;
using U16 = unsigned short;
using U8 = unsigned char;
struct LLThread { static int currentID() { return 1; } };
bool main_thread=true;
bool on_main_thread() { return main_thread; }
template<class T> struct LLPointer
{
    T* value = nullptr;
    LLPointer() = default;
    LLPointer(T* input) : value(input) {}
    T* operator->() const { return value; }
    T* get() const { return value; }
};
struct WeakQueueBase : std::enable_shared_from_this<WeakQueueBase>
{
    std::weak_ptr<struct Queue> getWeak();
};
namespace LL
{
class WorkQueueBase : public WeakQueueBase
{
    using super = WeakQueueBase;
public:
    using weak_t = std::weak_ptr<struct ::Queue>;
    template<class C, class F, class R> struct MakeReplyLambda;
    template<class C, class F> static auto makeReplyLambda(C&&, F&&);
    template<class C, class F, class... A> bool postTo(weak_t, C&&, F&&, A&&...);
    template<class... A> static bool postMaybe(weak_t, A&&...);
};
}
struct Queue : LL::WorkQueueBase
{
    bool reject=false, worker_thread=false, throw_post=false;
    std::queue<std::function<void()>> work;
    bool post(std::function<void()> call) { if(throw_post)throw std::runtime_error("queue allocation boundary");if(reject)return false;work.push(std::move(call));return true; }
    void one()
    {
        assert(!work.empty());
        auto call=std::move(work.front());work.pop();
        const bool previous=main_thread;main_thread=!worker_thread;
        call();main_thread=previous;
    }
};
std::weak_ptr<Queue> WeakQueueBase::getWeak() { return std::static_pointer_cast<Queue>(shared_from_this()); }
namespace LL
{
// @ACTUAL_POST_TO@
}
struct ALNativeFrameBudget
{
    using Lease = std::shared_ptr<int>;
    static ALNativeFrameBudget& instance() { static ALNativeFrameBudget value; return value; }
    void setPendingPublications(std::size_t) {}
};
struct LLImageGLThread { static inline bool sEnabledTextures = true; static void deleteSingleton() { assert(main_thread); } };
struct RawImage;
struct LLImageGL
{
    using UploadCompletion = std::shared_ptr<int>;
    struct NativeUploadCreditScope { explicit NativeUploadCreditScope(ALNativeFrameBudget::Lease) {} };
    bool mUploadInFlight = false;
    U32 mPublishesPending = 0;
    bool mPublishesSucceeded = true;
    std::vector<std::function<void(bool)>> mOnPublished;
    std::unique_ptr<int> mPendingMetalTexture;
    int visible = 3, staged = 7, discarded_alpha = 0;
    bool create_succeeds=true;
    std::weak_ptr<Queue> main_queue;
    int mActiveThread=1;
    void checkActiveThread() const { assert(mActiveThread==LLThread::currentID()); }
    struct Snapshot
    {
        int mWidth=0, mHeight=0, mComponents=0, mCurrentDiscardLevel=0, mMaxDiscardLevel=0;
        int mFormatPrimary=0, mDeprecatedSourceFormat=0, mFormatInternal=0, mFormatType=0;
        int mFormatComponents=0, mAlphaStride=0, mAlphaOffset=0, mFormatSwapBytes=0;
        int mHasExplicitFormat=0, mNeedsAlphaAndPickMask=0, mHasMipMaps=0, mMipLevels=0;
        int mCategory=0, mGLTextureCreated=0, mTextureMemory=0;
    } mPublished;
    int mWidth=1, mHeight=1, mComponents=4, mCurrentDiscardLevel=0, mMaxDiscardLevel=0;
    int mFormatPrimary=0, mDeprecatedSourceFormat=0, mFormatInternal=0, mFormatType=0;
    int mFormatComponents=4, mAlphaStride=0, mAlphaOffset=0, mFormatSwapBytes=0;
    int mHasExplicitFormat=0, mNeedsAlphaAndPickMask=0, mHasMipMaps=0, mMipLevels=0;
    int mCategory=0, mTextureMemory=0;
    std::atomic<int> mGLTextureCreated{0};
    static void cleanupClass();
    U64 retainUploadOwner(std::function<void()>);
    bool ownsUploadOwner(U64) const;
    void releaseUploadOwner(U64 token=0);
    std::function<void()> mUploadOwnerRelease;
    U64 mUploadOwnerToken=0;
    void beginUpload();
    void beginUpload(S8);
    void queuePublish(UploadCompletion);
    void finishPublish(bool);
    void afterPublish(std::function<void()>);
    void afterPublish(std::function<void(bool)>);
    void syncTexName(int) { visible = staged; endUpload(); }
    bool createMetalTexture(S32, const RawImage*, int);
    bool syncMetalToMainThread();
    void publishMetalTextureAuthoritative() { syncTexName(0); }
    void endUpload();
    void cancelUpload();
    void discardPendingAlpha() { ++discarded_alpha; }
    void resolveComponentFormat(S8 components)
    {
        mFormatComponents=components;
        mFormatInternal=100+components;
        mFormatPrimary=200+components;
        mFormatType=300+components;
        mAlphaStride=components;
    }
    std::vector<int> metadata() const
    {
        return {mWidth,mHeight,mComponents,mCurrentDiscardLevel,mMaxDiscardLevel,
            mFormatPrimary,mDeprecatedSourceFormat,mFormatInternal,mFormatType,
            mFormatComponents,mAlphaStride,mAlphaOffset,mFormatSwapBytes,
            mHasExplicitFormat,mNeedsAlphaAndPickMask,mHasMipMaps,mMipLevels,
            mCategory,mTextureMemory,mGLTextureCreated.load()};
    }
};
struct PendingPublish { LLPointer<LLImageGL> mImage; LLImageGL::UploadCompletion mMetalCompletion; };
std::vector<PendingPublish> sPendingPublishes;
std::vector<LLPointer<LLImageGL>> sUploadOwners;
struct LLViewerFetchedTexture;
struct TextureList
{
    int retries = 0;
    LLViewerFetchedTexture* retry = nullptr;
    void queueCreateTexture(LLViewerFetchedTexture*, bool preserve_age);
} gTextureList;
struct RawImage
{
    U8 pixels[4] = {1,2,3,4};
    int getComponents() const { return 3; }
    U8* getData() { return pixels; }
    int getDataSize() const { return sizeof(pixels); }
};
struct LLViewerFetchedTexture
{
    RawImage pixels;
    RawImage* mRawImage = &pixels;
    LLPointer<LLImageGL> mGLTexturep;
    std::weak_ptr<Queue> mMainQueue, mImageQueue;
    bool mNeedsCreateTexture=true, mNeedsAux=true, mCreatePending=false;
    int mBoostLevel=1, refs=1, finalized=0, saved=0, mCreateQueuedFrame=91;
    S32 mRawDiscardLevel=2;
    bool save_raw=false;
    void ref() { ++refs; }
    int zero_releases=0;
    void unref() { assert(main_thread);--refs;assert(refs>=0);if(refs==0)++zero_releases; }
    void setActive() { ++finalized; }
    void rebuildSculptVolumes() {}
    bool needsToSaveRawImage() const { return save_raw; }
    void destroyRawImage() { if(save_raw)++saved; mRawImage=nullptr; }
    bool createTexture(S32, S32);
    void postCreateTexture();
    void submitFromList();
    bool postNativeTextureUpload(ALNativeFrameBudget::Lease);
};
bool LLImageGL::createMetalTexture(S32 discard, const RawImage* raw, int category)
{
    mComponents=raw->getComponents(); mCurrentDiscardLevel=discard;
    mCategory=category; mTextureMemory=44;
    if (!create_succeeds) return false;
    mPendingMetalTexture=std::make_unique<int>(7);
    return true;
}
bool LLImageGL::syncMetalToMainThread()
{
    // Actual worker createTexture selects this boundary and queues registration
    // before WorkQueue posts its reply. Actual queuePublish body runs on main.
    return LL::WorkQueueBase::postMaybe(main_queue, [this]{ queuePublish({}); });
}
void TextureList::queueCreateTexture(LLViewerFetchedTexture* texture, bool preserve_age)
{
    assert(preserve_age);
    assert(texture->mRawImage);
    assert(texture->mNeedsCreateTexture);
    assert(!texture->mCreatePending);
    ++retries; retry=texture; texture->mCreatePending=true;
}
// @PRODUCTION_BODIES@
void LLViewerFetchedTexture::submitFromList()
{
    auto* imagep=this;
    struct Admission { ALNativeFrameBudget::Lease credit; explicit operator bool() const { return true; } } admission;
    struct Census { int native_worker_posts=0,worker_raw_input_bytes=0; } sTexturePublicationCensus;
    struct Timer { float getElapsedTimeF32() const { return 0; } } create_timer;
    const int raw_input_bytes=4;
    const float max_time=1;
    do
    {
        // @POST_BRANCH@
    } while(false);
}

int checks=0;
void check(bool value, const char* expression, int line)
{
    ++checks;
    if (!value) throw std::runtime_error(std::string(expression)+" at "+std::to_string(line));
}
#define CHECK(expression) check(bool(expression), #expression, __LINE__)
struct Context
{
    LLImageGL image;
    LLViewerFetchedTexture texture;
    std::shared_ptr<Queue> main=std::make_shared<Queue>(), worker=std::make_shared<Queue>();
    Context()
    {
        gTextureList={}; sPendingPublishes.clear(); LLImageGLThread::sEnabledTextures=true;
        texture.mGLTexturep=&image; texture.mMainQueue=main; texture.mImageQueue=worker;
        image.main_queue=main; worker->worker_thread=true; main_thread=true;
    }
    ~Context() { main_thread=true; LLImageGL::cleanupClass(); }
    void post()
    {
        CHECK(texture.postNativeTextureUpload({})); CHECK(texture.refs==2);
        CHECK(image.mUploadInFlight); worker->one();
    }
    void queued() { main->one(); CHECK(image.mPublishesPending==1); }
    void reply() { main->one(); }
    void failed()
    {
        CHECK(texture.refs==1); CHECK(texture.mRawImage==&texture.pixels);
        CHECK(texture.mNeedsCreateTexture); CHECK(texture.mCreatePending);
        CHECK(texture.mCreateQueuedFrame==91); CHECK(texture.finalized==0);
        CHECK(gTextureList.retries==1); CHECK(gTextureList.retry==&texture);
        CHECK(image.visible==3); CHECK(!image.mUploadInFlight);
    }
    void passed()
    {
        CHECK(texture.refs==1); CHECK(texture.mRawImage==nullptr);
        CHECK(!texture.mNeedsCreateTexture); CHECK(!texture.mNeedsAux);
        CHECK(texture.finalized==1); CHECK(gTextureList.retries==0);
        CHECK(image.visible==7); CHECK(!image.mUploadInFlight);
        CHECK(texture.mCreateQueuedFrame==0);
    }
};
int main()
{
    using Case=std::pair<const char*, std::function<void()>>;
    const std::vector<Case> cases={
        {"closed main queue drops failure reply but cleanup releases owner",[]{ Context c; c.image.create_succeeds=false; CHECK(c.texture.postNativeTextureUpload({})); c.main->reject=true;c.worker->one();CHECK(c.texture.refs==2);CHECK(c.main->work.empty());LLImageGL::cleanupClass();CHECK(c.texture.refs==1);CHECK(c.texture.finalized==0);CHECK(gTextureList.retries==0);LLImageGL::cleanupClass();CHECK(c.texture.refs==1); }},
        {"closed main queue drops successful worker continuation safely",[]{ Context c;CHECK(c.texture.postNativeTextureUpload({}));c.main->reject=true;c.worker->one();CHECK(c.texture.refs==2);LLImageGL::cleanupClass();CHECK(c.texture.refs==1);CHECK(c.texture.mRawImage);CHECK(c.texture.finalized==0); }},
        {"cleanup abandons registered publication without invoking fetch callback",[]{ Context c;const auto original=c.image.metadata();c.post();c.queued();c.reply();CHECK(c.texture.refs==2);CHECK(c.image.mOnPublished.size()==1);LLImageGL::cleanupClass();CHECK(c.texture.refs==1);CHECK(c.image.mOnPublished.empty());CHECK(c.image.metadata()==original);CHECK(c.texture.finalized==0);CHECK(gTextureList.retries==0); }},
        {"last external owner leaves; shutdown releases final ref on main",[]{ Context c;CHECK(c.texture.postNativeTextureUpload({}));c.texture.unref();CHECK(c.texture.refs==1);c.main->reject=true;c.worker->one();CHECK(c.texture.zero_releases==0);LLImageGL::cleanupClass();CHECK(c.texture.refs==0);CHECK(c.texture.zero_releases==1);LLImageGL::cleanupClass();CHECK(c.texture.zero_releases==1); }},
        {"expired worker post releases owner and metadata",[]{ Context c;const auto original=c.image.metadata();c.worker.reset();CHECK(!c.texture.postNativeTextureUpload({}));CHECK(c.texture.refs==1);CHECK(c.image.metadata()==original);LLImageGL::cleanupClass();CHECK(c.texture.refs==1); }},
        {"expired main queue leaves synchronous owner unchanged",[]{ Context c;c.main.reset();CHECK(!c.texture.postNativeTextureUpload({}));CHECK(c.texture.refs==1);CHECK(!c.image.mUploadInFlight);CHECK(c.texture.mRawImage); }},
        {"queued reply after cleanup cannot use released fetched owner",[]{ Context c;c.image.create_succeeds=false;CHECK(c.texture.postNativeTextureUpload({}));c.worker->one();c.texture.unref();LLImageGL::cleanupClass();CHECK(c.texture.refs==0);CHECK(c.texture.zero_releases==1);c.reply();CHECK(c.texture.refs==0);CHECK(c.texture.finalized==0);CHECK(gTextureList.retries==0); }},
        {"stale reply cannot consume a newer upload owner",[]{ Context c;c.image.create_succeeds=false;CHECK(c.texture.postNativeTextureUpload({}));c.worker->one();LLImageGL::cleanupClass();CHECK(c.texture.refs==1);CHECK(c.texture.postNativeTextureUpload({}));c.worker->one();CHECK(c.texture.refs==2);c.reply();CHECK(c.texture.refs==2);CHECK(gTextureList.retries==0);c.reply();c.failed(); }},
        {"synchronous queue throw restores metadata and releases owner before rethrow",[]{ Context c;const auto original=c.image.metadata();c.worker->throw_post=true;bool threw=false;try{c.texture.postNativeTextureUpload({});}catch(const std::runtime_error& error){threw=std::string(error.what())=="queue allocation boundary";}CHECK(threw);CHECK(c.texture.refs==1);CHECK(c.image.metadata()==original);CHECK(!c.image.mUploadInFlight);CHECK(c.worker->work.empty());CHECK(sUploadOwners.empty());CHECK(gTextureList.retries==0); }},
        {"cleanup during moved publication callbacks cannot revive a retired owner",[]{ Context c;c.post();c.queued();c.image.afterPublish([&](bool){LLImageGL::cleanupClass();CHECK(c.texture.refs==0);});c.reply();c.texture.unref();c.image.finishPublish(true);CHECK(c.texture.refs==0);CHECK(c.texture.zero_releases==1);CHECK(c.texture.finalized==0); }},
        {"normal publication then cleanup releases no reference twice",[]{ Context c;c.post();c.queued();c.reply();c.image.finishPublish(true);c.passed();LLImageGL::cleanupClass();CHECK(c.texture.refs==1);CHECK(c.texture.finalized==1); }},
        {"creation false retains raw and retries",[]{ Context c; c.image.create_succeeds=false; c.post(); c.reply(); c.failed(); }},
        {"pending commit failure retains raw and retries",[]{ Context c; c.post(); c.queued(); c.reply(); CHECK(c.texture.refs==2); c.image.finishPublish(false); c.failed(); }},
        {"settled commit failure retains raw and retries",[]{ Context c; c.post(); c.queued(); c.image.finishPublish(false); c.reply(); c.failed(); }},
        {"pending success finalizes once",[]{ Context c; c.post(); c.queued(); c.reply(); CHECK(c.texture.refs==2); c.image.finishPublish(true); c.passed(); }},
        {"settled success finalizes once",[]{ Context c; c.post(); c.queued(); c.image.finishPublish(true); c.reply(); c.passed(); }},
        {"accepted list post retains age through failure",[]{ Context c; c.image.create_succeeds=false; c.texture.submitFromList(); CHECK(c.texture.refs==2); CHECK(c.texture.mCreateQueuedFrame==91); c.worker->one(); c.reply(); c.failed(); }},
        {"accepted list post clears age only on publication success",[]{ Context c; c.texture.submitFromList(); CHECK(c.texture.mCreateQueuedFrame==91); c.worker->one(); c.queued(); c.reply(); CHECK(c.texture.mCreateQueuedFrame==91); c.image.finishPublish(true); c.passed(); }},
        {"new successful retry resets prior failure",[]{ Context c; c.post(); c.queued(); c.image.finishPublish(false); c.reply(); c.failed(); c.texture.mCreatePending=false; gTextureList={}; c.post(); c.queued(); c.image.finishPublish(true); c.reply(); c.passed(); }},
        {"new no-publication attempt resets prior failure",[]{ Context c; c.image.mPublishesSucceeded=false; c.image.beginUpload(); bool succeeded=false; c.image.afterPublish([&](bool value){succeeded=value;}); CHECK(succeeded); }},
        {"in-flight begin preserves settled failure",[]{ Context c; c.image.mUploadInFlight=true; c.image.mPublishesSucceeded=false; c.image.beginUpload(); CHECK(!c.image.mPublishesSucceeded); }},
        {"begin preserves pending failed aggregate",[]{ Context c; c.image.mPublishesPending=1; c.image.mPublishesSucceeded=false; c.image.beginUpload(); CHECK(!c.image.mPublishesSucceeded); }},
        {"multi-publication failure aggregate waits and stays false",[]{ Context c; c.image.queuePublish({}); c.image.queuePublish({}); int calls=0; bool succeeded=true; c.image.afterPublish([&](bool value){++calls;succeeded=value;}); c.image.finishPublish(false); CHECK(calls==0); c.image.finishPublish(true); CHECK(calls==1); CHECK(!succeeded); bool late=true; c.image.afterPublish([&](bool value){late=value;}); CHECK(!late); }},
        {"rejected post restores original metadata before sync failure and retry",[]{ Context c; const auto original=c.image.metadata(); c.worker->reject=true; CHECK(!c.texture.postNativeTextureUpload({})); CHECK(c.image.metadata()==original); CHECK(!c.image.mUploadInFlight); c.image.create_succeeds=false; CHECK(!c.texture.createTexture(0,1)); CHECK(c.image.metadata()==original); CHECK(!c.image.mUploadInFlight); CHECK(c.texture.mRawImage); CHECK(c.texture.mNeedsCreateTexture); c.worker->reject=false; c.post(); c.reply(); CHECK(c.image.metadata()==original); CHECK(c.texture.mCreatePending); CHECK(c.texture.mCreateQueuedFrame==91); c.texture.mCreatePending=false; gTextureList={}; c.image.create_succeeds=true; c.post(); c.queued(); c.reply(); c.image.finishPublish(true); c.passed(); }},
        {"closed worker post leaves caller reference unchanged",[]{ Context c; c.worker->reject=true; CHECK(!c.texture.postNativeTextureUpload({})); CHECK(c.texture.refs==1); CHECK(!c.image.mUploadInFlight); CHECK(c.texture.mRawImage); CHECK(gTextureList.retries==0); }},
        {"disabled worker leaves synchronous caller responsible",[]{ Context c; LLImageGLThread::sEnabledTextures=false; CHECK(!c.texture.postNativeTextureUpload({})); CHECK(c.texture.refs==1); CHECK(c.texture.mRawImage); CHECK(gTextureList.retries==0); }},
        {"success retains requested raw-copy contract",[]{ Context c; c.texture.save_raw=true; c.post(); c.queued(); c.reply(); c.image.finishPublish(true); CHECK(c.texture.saved==1); CHECK(c.texture.mNeedsAux); CHECK(!c.texture.mNeedsCreateTexture); CHECK(c.texture.refs==1); }}
    };
    int failed=0;
    for (const auto& [name, run] : cases)
    {
        try { run(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& error) { ++failed; std::cout<<"FAIL "<<name<<": "<<error.what()<<'\n'; }
    }
    std::cout<<cases.size()<<" cases, "<<checks<<" checks, "<<failed<<" failures\n";
    return failed?1:0;
}
