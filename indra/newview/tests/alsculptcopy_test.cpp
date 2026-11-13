/**
 * @file alsculptcopy_test.cpp
 * @brief Real sculpt copy production and LLVolume consumption at the same level.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "../alsculptcopy.h"
#include "llvolume.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
U8 byte(F32 value) { return U8(llclamp(ll_round((value+.5f)*255.f),0,255)); }
template<class Position>
LLPointer<LLImageRaw> map(S32 width,S32 height,Position position)
{
    LLPointer<LLImageRaw> image = new LLImageRaw(width,height,3);
    LLImageDataLock lock(image);
    for(S32 y=0;y<height;++y) for(S32 x=0;x<width;++x)
    {
        const auto p=position(x,y);
        for(S32 c=0;c<3;++c) image->getData()[(y*width+x)*3+c]=byte(p.mV[c]);
    }
    return image;
}
LLPointer<LLImageRaw> cylinder(S32 width,S32 height)
{
    return map(width,height,[=](S32 x,S32 y) {
        const F32 angle=F_TWO_PI*F32(x)/width;
        return LLVector3(.4f*cosf(angle),-.4f*sinf(angle),F32(y)/(height-1)-.5f); });
}
LLPointer<LLVolume> volume(F32 detail=4.f,U8 type=LL_SCULPT_TYPE_CYLINDER)
{
    LLVolumeParams params;
    params.setType(LL_PCODE_PROFILE_CIRCLE,LL_PCODE_PATH_CIRCLE);
    params.setRatio(1.f,.5f);
    params.setSculptID(LLUUID("c8b26d0b-6f5a-4f10-8d1e-5c3b3e4a9a01"),type);
    return new LLVolume(params,detail);
}
void build(LLVolume& target,const LLImageRaw& image,S32 discard)
{
    LLImageDataSharedLock lock(&image);
    target.sculpt(image.getWidth(),image.getHeight(),image.getComponents(),image.getData(),discard,true);
}
}

TEST_CASE("Sculpt copy keeps a thin map's pixels and proportions instead of squeezing it square", "[sculpt-copy]")
{
    for(const auto dimensions : {std::array<S32,2>{512,8},{8,512},{4096,4},{4,4096}})
    {
        auto source=cylinder(dimensions[0],dimensions[1]);
        const auto size=ALSculptCopy::size(source->getWidth(),source->getHeight(),0,128,128);
        LLImageDataSharedLock lock(source);
        auto copy=ALSculptCopy::image(source,size); REQUIRE(copy);
        CHECK(copy->getWidth()==source->getWidth()); CHECK(copy->getHeight()==source->getHeight());
        CHECK(copy->getDataSize()==source->getDataSize());
        CHECK(std::memcmp(copy->getData(),source->getData(),source->getDataSize())==0);
        auto target=volume(); build(*target,*copy,0);
        CHECK(target->getSculptPixels()==dimensions[0]*dimensions[1]);
        CHECK(target->getSurfaceArea()>.02f);
    }
}

TEST_CASE("Sculpt native readback chooses the copy mip and applies the same producer to a larger fallback", "[sculpt-copy]")
{
    for(const auto dimensions : {std::array<S32,2>{1024,512},{512,1024},{4096,4096}})
    {
        const auto wanted=ALSculptCopy::size(dimensions[0],dimensions[1],0,128,128);
        CAPTURE(dimensions,wanted.width,wanted.height,wanted.discard);
        CHECK(wanted.pixels()<=128*128);
        CHECK(wanted.width*dimensions[1]==wanted.height*dimensions[0]);
        CHECK(wanted.width==(dimensions[0]>>wanted.discard));
        CHECK(wanted.height==(dimensions[1]>>wanted.discard));
    }
    auto source=cylinder(1024,512);
    const auto wanted=ALSculptCopy::size(1024,512,0,128,128);
    LLImageDataSharedLock lock(source);
    auto copy=ALSculptCopy::image(source,wanted); REQUIRE(copy);
    CHECK(copy->getWidth()==128); CHECK(copy->getHeight()==64); CHECK(wanted.discard==3);
    auto target=volume(); build(*target,*copy,wanted.discard);
    CHECK(target->getSculptPixels()==128*64); CHECK(target->getSculptLevel()==3);
}

TEST_CASE("Larger saved sculpt pixels at the same discard are selected and rebuild the real volume", "[sculpt-copy]")
{
    auto coarse=cylinder(128,8), source=cylinder(512,8);
    auto target=volume(); build(*target,*coarse,0);
    REQUIRE(target->getSculptPixels()==128*8);
    LLImageDataSharedLock lock(source);
    auto saved=ALSculptCopy::image(source,ALSculptCopy::size(512,8,0,128,128)); REQUIRE(saved);
    REQUIRE(ALSculptCopy::needsSave(0,saved->getWidth()*saved->getHeight(),0,128*8,true));
    CHECK_FALSE(ALSculptCopy::needsSave(0,128*8,0,128*8,true));
    CHECK_FALSE(ALSculptCopy::needsSave(0,512*8,0,128*8,false));
    const auto selected=ALSculptCopy::select(coarse,0,saved,0);
    CHECK(selected.image==saved.get()); CHECK(selected.pixels()==512*8);
    REQUIRE(ALSculptCopy::needsRebuild(selected.discard,selected.pixels(),target->getSculptLevel(),target->getSculptPixels()));
    CHECK_FALSE(ALSculptCopy::alreadyBuilt(selected.discard,selected.pixels(),target->getSculptLevel(),target->getSculptPixels()));
    build(*target,*selected.image,selected.discard);
    CHECK(target->getSculptPixels()==512*8);
    CHECK_FALSE(ALSculptCopy::needsRebuild(selected.discard,selected.pixels(),target->getSculptLevel(),target->getSculptPixels()));
    CHECK(ALSculptCopy::alreadyBuilt(selected.discard,selected.pixels(),target->getSculptLevel(),target->getSculptPixels()));
    CHECK_FALSE(ALSculptCopy::needsRebuild(1,selected.pixels(),0,selected.pixels()));
    const auto retained=ALSculptCopy::select(nullptr,-1,saved,0);
    CHECK(retained.image==saved.get()); CHECK(retained.discard==0);
}

TEST_CASE("Sculpt low-area coarse samples retry their map without changing the finest detail", "[sculpt-copy]")
{
    const auto tube=map(512,8,[](S32 x,S32 y) {
        if(x%8==0) return LLVector3(0,0,F32(x)/512-.5f);
        const F32 angle=F_TWO_PI*F32(y)/8;
        return LLVector3(.2f*cosf(angle),.2f*sinf(angle),F32(x/2*2)/512-.5f); });
    for(F32 detail : {1.5f,2.5f,4.f})
    {
        auto target=volume(detail,LL_SCULPT_TYPE_TORUS); build(*target,*tube,0);
        CAPTURE(detail,target->getSurfaceArea());
        CHECK(target->getSurfaceArea()>.02f);
        CHECK(target->getSculptPixels()==512*8);
        if(detail==4.f) CHECK(target->getProfile().mProfile.size()==257);
    }
    auto empty=volume(); empty->sculpt(0,0,0,nullptr,-1,true); CHECK(empty->getSculptPixels()==0);
}

TEST_CASE("Sculpt excessive-area noise still builds the sphere rather than being smoothed into a shape", "[sculpt-copy]")
{
    const auto hash=[](U32 value) {
        value^=value>>16; value*=0x7feb352du; value^=value>>15;
        value*=0x846ca68bu; return value^(value>>16); };
    const auto noise=map(128,128,[&](S32 x,S32 y) {
        const U32 h=hash(U32(y)*128+U32(x));
        return LLVector3(h&1?.5f:-.5f,h&2?.5f:-.5f,h&4?.5f:-.5f); });
    auto target=volume(4.f,LL_SCULPT_TYPE_PLANE); build(*target,*noise,0);
    CHECK(target->getSurfaceArea()>384.f);
    const auto* first=target->getMeshPt(0).getF32ptr();
    CHECK(std::abs(first[0])<.00001f); CHECK(std::abs(first[1])<.00001f);
    CHECK(std::abs(first[2]-.3f)<.00001f);
}
