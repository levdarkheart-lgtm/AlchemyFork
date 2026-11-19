// PPD-340/477: production bodies are inserted by the driver. Files are synthetic.
#include "linden_common.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llmemory.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
#undef LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#undef LL_PROFILE_ZONE_SCOPED
#undef LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE
#undef LL_WARNS
#undef LL_ENDL
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE
#define LL_PROFILE_ZONE_SCOPED
#define LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE(...)
#define LL_WARNS(...) std::ostringstream()
#define LL_ENDL std::endl
constexpr S32 TEXTURE_CACHE_ENTRY_SIZE=600;

struct IOControl
{
    std::filesystem::path body,header;
    bool fail_open=false,fail_write=false;
    S64 body_cap=-1,header_cap=-1;
    std::vector<std::string>events;
} io;

// The boundary delegates successful operations to the ACTUAL production LLFile.
// Limits/errors are injected only for this fixture's synthetic paths.
struct ControlledFile
{
    static inline const auto out=LLFile::out,binary=LLFile::binary,trunc=LLFile::trunc;
    LLFile file;
    bool body=false,failed=false;
    ControlledFile(const std::filesystem::path&p,std::ios_base::openmode mode,std::error_code&ec)
        :body(p==io.body)
    {
        if(body)io.events.push_back((mode&trunc)?"body-open-truncate":"body-open");
        if(body&&io.fail_open){failed=true;ec=std::make_error_code(std::errc::permission_denied);return;}
        file.open(p,mode,ec);
    }
    explicit operator bool()const{return !failed&&bool(file);}
    S64 write(const void*data,S64 size,std::error_code&ec)
    {
        if(body)io.events.push_back("body-write");
        if(body&&io.fail_write){ec=std::make_error_code(std::errc::io_error);return -1;}
        return file.write(data,body&&io.body_cap>=0?std::min(size,io.body_cap):size,ec);
    }
    static S64 write(const std::filesystem::path&p,const void*data,S64 offset,S64 size)
    {
        bool body=p==io.body;io.events.push_back(body?"body-write-static":"header-write");
        if(body&&(io.fail_open||io.fail_write))return -1;
        S64 cap=body?io.body_cap:io.header_cap;
        return LLFile::write(p,data,offset,cap>=0?std::min(size,cap):size);
    }
};
struct Raw{bool invalid=false;bool isBufferInvalid()const{return invalid;}};
struct LLTextureCache
{
    struct Entry{S32 mImageSize=-1,mBodySize=0;};
    struct Responder:LLRefCount
    {
        static inline int destroyed=0,total_callbacks=0;
        U8*owned=nullptr;int calls=0;bool success=false;S32 data_size=0;
        ~Responder(){if(owned)ll_aligned_free_16(owned);++destroyed;}
        void setData(U8*data,S32 size,S32,int,bool){owned=data;data_size=size;}
        void completed(bool ok){++calls;++total_callbacks;success=ok;}
    };
    std::filesystem::path mHeaderDataFilePath;
    Entry indexed;bool exists=true;int removals=0;
    std::vector<std::pair<LLPointer<Responder>,bool>>completed;
    S32 getHeaderCacheEntry(const std::string&,Entry&e){e=indexed;return exists?0:-1;}
    S32 setHeaderCacheEntry(const std::string&,Entry&e,S32 image,S32 size){exists=true;updateEntry(0,e,image,size);return 0;}
    bool updateEntry(S32,Entry&e,S32 image,S32 size){io.events.push_back("index-commit");bool same=indexed.mImageSize==image&&indexed.mBodySize==std::max(0,size-600);indexed={image,std::max(0,size-600)};e=indexed;return same;}
    bool writeToFastCache(const std::string&,S32,Raw*,S32){return true;}
    std::filesystem::path getTextureFileName(const std::string&){return io.body;}
    void addCompleted(Responder*r,bool ok){completed.emplace_back(r,ok);}
    // Controlled invalidation owner models removal; the full owner/lock path is source-reviewed.
    void removeFromCache(const std::string&){++removals;io.events.push_back("invalidate");exists=false;indexed={-1,0};LLFile::remove(io.body);}
    void settle(){auto ready=std::move(completed);completed.clear();for(auto&[r,ok]:ready)r->completed(ok);}
};
struct LLTextureCacheWorker
{
    LLTextureCache*mCache=nullptr;std::string mID="synthetic";
    const U8*mWriteData=nullptr;U8*mReadData=nullptr;
    S32 mDataSize=0,mOffset=0,mImageSize=0,mImageFormat=0;bool mImageLocal=false;
    LLPointer<LLTextureCache::Responder>mResponder;
    void finishWork(S32,bool);void endWork(S32,bool);
};
struct LLTextureCacheRemoteWorker:LLTextureCacheWorker
{
    enum{INIT=0,LOCAL=1,CACHE=2,HEADER=3,BODY=4};int mState=INIT;
    Raw*mRawImage=nullptr;S32 mRawDiscardLevel=0;
    bool doWrite();
};
#define LLFile ControlledFile
// @PRODUCTION@
#undef LLFile

