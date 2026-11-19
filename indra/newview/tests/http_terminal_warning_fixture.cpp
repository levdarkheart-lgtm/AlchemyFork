// Controlled HTTP inputs; the production pre-URI admission body is inserted verbatim.
#include "alwarningdiagnostics.h"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace LLCore
{
struct Status { std::string toTerseString()const{return "Http_403";} };
struct HttpOpRequest
{
    std::uintptr_t handle=0x125;
    Status mStatus;
    unsigned mReqPolicy=1;
    std::uint64_t mReqOffset=0,mReqLength=600;
    std::uintptr_t getHandle()const{return handle;}
};
}
struct Observation { unsigned policy;std::uint64_t offset,length; };
std::vector<Observation> admitted;
unsigned exceptions=0;
void diagnose_http_failure(LLCore::HttpOpRequest& operation)
{
    // @HTTP_ADMISSION_PREFIX@
        admitted.push_back({operation.mReqPolicy,operation.mReqOffset,operation.mReqLength});
    }
    catch (...) { ++exceptions; }
}
unsigned checks=0,failures=0;
void check(bool value,const char* description)
{
    ++checks;
    if(!value){++failures;std::cerr<<"FAIL: "<<description<<'\n';}
}
int main()
{
    const bool enabled=ALWarningDiagnostics::enabled();
    LLCore::HttpOpRequest request;
    diagnose_http_failure(request);
    check(admitted.size()==(enabled?1u:0u),"first terminal boundary");
    // A later operation reuses exactly the same core handle and status, but its
    // policy/range differs. It must not lose its private diagnostic context.
    request.mReqPolicy=7;request.mReqOffset=600;request.mReqLength=4096;
    diagnose_http_failure(request);
    check(admitted.size()==(enabled?2u:0u),"recycled handle and status");
    if(enabled&&admitted.size()==2)
    {
        check(admitted[1].policy==7,"later policy retained");
        check(admitted[1].offset==600&&admitted[1].length==4096,"later range retained");
    }
    // Even identical metadata at another terminal boundary is a new event.
    diagnose_http_failure(request);
    check(admitted.size()==(enabled?3u:0u),"distinct boundary with identical metadata");
    // Exercise the actual shared limiter, not a fixture copy of its key logic.
    // The first 64 boundaries are admitted; later boundaries remain bounded.
    for(unsigned i=0;i<100;++i)
    {
        request.handle=0x1000+i;
        diagnose_http_failure(request);
    }
    check(admitted.size()==(enabled?64u:0u),"64-event category limit");
    check(exceptions==0,"admission did not throw");
    if(!enabled)check(admitted.empty(),"disabled diagnostics have no admissions");
    std::cout<<"HTTP terminal admission enabled="<<enabled<<": "<<checks
             <<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
