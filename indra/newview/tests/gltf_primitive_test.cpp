/**
 * @file gltf_primitive_test.cpp
 * @brief CPU preparation, tangent ownership and picking admission regressions (PPD-059).
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../gltf/asset.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
// This dedicated CPU test binary has no viewer globals or render-device startup.
F64SecondsImplicit gFrameTimeSeconds(0.);
using namespace LL::GLTF;
namespace {
void quad(Primitive& p, Primitive::Mode mode, bool authored) {
 p.mMode=mode;
 p.mPositions={LLVector4a(0,0,0,0),LLVector4a(1,0,0,0),LLVector4a(0,1,0,0),LLVector4a(1,1,0,0)};
 p.mTexCoords0={LLVector2(0,0),LLVector2(1,0),LLVector2(0,1),LLVector2(1,1)};
 if(mode==Primitive::Mode::TRIANGLE_FAN){std::swap(p.mPositions[2],p.mPositions[3]);std::swap(p.mTexCoords0[2],p.mTexCoords0[3]);}
 p.mNormals.assign(4,LLVector4a(0,0,1,0));
 if(authored)p.mTangents.assign(4,LLVector4a(1,0,0,-1));
}
Asset unlitAsset(){Asset a;a.mMaterials.emplace_back();a.mMaterials[0].mUnlit.mPresent=true;return a;}
TEST_CASE("GLTF primitives: generated and authored strip/fan data keep correct runtime picking", "[gltf-primitive]") {
 for(auto mode:{Primitive::Mode::TRIANGLE_STRIP,Primitive::Mode::TRIANGLE_FAN})for(bool authored:{false,true}) {
  CAPTURE((int)mode,authored);Primitive p;quad(p,mode,authored);Asset a;REQUIRE(p.prep(a));CHECK(p.mMode==mode);
  CHECK(p.mGLMode==(authored?(mode==Primitive::Mode::TRIANGLE_STRIP?LLRender::TRIANGLE_STRIP:LLRender::TRIANGLE_FAN):LLRender::TRIANGLES));
  REQUIRE(p.mOctreeTriangles.size()==2);REQUIRE(p.mTangents.size()==p.mPositions.size());CHECK((p.mAttributeMask&LLVertexBuffer::MAP_TANGENT)!=0);
  LLVector4a tangent,normal;float x=mode==Primitive::Mode::TRIANGLE_FAN?.25f:.75f;
  CHECK(p.lineSegmentIntersect(LLVector4a(x,.75f,1,0),LLVector4a(x,.75f,-1,0),nullptr,nullptr,&normal,&tangent)!=nullptr);
  for(auto&t:p.mTangents){for(int j=0;j<4;++j)CHECK(std::isfinite(t[j]));CHECK(t[0]>.99f);CHECK(t[3]<0.f);}
 }
}
TEST_CASE("GLTF primitives: reject every short attribute before readers", "[gltf-primitive][gltf-attribute-control]") {
 for(int kind=0;kind<7;++kind){CAPTURE(kind);Primitive p;quad(p,Primitive::Mode::TRIANGLE_FAN,true);p.mMaterial=0;Asset a=unlitAsset();
  switch(kind){case 0:p.mNormals.resize(2);break;case 1:p.mTangents.resize(2);break;case 2:p.mColors.resize(2);break;case 3:p.mTexCoords0.resize(2);break;case 4:p.mTexCoords1.resize(2);break;case 5:p.mJoints.resize(2);p.mWeights.resize(4);break;case 6:p.mWeights.resize(2);p.mJoints.resize(4);break;}
  CHECK_FALSE(p.prep(a));CHECK(p.mOctree.isNull());
 }
 for(bool joints:{false,true}){Primitive p;quad(p,Primitive::Mode::TRIANGLE_FAN,true);p.mMaterial=0;Asset a=unlitAsset();if(joints)p.mJoints.resize(4);else p.mWeights.resize(4);CHECK_FALSE(p.prep(a));}
}
TEST_CASE("GLTF primitives: reject short surface topology without unsigned underflow", "[gltf-primitive][gltf-topology-control]") {
 for(auto mode:{Primitive::Mode::TRIANGLES,Primitive::Mode::TRIANGLE_STRIP,Primitive::Mode::TRIANGLE_FAN})for(int count:{0,1,2})for(bool authored:{false,true}) {
  CAPTURE((int)mode,count,authored);Primitive p;p.mMode=mode;p.mPositions.resize(count);p.mNormals.assign(count,LLVector4a(0,0,1,0));if(authored)p.mTangents.assign(count,LLVector4a(1,0,0,-1));Asset a;CHECK_FALSE(p.prep(a));CHECK(p.mOctree.isNull());
 }
 Primitive incomplete;quad(incomplete,Primitive::Mode::TRIANGLES,true);Asset a;CHECK_FALSE(incomplete.prep(a));
}
TEST_CASE("GLTF primitives: points and lines keep their existing preparation", "[gltf-primitive]") {
 for(auto mode:{Primitive::Mode::POINTS,Primitive::Mode::LINES,Primitive::Mode::LINE_LOOP,Primitive::Mode::LINE_STRIP}){Primitive p;p.mMode=mode;p.mPositions={LLVector4a(0,0,0,0),LLVector4a(1,0,0,0)};Asset a;CHECK(p.prep(a));CHECK(p.mOctreeTriangles.empty());CHECK(p.mMode==mode);}
}
TEST_CASE("GLTF primitives: direct unindexed picking uses sequential corners", "[gltf-primitive]") {
 Primitive p;quad(p,Primitive::Mode::TRIANGLE_FAN,true);p.mGLMode=LLRender::TRIANGLE_FAN;p.createOctree();REQUIRE(p.mOctreeTriangles.size()==2);CHECK(p.mIndexArray.empty());CHECK(p.lineSegmentIntersect(LLVector4a(.25f,.75f,1,0),LLVector4a(.25f,.75f,-1,0))!=nullptr);
}
}
