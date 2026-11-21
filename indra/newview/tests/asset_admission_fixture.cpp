// Actual cache admission, fetch write-state and audio finalization bodies are
// inserted by asset_admission_contract_test.py. Only admission and I/O are controlled.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using U8 = std::uint8_t; using S16 = std::int16_t; using S32 = std::int32_t;
using U32 = std::uint32_t; using LLUUID = unsigned; using F32 = float;
using LLMutex = std::recursive_mutex;
struct LLMutexLock : std::lock_guard<LLMutex> { explicit LLMutexLock(LLMutex* m) : std::lock_guard<LLMutex>(*m) {} };
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_PROFILE_ZONE_NAMED_CATEGORY_THREAD(...)
#define llassert_always(c) do { if (!(c)) throw std::runtime_error(#c); } while (false)
std::ostringstream logs;
#define LL_WARNS(...) logs
#define LL_DEBUGS(...) logs
#define LL_ENDL '\n'
template<class T> T llmin(T a, T b) { return std::min(a, b); }
S32 llfloor(F32 value) { return static_cast<S32>(std::floor(value)); }
void llendianswizzle(void*, int, int) {}
void ov_clear(int* count) { ++*count; }
struct RefCount
{
    std::atomic<int> refs{0};
    virtual ~RefCount() = default;
    void ref() { ++refs; } void unref() { if (--refs == 0) delete this; }
};
template<class T> class LLPointer
{
public:
    LLPointer() = default; LLPointer(T* p) : ptr(p) { if (ptr) ptr->ref(); }
    LLPointer(const LLPointer& other) : LLPointer(other.ptr) {}
    LLPointer(LLPointer&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    ~LLPointer() { if (ptr) ptr->unref(); }
    LLPointer& operator=(LLPointer other) { std::swap(ptr, other.ptr); return *this; }
    T* operator->() const { return ptr; } operator T*() const { return ptr; }
    bool isNull() const { return !ptr; }
private: T* ptr = nullptr;
};
struct LLImageRaw : RefCount {};
struct Image : RefCount
{
    U8 data[16] = {}; S32 getDataSize() const { return sizeof(data); } U8* getData() { return data; }
};
struct LLImageDataSharedLock { explicit LLImageDataSharedLock(Image*) {} };
struct Timer { void reset() {} };
struct LLWorkerThread { static U32 nullHandle() { return 0; } };
class LLTextureCacheWorker;
struct LLTextureCache
{
    using handle_t = U32;
    enum class Admission { ACCEPTED, BUSY, CLOSED, READ_ONLY };
    struct Responder : RefCount
    {
        static inline int live = 0;
        Responder() { ++live; } ~Responder() override { --live; }
    };
    struct ReadResponder : Responder {}; struct WriteResponder : Responder {};
    LLMutex mWorkersMutex;
    bool closed = false, mReadOnly = false, mDoPurge = false;
    size_t capacity = 1, registered = 0;
    unsigned next = 0, discarded = 0;
    std::vector<int> mPurgeEntryList;
    std::map<U32, LLTextureCacheWorker*> mReaders, mWriters;
    ~LLTextureCache();
    bool isAdmissionClosed() const { return closed; }
    static U32 nullHandle() { return 0; }
    void purgeTexturesLazy(F32) {}
    handle_t readFromCache(const std::string&, const LLUUID&, S32, S32, ReadResponder*);
    handle_t readFromCache(const LLUUID&, S32, S32, ReadResponder*);
    handle_t writeToCache(const LLUUID&, const U8*, S32, S32, LLPointer<LLImageRaw>, S32,
                         WriteResponder*, Admission* = nullptr);
};
constexpr F32 TEXTURE_LAZY_PURGE_TIME_LIMIT = .01f;
class LLTextureCacheWorker
{
public:
    LLTextureCacheWorker(LLTextureCache* c, LLTextureCache::Responder* r) : cache(c), responder(r) {}
    virtual ~LLTextureCacheWorker() { if (handle) --cache->registered; }
    U32 read() { return submit(); } U32 write() { return submit(); }
    void discardUnaccepted() { llassert_always(!handle); ++cache->discarded; delete this; }
private:
    U32 submit()
    {
        if (cache->closed || cache->registered >= cache->capacity) return 0;
        ++cache->registered; return handle = ++cache->next;
    }
    LLTextureCache* cache; LLPointer<LLTextureCache::Responder> responder; U32 handle = 0;
};
struct LLTextureCacheLocalFileWorker : LLTextureCacheWorker
{
    LLTextureCacheLocalFileWorker(LLTextureCache* c, const std::string&, LLUUID, U8*, S32, S32, S32,
                                 LLTextureCache::ReadResponder* r) : LLTextureCacheWorker(c, r) {}
};
struct LLTextureCacheRemoteWorker : LLTextureCacheWorker
{
    LLTextureCacheRemoteWorker(LLTextureCache* c, LLUUID, const U8*, S32, S32, S32,
                              LLPointer<LLImageRaw>, S32, LLTextureCache::Responder* r) : LLTextureCacheWorker(c, r) {}
};
LLTextureCache::~LLTextureCache()
{
    for (auto& [id, worker] : mReaders) delete worker;
    for (auto& [id, worker] : mWriters) delete worker;
}
struct LLTextureFetch { LLTextureCache* mTextureCache; };
struct LLTextureFetchWorker
{
    enum { WRITE_TO_CACHE, WAIT_ON_WRITE, DONE, SHOULD_WRITE };
    struct CacheWriteResponder : LLTextureCache::WriteResponder
    { CacheWriteResponder(LLTextureFetch*, LLUUID, LLTextureFetchWorker*) {} };
    LLTextureFetch* mFetcher; LLUUID mID = 1;
    int mState = WRITE_TO_CACHE, mWriteToCacheState = SHOULD_WRITE;
    LLPointer<Image> mFormattedImage{new Image}; LLPointer<LLImageRaw> mRawImage{new LLImageRaw};
    S32 mFileSize = 0, mDecodedDiscard = 2; bool mHaveAllData = true, mWritten = false;
    unsigned mCacheWriteCount = 0, continued = 0; U32 mCacheWriteHandle = 0; Timer mCacheWriteTimer;
    void setState(int state) { mState = state; }
    bool doWork(S32) { ++continued; return true; }
    bool writeStep(S32);
};
struct LLLFSThread
{
    using handle_t = U32;
    enum class Admission { ACCEPTED, BUSY, CLOSED };
    struct Responder : RefCount { virtual void completed(S32) = 0; };
    static inline LLLFSThread* sLocal = nullptr;
    static U32 nullHandle() { return 0; }
    Admission outcome = Admission::BUSY;
    LLPointer<Responder> pending; unsigned attempts = 0;
    U32 write(const std::string&, U8*, S32, S32, Responder* responder, Admission* admission)
    {
        ++attempts; LLPointer<Responder> owned(responder); *admission = outcome;
        if (outcome != Admission::ACCEPTED) return 0;
        pending = owned; return 23;
    }
    void complete(S32 bytes) { pending->completed(bytes); pending = {}; }
};
constexpr S32 WAV_HEADER_SIZE = 44;
struct LLVorbisDecodeState : RefCount
{
    struct WriteResponder : LLLFSThread::Responder
    {
        explicit WriteResponder(LLVorbisDecodeState* owner) : state(owner) {}
        void completed(S32 bytes) override { state->mBytesRead = bytes; }
        LLPointer<LLVorbisDecodeState> state;
    };
    bool mValid = true, mDone = false, mWAVFinalized = false;
    std::atomic<S32> mBytesRead{-1}; int mVF = 0; U32 mFileHandle = 0;
    std::vector<U8> mWAVBuffer = std::vector<U8>(WAV_HEADER_SIZE + 512, 19);
    std::string mOutFilename = "controlled";
    bool isValid() const { return mValid; } unsigned getUUID() const { return 1; }
    bool finishDecode();
};
// @PRODUCTION@
unsigned checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) throw std::runtime_error(#c); } while (false)
int main()
{
    {
        LLTextureCache cache; cache.capacity = 0;
        CHECK(cache.readFromCache(1, 0, 16, new LLTextureCache::ReadResponder) == 0);
        CHECK(cache.readFromCache("controlled", 1, 0, 16, new LLTextureCache::ReadResponder) == 0);
        CHECK(cache.mReaders.empty()); CHECK(cache.registered == 0);
        CHECK(cache.discarded == 2); CHECK(LLTextureCache::Responder::live == 0);
        LLTextureCache::Admission admission;
        U8 bytes[16] = {};
        CHECK(cache.writeToCache(1, bytes, 16, 16, {}, 0, new LLTextureCache::WriteResponder, &admission) == 0);
        CHECK(admission == LLTextureCache::Admission::BUSY); CHECK(cache.mWriters.empty());
        CHECK(cache.discarded == 3); CHECK(LLTextureCache::Responder::live == 0);
        cache.mReadOnly = true;
        CHECK(cache.writeToCache(1, bytes, 16, 16, {}, 0, new LLTextureCache::WriteResponder, &admission) == 0);
        CHECK(admission == LLTextureCache::Admission::READ_ONLY); CHECK(LLTextureCache::Responder::live == 0);
        cache.closed = true;
        CHECK(cache.writeToCache(1, bytes, 16, 16, {}, 0, new LLTextureCache::WriteResponder, &admission) == 0);
        CHECK(admission == LLTextureCache::Admission::CLOSED); CHECK(LLTextureCache::Responder::live == 0);
        cache.closed = false; cache.mReadOnly = false; cache.capacity = 1;
        CHECK(cache.readFromCache(1, 0, 16, new LLTextureCache::ReadResponder) != 0);
        CHECK(cache.mReaders.size() == 1); CHECK(cache.registered == 1);
    }
    CHECK(LLTextureCache::Responder::live == 0);
    for (int mode : {0, 1, 2})
    {
        LLTextureCache cache; cache.capacity = 0; LLTextureFetch fetch{&cache};
        LLTextureFetchWorker worker; worker.mFetcher = &fetch;
        CHECK(!worker.writeStep(0)); CHECK(worker.mState == worker.WRITE_TO_CACHE);
        CHECK(worker.mCacheWriteHandle == 0); CHECK(worker.mCacheWriteCount == 0);
        CHECK(cache.mWriters.empty()); CHECK(LLTextureCache::Responder::live == 0);
        if (mode == 1) cache.mReadOnly = true;
        else if (mode == 2) cache.closed = true;
        else cache.capacity = 1;
        CHECK(worker.writeStep(0) == (mode != 0));
        CHECK(worker.mState == (mode == 0 ? worker.WAIT_ON_WRITE : worker.DONE));
        CHECK(worker.mCacheWriteCount == (mode == 0 ? 1u : 0u));
        CHECK(cache.registered == (mode == 0 ? 1u : 0u));
    }
    CHECK(LLTextureCache::Responder::live == 0);
    {
        LLLFSThread lfs; LLLFSThread::sLocal = &lfs;
        LLPointer<LLVorbisDecodeState> audio = new LLVorbisDecodeState;
        CHECK(!audio->finishDecode()); CHECK(audio->mVF == 1);
        const auto finalized = audio->mWAVBuffer;
        CHECK(!audio->finishDecode()); CHECK(audio->mVF == 1); CHECK(audio->mWAVBuffer == finalized);
        CHECK(!audio->mDone); CHECK(audio->refs == 1);
        lfs.outcome = LLLFSThread::Admission::ACCEPTED;
        CHECK(!audio->finishDecode()); CHECK(audio->refs == 2);
        CHECK(!audio->finishDecode()); CHECK(lfs.attempts == 3);
        lfs.complete(finalized.size()); CHECK(audio->refs == 1);
        CHECK(audio->finishDecode()); CHECK(audio->mValid); CHECK(audio->mDone);
        CHECK(audio->mVF == 1); CHECK(audio->mWAVBuffer == finalized);
        CHECK(lfs.attempts == 3);
    }
    {
        LLLFSThread lfs; lfs.outcome = LLLFSThread::Admission::CLOSED; LLLFSThread::sLocal = &lfs;
        LLPointer<LLVorbisDecodeState> audio = new LLVorbisDecodeState;
        CHECK(audio->finishDecode()); CHECK(!audio->mValid); CHECK(audio->refs == 1);
        CHECK(audio->mVF == 1); CHECK(lfs.attempts == 1);
    }
    std::cout << "Actual asset admission: " << checks << " checks passed\n";
}
