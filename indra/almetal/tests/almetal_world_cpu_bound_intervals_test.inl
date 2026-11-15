/**
 * @file almetal_world_cpu_bound_intervals_test.inl
 * @brief CPU-only support-interval checks against independent corner oracles.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included by the existing world visibility native fixture translation unit.
 */
#include <array>
#include <bit>
#include <cfloat>
#include <cmath>
#include <limits>

namespace CpuBoundIntervalsTest
{
using Result=ALMetalWorldCpuVisibilityResult;
using Matrix=std::array<float,16>;
Matrix unit()
{
    Matrix value{};value[0]=value[5]=value[10]=value[15]=1;return value;
}
ALMetalWorldConservativeBounds box(float lower=-.25f,float upper=.25f)
{
    ALMetalWorldConservativeBounds value;value.qualified=true;
    for(unsigned axis=0;axis<3;++axis){value.minimum[axis]=lower;value.maximum[axis]=upper;}
    return value;
}
struct Corners
{
    unsigned common_outside=63;
    bool inside=true,positive_w=true;
};
// Deliberately use corner transforms, not the production center/radius or
// plane-coefficient formula. The dyadic finite-grid corpus below is exactly
// representable here, even on ARM64 where long double has double precision.
Corners oracle(const ALMetalWorldConservativeBounds& bounds,const Matrix& matrix)
{
    Corners result;
    for(unsigned corner=0;corner<8;++corner)
    {
        const long double point[4]={(corner&1)?bounds.maximum[0]:bounds.minimum[0],
            (corner&2)?bounds.maximum[1]:bounds.minimum[1],
            (corner&4)?bounds.maximum[2]:bounds.minimum[2],1};
        long double clip[4]{};
        for(unsigned component=0;component<4;++component)
            for(unsigned axis=0;axis<4;++axis)
                clip[component]+=static_cast<long double>(matrix[axis*4+component])*point[axis];
        result.positive_w=result.positive_w && clip[3]>0;
        unsigned outside=0;
        for(unsigned axis=0;axis<3;++axis)
        {
            if(clip[axis]<-clip[3])outside|=1u<<(2*axis);
            if(clip[axis]>clip[3])outside|=2u<<(2*axis);
            result.inside=result.inside && clip[axis]>-clip[3] && clip[axis]<clip[3];
        }
        result.common_outside&=outside;
    }
    return result;
}
void checkOracle(const ALMetalWorldConservativeBounds& bounds,const Matrix& matrix,Result result)
{
    if(result!=Result::outside && result!=Result::inside)return;
    const auto expected=oracle(bounds,matrix);
    if(!expected.positive_w || (result==Result::outside?!expected.common_outside:!expected.inside))
    {
        CAPTURE(bounds.minimum[0],bounds.minimum[1],bounds.minimum[2],
            bounds.maximum[0],bounds.maximum[1],bounds.maximum[2],matrix,unsigned(result));
        FAIL("A certified interval classification disagrees with the corner oracle");
    }
}
bool finite(float value)
{return (std::bit_cast<std::uint32_t>(value)&0x7fffffffu)<0x7f800000u;}
float rounded(long double value,bool flush)
{
    volatile float stored=static_cast<float>(value);float result=stored;
    if(flush && (std::bit_cast<std::uint32_t>(result)&0x7fffffffu)<0x00800000u)
        result=std::bit_cast<float>(std::bit_cast<std::uint32_t>(result)&0x80000000u);
    return result;
}
// Force separate rounded products/additions, and also try both FMA orders.
// Explicit intermediate FTZ covers products and cancellation near FLT_MIN.
void checkFloatArithmetic(const ALMetalWorldConservativeBounds& bounds,const Matrix& matrix,Result result)
{
    if(result!=Result::outside && result!=Result::inside)return;
    for(unsigned mode=0;mode<6;++mode)
    {
        unsigned common_outside=63;bool inside=true,positive_w=true;
        for(unsigned sample=0;sample<27;++sample)
        {
            float point[4]{0,0,0,1};unsigned digits=sample;
            for(unsigned axis=0;axis<3;++axis,digits/=3)
                point[axis]=digits%3==0?bounds.minimum[axis]:digits%3==2?bounds.maximum[axis]:
                    rounded((static_cast<long double>(bounds.minimum[axis])+bounds.maximum[axis])*.5L,false);
            float clip[4]{};const bool flush=mode>=4;
            for(unsigned component=0;component<4;++component)
                for(unsigned step=0;step<4;++step)
                {
                    const unsigned axis=(mode&1)?3-step:step;
                    const float coefficient=matrix[axis*4+component];
                    clip[component]=mode==2 || mode==3?
                        std::fma(coefficient,point[axis],clip[component]):
                        rounded(static_cast<long double>(clip[component])+
                            rounded(static_cast<long double>(coefficient)*point[axis],flush),flush);
                }
            unsigned outside=0;positive_w=positive_w && clip[3]>0;
            for(unsigned component=0;component<4;++component)positive_w=positive_w && finite(clip[component]);
            for(unsigned axis=0;axis<3;++axis)
            {
                if(clip[axis]<-clip[3])outside|=1u<<(2*axis);
                if(clip[axis]>clip[3])outside|=2u<<(2*axis);
                inside=inside && clip[axis]>-clip[3] && clip[axis]<clip[3];
            }
            // Actual native clip-Z conversion may contract the add/multiply.
            for(bool fused:{false,true})
            {
                const float z=fused?std::fma(.5f,clip[2],rounded(.5L*clip[3],flush)):
                    rounded(.5L*rounded(static_cast<long double>(clip[2])+clip[3],flush),flush);
                const bool depth_inside=z>0 && z<clip[3];
                if(result==Result::inside && !depth_inside)
                {CAPTURE(mode,sample,fused,matrix);FAIL("Certified inside lost native clip-Z margin");}
            }
            common_outside&=outside;
        }
        if(!positive_w || (result==Result::outside?!common_outside:!inside))
        {CAPTURE(mode,matrix,unsigned(result));FAIL("Rounded vertex arithmetic escaped its certified interval");}
    }
}
}

