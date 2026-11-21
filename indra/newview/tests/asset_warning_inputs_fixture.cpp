// Controlled owners/status inputs; production failure/diagnostic bodies are inserted verbatim.
#include "alwarningdiagnostics.h"
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using S32=int;
using S8=signed char;
using U32=unsigned;
using F32=float;
using S32Bytes=int;
template<class T> T llmin(T a,T b){return a<b?a:b;}
struct Entry { std::string tag,text; };
std::vector<Entry> entries;
bool throw_diagnostics=false;
struct End {};
constexpr End LL_ENDL{};
struct Log
{
    std::string tag;
    std::ostringstream value;
    explicit Log(const char* input):tag(input)
    {
        if(throw_diagnostics&&tag=="WarningInputs")throw std::bad_alloc();
    }
    template<class T> Log& operator<<(const T& item) { value<<item; return *this; }
    Log& operator<<(End) { entries.push_back({tag,value.str()});return *this; }
};
#define LL_INFOS(tag) Log(tag)
#define LL_WARNS(tag) Log(tag)
#define LL_WARNS_ONCE(tag) Log(tag)
#define LL_DEBUGS(tag) Log(tag)
#define llassert_always(value) assert(value)
struct LLMutexLock { explicit LLMutexLock(int*) {} };
constexpr const char* LOG_TXT="Texture";
std::string llformat(const char* format, void* handle)
{
    char out[64];std::snprintf(out,sizeof(out),format,handle);return out;
}
std::string llformat(const char*format,int width,int height)
{
    char out[64];std::snprintf(out,sizeof(out),format,width,height);return out;
}
struct ID
{
    std::string text="asset-fixture-uuid";
    std::string asString()const{return text;}
    friend std::ostream&operator<<(std::ostream&out,const ID&id){return out<<id.text;}
};
struct Status
{
    int code=200;
    explicit Status(int input=200):code(input){}
    explicit operator bool()const{return code>=200&&code<300;}
    bool operator==(const Status&other)const{return code==other.code;}
    std::string toTerseString()const{return "Http_"+std::to_string(code);}
    std::string toString()const{return "original-reason-only";}
    int getType()const{return code;}
    int getStatus()const{return code>=400?1:0;}
};
namespace LLCore{using HttpStatus=Status;}
struct Response{Status status;Status getStatus()const{return status;}};
struct Retry
{
    bool again=false;
    int failures=0,successes=0,decisions=0;
    void onFailure(Response*){++failures;}
    void onSuccess(){++successes;}
    bool shouldRetry(F32&delay){++decisions;delay=12;return again;}
};
struct Formatted
{
    S8 components=5;
    int getDataSize()const{return 321;}
    S8 getComponents()const{return components;}
    int getDiscardLevel()const{return 2;}
};
struct LLImageRaw
{
    int components=4;
    int getWidth()const{return 4;}
    int getHeight()const{return 4;}
    int getComponents()const{return components;}
    int getDataSize()const{return 16*components;}
};
struct ImagePointer
{
    Formatted value;
    bool notNull()const{return true;}
    const Formatted*operator->()const{return &value;}
};
struct Fetcher{int removed=0;void removeFromHTTPQueue(const ID&,int){++removed;}};
constexpr int FTT_DEFAULT=0,FTT_SERVER_BAKE=1,FTT_MAP_TILE=2,LOAD_FROM_NETWORK=3;
constexpr int HTTP_PARTIAL_CONTENT=206;
constexpr int DECODE_IMAGE_UPDATE=2;
const char*e_state_name[]={"initial","request","waiting","network"};
struct TextureOwner
{
    ID mID;
    void*mHttpHandle=reinterpret_cast<void*>(0x125);
    int mFTType=FTT_DEFAULT,mHttpPolicyClass=7;
    std::atomic<int>mState{2};
    int mRequestedOffset=31,mRequestedSize=99,mRequestedDiscard=2,mDesiredDiscard=1;
    int mLoadedDiscard=3,mDecodedDiscard=3,mCachedSize=63,mFileSize=700;
    ImagePointer mFormattedImage;
    bool mInCache=true,mInLocalCache=false,mHaveAllData=false,mFetchFailed=false;
    bool mNeedsAux=false,mDecoded=false;
    bool mDecodeNeedsAux=false;
    S32 mAuxComponents=-1;
    int mDecodeHandle=8,mWorkMutex=0,cache_removals=0;
    std::shared_ptr<std::atomic<bool>>mDecodeCancellation=std::make_shared<std::atomic<bool>>(false);
    LLImageRaw*mRawImage=nullptr,*mAuxImage=nullptr;
    int mRetryAttempt=2,mRegionRetryAttempt=1,released=0;
    Retry mFetchRetryPolicy;
    Fetcher owner;
    Fetcher*mFetcher=&owner;
    std::string mUrl="SECRET_CAPABILITY_URL";
    void releaseHttpSemaphore(){++released;}
    void setState(int state){mState=state;}
    void setGetStatus(const Status&,const std::string&){}
    void removeFromCache(){++cache_removals;}
    void decoded(bool,const std::string&,LLImageRaw*,LLImageRaw*,S32);
    void completed(Response*response,void*handle)
    {
        // @TEXTURE_FAILURE_BODY@
        (void)success;(void)partial;
    }
};
// @DECODE_BODY@
struct Material
{
    bool mFetching=true,mFetchSuccess=false;
    std::vector<int>materialCompleteCallbacks{1,2};
};
using LLFetchedGLTFMaterial=Material;
struct MaterialPointer
{
    Material value;
    const Material*get()const{return &value;}
};
struct Data{MaterialPointer mMaterial;};
void materialFailure(ID id,int asset_type,int status,U32 ext_status,Data*asset_data)
{
    // @MATERIAL_FAILURE_DIAGNOSTIC@
}
int checks=0;
void checkImpl(bool value,const char* expression)
{
    ++checks;
    if(!value){std::cerr<<"FAIL: "<<expression<<'\n';std::abort();}
}
#define check(value) checkImpl((value),#value)
std::vector<std::string> diagnostics()
{
    std::vector<std::string>out;
    for(const auto&entry:entries)if(entry.tag=="WarningInputs")out.push_back(entry.text);
    return out;
}
int main()
{
    const bool enabled=ALWarningDiagnostics::enabled();
    TextureOwner ordinary;Response forbidden{Status(403)};
    ordinary.completed(&forbidden,ordinary.mHttpHandle);
    check(ordinary.mFetchRetryPolicy.successes==1);
    check(ordinary.mFetchRetryPolicy.failures==0);
    check(ordinary.mFetchRetryPolicy.decisions==0);
    check(ordinary.released==0);
    auto out=diagnostics();check(out.size()==(enabled?1:0));
    if(enabled)
    {
        check(out.back().find("asset=asset-fixture-uuid")!=std::string::npos);
        check(out.back().find("request_handle=0x125")!=std::string::npos);
        check(out.back().find("retry_action=complete_failed")!=std::string::npos);
        check(out.back().find("SECRET")==std::string::npos);
        check(out.back().find("original-reason-only")==std::string::npos);
        check(out.back().find("cached_bytes=63")!=std::string::npos);
    }
    ordinary.completed(&forbidden,ordinary.mHttpHandle);
    check(diagnostics().size()==out.size()); // repeated asset/handle/status is bounded
    TextureOwner bake;bake.mID.text="bake-fixture";bake.mFTType=FTT_SERVER_BAKE;
    bake.mFetchRetryPolicy.again=true;bake.completed(&forbidden,bake.mHttpHandle);
    check(bake.mFetchRetryPolicy.failures==1);check(bake.mFetchRetryPolicy.decisions==1);
    check(bake.mFetchRetryPolicy.successes==0);check(bake.released==1);
    check(bake.mState==LOAD_FROM_NETWORK);check(bake.owner.removed==1);
    if(enabled)check(diagnostics().back().find("retry_action=server_bake_retry retry_delay_seconds=12")!=std::string::npos);
    TextureOwner exhausted;exhausted.mID.text="exhausted-fixture";exhausted.mFTType=FTT_SERVER_BAKE;
    exhausted.completed(&forbidden,exhausted.mHttpHandle);
    check(exhausted.mFetchRetryPolicy.decisions==1);check(exhausted.released==0);
    if(enabled)check(diagnostics().back().find("retry_action=server_bake_retry_exhausted")!=std::string::npos);
    const auto before_success=diagnostics().size();Response success{Status(200)};
    TextureOwner good;good.completed(&success,good.mHttpHandle);
    check(diagnostics().size()==before_success);check(good.mFetchRetryPolicy.successes==1);
    Data material;materialFailure(ID{"material-fixture"},57,-1,0x4000001cu,&material);
    if(enabled)
    {
        const auto line=diagnostics().back();
        check(line.find("material_asset_failure asset=material-fixture")!=std::string::npos);
        check(line.find("extended_status=1073741852")!=std::string::npos);
        check(line.find("fetching=1 previous_fetch_success=0 waiting_callbacks=2")!=std::string::npos);
        check(line.find("SECRET")==std::string::npos);
    }
    const auto material_once=diagnostics().size();
    materialFailure(ID{"material-fixture"},57,-1,0x4000001cu,&material);
    check(diagnostics().size()==material_once);
    check(material.mMaterial.value.mFetching); // logging does not complete the material
    check(!material.mMaterial.value.mFetchSuccess);
    check(material.mMaterial.value.materialCompleteCallbacks.size()==2);
    throw_diagnostics=true;
    TextureOwner noisy;noisy.mID.text="log-throw-ordinary";
    noisy.completed(&forbidden,noisy.mHttpHandle);
    check(noisy.mFetchRetryPolicy.successes==1);check(noisy.released==0);
    TextureOwner noisy_bake;noisy_bake.mID.text="log-throw-bake";
    noisy_bake.mFTType=FTT_SERVER_BAKE;noisy_bake.mFetchRetryPolicy.again=true;
    noisy_bake.completed(&forbidden,noisy_bake.mHttpHandle);
    check(noisy_bake.mFetchRetryPolicy.decisions==1);check(noisy_bake.released==1);
    check(noisy_bake.mState==LOAD_FROM_NETWORK);
    materialFailure(ID{"log-throw-material"},57,-1,0x4000001cu,&material);
    check(material.mMaterial.value.mFetching);
    throw_diagnostics=false;
    LLImageRaw rgba,mask{1};
    TextureOwner decode_aux;decode_aux.mID.text="decode-with-aux";decode_aux.mNeedsAux=true;
    const auto completed_cancellation=decode_aux.mDecodeCancellation;
    decode_aux.decoded(true,"",&rgba,&mask,8);
    check(decode_aux.mRawImage==&rgba);check(decode_aux.mAuxImage==&mask);
    check(decode_aux.mDecoded);check(decode_aux.mDecodeHandle==0);
    check(decode_aux.mDecodedDiscard==2);
    check(decode_aux.mAuxComponents==1);
    check(!decode_aux.mDecodeCancellation);
    check(!completed_cancellation->load(std::memory_order_acquire));
    if(enabled)
    {
        const auto line=diagnostics().back();
        check(line.find("bake_decode_result asset=decode-with-aux")!=std::string::npos);
        check(line.find("decode_id=8 active_decode_id=8 needs_aux=1")!=std::string::npos);
        check(line.find("formatted_components=5")!=std::string::npos);
        check(line.find('\x05')==std::string::npos);
        check(line.find("aux_components=1 aux_bytes=16")!=std::string::npos);
        check(line.find("SECRET")==std::string::npos);
    }
    TextureOwner early_bake;early_bake.mID.text="decode-before-aux-demand";early_bake.mFTType=FTT_SERVER_BAKE;
    early_bake.decoded(true,"",&rgba,nullptr,8);
    check(early_bake.mDecoded);check(early_bake.mAuxImage==nullptr);
    if(enabled)
    {
        const auto line=diagnostics().back();
        check(line.find("needs_aux=0")!=std::string::npos);
        check(line.find("aux_present=0")!=std::string::npos);
    }
    TextureOwner maskless;maskless.mID.text="decode-four-components";maskless.mNeedsAux=true;
    maskless.mFormattedImage.value.components=4;
    maskless.decoded(true,"",&rgba,nullptr,8);
    check(maskless.mDecoded);check(maskless.mAuxComponents==0);
    if(enabled)
    {
        const auto line=diagnostics().back();
        check(line.find("formatted_components=4")!=std::string::npos);
        check(line.find('\x04')==std::string::npos);
    }
    const auto before_plain=diagnostics().size();
    TextureOwner plain;plain.decoded(true,"",&rgba,nullptr,8);
    check(plain.mDecoded);check(diagnostics().size()==before_plain);
    TextureOwner aborted;aborted.mNeedsAux=true;aborted.mDecodeHandle=0;
    const auto aborted_cancellation=aborted.mDecodeCancellation;
    aborted_cancellation->store(true,std::memory_order_release);
    aborted.decoded(true,"",&rgba,&mask,8);check(!aborted.mDecoded);
    check(aborted.mDecodeCancellation==aborted_cancellation);
    check(aborted_cancellation->load(std::memory_order_acquire));
    check(diagnostics().size()==before_plain);
    TextureOwner obsolete;obsolete.mNeedsAux=true;
    const auto current_cancellation=obsolete.mDecodeCancellation;
    obsolete.decoded(true,"",&rgba,&mask,7);check(!obsolete.mDecoded);
    check(obsolete.mDecodeHandle==8);check(diagnostics().size()==before_plain);
    check(obsolete.mDecodeCancellation==current_cancellation);
    check(!current_cancellation->load(std::memory_order_acquire));
    TextureOwner wrong_state;wrong_state.mNeedsAux=true;wrong_state.mState=3;
    const auto wrong_state_cancellation=wrong_state.mDecodeCancellation;
    wrong_state.decoded(true,"",&rgba,&mask,8);check(!wrong_state.mDecoded);
    check(wrong_state.mDecodeHandle==0);check(diagnostics().size()==before_plain);
    check(wrong_state.mDecodeCancellation==wrong_state_cancellation);
    check(!wrong_state_cancellation->load(std::memory_order_acquire));
    TextureOwner failed_decode;failed_decode.mNeedsAux=true;
    const auto failed_cancellation=failed_decode.mDecodeCancellation;
    failed_decode.decoded(false,"fixture",nullptr,nullptr,8);
    check(failed_decode.mDecoded);check(failed_decode.mDecodedDiscard==-1);
    check(failed_decode.cache_removals==1);check(diagnostics().size()==before_plain);
    check(!failed_decode.mDecodeCancellation);
    check(!failed_cancellation->load(std::memory_order_acquire));
    TextureOwner logging_fail;logging_fail.mID.text="decode-log-allocation-fail";logging_fail.mNeedsAux=true;
    const auto logging_cancellation=logging_fail.mDecodeCancellation;
    throw_diagnostics=true;logging_fail.decoded(true,"",&rgba,&mask,8);throw_diagnostics=false;
    check(logging_fail.mDecoded);check(logging_fail.mRawImage==&rgba);
    check(logging_fail.mAuxImage==&mask);check(logging_fail.mDecodeHandle==0);
    check(!logging_fail.mDecodeCancellation);
    check(!logging_cancellation->load(std::memory_order_acquire));
    // A stale callback retains the current request's token until its own
    // completion. Keep a decoder-side lease to observe the reset distinctly
    // from cancellation or premature destruction.
    obsolete.mNeedsAux=false;
    obsolete.decoded(true,"",&rgba,nullptr,8);
    check(obsolete.mDecoded);check(obsolete.mRawImage==&rgba);
    check(!obsolete.mDecodeCancellation);
    check(!current_cancellation->load(std::memory_order_acquire));
    if(!enabled)check(diagnostics().empty());
    std::cout<<"asset warning diagnostics enabled="<<enabled<<": "<<checks<<" checks passed\n";
}
