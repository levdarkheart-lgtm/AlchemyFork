// PPD-467: controlled queue/storage inputs around verbatim production bodies.
// Run llviewertexture_upload_recovery_test.py; this template is not built alone.
#include <atomic>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
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
#define llassert(value) assert(value)
using S8 = signed char;
using S32 = int;
using U32 = unsigned;
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
};
struct Queue
{
    bool reject = false;
    bool worker_thread=false;
    std::queue<std::function<void()>> work;
    template<class F, class C>
    bool postTo(std::weak_ptr<Queue> target, F callable, C callback)
    {
        const auto destination = target.lock();
        if (!destination || destination->reject) return false;
        destination->work.push([this, callable, callback]() mutable
        {
            if constexpr (std::is_void_v<std::invoke_result_t<F>>)
            {
                callable();
                work.push(std::move(callback));
            }
            else
            {
                auto result = callable();
                work.push([callback, result]() mutable { callback(result); });
            }
        });
        return true;
    }
    void one()
    {
        assert(!work.empty());
        auto call = std::move(work.front()); work.pop();
        const bool previous=main_thread; main_thread=!worker_thread;
        call(); main_thread=previous;
    }
};
struct ALNativeFrameBudget
{
    using Lease = std::shared_ptr<int>;
    static ALNativeFrameBudget& instance() { static ALNativeFrameBudget value; return value; }
    void setPendingPublications(std::size_t) {}
};
struct LLImageGLThread { static inline bool sEnabledTextures = true; };
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
struct PendingPublish { LLPointer<LLImageGL> image; LLImageGL::UploadCompletion token; };
std::vector<PendingPublish> sPendingPublishes;
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
    void unref() { --refs; assert(refs>=1); }
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
    main_queue.lock()->work.push([this]{ queuePublish({}); });
    return true;
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