TEST_CASE("almetal: affine bound intervals conservatively enclose the exhaustive finite dyadic corner corpus",
          "[almetal][world-cpu-bound-intervals][PPD-284]")
{
    using namespace CpuBoundIntervalsTest;
    constexpr std::array<float,7> values{-4,-1,-.25f,0,.25f,1,4};
    std::array<Matrix,8> matrices;for(auto& matrix:matrices)matrix=unit();
    matrices[1][0]=-.5f;matrices[1][5]=2;matrices[1][10]=.25f;matrices[1][12]=1;
    matrices[2][3]=.25f;matrices[2][7]=-.125f;matrices[2][11]=.5f; // Perspective/eye crossings.
    matrices[3][4]=.5f;matrices[3][8]=-.25f;matrices[3][9]=.25f;matrices[3][12]=-.5f;
    matrices[4][0]=.5f;matrices[4][4]=-.5f;matrices[4][1]=.5f;matrices[4][5]=.5f;
    matrices[5][10]=-1;matrices[5][14]=.5f;
    matrices[6][0]=matrices[6][5]=matrices[6][10]=0;matrices[6][12]=2;
    matrices[7][0]=matrices[7][5]=matrices[7][10]=.125f;matrices[7][15]=2;
    std::array<std::uint64_t,4> counts{};std::uint64_t tested=0;
    // 28 intervals per axis, including degenerate points: all 175616 finite
    // box/matrix combinations are checked. Partial/unknown need not match an
    // alternate classifier; only certified outside/inside are constrained.
    for(unsigned lx=0;lx<values.size();++lx)for(unsigned ux=lx;ux<values.size();++ux)
    for(unsigned ly=0;ly<values.size();++ly)for(unsigned uy=ly;uy<values.size();++uy)
    for(unsigned lz=0;lz<values.size();++lz)for(unsigned uz=lz;uz<values.size();++uz)
    {
        auto bounds=box();bounds.minimum[0]=values[lx];bounds.maximum[0]=values[ux];
        bounds.minimum[1]=values[ly];bounds.maximum[1]=values[uy];bounds.minimum[2]=values[lz];bounds.maximum[2]=values[uz];
        for(const auto& matrix:matrices)
        {const auto result=alMetalClassifyWorldBoundsCPU(bounds,matrix.data());checkOracle(bounds,matrix,result);++counts[unsigned(result)];++tested;}
    }
    CHECK(tested==175616);for(auto count:counts)CHECK(count>0);
}

