// Production bodies are inserted by texture_scheduling_contract_test.py.
// Only the service boundary, texture identity and images are controlled here.
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>
using U32 = std::uint32_t;
using U64 = std::uint64_t;
using S32 = std::int32_t;
using F32 = float;
using LLUUID = unsigned;
using LLMutex = std::recursive_mutex;
struct LLMutexLock: std::lock_guard<LLMutex>
{ explicit LLMutexLock(LLMutex* mutex): std::lock_guard<LLMutex>(*mutex) {} };
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define llassert_always(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
#define LL_PROFILE_ZONE_SCOPED
struct RefCount
{
    std::atomic<unsigned> refs{0};
    virtual ~RefCount() = default;
    void ref() { ++refs; }
    void unref() { if (--refs == 0) delete this; }
};
template<class T> class LLPointer
{
public:
    LLPointer() = default;
    LLPointer(T* value): pointer(value) { if (pointer) pointer->ref(); }
    LLPointer(const LLPointer& other): LLPointer(other.pointer) {}
    LLPointer(LLPointer&& other) noexcept: pointer(std::exchange(other.pointer, nullptr)) {}
    ~LLPointer() { if (pointer) pointer->unref(); }
    LLPointer& operator=(LLPointer other) { std::swap(pointer, other.pointer); return *this; }
    T* operator->() const { return pointer; }
    operator T*() const { return pointer; }
private:
    T* pointer = nullptr;
};
struct LLImageRaw: RefCount { unsigned value = 0; explicit LLImageRaw(unsigned v): value(v) {} };
struct LLImageFormatted: RefCount { int getCodec() const { return 1; } int getDataSize() const { return 17; } };
struct LLTimer { F32 getElapsedTimeF32() const { return .25f; } };
struct Service
{
    std::deque<std::function<void()>> jobs;
    bool closed = false;
    size_t capacity = 10000;
    bool tryPost(std::function<void()> job)
    {
        if (closed || jobs.size() >= capacity) return false;
        jobs.emplace_back(std::move(job)); return true;
    }
    bool post(std::function<void()> job) { return tryPost(std::move(job)); }
    void run()
    {
        while (!jobs.empty())
        {
            auto job = std::move(jobs.front()); jobs.pop_front(); job();
        }
    }
};
struct LLTextureCacheFastResult
{
    explicit LLTextureCacheFastResult(std::shared_ptr<std::atomic<U32>> slots);
    ~LLTextureCacheFastResult();
    std::atomic<bool> ready{false};
    LLPointer<LLImageRaw> image;
    S32 discard = -1;
    F32 elapsed = 0;
    std::shared_ptr<std::atomic<U32>> slots;
};
struct LLTextureCache
{
    using handle_t = U32;
    struct Responder: RefCount
    {
        virtual bool postCompleted(bool) { return false; }
        virtual void completed(bool) = 0;
    };
    struct ReadResponder: Responder
    {
        LLPointer<LLImageFormatted> mFormattedImage;
        S32 mImageSize = 17;
        bool mImageLocal = true;
        void setImage(LLImageFormatted* image) { mFormattedImage = image; }
    };
    struct WriteResponder: Responder {};
    Service mRequestQueue;
    LLMutex mWorkersMutex, mListMutex;
    std::map<handle_t, int> mWriters;
    std::vector<handle_t> promoted;
    std::vector<std::pair<LLPointer<Responder>, bool>> mCompletedList;
    std::shared_ptr<std::atomic<U32>> mFastCacheSlots = std::make_shared<std::atomic<U32>>(0);
    bool quitting = false, fail_read = false, miss_read = false;
    static handle_t nullHandle() { return 0; }
    bool readComplete(handle_t, bool) { return true; }
    bool writeComplete(handle_t) { return true; }
    unsigned wakes = 0, reads = 0;
    bool isQuitting() const { return quitting; }
    void unpause() { ++wakes; }
    void promoteRequest(handle_t handle) { promoted.push_back(handle); }
    LLPointer<LLImageRaw> readFromFastCache(LLUUID id, S32& discard)
    {
        ++reads;
        if (fail_read) throw std::runtime_error("controlled read failure");
        if (miss_read) return {};
        discard = 4; return new LLImageRaw(id);
    }
    void prioritizeWrite(handle_t);
    void addCompleted(Responder*, bool);
    std::shared_ptr<LLTextureCacheFastResult> readFromFastCacheAsync(const LLUUID&);
    void publish()
    {
        auto copy = std::move(mCompletedList); mCompletedList.clear();
        for (auto& entry: copy) entry.first->completed(entry.second);
    }
};
struct LLTextureFetchWorker;
struct LLTextureFetch
{
    LLMutex mQueueMutex, mReadyMutex;
    LLTextureCache* mTextureCache = nullptr;
    bool isHttpWaiter(LLUUID) const { return false; }
    Service mRequestQueue;
    std::map<LLUUID, LLTextureFetchWorker*> workers;
    std::deque<std::pair<LLUUID, U64>> mReadyQueue;
    std::map<LLUUID, U64> mReadyGenerations;
    LLTextureFetchWorker* getWorkerAfterLock(LLUUID id)
    {
        auto found = workers.find(id); return found == workers.end() ? nullptr : found->second;
    }
    LLTextureFetchWorker* getWorker(LLUUID id) { LLMutexLock lock(&mQueueMutex); return getWorkerAfterLock(id); }
    void notifyReady(LLTextureFetchWorker*);
    bool popReadyTexture(LLUUID&);
};
struct LLTextureFetchWorker
{
    LLUUID mID;
    U64 mGeneration;
    U64 mCacheReadGeneration = 0, mCacheWriteGeneration = 0;
    std::atomic<S32> mCacheRespondersInFlight{0};
    LLMutex mWorkMutex;
    LLTextureFetch* mFetcher = nullptr;
    enum { LOAD_FROM_TEXTURE_CACHE = 1, WAIT_HTTP_RESOURCE2 = 3, WRITE_TO_CACHE = 10, WAIT_ON_WRITE = 11 };
    int mState = LOAD_FROM_TEXTURE_CACHE;
    bool mHttpActive = false, mInLocalCache = false, mHaveAllData = false, mLoaded = false, mWritten = false;
    S32 mFileSize = 0, mImageCodec = 0;
    LLPointer<LLImageFormatted> mFormattedImage;
    U32 mCacheReadHandle = 0, mCacheWriteHandle = 0;
    std::atomic<S32> mDecodesInFlight{0};
    unsigned wakes = 0;
    LLTextureFetchWorker(LLUUID id, U64 generation): mID(id), mGeneration(generation) {}
    bool haveWork() const { return false; }
    bool deleteOK();
    void callbackCacheRead(bool, LLImageFormatted*, S32, bool, U64);
    void callbackCacheWrite(bool, U64);
    void wakeWork() { ++wakes; }
    // @RESPONDERS@
};
struct { bool mIsDisabled = false; } gGLManager;
struct LLViewerFetchedTexture: RefCount
{
    LLTextureCache* cache;
    LLUUID id;
    bool flagged = true;
    unsigned fetches = 0, priority_updates = 0;
    std::shared_ptr<LLTextureCacheFastResult> request;
    std::function<void()> on_fetch;
    LLViewerFetchedTexture(LLTextureCache* service, LLUUID value): cache(service), id(value) {}
    bool getGLTexture() const { return true; }
    bool isInFastCacheList() const { return flagged; }
    bool hasFastCacheRequest() const { return bool(request); }
    void loadFromFastCache()
    {
        if (!flagged) return;
        if (!request) request = cache->readFromFastCacheAsync(id);
        if (!request || !request->ready.load(std::memory_order_acquire)) return;
        request.reset(); flagged = false;
    }
    void updateFetch() { ++fetches; if (on_fetch) on_fetch(); }
};
struct LLViewerTextureList
{
    std::deque<LLPointer<LLViewerFetchedTexture>> mFastCacheList, mFastCachePending;
    void updateImageDecodePriority(const LLPointer<LLViewerFetchedTexture>& image) { ++image->priority_updates; }
    F32 updateImagesLoadingFastCache(F32);
};
// @PRODUCTION@
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) throw std::runtime_error(#condition); } while (false)
int main()
{
    LLTextureCache cache;
    cache.mWriters[19] = 1;
    cache.prioritizeWrite(19); cache.prioritizeWrite(20);
    CHECK(cache.promoted == std::vector<U32>{19});
    LLTextureFetch fetch;
    LLTextureFetchWorker old(7, 1), newer(7, 2);
    fetch.workers[7] = &old;
    old.mFetcher = &fetch; newer.mFetcher = &fetch;
    CHECK(old.deleteOK());
    {
        LLPointer<LLTextureCache::Responder> read = new LLTextureFetchWorker::CacheReadResponder(&fetch, 7, new LLImageFormatted, &old);
        CHECK(old.mCacheRespondersInFlight == 1); CHECK(!old.deleteOK());
        std::unique_lock held_work(old.mWorkMutex);
        auto posting = std::async(std::launch::async, [&] { cache.addCompleted(read, true); });
        const bool posted_without_work_lock = posting.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
        held_work.unlock(); posting.get();
        CHECK(posted_without_work_lock);
        CHECK(cache.mCompletedList.empty()); CHECK(!old.mLoaded);
        read = {}; // fetch service, not the cache or a borrowed pointer, pins receiver
        CHECK(old.mCacheRespondersInFlight == 1); CHECK(!old.deleteOK());
        fetch.mRequestQueue.run();
        CHECK(old.mLoaded); CHECK(old.wakes == 1); CHECK(old.mCacheRespondersInFlight == 0); CHECK(old.deleteOK());
    }
    {
        LLPointer<LLTextureCache::Responder> read = new LLTextureFetchWorker::CacheReadResponder(&fetch, 7, new LLImageFormatted, &old);
        old.mLoaded = false;
        cache.addCompleted(read, true);
        fetch.workers[7] = &newer;
        fetch.mRequestQueue.run();
        CHECK(!old.mLoaded); CHECK(!newer.mLoaded); CHECK(!old.deleteOK());
    }
    fetch.workers[7] = &old;
    {
        LLPointer<LLTextureCache::Responder> stale = new LLTextureFetchWorker::CacheReadResponder(&fetch, 7, new LLImageFormatted, &old);
        LLPointer<LLTextureCache::Responder> current = new LLTextureFetchWorker::CacheReadResponder(&fetch, 7, new LLImageFormatted, &old);
        cache.addCompleted(stale, true);
        fetch.mRequestQueue.run(); CHECK(!old.mLoaded);
        cache.addCompleted(current, true);
        fetch.mRequestQueue.run(); CHECK(old.mLoaded);
    }
    {
        fetch.mRequestQueue.capacity = 0;
        old.mState = LLTextureFetchWorker::WAIT_ON_WRITE;
        LLPointer<LLTextureCache::Responder> write = new LLTextureFetchWorker::CacheWriteResponder(&fetch, 7, &old);
        cache.addCompleted(write, false);
        CHECK(cache.mCompletedList.size() == 1); CHECK(!old.mWritten); CHECK(!old.deleteOK());
        cache.publish(); CHECK(old.mWritten);
        fetch.mRequestQueue.capacity = 10000;
    }
    CHECK(old.mCacheRespondersInFlight == 0); CHECK(old.deleteOK());
    std::unique_lock held_map(fetch.mQueueMutex);
    auto notifying = std::async(std::launch::async, [&] { fetch.notifyReady(&old); });
    const bool posted_without_map_lock = notifying.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
    held_map.unlock(); notifying.get(); CHECK(posted_without_map_lock);
    fetch.notifyReady(&old);
    CHECK(fetch.mReadyQueue.size() == 1);
    fetch.workers[7] = &newer; fetch.notifyReady(&newer); fetch.notifyReady(&old);
    LLUUID id = 0;
    CHECK(fetch.popReadyTexture(id)); CHECK(id == 7); CHECK(!fetch.popReadyTexture(id));
    for (unsigned i = 0; i < 100; ++i) fetch.mReadyQueue.emplace_back(99, i);
    CHECK(!fetch.popReadyTexture(id)); CHECK(fetch.mReadyQueue.size() == 36);
    CHECK(!fetch.popReadyTexture(id)); CHECK(fetch.mReadyQueue.empty());
    for (unsigned i = 0; i < 4096; ++i) fetch.mReadyQueue.emplace_back(99, i);
    fetch.notifyReady(&newer); CHECK(fetch.mReadyQueue.size() == 4096); CHECK(fetch.mReadyGenerations.empty());
    fetch.mReadyQueue.clear();
    std::vector<std::shared_ptr<LLTextureCacheFastResult>> results;
    for (unsigned i = 0; i < 64; ++i) { auto result = cache.readFromFastCacheAsync(i); CHECK(bool(result)); results.push_back(result); }
    CHECK(!cache.readFromFastCacheAsync(65)); CHECK(cache.reads == 0);
    cache.mRequestQueue.run();
    CHECK(cache.reads == 64); CHECK(cache.mFastCacheSlots->load() == 64);
    CHECK(!cache.readFromFastCacheAsync(65)); // completed, uncollected results still own slots
    for (unsigned i = 0; i < 64; ++i)
    { CHECK(results[i]->ready.load(std::memory_order_acquire)); CHECK(results[i]->image->value == i); CHECK(results[i]->discard == 4); }
    results.erase(results.begin()); CHECK(cache.mFastCacheSlots->load() == 63);
    results.push_back(cache.readFromFastCacheAsync(65)); CHECK(cache.mFastCacheSlots->load() == 64);
    cache.mRequestQueue.run(); results.clear(); CHECK(cache.mFastCacheSlots->load() == 0);
    cache.mRequestQueue.closed = true;
    CHECK(!cache.readFromFastCacheAsync(66)); CHECK(cache.mFastCacheSlots->load() == 0);
    cache.mRequestQueue.closed = false; cache.fail_read = true;
    auto failed = cache.readFromFastCacheAsync(67);
    try { cache.mRequestQueue.run(); } catch (const std::runtime_error&) {}
    CHECK(failed->ready.load(std::memory_order_acquire)); CHECK(!failed->image);
    failed.reset(); CHECK(cache.mFastCacheSlots->load() == 0);
    cache.fail_read = false; cache.miss_read = true;
    auto miss = cache.readFromFastCacheAsync(68); CHECK(!miss->ready.load());
    cache.mRequestQueue.run(); CHECK(miss->ready.load(std::memory_order_acquire)); CHECK(!miss->image);
    miss.reset(); CHECK(cache.mFastCacheSlots->load() == 0);
    auto abandoned = cache.readFromFastCacheAsync(69);
    std::weak_ptr<LLTextureCacheFastResult> weak = abandoned; abandoned.reset();
    CHECK(!weak.expired()); CHECK(cache.mFastCacheSlots->load() == 1);
    cache.quitting = true; CHECK(!cache.readFromFastCacheAsync(70));
    cache.mRequestQueue.closed = true; cache.mRequestQueue.run();
    CHECK(weak.expired()); CHECK(cache.mFastCacheSlots->load() == 0);
    LLTextureCache owner_cache; owner_cache.miss_read = true;
    LLViewerTextureList list;
    std::vector<LLPointer<LLViewerFetchedTexture>> textures;
    for (unsigned index = 0; index < 100; ++index)
    {
        textures.emplace_back(new LLViewerFetchedTexture(&owner_cache, index));
        list.mFastCacheList.push_back(textures.back());
    }
    // Owner publication can append a texture; no deque iterator may survive it.
    textures[0]->on_fetch = [&]
    {
        textures.emplace_back(new LLViewerFetchedTexture(&owner_cache, 101));
        list.mFastCacheList.push_back(textures.back());
    };
    list.updateImagesLoadingFastCache(1.f);
    CHECK(owner_cache.reads == 0); CHECK(list.mFastCachePending.size() == 64);
    CHECK(list.mFastCacheList.size() == 36); CHECK(owner_cache.mFastCacheSlots->load() == 64);
    owner_cache.mRequestQueue.run(); list.updateImagesLoadingFastCache(1.f);
    CHECK(textures[0]->fetches == 1); CHECK(textures[63]->fetches == 1);
    CHECK(textures[64]->fetches == 0); CHECK(list.mFastCachePending.size() == 37);
    owner_cache.mRequestQueue.run(); list.updateImagesLoadingFastCache(1.f);
    CHECK(list.mFastCacheList.empty()); CHECK(list.mFastCachePending.empty());
    CHECK(owner_cache.mFastCacheSlots->load() == 0);
    for (const auto& texture: textures) { CHECK(texture->fetches == 1); CHECK(texture->priority_updates == 1); }
    std::cout << "Production texture scheduling: " << checks << " checks passed\n";
}