int checks=0;
void check(bool pass,const char*expression,int line){++checks;if(!pass)throw std::runtime_error(std::string(expression)+"@"+std::to_string(line));}
#define CHECK(x) check(bool(x),#x,__LINE__)
struct Synthetic
{
    std::filesystem::path dir;LLTextureCache cache;Raw raw;
    Synthetic()
    {
        char pattern[]="/tmp/alchemy-texture-write-XXXXXX";char*p=mkdtemp(pattern);if(!p)throw std::runtime_error("mkdtemp");dir=p;io={};io.body=dir/"body.texture";io.header=dir/"header.cache";cache.mHeaderDataFilePath=io.header;
    }
    ~Synthetic(){std::error_code ec;std::filesystem::remove_all(dir,ec);}
    void seed(S32 body_size)
    {
        std::vector<U8>old(body_size,17);CHECK(LLFile::write(io.body,old.data(),0,old.size())==body_size);cache.indexed={body_size+600,body_size};
    }
    void prepare(LLTextureCacheRemoteWorker&w,const std::vector<U8>&data)
    {
        w.mCache=&cache;w.mWriteData=data.data();w.mDataSize=data.size();w.mImageSize=data.size();w.mRawImage=&raw;
    }
};
void successful_rewrite(S32 old_body,S32 new_body)
{
    Synthetic t;t.seed(old_body);std::vector<U8>data(new_body+600,73);LLTextureCacheRemoteWorker w;t.prepare(w,data);CHECK(w.doWrite());CHECK(w.mDataSize==S32(data.size()));CHECK(t.cache.indexed.mBodySize==new_body);CHECK(LLFile::size(io.body)==new_body);auto body=LLFile::getContents(io.body);CHECK(body==std::string(new_body,char(73)));CHECK(!io.events.empty()&&io.events.front()=="index-commit");CHECK(w.mRawImage==nullptr);
}
std::vector<std::pair<const char*,std::function<void()>>> completion_cases()
{
    return {
        {"old failed completion cannot purge later successful body",[]{Synthetic t;t.seed(100);std::vector<U8>a(650,21),b(680,42);LLTextureCacheRemoteWorker old,newer;t.prepare(old,a);old.mResponder=new LLTextureCache::Responder;io.fail_write=true;CHECK(old.doWrite());CHECK(old.mDataSize==-1);old.finishWork(1,true);bool invalidated_before_newer=t.cache.removals==1&&!t.cache.exists;io.fail_write=false;t.prepare(newer,b);CHECK(newer.doWrite());CHECK(t.cache.exists);CHECK(t.cache.indexed.mBodySize==80);CHECK(LLFile::size(io.body)==80);int removals_after_newer=t.cache.removals;old.endWork(1,false);CHECK(t.cache.removals==removals_after_newer);CHECK(invalidated_before_newer);CHECK(t.cache.exists);CHECK(LLFile::isfile(io.body));CHECK(LLFile::getContents(io.body)==std::string(80,char(42)));t.cache.settle();CHECK(old.mResponder->calls==1&&!old.mResponder->success);}},
        {"failure without responder cannot purge later successful body",[]{Synthetic t;t.seed(100);std::vector<U8>a(650,21),b(680,42);LLTextureCacheRemoteWorker old,newer;t.prepare(old,a);io.fail_write=true;CHECK(old.doWrite());old.finishWork(1,true);bool invalidated_before_newer=t.cache.removals==1&&!t.cache.exists;io.fail_write=false;t.prepare(newer,b);CHECK(newer.doWrite());int removals_after_newer=t.cache.removals;old.endWork(1,false);CHECK(t.cache.removals==removals_after_newer);CHECK(invalidated_before_newer);CHECK(t.cache.indexed.mBodySize==80);CHECK(LLFile::getContents(io.body)==std::string(80,char(42)));}},
        {"late successful completion preserves newer successful body",[]{Synthetic t;t.seed(100);std::vector<U8>a(650,21),b(680,42);LLTextureCacheRemoteWorker old,newer;t.prepare(old,a);old.mResponder=new LLTextureCache::Responder;CHECK(old.doWrite());old.finishWork(1,true);t.prepare(newer,b);newer.mResponder=new LLTextureCache::Responder;CHECK(newer.doWrite());newer.finishWork(1,true);old.endWork(1,false);newer.endWork(1,false);CHECK(t.cache.removals==0);CHECK(t.cache.indexed.mBodySize==80);CHECK(LLFile::getContents(io.body)==std::string(80,char(42)));t.cache.settle();CHECK(old.mResponder->success&&newer.mResponder->success);}},
        {"genuinely aborted failure does not purge",[]{Synthetic t;t.seed(100);LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=-1;w.mResponder=new LLTextureCache::Responder;w.finishWork(1,false);w.endWork(1,true);CHECK(t.cache.removals==0);CHECK(t.cache.exists);CHECK(LLFile::size(io.body)==100);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"failed body is invalidated before finish returns",[]{Synthetic t;t.seed(100);io.fail_write=true;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);w.mResponder=new LLTextureCache::Responder;CHECK(w.doWrite());CHECK(w.mDataSize==-1);w.finishWork(1,true);CHECK(w.mWriteData==nullptr);CHECK(w.mDataSize==0);CHECK(t.cache.removals==1);CHECK(t.cache.completed.size()==1&&!t.cache.completed.front().second);CHECK(w.mResponder->calls==0);w.endWork(1,false);CHECK(t.cache.removals==1);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"failed header is invalidated before finish returns",[]{Synthetic t;t.seed(100);io.header_cap=20;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);w.mResponder=new LLTextureCache::Responder;CHECK(w.doWrite());CHECK(w.mDataSize==-1);w.finishWork(1,true);CHECK(w.mDataSize==0);CHECK(t.cache.removals==1);w.endWork(1,false);CHECK(t.cache.removals==1);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"successful write completion does not purge",[]{Synthetic t;t.seed(100);std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);w.mResponder=new LLTextureCache::Responder;CHECK(w.doWrite());w.finishWork(1,true);CHECK(w.mWriteData==nullptr);CHECK(w.mDataSize==0);CHECK(t.cache.completed.size()==1&&t.cache.completed.front().second);w.endWork(1,false);CHECK(t.cache.removals==0);t.cache.settle();CHECK(w.mResponder->calls==1&&w.mResponder->success);}},
        {"aborted write keeps existing no-purge policy",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=10;w.mResponder=new LLTextureCache::Responder;w.finishWork(1,false);CHECK(w.mDataSize==0);w.endWork(1,true);CHECK(t.cache.removals==0);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"successful read transfers buffer ownership",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=8;w.mReadData=(U8*)ll_aligned_malloc_16(8);auto*read=w.mReadData;w.mResponder=new LLTextureCache::Responder;w.finishWork(0,true);CHECK(w.mReadData==nullptr);CHECK(w.mDataSize==0);CHECK(w.mResponder->owned==read&&w.mResponder->data_size==8);w.endWork(0,false);CHECK(t.cache.removals==0);t.cache.settle();CHECK(w.mResponder->calls==1&&w.mResponder->success);}},
        {"failed read frees buffer and still purges",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=-1;w.mReadData=(U8*)ll_aligned_malloc_16(8);w.mResponder=new LLTextureCache::Responder;w.finishWork(0,true);CHECK(w.mReadData==nullptr);CHECK(w.mDataSize==-1);CHECK(w.mResponder->owned==nullptr);w.endWork(0,false);CHECK(t.cache.removals==1);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"aborted read retains existing cleanup semantics",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=8;w.mReadData=(U8*)ll_aligned_malloc_16(8);w.mResponder=new LLTextureCache::Responder;w.finishWork(0,false);CHECK(w.mReadData==nullptr);CHECK(w.mDataSize==8);w.endWork(0,true);CHECK(t.cache.removals==0);t.cache.settle();CHECK(w.mResponder->calls==1&&!w.mResponder->success);}},
        {"failure without responder still purges",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=-1;w.finishWork(1,true);CHECK(w.mDataSize==0);CHECK(t.cache.removals==1);CHECK(t.cache.completed.empty());w.endWork(1,false);CHECK(t.cache.removals==1);}},
        {"queued responder owns lifetime through failure settlement",[]{Synthetic t;LLTextureCacheWorker w;w.mCache=&t.cache;w.mDataSize=-1;w.mResponder=new LLTextureCache::Responder;int destroyed=LLTextureCache::Responder::destroyed,calls=LLTextureCache::Responder::total_callbacks;CHECK(w.mResponder->getNumRefs()==1);w.finishWork(1,true);CHECK(w.mResponder->getNumRefs()==2);w.endWork(1,false);w.mResponder=nullptr;CHECK(LLTextureCache::Responder::destroyed==destroyed);CHECK(LLTextureCache::Responder::total_callbacks==calls);t.cache.settle();CHECK(LLTextureCache::Responder::destroyed==destroyed+1);CHECK(LLTextureCache::Responder::total_callbacks==calls+1);CHECK(t.cache.removals==1);}}
    };
}