TEST_CASE("almetal: affine bound margins enclose rounded float FMA and FTZ vertex arithmetic",
          "[almetal][world-cpu-bound-intervals][PPD-284]")
{
    using namespace CpuBoundIntervalsTest;
    std::array<std::uint64_t,4> counts{};
    for(int exponent:{-120,-100,-60,-20,0,20,60,100,120})
        for(unsigned variant=0;variant<12;++variant)
    {
        auto bounds=box();auto matrix=unit();
        if(variant<6)
        {
            const unsigned axis=variant/2;const float direction=(variant&1)?-1.f:1.f;
            matrix[12+axis]=direction*((variant&1)?2.f:std::nextafter(.75f,2.f));
        }
        else if(variant==6){matrix[3]=.25f;matrix[7]=.125f;matrix[12]=2;}
        else if(variant==7){matrix[4]=.5f;matrix[8]=-.25f;matrix[12]=-2;}
        else if(variant==8){matrix[0]=0;matrix[12]=1;}
        else if(variant==9){matrix[3]=4;matrix[15]=0;}
        else if(variant==10){matrix[0]=0x1p100f;matrix[12]=-0x1p100f;bounds.minimum[0]=bounds.maximum[0]=1;}
        else {matrix[0]=FLT_MAX/4;bounds.minimum[0]=2;bounds.maximum[0]=4;}
        for(auto& value:matrix)value=std::ldexp(value,exponent);
        const auto result=alMetalClassifyWorldBoundsCPU(bounds,matrix.data());
        ++counts[unsigned(result)];checkOracle(bounds,matrix,result);checkFloatArithmetic(bounds,matrix,result);
    }
    // Normal inputs whose intermediate products underflow, and a nearby
    // ordinary inside case: classification may decline, never falsely cull.
    for(float scale:{FLT_MIN,0x1p-120f,0x1p-100f})
    {
        auto bounds=box();bounds.minimum[0]=-FLT_MIN;bounds.maximum[0]=FLT_MIN;
        auto matrix=unit();matrix[0]=scale;
        const auto result=alMetalClassifyWorldBoundsCPU(bounds,matrix.data());
        ++counts[unsigned(result)];
        checkOracle(bounds,matrix,result);checkFloatArithmetic(bounds,matrix,result);
    }
    CHECK(counts[unsigned(Result::outside)]>0);CHECK(counts[unsigned(Result::inside)]>0);
    CHECK(counts[unsigned(Result::unknown)]>0);
}

TEST_CASE("almetal: affine intervals keep invalid subnormal overflow and cancellation inputs fail visible",
          "[almetal][world-cpu-bound-intervals][PPD-284]")
{
    using namespace CpuBoundIntervalsTest;
    const auto original=box();const auto identity_matrix=unit();
    CHECK(alMetalClassifyWorldBoundsCPU(original,nullptr)==Result::unknown);
    auto unqualified=original;unqualified.qualified=false;
    CHECK(alMetalClassifyWorldBoundsCPU(unqualified,identity_matrix.data())==Result::unknown);
    for(float invalid:{std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
    {
        for(unsigned entry=0;entry<16;++entry)
        {auto matrix=identity_matrix;matrix[entry]=invalid;CHECK(alMetalClassifyWorldBoundsCPU(original,matrix.data())==Result::unknown);}
        for(unsigned entry=0;entry<8;++entry)
        {auto bounds=original;(entry<4?bounds.minimum[entry]:bounds.maximum[entry-4])=invalid;
         CHECK(alMetalClassifyWorldBoundsCPU(bounds,identity_matrix.data())==Result::unknown);}
    }
    for(unsigned axis=0;axis<4;++axis)
    {auto bounds=original;bounds.minimum[axis]=1;bounds.maximum[axis]=-1;
     CHECK(alMetalClassifyWorldBoundsCPU(bounds,identity_matrix.data())==Result::unknown);}
    auto matrix=identity_matrix;matrix[0]=FLT_MAX/4;auto bounds=original;bounds.minimum[0]=2;bounds.maximum[0]=4;
    CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix.data())==Result::unknown);
    matrix=identity_matrix;matrix[0]=0x1p100f;matrix[12]=-0x1p100f;bounds=original;bounds.minimum[0]=bounds.maximum[0]=1;
    CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix.data())==Result::unknown);
    matrix=identity_matrix;matrix[3]=4;matrix[15]=0;
    CHECK(alMetalClassifyWorldBoundsCPU(original,matrix.data())==Result::unknown);
}
