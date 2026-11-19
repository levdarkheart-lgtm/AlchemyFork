// PPD-476: the driver inserts complete production fetch-result and callback bodies.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using S8=signed char; using S32=int; using U32=unsigned; using F32=float; using F64=double;
constexpr S8 S8_MAX=127;
constexpr S32 MAX_IMAGE_SIZE=4096,DEFAULT_ICON_DIMENSIONS=128,DEFAULT_THUMBNAIL_DIMENSIONS=256,FTT_MAP_TILE=2;
struct LLGLTexture{enum{BOOST_ICON=1,BOOST_THUMBNAIL=2};};
struct LLRender{enum{NUM_TEXTURE_CHANNELS=1};};
struct Face{void dirtyTexture(){}};
struct GPU{int component_changes=0,generation=9001;bool published=true;void setComponents(int){++component_changes;}};
struct PacketTimer{int resets=0;void reset(){++resets;}};
struct Status{std::string toHex()const{return "controlled";}};
constexpr S32 MAX_DISCARD_LEVEL=5, INVALID_DISCARD_LEVEL=-1, FTT_SERVER_BAKE=1;
#define LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE(...)
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
 LLPointer<LLImageRaw> scaled(int x,int y){return new LLImageRaw(x,y,c);}
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
struct TextureList{std::set<LLViewerFetchedTexture*>mCallbackList;std::vector<LLViewerFetchedTexture*>mCreates;void queueCreateTexture(LLViewerFetchedTexture*p){mCreates.push_back(p);}}gTextureList;
struct LLViewerFetchedTexture
{
 using callback_list_t=std::list<LLLoadedCallbackEntry*>;callback_list_t mLoadedCallbackList;
 bool mIsFetched=false;int mFullWidth=0,mFullHeight=0,mBoostLevel=0,mKnownDrawWidth=0,mKnownDrawHeight=0,mRequestedDiscardLevel=0,mMinDiscardLevel=6,mFetchState=13,mComponents=4,precreates=0,metadata_sets=0,texel_updates=0,saved_raw_calls=0;GPU gpu;GPU*mGLTexturep=&gpu;U32 mNumFaces[1]={0};std::vector<Face*>mFaceList[1];PacketTimer mLastPacketTimer;Status mLastHttpGetStatus;
 int getComponents()const{return mComponents;}bool isForSculptOnly()const{return false;}int getFTType()const{return mFTType;}
 void setTexelsPerImage(){++texel_updates;}void setIsMissingAsset(bool=true);void setImageMetadata(int,LLImageRaw*){++metadata_sets;}
 bool preCreateTexture(){++precreates;mNeedsCreateTexture=false;return true;}
 bool processFetchResults(S32&,S32,S32,F32);void addToCreateTexture();void scheduleCreateTexture();

 S8 mLoadedCallbackDesiredDiscardLevel=6;bool mPauseLoadedCallBacks=false,mNeedsAux=false,mSaveRawImage=false,mHasAux=false;
 bool mNeedsCreateTexture=true,mIsFetching=false,mIsRawImageValid=false,mForceCallbackFetch=false,mForceToSaveRawImage=false,mSavedRawFetchFailed=false;
 LLPointer<LLImageRaw>mAuxRawImage,mRawImage,mSavedRawImage;
 S32 mRawDiscardLevel=-1,mSavedRawDiscardLevel=-1,mDesiredSavedRawDiscardLevel=-1,mFTType=FTT_SERVER_BAKE,mAuxComponents=-1,mAuxRawDiscardLevel=-1;
 F32 mKeptSavedRawImageTime=0,mLastCallBackActiveTime=1,mLastReferencedSavedRawImageTime=1,mMaxVirtualSize=1;
 static inline F64 sCurrentTime=1;static inline S32 sRawCount=0,sAuxCount=0;
 std::string mID,mUrl;int gpu_discard=0;bool mIsMissingAsset=false,mHasFetcher=false;int mFetchPriority=0;
 std::string getID()const{return mID;}int getTextureListType()const{return 0;}S32 getDiscardLevel()const{return gpu_discard;}
 S32 getMaxDiscardLevel()const{return 5;}S32 getDesiredDiscardLevel()const{return 0;}bool isMissingAsset()const{return mIsMissingAsset;}
 void resetTextureStats(){}void setActive(){}void rebuildSculptVolumes(){}
 void clearCallbackEntryList();
 void destroySavedRawImage(){mSavedRawImage=nullptr;mSaveRawImage=false;mForceToSaveRawImage=false;}
 void readbackRawImage(){throw std::runtime_error("Unexpected GPU readback");}
 void saveRawImage(){++saved_raw_calls;mSavedRawImage=new LLImageRaw(mRawImage->getData(),mRawImage->w,mRawImage->h,mRawImage->c);mSavedRawDiscardLevel=mRawDiscardLevel;}
 void updateAuxDemand();void postCreateTexture();bool needsToSaveRawImage();void destroyRawImage();bool doLoadedCallbacks();
 void pauseLoadedCallbacks(const LLLoadedCallbackEntry::source_callback_list_t*);
 void unpauseLoadedCallbacks(const LLLoadedCallbackEntry::source_callback_list_t*);
 void forceToRefetchTexture(S32=0,F32=0);S32 getCurrentDiscardLevelForFetching();
 void setLoadedCallback(loaded_callback_func,S32,bool,bool,void*,LLLoadedCallbackEntry::source_callback_list_t*,bool);
 ~LLViewerFetchedTexture(){for(auto*x:mLoadedCallbackList)delete x;gTextureList.mCallbackList.erase(this);}
};