int main(int argc,char**argv)
{
    using Case=std::pair<const char*,std::function<void()>>;
    std::vector<Case>cases={
        {"shorter body replaces old tail",[]{successful_rewrite(100,25);}},
        {"same-size body rewrites exact data",[]{successful_rewrite(50,50);}},
        {"growing body preserves full prefix",[]{successful_rewrite(25,100);}},
        {"sequential same-UUID grow and shrink",[]{Synthetic t;t.seed(100);for(int size:{50,150,20}){std::vector<U8>d(600+size,41);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(t.cache.indexed.mBodySize==size);CHECK(LLFile::size(io.body)==size);}}},
        {"positive short body write reports failure",[]{Synthetic t;t.seed(100);io.body_cap=7;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);CHECK(t.cache.indexed.mBodySize==50);}},
        {"zero body write reports failure",[]{Synthetic t;t.seed(100);io.body_cap=0;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);}},
        {"body open failure reports failure",[]{Synthetic t;t.seed(100);io.fail_open=true;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);}},
        {"body write error reports failure",[]{Synthetic t;t.seed(100);io.fail_write=true;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);}},
        {"positive short header never proceeds to body",[]{Synthetic t;t.seed(100);io.header_cap=20;std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);CHECK(LLFile::size(io.body)==100);}},
        {"positive short padded header reports failure",[]{Synthetic t;t.seed(100);io.header_cap=20;std::vector<U8>d(10,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==-1);CHECK(LLFile::size(io.body)==100);}},
        {"header-only entry still skips body IO",[]{Synthetic t;t.seed(100);std::vector<U8>d(10,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(w.mDataSize==10);CHECK(t.cache.indexed.mBodySize==0);CHECK(LLFile::size(io.body)==100);CHECK(io.events.size()==2);}},
        {"offset writes remain rejected",[]{Synthetic t;t.seed(100);std::vector<U8>d(650,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);w.mOffset=1;CHECK(w.doWrite());CHECK(w.mDataSize==-1);CHECK(io.events.empty());CHECK(LLFile::size(io.body)==100);}},
        {"unchanged cached header-only data skips IO",[]{Synthetic t;t.cache.indexed={10,0};std::vector<U8>d(10,21);LLTextureCacheRemoteWorker w;t.prepare(w,d);CHECK(w.doWrite());CHECK(io.events.size()==1);CHECK(!LLFile::isfile(io.body));}}
    };
    const std::string suite=argc>1?argv[1]:"all";
    auto completion=completion_cases();
    if(suite=="completion")cases=std::move(completion);
    else if(suite=="all")cases.insert(cases.end(),completion.begin(),completion.end());
    else if(suite!="body")return 2;
    int failures=0;for(auto&[name,run]:cases){try{run();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
    std::cout<<cases.size()<<" cases, "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
