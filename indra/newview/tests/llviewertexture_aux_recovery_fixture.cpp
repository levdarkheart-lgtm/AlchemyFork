// PPD-471: generated driver inserts production callback/fetch bodies below.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using S8=signed char; using S32=int; using U32=unsigned; using F32=float; using F64=double;
constexpr S8 S8_MAX=127;
constexpr S32 MAX_DISCARD_LEVEL=5, INVALID_DISCARD_LEVEL=-1, FTT_SERVER_BAKE=1, TEXTURE_CACHE_ENTRY_SIZE=600;
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_PROFILE_ZONE_SCOPED
#define LL_WARNS(...) std::ostringstream()
#define LL_WARNS_ONCE(...) std::ostringstream()
#define LL_INFOS(...) std::ostringstream()
#define LL_DEBUGS(...) std::ostringstream()
#define LL_ENDL std::endl
#define llassert(x) assert(x)
#define llassert_always(x) assert(x)
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llmax(T a,T b){return std::max(a,b);}
struct Ref {int refs=0;virtual ~Ref()=default;void ref(){++refs;}void unref(){if(!--refs)delete this;}};
template<class T> struct LLPointer
{
 T* p=nullptr;LLPointer()=default;LLPointer(T*q):p(q){if(p)p->ref();}LLPointer(const LLPointer&x):LLPointer(x.p){}
 ~LLPointer(){if(p)p->unref();}LLPointer&operator=(LLPointer x){std::swap(p,x.p);return *this;}
 T*operator->()const{return p;}operator T*()const{return p;}T*get()const{return p;}bool notNull()const{return p;}bool isNull()const{return !p;}
};
struct LLImageRaw:Ref
{
 int w=16,h=16,c=4;std::vector<unsigned char> data;
 LLImageRaw(int width=16,int height=16,int components=4):w(width),h(height),c(components),data(w*h*c,7){}
 LLImageRaw(const unsigned char*p,int w,int h,int c):LLImageRaw(w,h,c){std::copy(p,p+data.size(),data.begin());}
 int getWidth()const{return w;}int getHeight()const{return h;}int getComponents()const{return c;}
 int getDataSize()const{return int(data.size());}unsigned char*getData(){return data.empty()?nullptr:data.data();}
};
struct LLViewerFetchedTexture;
using loaded_callback_func=void(*)(bool,LLViewerFetchedTexture*,LLImageRaw*,LLImageRaw*,S32,bool,void*);
using ETexListType=int;using LLTextureKey=std::pair<std::string,int>;
struct LLLoadedCallbackEntry
{
 using source_callback_list_t=std::set<LLTextureKey>;
 loaded_callback_func mCallback;S32 mLastUsedDiscard=6,mDesiredDiscard;bool mNeedsImageRaw,mNeedsAux=false;
 void*mUserData;source_callback_list_t*mSourceCallbackList;bool mPaused;
 // @ENTRY_DECL@
 ~LLLoadedCallbackEntry()=default;
};
struct TextureList{std::set<LLViewerFetchedTexture*>mCallbackList;}gTextureList;
struct LLViewerFetchedTexture
{
 using callback_list_t=std::list<LLLoadedCallbackEntry*>;callback_list_t mLoadedCallbackList;
 S8 mLoadedCallbackDesiredDiscardLevel=6;bool mPauseLoadedCallBacks=false,mNeedsAux=false,mSaveRawImage=false,mHasAux=false;
 bool mNeedsCreateTexture=true,mIsFetching=false,mIsRawImageValid=false,mForceCallbackFetch=false,mForceToSaveRawImage=false,mSavedRawFetchFailed=false;
 LLPointer<LLImageRaw>mAuxRawImage,mRawImage,mSavedRawImage;
 S32 mRawDiscardLevel=-1,mSavedRawDiscardLevel=-1,mDesiredSavedRawDiscardLevel=-1,mFTType=FTT_SERVER_BAKE,mAuxComponents=-1,mAuxRawDiscardLevel=-1;
 F32 mKeptSavedRawImageTime=0,mLastCallBackActiveTime=1,mLastReferencedSavedRawImageTime=1,mMaxVirtualSize=1;
 static inline F64 sCurrentTime=1;static inline S32 sRawCount=0,sAuxCount=0;
 std::string mID,mUrl;int gpu_discard=0;bool missing_asset=false;
 std::string getID()const{return mID;}int getTextureListType()const{return 0;}S32 getDiscardLevel()const{return gpu_discard;}
 S32 getMaxDiscardLevel()const{return 5;}S32 getDesiredDiscardLevel()const{return 0;}bool isMissingAsset()const{return missing_asset;}
 void resetTextureStats(){}void setActive(){}void rebuildSculptVolumes(){}
 void clearCallbackEntryList();
 void destroySavedRawImage(){mSavedRawImage=nullptr;mSaveRawImage=false;mForceToSaveRawImage=false;}
 void readbackRawImage(){throw std::runtime_error("Unexpected GPU readback");}
 void saveRawImage(){mSavedRawImage=new LLImageRaw(mRawImage->getData(),mRawImage->w,mRawImage->h,mRawImage->c);mSavedRawDiscardLevel=mRawDiscardLevel;}
 void updateAuxDemand();void postCreateTexture();bool needsToSaveRawImage();void destroyRawImage();bool doLoadedCallbacks();
 void pauseLoadedCallbacks(const LLLoadedCallbackEntry::source_callback_list_t*);
 void unpauseLoadedCallbacks(const LLLoadedCallbackEntry::source_callback_list_t*);
 void forceToRefetchTexture(S32=0,F32=0);S32 getCurrentDiscardLevelForFetching();
 void setLoadedCallback(loaded_callback_func,S32,bool,bool,void*,LLLoadedCallbackEntry::source_callback_list_t*,bool);
 ~LLViewerFetchedTexture(){for(auto*x:mLoadedCallbackList)delete x;gTextureList.mCallbackList.erase(this);}
};
namespace ALWarningDiagnostics{enum class Kind{bake_decode};bool enabled(){return false;}bool take(Kind,const std::string&){return false;}}
struct UID{std::string asString()const{return "opaque";}friend std::ostream&operator<<(std::ostream&o,const UID&){return o<<"opaque";}};
std::string llformat(const char*,int,int){return {};}
struct LLMutexLock{explicit LLMutexLock(void*){}};
struct Timer{F32 getElapsedTimeF32()const{return 0;}};
struct Formatted:Ref{S32 components=5,bytes=600,discard=0;S32 getComponents()const{return components;}S32 getDataSize()const{return bytes;}S32 getDiscardLevel()const{return discard;}bool updateData(){return components>0;}void deleteData(){bytes=0;}};
struct LLTextureFetchWorker;
struct LLTextureFetch
{
 struct FetchStatus{S32 mState=0,mAuxComponents=-1;F32 mDataProgress=0,mRequestedPriority=0,mFetchDeltaTime=0,mRequestDeltaTime=0;U32 mFetchPriority=0;bool mCanUseHTTP=false;};
 static constexpr S32 CREATE_REQUEST_ERROR_TRANSITION=-3,CREATE_REQUEST_ERROR_ABORTED=-2;
 FetchStatus status;S32 admit(LLTextureFetchWorker*,bool);bool complete(LLTextureFetchWorker*);
};
struct LLTextureFetchWorker
{
 struct HTTPBuffer{void release(){}};HTTPBuffer*mHttpBufferArray=nullptr;S32 mHttpReplySize=0,mHttpReplyOffset=0;std::string mUrl;void setUrl(const std::string&);void resetFormattedData();
 enum{INVALID=0,INIT=1,LOAD_FROM_NETWORK=3,WAIT_HTTP_REQ=7,DECODE_IMAGE=9,DECODE_IMAGE_UPDATE=10,WAIT_ON_WRITE=12,DONE=13};
 std::atomic<S32>mState{DONE};bool mNeedsAux=false,mDecodeNeedsAux=false,mHaveAllData=true,mDecoded=true;
 S32 mAuxComponents=-1,mDecodedDiscard=0,mDesiredDiscard=0,mDesiredSize=600,mActiveCount=0,mDecodeHandle=11,mLoadedDiscard=0,mRequestedDiscard=0,mFTType=FTT_SERVER_BAKE;
 S32 mFileSize=600;F32 mRequestedPriority=1,mImagePriority=1;UID mID;void*mWorkMutex=nullptr;
 std::shared_ptr<std::atomic<bool>>mDecodeCancellation=std::make_shared<std::atomic<bool>>(false);
 Timer mFetchDeltaTimer,mRequestedDeltaTimer;LLPointer<Formatted>mFormattedImage{new Formatted};LLPointer<LLImageRaw>mRawImage{new LLImageRaw},mAuxImage;
 bool have_work=false,aborted=false;int posts=0,cache_removes=0;
 void lockWorkMutex(){}void unlockWorkMutex(){}bool wasAborted()const{return aborted;}bool haveWork()const{return have_work;}
 S32 calcDesiredSize(S32,S32 n)const{return n;}void setImagePriority(F32){}void setDesiredDiscard(S32 d,S32 n){mDesiredDiscard=d;mDesiredSize=n;}
 void setCanUseHTTP(bool){}bool getCanUseHTTP()const{return true;}F32 getImagePriority()const{return 1;}
 void setState(S32 s){mState=s;}void addWork(S32 n){startWork(n);have_work=true;++posts;}void removeFromCache(){++cache_removes;}
 void getStatus(LLTextureFetch::FetchStatus&)const;bool requiresAuxDecode()const;void startWork(S32);void endWork(S32,bool);
 void callbackDecoded(bool,const std::string&,LLImageRaw*,LLImageRaw*,S32);
 void prepareDecode();bool advanceDecode();int decode_reentries=0;bool doWork(S32){++decode_reentries;return false;}
};
// @PRODUCTION@
S32 LLTextureFetch::admit(LLTextureFetchWorker*worker,bool needs_aux)
{
 S32 desired_discard=0,desired_size=600;F32 priority=1;bool can_use_http=true;
 // @ADMIT@
 return desired_discard;
}
bool LLTextureFetch::complete(LLTextureFetchWorker*worker)
{
 worker->have_work=false;worker->endWork(0,false);
 // @COMPLETE@
 return true;
}
void LLTextureFetchWorker::prepareDecode(){
 // @PREPARE@
}
bool LLTextureFetchWorker::advanceDecode(){[[maybe_unused]] S32 param=0;
 // @ADVANCE@
 return true;
}
int checks=0;void check(bool b,const char*e,int l){++checks;if(!b)throw std::runtime_error(std::string(e)+"@"+std::to_string(l));}
#define CHECK(x) check(bool(x),#x,__LINE__)
struct Seen{int calls=0;bool success=false,final=false,aux=false;int discard=-1;};
void observed(bool success,LLViewerFetchedTexture*,LLImageRaw*,LLImageRaw*aux,S32 d,bool final,void*p){auto&x=*static_cast<Seen*>(p);++x.calls;x.success=success;x.final=final;x.aux=aux;x.discard=d;}
void data(LLViewerFetchedTexture&t,bool aux,int level=0){t.mRawImage=new LLImageRaw;t.mIsRawImageValid=true;t.mRawDiscardLevel=level;++t.sRawCount;if(aux){t.mAuxRawImage=new LLImageRaw(16,16,1);t.mAuxRawDiscardLevel=level;t.mHasAux=true;++t.sAuxCount;}t.mNeedsCreateTexture=false;}
int main()
{
 using Case=std::pair<const char*,std::function<void()>>;
 std::vector<Case>cases={
 {"early aux success",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);data(t,true);CHECK(t.doLoadedCallbacks());CHECK(x.success&&x.final&&x.aux);}},
 {"late aux waits then succeeds",[]{LLViewerFetchedTexture t;Seen x;t.mNeedsCreateTexture=false;t.mSavedRawImage=new LLImageRaw;t.mSavedRawDiscardLevel=0;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);t.doLoadedCallbacks();CHECK(x.calls==0);data(t,true);CHECK(t.doLoadedCallbacks());CHECK(x.calls==1&&x.success&&x.aux);}},
 {"pause resume restores demand",[]{LLViewerFetchedTexture t;Seen x;LLLoadedCallbackEntry::source_callback_list_t owner;t.setLoadedCallback(observed,0,true,true,&x,&owner,false);t.pauseLoadedCallbacks(&owner);data(t,true);t.mNeedsCreateTexture=true;t.postCreateTexture();CHECK(t.mAuxRawImage.isNull());t.unpauseLoadedCallbacks(&owner);CHECK(t.mNeedsAux);data(t,true);CHECK(t.doLoadedCallbacks());CHECK(x.success&&x.aux);}},
 {"paused aux does not block active raw",[]{LLViewerFetchedTexture t;Seen a,b;LLLoadedCallbackEntry::source_callback_list_t owner;t.setLoadedCallback(observed,0,true,true,&a,&owner,false);t.pauseLoadedCallbacks(&owner);t.setLoadedCallback(observed,0,true,false,&b,nullptr,false);data(t,false);t.doLoadedCallbacks();CHECK(a.calls==0);CHECK(b.calls==1&&b.success);}},
 {"mixed raw proceeds aux waits",[]{LLViewerFetchedTexture t;Seen a,b;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.setLoadedCallback(observed,0,true,false,&b,nullptr,false);data(t,false);t.doLoadedCallbacks();CHECK(a.calls==0);CHECK(b.calls==1&&b.success);data(t,true);t.doLoadedCallbacks();CHECK(a.calls==1&&a.aux);}},
 {"known complete maskless fails only aux",[]{LLViewerFetchedTexture t;Seen a,b;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.setLoadedCallback(observed,0,true,false,&b,nullptr,false);t.mAuxComponents=0;data(t,false);t.doLoadedCallbacks();CHECK(a.calls==1&&!a.success&&a.final);CHECK(b.calls==1&&b.success);CHECK(t.mLoadedCallbackList.empty());t.doLoadedCallbacks();CHECK(a.calls==1);}},
 {"partial unknown does not fail aux",[]{LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);data(t,false);t.mAuxComponents=-1;t.doLoadedCallbacks();CHECK(a.calls==0);CHECK(t.mNeedsAux);}},
 {"aux quality controls final",[]{LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);data(t,true);t.mAuxRawDiscardLevel=3;t.doLoadedCallbacks();CHECK(a.calls==1&&!a.final&&a.discard==3);data(t,true);t.doLoadedCallbacks();CHECK(a.calls==2&&a.final&&a.discard==0);}},
 {"failed decode fails aux without fake success",[]{LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.mAuxComponents=-2;t.mNeedsCreateTexture=false;t.doLoadedCallbacks();CHECK(a.calls==1&&!a.success&&a.final);}},
 {"DONE same quality admits retained decode",[]{LLTextureFetch f;LLTextureFetchWorker w;w.endWork(0,false);CHECK(w.mFormattedImage.notNull());CHECK(f.admit(&w,true)>=0);CHECK(w.mNeedsAux&&w.posts==1);CHECK(w.mState==w.DECODE_IMAGE);}},
 {"same DONE primary request stays duplicate",[]{LLTextureFetch f;LLTextureFetchWorker w;CHECK(f.admit(&w,false)==f.CREATE_REQUEST_ERROR_TRANSITION);CHECK(w.posts==0);}},
 {"active request records late demand",[]{LLTextureFetch f;LLTextureFetchWorker w;w.mState=w.DECODE_IMAGE_UPDATE;w.have_work=true;CHECK(f.admit(&w,true)>=0);CHECK(w.requiresAuxDecode());CHECK(w.posts==0);}},
 {"completed race restarts before old output",[]{LLTextureFetch f;LLTextureFetchWorker w;w.have_work=true;w.mNeedsAux=true;CHECK(!f.complete(&w));CHECK(w.posts==1&&w.mState==w.DECODE_IMAGE);}},
 {"terminal maskless does not restart",[]{LLTextureFetch f;LLTextureFetchWorker w;w.mAuxComponents=0;w.mNeedsAux=true;w.mFormattedImage->components=4;CHECK(f.complete(&w));CHECK(w.posts==0);}},
 {"aborted request not revived",[]{LLTextureFetch f;LLTextureFetchWorker w;w.aborted=true;w.have_work=true;CHECK(f.admit(&w,true)==f.CREATE_REQUEST_ERROR_ABORTED);CHECK(w.posts==0);w.endWork(0,true);CHECK(w.mFormattedImage.isNull());}},
 {"obsolete decode cannot publish",[]{LLTextureFetchWorker w;auto old=w.mRawImage.get();const auto cancellation=w.mDecodeCancellation;LLPointer<LLImageRaw>raw=new LLImageRaw,aux=new LLImageRaw(16,16,1);w.mState=w.DECODE_IMAGE_UPDATE;w.mDecoded=false;w.callbackDecoded(true,"",raw,aux,99);CHECK(w.mRawImage.get()==old);CHECK(w.mAuxComponents==-1);CHECK(!w.mDecoded);CHECK(w.mDecodeHandle==11);CHECK(w.mDecodeCancellation==cancellation);CHECK(!cancellation->load(std::memory_order_acquire));}},
 {"aborted decode cannot publish",[]{LLTextureFetchWorker w;w.have_work=true;w.endWork(0,true);const auto cancellation=w.mDecodeCancellation;auto old=w.mRawImage.get();LLPointer<LLImageRaw>raw=new LLImageRaw;w.callbackDecoded(true,"",raw,nullptr,11);CHECK(w.mRawImage.get()==old);CHECK(w.mDecodeCancellation==cancellation);CHECK(cancellation->load(std::memory_order_acquire));}},
 {"endWork cancellation outlives worker ownership",[]{for(bool aborted:{false,true}){std::shared_ptr<std::atomic<bool>>decoder;std::weak_ptr<std::atomic<bool>>weak;{LLTextureFetchWorker w;w.have_work=true;decoder=w.mDecodeCancellation;weak=decoder;CHECK(!decoder->load(std::memory_order_acquire));w.endWork(0,aborted);CHECK(w.mDecodeHandle==0);CHECK(w.mDecodeCancellation==decoder);CHECK(decoder->load(std::memory_order_acquire));}CHECK(!weak.expired());CHECK(decoder->load(std::memory_order_acquire));decoder.reset();CHECK(weak.expired());}}},
 {"busy aux admission retains codestream for retry",[]{LLTextureFetchWorker w;w.mState=w.DECODE_IMAGE;w.mDecodeHandle=0;w.have_work=false;const auto* formatted=w.mFormattedImage.get();w.startWork(0);w.endWork(0,true);CHECK(w.mFormattedImage.get()==formatted);CHECK(w.mDecodeHandle==0);w.addWork(0);CHECK(w.haveWork());CHECK(w.mFormattedImage.get()==formatted);}},
 {"stale decode retains replacement cancellation",[]{LLTextureFetchWorker w;const auto old=w.mDecodeCancellation;w.endWork(0,false);CHECK(old->load(std::memory_order_acquire));w.mDecodeCancellation=std::make_shared<std::atomic<bool>>(false);const auto current=w.mDecodeCancellation;w.mDecodeHandle=12;w.mState=w.DECODE_IMAGE_UPDATE;w.mDecoded=false;LLPointer<LLImageRaw>raw=new LLImageRaw;w.callbackDecoded(true,"",raw,nullptr,11);CHECK(!w.mDecoded);CHECK(w.mDecodeHandle==12);CHECK(w.mDecodeCancellation==current);CHECK(!current->load(std::memory_order_acquire));w.callbackDecoded(true,"",raw,nullptr,12);CHECK(w.mDecoded);CHECK(w.mDecodeHandle==0);CHECK(!w.mDecodeCancellation);CHECK(!current->load(std::memory_order_acquire));CHECK(old->load(std::memory_order_acquire));}},
 {"valid complete maskless metadata",[]{LLTextureFetchWorker w;w.mState=w.DECODE_IMAGE_UPDATE;w.mFormattedImage->components=4;const auto cancellation=w.mDecodeCancellation;LLPointer<LLImageRaw>raw=new LLImageRaw;w.callbackDecoded(true,"",raw,nullptr,11);LLTextureFetch::FetchStatus s;w.getStatus(s);CHECK(s.mAuxComponents==0);CHECK(w.mRawImage.notNull());CHECK(!w.mDecodeCancellation);CHECK(!cancellation->load(std::memory_order_acquire));}},
 {"successful partial maskless has finite terminal progress",[]{LLTextureFetch f;LLTextureFetchWorker w;w.mState=w.DECODE_IMAGE_UPDATE;w.mNeedsAux=true;w.mHaveAllData=false;w.mFormattedImage->components=4;LLPointer<LLImageRaw>raw=new LLImageRaw;w.callbackDecoded(true,"",raw,nullptr,11);w.mState=w.DONE;LLTextureFetch::FetchStatus s;w.getStatus(s);CHECK(s.mAuxComponents==0);CHECK(f.admit(&w,true)==f.CREATE_REQUEST_ERROR_TRANSITION);LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.mNeedsCreateTexture=false;t.mAuxComponents=s.mAuxComponents;t.doLoadedCallbacks();CHECK(a.calls==1&&!a.success&&a.final);CHECK(!t.mNeedsAux);}},
 {"maskless metadata skips optional decode",[]{LLTextureFetchWorker w;w.mNeedsAux=true;w.mFormattedImage->components=4;w.prepareDecode();CHECK(!w.mDecodeNeedsAux);CHECK(w.mState==w.DECODE_IMAGE_UPDATE);}},
 {"five channel metadata requests auxiliary decode",[]{LLTextureFetchWorker w;w.mNeedsAux=true;w.prepareDecode();CHECK(w.mDecodeNeedsAux);}},
 {"invalid metadata cannot declare maskless",[]{LLTextureFetchWorker w;w.mNeedsAux=true;w.mFormattedImage->components=0;w.prepareDecode();CHECK(w.mDecodeNeedsAux);}},
 {"active primary completion re-enters actual decode branch",[]{LLTextureFetchWorker w;w.mNeedsAux=true;w.mDecodeNeedsAux=false;w.mDecodedDiscard=0;CHECK(!w.advanceDecode());CHECK(w.mState==w.DECODE_IMAGE);CHECK(w.decode_reentries==1);}},
 {"nonempty clear retires demand",[]{LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.clearCallbackEntryList();CHECK(!t.mNeedsAux);CHECK(a.calls==1&&!a.success&&a.final);}},
 {"missing asset retires demand",[]{LLViewerFetchedTexture t;Seen a;t.setLoadedCallback(observed,0,true,true,&a,nullptr,false);t.mNeedsCreateTexture=false;t.missing_asset=true;t.doLoadedCallbacks();CHECK(!t.mNeedsAux);CHECK(a.calls==1&&!a.success&&a.final);}},
 {"empty clear retires aux demand",[]{LLViewerFetchedTexture t;t.mNeedsAux=true;t.clearCallbackEntryList();CHECK(!t.mNeedsAux);}},
 {"explicit refetch invalidates terminal knowledge",[]{LLViewerFetchedTexture t;t.mAuxComponents=0;t.forceToRefetchTexture(0);CHECK(t.mAuxComponents==-1);}},
 {"worker content reset invalidates knowledge",[]{LLTextureFetchWorker w;w.mAuxComponents=0;w.resetFormattedData();CHECK(w.mAuxComponents==-1);CHECK(w.mFormattedImage->getDataSize()==0);}},
 {"changed endpoint invalidates knowledge",[]{LLTextureFetchWorker w;w.mAuxComponents=0;w.setUrl("new-endpoint");CHECK(w.mAuxComponents==-1);}},
 {"failed aux result is terminal only DONE",[]{LLTextureFetchWorker w;w.mNeedsAux=true;w.mState=w.DECODE_IMAGE_UPDATE;const auto cancellation=w.mDecodeCancellation;w.callbackDecoded(false,"controlled failure",nullptr,nullptr,11);LLTextureFetch::FetchStatus s;w.getStatus(s);CHECK(s.mAuxComponents==-1);w.mState=w.DONE;w.getStatus(s);CHECK(s.mAuxComponents==-2);CHECK(w.cache_removes==1);CHECK(!w.mDecodeCancellation);CHECK(!cancellation->load(std::memory_order_acquire));}}
 };
 int failures=0;for(auto&[name,run]:cases){try{run();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
 std::cout<<cases.size()<<" cases, "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