// Controlled external boundaries: request cancellation, GPU publication, and upload queue.
// No viewer, codec, worker thread, or Metal object is created by this fixture.
struct FetchBoundary{int cancellations=0;void deleteRequest(const std::string&,bool cancel){assert(cancel);++cancellations;}};
struct LLAppViewer{static FetchBoundary*getTextureFetch(){static FetchBoundary fetch;return &fetch;}};
// @PRODUCTION@
int checks=0;void check(bool b,const char*e,int l){++checks;if(!b)throw std::runtime_error(std::string(e)+"@"+std::to_string(l));}
#define CHECK(x) check(bool(x),#x,__LINE__)
struct Seen{int calls=0;bool success=false,final=false,aux=false;int discard=-1;};
void observed(bool success,LLViewerFetchedTexture*,LLImageRaw*,LLImageRaw*aux,S32 d,bool final,void*p){auto&x=*static_cast<Seen*>(p);++x.calls;x.success=success;x.final=final;x.aux=aux;x.discard=d;}
void data(LLViewerFetchedTexture&t,bool aux,int level=0){t.mRawImage=new LLImageRaw;t.mIsRawImageValid=true;t.mRawDiscardLevel=level;++t.sRawCount;if(aux){t.mAuxRawImage=new LLImageRaw(16,16,1);t.mAuxRawDiscardLevel=level;t.mHasAux=true;++t.sAuxCount;}t.mNeedsCreateTexture=false;}
// Scenarios call the inserted whole processFetchResults body. Input storage and GPU/queue boundaries are controlled.
void fresh(LLViewerFetchedTexture&t,bool aux,int level=0,int width=16,int height=16){data(t,aux,level);t.mRawImage=new LLImageRaw(width,height,4);if(aux)t.mAuxRawImage=new LLImageRaw(width,height,1);t.mIsRawImageValid=false;t.mNeedsCreateTexture=false;t.mIsFetching=false;gTextureList.mCreates.clear();LLAppViewer::getTextureFetch()->cancellations=0;}
void gpu_still_published(LLViewerFetchedTexture&t){CHECK(t.gpu.generation==9001);CHECK(t.gpu.published);CHECK(t.gpu.component_changes==0);CHECK(t.precreates==0);CHECK(gTextureList.mCreates.empty());}
void rejected_aux(int width,int height,int level){LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true,level,width,height);t.mFullWidth=64;t.mFullHeight=32;t.mHasFetcher=true;t.mIsFetching=true;S32 desired=0;CHECK(!t.processFetchResults(desired,0,level,1));CHECK(t.mRawImage.isNull());CHECK(!t.mIsRawImageValid);CHECK(t.mRawDiscardLevel==INVALID_DISCARD_LEVEL);CHECK(t.isMissingAsset());CHECK(!t.mHasFetcher&&!t.mIsFetching);CHECK(LLAppViewer::getTextureFetch()->cancellations==1);CHECK(t.mFetchState==0&&t.mFetchPriority==0);CHECK(t.mLastPacketTimer.resets==2);CHECK(t.mFullWidth==64&&t.mFullHeight==32);CHECK(t.texel_updates==0);gpu_still_published(t);t.doLoadedCallbacks();CHECK(x.calls==1&&!x.success&&x.final);CHECK(!t.mNeedsAux);gpu_still_published(t);}
int main(){using Case=std::pair<const char*,std::function<void()>>;std::vector<Case>cases={
 {"rejected incoming tuple cannot replace saved valid raw",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true,0,MAX_IMAGE_SIZE+1,1);t.mIsRawImageValid=true;t.mSaveRawImage=true;t.mForceToSaveRawImage=true;t.mSavedRawImage=new LLImageRaw;t.mSavedRawDiscardLevel=1;auto*saved=t.mSavedRawImage.get();t.mFullWidth=64;t.mFullHeight=32;t.mHasFetcher=true;t.mIsFetching=true;S32 d=0;CHECK(!t.processFetchResults(d,0,0,1));CHECK(t.mRawImage.isNull());CHECK(!t.mIsRawImageValid);CHECK(t.mSavedRawImage.get()==saved);CHECK(t.mSavedRawDiscardLevel==1);CHECK(t.saved_raw_calls==0);CHECK(t.mFullWidth==64&&t.mFullHeight==32);CHECK(LLAppViewer::getTextureFetch()->cancellations==1);gpu_still_published(t);}},
 {"same-quality auxiliary width above limit is rejected",[]{rejected_aux(MAX_IMAGE_SIZE+1,1,0);}},
 {"same-quality auxiliary height above limit is rejected",[]{rejected_aux(1,MAX_IMAGE_SIZE+1,0);}},
 {"coarser auxiliary reconstructed width above limit is rejected",[]{rejected_aux(MAX_IMAGE_SIZE/2+1,1,1);}},
 {"coarser auxiliary reconstructed height above limit is rejected",[]{rejected_aux(1,MAX_IMAGE_SIZE/2+1,1);}},
 {"exact full-size limit stays callback only",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,1,true,true,&x,nullptr,false);fresh(t,true,1,MAX_IMAGE_SIZE/2,1);t.mFullWidth=64;t.mFullHeight=32;S32 d=1;CHECK(t.processFetchResults(d,0,1,1));CHECK(t.mFullWidth==64&&t.mFullHeight==32);gpu_still_published(t);CHECK(t.doLoadedCallbacks());CHECK(x.calls==1&&x.success&&x.final&&x.discard==1);}},
 {"generic multi-channel subsampled aux remains admitted",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true);t.mAuxRawImage=new LLImageRaw(8,8,4);S32 d=0;CHECK(t.processFetchResults(d,0,0,1));gpu_still_published(t);CHECK(t.doLoadedCallbacks());CHECK(x.calls==1&&x.success&&x.final&&x.aux);}},
 {"empty aux data cannot fulfill callback",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true);t.mAuxRawImage->data.clear();S32 d=0;CHECK(t.processFetchResults(d,0,0,1));gpu_still_published(t);CHECK(!t.doLoadedCallbacks());CHECK(x.calls==0);CHECK(!t.mLoadedCallbackList.empty());}},

 {"same quality aux reaches callback without GPU work",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true);auto*raw=t.mRawImage.get();auto*aux=t.mAuxRawImage.get();S32 d=0;CHECK(t.processFetchResults(d,0,0,1));CHECK(t.mRawImage.get()==raw&&t.mAuxRawImage.get()==aux);CHECK(t.mIsRawImageValid&&t.mRawDiscardLevel==0);CHECK(t.mIsFetched);gpu_still_published(t);CHECK(t.doLoadedCallbacks());CHECK(x.calls==1&&x.success&&x.final&&x.aux&&x.discard==0);gpu_still_published(t);}},
 {"coarser aux is callback only and cannot claim final quality",[]{LLViewerFetchedTexture t;Seen x;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true,3);S32 d=0;CHECK(t.processFetchResults(d,0,3,1));CHECK(t.mIsRawImageValid&&t.mRawDiscardLevel==3);gpu_still_published(t);CHECK(t.doLoadedCallbacks());CHECK(x.calls==1&&x.success&&!x.final&&x.aux&&x.discard==3);CHECK(!t.mLoadedCallbackList.empty());CHECK(t.getDiscardLevel()==0);}},
 {"better quality color and aux follow normal upload queue",[]{LLViewerFetchedTexture t;Seen x;t.gpu_discard=3;t.setLoadedCallback(observed,0,true,true,&x,nullptr,false);fresh(t,true,0);S32 d=0;CHECK(t.processFetchResults(d,3,0,1));CHECK(t.mIsRawImageValid);CHECK(t.precreates==1);CHECK(gTextureList.mCreates.size()==1&&gTextureList.mCreates.front()==&t);CHECK(t.mNeedsCreateTexture);CHECK(t.mFullWidth==16&&t.mFullHeight==16);CHECK(!t.doLoadedCallbacks());CHECK(x.calls==0);}},
 {"no GPU level follows normal upload even with aux",[]{LLViewerFetchedTexture t;t.gpu_discard=-1;t.mNeedsAux=true;fresh(t,true);S32 d=0;CHECK(t.processFetchResults(d,-1,0,1));CHECK(t.precreates==1);CHECK(gTextureList.mCreates.size()==1);CHECK(t.mNeedsCreateTexture);}},
 {"raw only duplicate still discarded",[]{LLViewerFetchedTexture t;fresh(t,false);S32 d=0;CHECK(!t.processFetchResults(d,0,0,1));CHECK(t.mRawImage.isNull());CHECK(!t.mIsRawImageValid);gpu_still_published(t);}},
 {"aux pointer without demand still duplicate",[]{LLViewerFetchedTexture t;fresh(t,true);t.mNeedsAux=false;S32 d=0;CHECK(!t.processFetchResults(d,0,0,1));CHECK(t.mRawImage.isNull());CHECK(t.mAuxRawImage.isNull());gpu_still_published(t);}},
 {"zero byte primary retains original false path",[]{LLViewerFetchedTexture t;t.mNeedsAux=true;fresh(t,true);t.mRawImage->data.clear();S32 d=0;CHECK(!t.processFetchResults(d,0,0,1));CHECK(t.mRawImage.isNull());CHECK(!t.mIsRawImageValid);gpu_still_published(t);}},
 {"negative fetched discard retains original false path",[]{LLViewerFetchedTexture t;t.mNeedsAux=true;fresh(t,true);S32 d=0;CHECK(!t.processFetchResults(d,0,-1,1));CHECK(t.mRawImage.isNull());gpu_still_published(t);}},
 {"oversized better-quality raw still missing false path",[]{LLViewerFetchedTexture t;t.mNeedsAux=true;t.gpu_discard=3;fresh(t,true,0,MAX_IMAGE_SIZE+1,1);t.mIsFetching=true;S32 d=0;CHECK(!t.processFetchResults(d,3,0,1));CHECK(t.mRawImage.isNull());CHECK(t.isMissingAsset());CHECK(t.mRawDiscardLevel==-1);CHECK(!t.mIsFetching);CHECK(t.mLastPacketTimer.resets==1);gpu_still_published(t);}},
 {"oversized reconstructed full width still missing false path",[]{LLViewerFetchedTexture t;t.gpu_discard=3;fresh(t,false,1,MAX_IMAGE_SIZE,1);S32 d=0;CHECK(!t.processFetchResults(d,3,1,1));CHECK(t.mFullWidth==MAX_IMAGE_SIZE*2);CHECK(t.isMissingAsset());gpu_still_published(t);}},
 {"raw-only finished-short sets terminal minimum",[]{LLViewerFetchedTexture t;fresh(t,false,3);t.mRequestedDiscardLevel=0;S32 d=0;CHECK(!t.processFetchResults(d,0,3,1));CHECK(t.mMinDiscardLevel==0);gpu_still_published(t);}},
 {"no raw completed failure retains original missing path",[]{LLViewerFetchedTexture t;t.gpu_discard=-1;t.mNeedsCreateTexture=false;t.mIsFetching=false;t.mFetchState=13;t.mRawDiscardLevel=-1;gTextureList.mCreates.clear();S32 d=0;CHECK(t.processFetchResults(d,-1,0,1));CHECK(t.isMissingAsset());CHECK(d==-1);gpu_still_published(t);}},
 {"ordinary better-quality RGB still queues",[]{LLViewerFetchedTexture t;t.gpu_discard=3;fresh(t,false);t.mRawImage=new LLImageRaw(16,16,3);S32 d=0;CHECK(t.processFetchResults(d,3,0,1));CHECK(t.mComponents==3);CHECK(t.gpu.component_changes==1);CHECK(t.precreates==1);CHECK(gTextureList.mCreates.size()==1);}},
 {"icon better quality retains original scaling",[]{LLViewerFetchedTexture t;t.gpu_discard=3;fresh(t,false,0,32,32);t.mBoostLevel=LLGLTexture::BOOST_ICON;t.mKnownDrawWidth=8;t.mKnownDrawHeight=8;S32 d=0;CHECK(t.processFetchResults(d,3,0,1));CHECK(t.mRawImage->getWidth()==8&&t.mRawImage->getHeight()==8);CHECK(t.precreates==1);CHECK(gTextureList.mCreates.size()==1);}}
};int failures=0;for(auto&[name,run]:cases){try{run();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}std::cout<<cases.size()<<" cases, "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}
