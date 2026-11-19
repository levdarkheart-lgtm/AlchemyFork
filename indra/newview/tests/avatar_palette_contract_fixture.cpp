// Actual avatar packing/cache body with controlled pose and palette producers.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using U32=std::uint32_t;using U64=std::uint64_t;using S32=std::int32_t;using F32=float;
template<typename T> T llmin(T a,T b){return std::min(a,b);}
constexpr unsigned VX=0,VY=1,VZ=2;
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR
#define LL_WARNS_ONCE(...) std::clog
#define LL_ENDL '\n'
struct LLVector3
{
    F32 mV[3]{};
    bool isFinite()const{return std::isfinite(mV[0])&&std::isfinite(mV[1])&&std::isfinite(mV[2]);}
    static const LLVector3 zero;
};
const LLVector3 LLVector3::zero{};
struct alignas(16) Row
{
    F32 v[4]{};
    const F32* getF32ptr()const{return v;}
};
struct alignas(16) LLMatrix4a{Row mMatrix[4];};
struct LLMeshSkinInfo
{
    U64 mHash=123;
    std::vector<std::string> mJointNames;
};
struct Root
{
    U64 revision=2;
    LLVector3 position{{10.f,20.f,30.f}};
    void touchIfXformParentMoved(){}
    U64 getPoseRevision()const{return revision;}
    LLVector3 getWorldPosition()const{return position;}
};
U32 gFrameCount=1;
struct LLVOAvatar
{
    struct MatrixPaletteCache
    {
        U32 mFrame=0;
        U64 mPoseRevision=0,mLifecycleRevision=0;
        std::vector<LLMatrix4a> mMatrixPalette;
        std::vector<F32> mGLMp;
        std::shared_ptr<int> mMetalAttachmentPalette;
        bool mMetalAttachmentPaletteAttempted=false;
    };
    Root root;
    Root* mRoot=&root;
    U64 mNativeBodyPoseLifecycleRevision=3;
    LLVector3 agent{{4.f,5.f,6.f}};
    std::unordered_map<U64,MatrixPaletteCache> mMatrixPaletteCache;
    LLVector3 getPositionAgent()const{return agent;}
    const MatrixPaletteCache& updateSkinInfoMatrixPalette(const LLMeshSkinInfo*);
};
namespace LLSkinningUtil
{
unsigned producer_calls=0;
std::vector<LLMatrix4a> supplied;
S32 getMaxJointCount(){return 110;}
U32 getMeshJointCount(const LLMeshSkinInfo*);
void initSkinningMatrixPalette(LLMatrix4a* destination,S32 count,const LLMeshSkinInfo*,LLVOAvatar*)
{
    ++producer_calls;
    for(S32 i=0;i<count;++i)destination[i]=supplied.at(i);
}
}
// @PRODUCTION_COUNT@
// @PRODUCTION_BODY@
unsigned checks=0,failures=0;
void check(bool ok,const char* message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
int main(int argc,char**argv)
{
    const std::string mode=argc>1?argv[1]:"all";
    if(mode=="zero"||mode=="all")
    {
        LLVOAvatar avatar;LLMeshSkinInfo empty;
        const auto& entry=avatar.updateSkinInfoMatrixPalette(&empty);
        check(entry.mMatrixPalette.empty(),"zero palette remains empty");
        check(entry.mGLMp==std::vector<F32>({10.f,20.f,30.f,0.f}),"zero palette keeps four finite origin values");
        check(entry.mFrame==1&&entry.mPoseRevision==2&&entry.mLifecycleRevision==3,"zero palette commits exact cache stamp");
        const auto calls=LLSkinningUtil::producer_calls;
        check(&avatar.updateSkinInfoMatrixPalette(&empty)==&entry,"same pose reuses entry");
        check(LLSkinningUtil::producer_calls==calls,"same pose does not rebuild zero palette");
    }
    if(mode=="valid"||mode=="all")
    {
        LLVOAvatar avatar;LLMeshSkinInfo skin;skin.mJointNames={"one","two"};
        LLSkinningUtil::supplied.resize(2);
        for(unsigned joint=0;joint<2;++joint)
        {
            auto& m=LLSkinningUtil::supplied[joint];
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)m.mMatrix[r].v[c]=F32(100*joint+10*r+c);
        }
        const auto& entry=avatar.updateSkinInfoMatrixPalette(&skin);
        check(entry.mMatrixPalette.size()==2&&entry.mGLMp.size()==28,"valid palette has original dimensions");
        for(unsigned joint=0;joint<2;++joint)
        {
            const auto& m=LLSkinningUtil::supplied[joint];
            for(unsigned axis=0;axis<3;++axis)
            {
                for(unsigned lane=0;lane<3;++lane)check(entry.mGLMp[4+joint*12+axis*4+lane]==m.mMatrix[axis].v[lane],"valid basis packing unchanged");
                check(entry.mGLMp[4+joint*12+axis*4+3]==m.mMatrix[3].v[axis]-avatar.root.position.mV[axis],"valid translation rebase unchanged");
            }
        }
        avatar.root.revision++;avatar.root.position.mV[0]=std::numeric_limits<F32>::infinity();
        const auto& finite_agent=avatar.updateSkinInfoMatrixPalette(&skin);
        check(finite_agent.mGLMp[0]==4&&finite_agent.mGLMp[1]==5&&finite_agent.mGLMp[2]==6,"nonfinite root uses existing finite agent origin");
        avatar.root.revision++;avatar.agent.mV[1]=std::numeric_limits<F32>::quiet_NaN();
        const auto& finite_zero=avatar.updateSkinInfoMatrixPalette(&skin);
        check(finite_zero.mGLMp[0]==0&&finite_zero.mGLMp[1]==0&&finite_zero.mGLMp[2]==0,"nonfinite root+agent uses existing zero origin");
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
