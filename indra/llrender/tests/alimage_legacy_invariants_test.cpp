/**
 * @file alimage_legacy_invariants_test.cpp
 * @brief Image format, mask, mip and publication invariants on actual native textures.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llimagegl.h"
#include "../llgl.h"
#include "../llrender.h"
#include "../alrendercapabilities.h"
#include "../../almetal/almetalpipeline.h"
#include "llimage.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <span>

namespace
{
struct NativeImages
{
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    bool previous_authority = LLImageGL::isMetalTextureAuthority();
    std::array<ALTextureSlot, AL_NUM_TEXTURE_SLOTS> previous_slots;
    std::unique_ptr<ALMetalDevice> device;
    std::string error;
    NativeImages()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        for (U32 i=0; i<AL_NUM_TEXTURE_SLOTS; ++i) previous_slots[i]=*gGL.getTextureSlot(i);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
    }
    ~NativeImages()
    {
        for (U32 i=0; i<AL_NUM_TEXTURE_SLOTS; ++i) *gGL.getTextureSlot(i)=previous_slots[i];
        LLImageGL::setMetalDevice(previous_device);
        LLImageGL::setMetalTextureAuthority(previous_authority);
        gRenderCapabilities=previous_capabilities;
    }
    static LLPointer<LLImageRaw> makeRaw(U16 w,U16 h,S8 components,U8 fill)
    {
        LLPointer<LLImageRaw> raw=new LLImageRaw(w,h,components);
        std::memset(raw->getData(),fill,static_cast<size_t>(w)*h*components);
        return raw;
    }
    template<typename T,std::size_t extent>
    void readTexture(const LLImageGL* image, std::span<T,extent> out, S32 level=0)
    {
        REQUIRE(image->getMetalTexture());
        const auto* texture=image->getMetalTexture();
        REQUIRE(level>=0);
        REQUIRE(U32(level)<texture->descriptor().mipmap_level_count);
        REQUIRE_FALSE(texture->isCompressed());
        const auto width=std::max(1u,texture->descriptor().width>>level);
        const auto height=std::max(1u,texture->descriptor().height>>level);
        const auto row_bytes=width*texture->bytesPerBlock();
        REQUIRE(out.size_bytes()>=size_t(row_bytes)*height);
        REQUIRE(texture->readback(*device,level,0,{},out.data(),row_bytes,&error));
        INFO(error);
    }
    U32 readTexelRGBA(const LLImageGL* image,S32 x,S32 y)
    {
        LLPointer<LLImageRaw> raw=new LLImageRaw;
        REQUIRE(image->readBackRaw(-1,raw,false));
        REQUIRE(raw->getComponents()==4);
        REQUIRE(x>=0); REQUIRE(x<raw->getWidth());
        REQUIRE(y>=0); REQUIRE(y<raw->getHeight());
        U32 texel;
        std::memcpy(&texel,raw->getData()+(size_t(y)*raw->getWidth()+x)*4,4);
        return texel;
    }
    std::array<U8,4> sample(const LLImageGL& image,float lod=0)
    {
        // This executes the image's native swizzle view and sampler LOD, rather
        // than treating raw storage readback as proof of shader-visible channels.
        const std::string source=R"metal(
#include <metal_stdlib>
using namespace metal;
vertex float4 image_vertex(uint id [[vertex_id]])
{
    const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(p[id],0,1);
}
fragment float4 image_sample(texture2d<float> image [[texture(0)]],
    sampler state [[sampler(0)]],constant float& lod [[buffer(0)]])
{ return image.sample(state,float2(.5),level(lod)); }
)metal";
        auto library=ALMetalShaderLibrary::compileSource(*device,source,"Image sampling invariant",&error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;
        pd.library=library.get();pd.vertex_function="image_vertex";pd.fragment_function="image_sample";
        pd.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error); INFO(error); REQUIRE(pipeline);
        ALMetalRenderTargetDescriptor td;td.width=td.height=1;td.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
        auto target=ALMetalRenderTarget::create(*device,td,&error);REQUIRE(target);
        ALMetalSamplerDescriptor sd;
        sd.min_filter=sd.mag_filter=ALMetalSamplerFilter::nearest;
        sd.mip_filter=ALMetalSamplerMipFilter::nearest;
        auto sampler=ALMetalSamplerState::create(*device,sd,&error);REQUIRE(sampler);
        auto command=device->makeCommandBuffer("Image shader-visible invariant",&error);REQUIRE(command);
        auto pass=target->makeRenderPass({},&error);REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Image invariant",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));
        REQUIRE(encoder->setViewport({0,0,1,1,0,1},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        REQUIRE(encoder->setFragmentTexture(0,image.getMetalTexture(),&error));
        REQUIRE(encoder->setFragmentSampler(0,sampler.get(),&error));
        REQUIRE(encoder->setFragmentBytes(0,&lod,sizeof(lod),&error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));INFO(error);
        std::array<U8,4> pixel{};
        REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},pixel.data(),4,&error));
        return pixel;
    }
};
// Original remaining image case 1: LLImageGL: createGLTexture(LLImageRaw) makes a texture under a fresh GL name
TEST_CASE_METHOD(NativeImages, "LLImageGL: native creation(LLImageRaw) makes a texture under a fresh native storage", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageRaw> raw = makeRaw(16, 16, 4, /*fill=*/0x80);
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);

        CHECK(img->createMetalTexture(/*discard_level=*/0, raw.get()));
        CHECK(img->getTexName() == 0);
        REQUIRE(img->getMetalTexture());
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getMetalTexture());
    }

// Original remaining image case 2: LLImageGL: destroyGLTexture drops the GL name and the image takes a new upload
TEST_CASE_METHOD(NativeImages, "LLImageGL: destroyGLTexture drops the native storage and the image takes a new upload", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageRaw> raw = makeRaw(16, 16, 4, /*fill=*/0x80);
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        CHECK(img->createMetalTexture(0, raw.get()));
        CHECK(img->getTexName() == 0);
        REQUIRE(img->getMetalTexture());

        img->destroyGLTexture();

        CHECK((S32)img->getTexName() == 0);
        CHECK_FALSE(img->getHasGLTexture());
        CHECK_FALSE(img->getMetalTexture());

        // Uploaded again, it holds a texture again.
        CHECK(img->createMetalTexture(0, raw.get()));
        UNSCOPED_INFO("re-uploaded");
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getMetalTexture());
    }

// Original remaining image case 3: LLImageGL: a partial setSubImage leaves the texture bound to unit 0
TEST_CASE_METHOD(NativeImages, "LLImageGL: a partial setSubImage leaves the texture bound to its native slot", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageRaw> raw = makeRaw(32, 32, 4, /*fill=*/0x80);
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, raw.get()));

        // Native subimage publication must leave the caller's logical slot
        // and sampled image ownership intact while changing only the rectangle.
        gGL.getTextureSlot(0)->bind(img.get());
        const U32 token_before=gGL.getTextureSlot(0)->getCurrTexture();
        const auto* native_before=img->getMetalTexture();
        REQUIRE(native_before);
        // Partial-rect upload — this is the path that used to
        // disable() the unit on its way out. The source raw must
        // span the (x_pos, y_pos, +width, +height) region: setSubImage
        // uses x_pos/y_pos as BOTH the source offset and the dest
        // offset, so a 32x32 source matching the destination is
        // simplest.
        LLPointer<LLImageRaw> patch = makeRaw(32, 32, 4, /*fill=*/0xFF);
        CHECK(img->uploadMetalSubImage(patch->getData(), patch->getWidth(), patch->getHeight(), 4, 4, 8, 8));

        CHECK(gGL.getTextureSlot(0)->getCurrTexture()==token_before);
        CHECK(img->getMetalTexture()==native_before);
        CHECK(readTexelRGBA(img,5,5)==0xffffffffu);
        CHECK(readTexelRGBA(img,0,0)==0x80808080u);
    }

// Original remaining image case 4: LLImageGL: the content stamp moves with every upload and holds through binds
TEST_CASE_METHOD(NativeImages, "LLImageGL: the content stamp moves with every upload and holds through binds", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageRaw> raw   = makeRaw(16, 16, 4, /*fill=*/0x80);
        LLPointer<LLImageGL>  img   = new LLImageGL(/*usemipmaps=*/false);
        LLPointer<LLImageGL>  other = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, raw.get()));
        REQUIRE(other->createMetalTexture(0, raw.get()));
        CHECK(img->getContentStamp() != 0);
        CHECK(img->getContentStamp() != other->getContentStamp());

        U64 stamp = img->getContentStamp();
        gGL.getTextureSlot(0)->bind(img.get());
        gGL.getTextureSlot(0)->unbind();
        CHECK(img->getContentStamp() == stamp);

        // A same-geometry native replacement changes its sampled content stamp.
        LLPointer<LLImageRaw> again = makeRaw(16, 16, 4, /*fill=*/0x20);
        REQUIRE(img->createMetalTexture(0, again.get()));
        CHECK(img->getTexName()==0);
        REQUIRE(img->getMetalTexture());
        CHECK(readTexelRGBA(img,0,0)==0x20202020u);
        CHECK(img->getContentStamp() != stamp);
        stamp = img->getContentStamp();

        LLPointer<LLImageRaw> patch = makeRaw(16, 16, 4, /*fill=*/0xFF);
        REQUIRE(img->uploadMetalSubImage(patch->getData(), patch->getWidth(), patch->getHeight(), 4, 4, 8, 8));
        CHECK(img->getContentStamp() != stamp);
    }

// Original remaining image case 5: LLImageGL: setExplicitFormat rewrites deprecated formats and keeps modern ones
TEST_CASE_METHOD(NativeImages, "LLImageGL: setExplicitFormat rewrites deprecated formats and keeps modern ones", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageGL> la = new LLImageGL(/*usemipmaps=*/false);
        la->setExplicitFormat(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
        CHECK((S32)la->getPrimaryFormat() == (S32)GL_RG);

        LLPointer<LLImageGL> lum = new LLImageGL(/*usemipmaps=*/false);
        lum->setExplicitFormat(GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE);
        CHECK((S32)lum->getPrimaryFormat() == (S32)GL_RED);

        LLPointer<LLImageGL> alpha = new LLImageGL(/*usemipmaps=*/false);
        alpha->setExplicitFormat(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE);
        CHECK((S32)alpha->getPrimaryFormat() == (S32)GL_RED);

        // Modern formats shouldn't be touched.
        LLPointer<LLImageGL> rgba = new LLImageGL(/*usemipmaps=*/false);
        rgba->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        CHECK((S32)rgba->getPrimaryFormat() == (S32)GL_RGBA);
    }

// Original remaining image case 6: LLImageGL: dataFormatBits/Bytes/Components give the documented host and VRAM sizes
TEST_CASE_METHOD(NativeImages, "LLImageGL: dataFormatBits/Bytes/Components give the documented host and VRAM sizes", "[almetal][native-image-invariants]")
    {
        CHECK(LLImageGL::dataFormatBits(GL_RGBA8) == 32);
        CHECK(LLImageGL::dataFormatBits(GL_RG8) == 16);
        CHECK(LLImageGL::dataFormatBits(GL_R8) == 8);
        CHECK(LLImageGL::dataFormatBits(GL_RGBA16F) == 64);

        CHECK(LLImageGL::dataFormatComponents(GL_RGBA) == 4);
        CHECK(LLImageGL::dataFormatComponents(GL_LUMINANCE_ALPHA) == 2);
        CHECK(LLImageGL::dataFormatComponents(GL_RED) == 1);

        // 16x16 RGBA8 = 16*16*4 = 1024 bytes, already 4-aligned.
        CHECK((S32)LLImageGL::dataFormatBytes(GL_RGBA8, 16, 16) == 1024);
        // 5x3 R8 = 15 bytes; rounds up to 16 (4-aligned).
        CHECK((S32)LLImageGL::dataFormatBytes(GL_R8, 5, 3) == 16);

        // Host vs. VRAM split: dataFormatBits returns the tight host
        // layout (used for CPU upload-buffer math); dataFormatVRAMBits
        // returns the padded driver allocation (used for VRAM stats).
        // For formats drivers don't pad, the two agree.
        CHECK(LLImageGL::dataFormatBits(GL_RGB8) == 24);
        UNSCOPED_INFO("padded to RGBX");
        CHECK(LLImageGL::dataFormatVRAMBits(GL_RGB8) == 32);
        CHECK(LLImageGL::dataFormatBits(GL_RGB16F) == 48);
        UNSCOPED_INFO("padded to RGBA16F");
        CHECK(LLImageGL::dataFormatVRAMBits(GL_RGB16F) == 64);
        CHECK(LLImageGL::dataFormatBits(GL_RGB32F) == 96);
        UNSCOPED_INFO("padded to RGBA32F");
        CHECK(LLImageGL::dataFormatVRAMBits(GL_RGB32F) == 128);
        CHECK(LLImageGL::dataFormatBits(GL_DEPTH_COMPONENT24) == 24);
        CHECK(LLImageGL::dataFormatVRAMBits(GL_DEPTH_COMPONENT24) == 32);

        // VRAM bytes path applies the same 4-byte alignment as the
        // host bytes path and routes unpadded formats through the
        // host table — so RGBA8 matches between the two.
        UNSCOPED_INFO("matches the host bytes");
        CHECK((S32)LLImageGL::dataFormatVRAMBytes(GL_RGBA8, 16, 16) == 1024);
        // 16x16 RGB8: host 16*16*3 = 768; VRAM 16*16*4 = 1024.
        CHECK((S32)LLImageGL::dataFormatBytes(GL_RGB8, 16, 16) == 768);
        CHECK((S32)LLImageGL::dataFormatVRAMBytes(GL_RGB8, 16, 16) == 1024);
    }

// Original remaining image case 7: LLImageGL: createGLTexture picks the primary format from the component count
TEST_CASE_METHOD(NativeImages, "LLImageGL: native creation picks the primary format from the component count", "[almetal][native-image-invariants]")
    {
        // 2-component raw — exercises the LUMINANCE_ALPHA -> GL_RG
        // rewrite path. This is the format the greyscale font atlas
        // uses (see commit c99dcaa7d3 — "Restore greyscale font atlas
        // to 2 component").
        LLPointer<LLImageRaw> raw2 = makeRaw(8, 8, 2, /*fill=*/0x80);
        LLPointer<LLImageGL> img2 = new LLImageGL(/*usemipmaps=*/false);
        CHECK(img2->createMetalTexture(0, raw2.get()));
        CHECK((S32)img2->getPrimaryFormat() == (S32)GL_RG);

        LLPointer<LLImageRaw> raw4 = makeRaw(8, 8, 4, /*fill=*/0x80);
        LLPointer<LLImageGL> img4 = new LLImageGL(/*usemipmaps=*/false);
        CHECK(img4->createMetalTexture(0, raw4.get()));
        CHECK((S32)img4->getPrimaryFormat() == (S32)GL_RGBA);
    }

// Original remaining image case 8: LLImageGL: a texture made with usemipmaps has a generated mip chain in GL
TEST_CASE_METHOD(NativeImages, "LLImageGL: a texture made with usemipmaps has a generated mip chain in Metal", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageRaw> raw = makeRaw(8, 8, 4, /*fill=*/0x80);
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(0, raw.get()));
        CHECK(img->getUseMipMaps());

        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count==4);
        CHECK(img->getMetalTexture()->descriptor().width/2==4);
        CHECK(img->getMetalTexture()->descriptor().height/2==4);
        std::vector<U8> mip1((size_t)4 * 4 * 4, 0);
        readTexture(img,std::span(mip1), /*level=*/1);
        UNSCOPED_INFO("bytes of mip 1 that are not the base's");
        CHECK(std::count_if(mip1.begin(), mip1.end(), [](U8 byte) { return byte != 0x80; }) == 0);
    }

// Original remaining image case 9: LLImageGL: createGLTexture sets the RRRA swizzle for luminance-alpha
TEST_CASE_METHOD(NativeImages, "LLImageGL: native creation sets the RRRA swizzle for luminance-alpha", "[almetal][native-image-invariants]")
    {
        // 2-component raw goes through LUMINANCE_ALPHA -> GL_RG
        // rewrite, which sets mDeprecatedSourceFormat=LUMINANCE_ALPHA.
        // createMetalTexture then applies the matching swizzle.
        LLPointer<LLImageRaw> raw = makeRaw(8, 8, 2, /*fill=*/0x80);
        // Distinct luminance/alpha exposes an incorrect native green swizzle.
        for (size_t pixel=0;pixel<8*8;++pixel)
        {
            raw->getData()[pixel*2]=0x40;
            raw->getData()[pixel*2+1]=0xc0;
        }
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, raw.get()));

        REQUIRE(img->getMetalTexture());
        const auto& mask=img->getMetalTexture()->descriptor().swizzle;
        CHECK(mask[0]==ALMetalTextureSwizzle::red);
        CHECK(mask[1]==ALMetalTextureSwizzle::red);
        CHECK(mask[2]==ALMetalTextureSwizzle::red);
        CHECK(mask[3]==ALMetalTextureSwizzle::green);
        CHECK(sample(*img)==std::array<U8,4>{0x40,0x40,0x40,0xc0});
    }

// Original remaining image case 10: LLImageGL: readBackRaw returns the bytes createGLTexture uploaded
TEST_CASE_METHOD(NativeImages, "LLImageGL: readBackRaw returns the bytes native creation uploaded", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 8, H = 8;
        for (S8 components = 1; components <= 4; ++components)
        {
            DYNAMIC_SECTION(static_cast<S32>(components) << " components")
            {
                LLPointer<LLImageRaw> src = new LLImageRaw(W, H, components);
                U8* sd = src->getData();
                // Per-pixel pattern distinguishes row, column and component swaps.
                for (size_t i = 0; i < (size_t)W * H * components; ++i)
                {
                    sd[i] = (U8)(i & 0xFF);
                }

                LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
                REQUIRE(img->createMetalTexture(0, src.get()));

                LLPointer<LLImageRaw> dst = new LLImageRaw(W, H, components);
                std::memset(dst->getData(), 0, (size_t)W * H * components);
                CHECK(img->readBackRaw(0, dst.get(), /*compressed_ok=*/false));

                CHECK(std::memcmp(src->getData(), dst->getData(),
                                  (size_t)W * H * components) == 0);
            }
        }
    }

// Original remaining image case 11: LLImageGL: a packed pixel type uploads and updates as one value a pixel
TEST_CASE_METHOD(NativeImages, "LLImageGL: a packed pixel type uploads and updates as one value a pixel", "[almetal][native-image-invariants]")
    {
        // Preserve the original packed-pixel source geometry and row pattern.
        constexpr U16 W = 64, H = 64;
        LLPointer<LLImageRaw> src = new LLImageRaw(W, H, 4);
        U8* sd = src->getData();
        for (size_t i = 0; i < (size_t)W * H * 4; ++i)
            sd[i] = (U8)(i & 0xFF);

        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        img->setExplicitFormat(GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
        REQUIRE(img->createMetalTexture(0, src.get()));

        std::vector<U8> got((size_t)W * H * 4);
        readTexture(img,std::span(got));
        UNSCOPED_INFO("the full upload");
        CHECK(std::memcmp(got.data(), sd, got.size()) == 0);

        // A partial-width update, which slices one row at a time.
        LLPointer<LLImageRaw> patch = new LLImageRaw(W, H, 4);
        U8* pd = patch->getData();
        for (size_t i = 0; i < (size_t)W * H * 4; ++i)
            pd[i] = (U8)(0xFF - (i & 0xFF));
        CHECK(img->uploadMetalSubImage(patch->getData(), patch->getWidth(), patch->getHeight(), 8, 4, 16, 8));

        readTexture(img,std::span(got));
        bool match = true;
        for (U32 y = 0; y < H && match; ++y)
        {
            for (U32 x = 0; x < W && match; ++x)
            {
                const bool in_patch = x >= 8 && x < 24 && y >= 4 && y < 12;
                const size_t at = ((size_t)y * W + x) * 4;
                match = std::memcmp(got.data() + at, (in_patch ? pd : sd) + at, 4) == 0;
            }
        }
        UNSCOPED_INFO("the update lands where it was asked and nowhere else");
        CHECK(match);
    }

// Original remaining image case 12: LLImageGL: scaleDown keeps a deprecated-format texture's swizzle and channels
TEST_CASE_METHOD(NativeImages, "LLImageGL: scaleDown keeps a deprecated-format texture's swizzle and channels", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 16, H = 16;
        LLPointer<LLImageRaw> raw = new LLImageRaw(W, H, 2);
        U8* d = raw->getData();
        for (size_t i = 0; i < (size_t)W * H; ++i)
        {
            d[i * 2]     = 0x40; // luminance
            d[i * 2 + 1] = 0xC0; // alpha
        }

        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(0, raw.get()));

        const bool scaled=img->scaleDown(1);
        CHECK(scaled);
        CHECK(img->getDiscardLevel() == 1);

        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().width==W/2);
        CHECK(img->getMetalTexture()->descriptor().height==H/2);
        const auto& mask=img->getMetalTexture()->descriptor().swizzle;
        CHECK(mask[0]==ALMetalTextureSwizzle::red);
        CHECK(mask[1]==ALMetalTextureSwizzle::red);
        CHECK(mask[2]==ALMetalTextureSwizzle::red);
        CHECK(mask[3]==ALMetalTextureSwizzle::green);
        CHECK(sample(*img)==std::array<U8,4>{0x40,0x40,0x40,0xc0});
        std::vector<U8> got((size_t)(W / 2) * (H / 2) * 2);
        readTexture(img,std::span(got));
        bool match = true;
        for (size_t i = 0; i < got.size() && match; i += 2)
        {
            match = got[i] == 0x40 && got[i + 1] == 0xC0;
        }
        UNSCOPED_INFO("luminance and alpha both survive the copy");
        CHECK(match);
    }

// Original remaining image case 13: LLImageGL: scaleDown waits until an upload in flight has ended
TEST_CASE_METHOD(NativeImages, "LLImageGL: scaleDown waits until an upload in flight has ended", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(0, makeRaw(16, 16, 4, 0x80).get()));
        const auto* native = img->getMetalTexture();

        img->beginUpload();
        const bool during = img->scaleDown(1);
        const auto* native_during = img->getMetalTexture();
        img->endUpload();
        const bool after = img->scaleDown(1);


        UNSCOPED_INFO("refused while an upload is in flight");
        CHECK_FALSE(during);
        CHECK(native_during == native);
        UNSCOPED_INFO("done once the upload has ended");
        CHECK(after);
    }

// Original remaining image case 14: LLImageGL: readBackRaw during an upload reads the texture on screen
TEST_CASE_METHOD(NativeImages, "LLImageGL: readBackRaw during an upload reads the texture on screen", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 16, H = 16;
        LLPointer<LLImageRaw> src = new LLImageRaw(W, H, 4);
        U8* sd = src->getData();
        for (size_t i = 0; i < (size_t)W * H * 4; ++i)
            sd[i] = (U8)(i & 0xFF);

        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(2, src.get()));
        CHECK(img->getWidth() == 16);

        img->beginUpload();
        img->setDiscardLevel(0); // what the worker's createMetalTexture writes first

        LLPointer<LLImageRaw> dst = new LLImageRaw();
        const bool read = img->readBackRaw(-1, dst.get(), /*compressed_ok=*/false);

        img->setDiscardLevel(2);
        img->endUpload();

        REQUIRE(read);
        {
            INFO("read at the size on screen");
            REQUIRE((S32)dst->getWidth() == (S32)W);
            REQUIRE((S32)dst->getHeight() == (S32)H);
        }
        REQUIRE((S32)dst->getComponents() == 4);
        UNSCOPED_INFO("and reads what is on screen");
        CHECK(std::memcmp(dst->getData(), sd, (size_t)W * H * 4) == 0);
    }

// Original remaining image case 15: LLImageGL: an upload in flight answers with the format on screen
TEST_CASE_METHOD(NativeImages, "LLImageGL: an upload in flight answers with the format on screen", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(16, 16, 4, 0x00).get()));
        REQUIRE(img->getPrimaryFormat() == (LLGLenum)GL_RGBA);
        REQUIRE(img->getMaxAlpha() == 0);

        img->beginUpload(3);
        UNSCOPED_INFO("the RGBA texture on screen, not the RGB one being built");
        CHECK(img->getPrimaryFormat() == (LLGLenum)GL_RGBA);
        CHECK(img->getMaxAlpha() == 0);
        img->endUpload();
        UNSCOPED_INFO("an upload that never published leaves the alpha alone");
        CHECK(img->getMaxAlpha() == 0);
    }

// Original remaining image case 16: LLImageGL: the pick mask of an upload in flight publishes with its texture
TEST_CASE_METHOD(NativeImages, "LLImageGL: the pick mask of an upload in flight publishes with its texture", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 16, H = 16;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 4, 0xFF).get()));
        const LLVector2 centre(0.5f, 0.5f);
        UNSCOPED_INFO("opaque texture picks");
        CHECK(img->getMask(centre));

        LLPointer<LLImageRaw> clear = makeRaw(W, H, 4, 0x00);

        img->beginUpload();
        img->updatePickMask(W, H, clear->getData());
        UNSCOPED_INFO("the mask on screen answers while the upload is in flight");
        CHECK(img->getMask(centre));
        img->endUpload();
        UNSCOPED_INFO("an upload that never published leaves the mask alone");
        CHECK(img->getMask(centre));

        const auto* old_native=img->getMetalTexture();
        img->beginUpload();
        REQUIRE(img->createMetalTexture(0,clear.get()));
        img->updatePickMask(W,H,clear->getData());
        CHECK(img->getMetalTexture()==old_native);
        CHECK(img->getMask(centre));
        img->publishMetalTextureAuthoritative();
        CHECK(img->getMetalTexture()!=old_native);
        CHECK(readTexelRGBA(img,0,0)==0u);
        UNSCOPED_INFO("the new mask publishes with its texture");
        CHECK_FALSE(img->getMask(centre));
    }

// Original remaining image case 17: LLImageGL: the alpha analysis reports the highest alpha, odd edges included
TEST_CASE_METHOD(NativeImages, "LLImageGL: the alpha analysis reports the highest alpha, odd edges included", "[almetal][native-image-invariants]")
    {
        // A clear w x h RGBA image with alpha `alpha` at (x, y).
        auto analysed = [](U16 w, U16 h, S32 x, S32 y, U8 alpha)
        {
            LLPointer<LLImageRaw> raw = makeRaw(w, h, 4, 0x00);
            if (x >= 0)
            {
                raw->getData()[((size_t)y * w + x) * 4 + 3] = alpha;
            }
            LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
            REQUIRE(img->createMetalTexture(0, raw.get()));
            return img->getMaxAlpha();
        };

        UNSCOPED_INFO("a clear texture");
        CHECK(analysed(16, 16, -1, -1, 0) == 0);
        UNSCOPED_INFO("one texel inside");
        CHECK(analysed(16, 16, 5, 7, 200) == 200);
        UNSCOPED_INFO("a texel in the odd column");
        CHECK(analysed(15, 15, 14, 3, 90) == 90);
        UNSCOPED_INFO("a texel in the odd row");
        CHECK(analysed(15, 15, 6, 14, 60) == 60);
        UNSCOPED_INFO("the odd corner");
        CHECK(analysed(15, 15, 14, 14, 30) == 30);
        UNSCOPED_INFO("a single row");
        CHECK(analysed(9, 1, 8, 0, 120) == 120);

        LLPointer<LLImageGL> rgb = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(rgb->createMetalTexture(0, makeRaw(16, 16, 3, 0x00).get()));
        UNSCOPED_INFO("no alpha channel reads as opaque");
        CHECK(rgb->getMaxAlpha() == 255);
    }

// Original remaining image case 18: LLImageGL: the alpha analysis reports the lowest alpha, odd edges included
TEST_CASE_METHOD(NativeImages, "LLImageGL: the alpha analysis reports the lowest alpha, odd edges included", "[almetal][native-image-invariants]")
    {
        // An opaque w x h RGBA image with alpha `alpha` at (x, y).
        auto analysed = [](U16 w, U16 h, S32 x, S32 y, U8 alpha)
        {
            LLPointer<LLImageRaw> raw = makeRaw(w, h, 4, 0xFF);
            if (x >= 0)
            {
                raw->getData()[((size_t)y * w + x) * 4 + 3] = alpha;
            }
            LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
            REQUIRE(img->createMetalTexture(0, raw.get()));
            return img->getMinAlpha();
        };

        UNSCOPED_INFO("an opaque texture");
        CHECK(analysed(16, 16, -1, -1, 0) == 255);
        UNSCOPED_INFO("one texel inside");
        CHECK(analysed(16, 16, 5, 7, 40) == 40);
        UNSCOPED_INFO("a texel in the odd column");
        CHECK(analysed(15, 15, 14, 3, 90) == 90);
        UNSCOPED_INFO("a texel in the odd row");
        CHECK(analysed(15, 15, 6, 14, 60) == 60);
        UNSCOPED_INFO("the odd corner");
        CHECK(analysed(15, 15, 14, 14, 30) == 30);
        UNSCOPED_INFO("a single row");
        CHECK(analysed(9, 1, 8, 0, 120) == 120);

        LLPointer<LLImageGL> rgb = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(rgb->createMetalTexture(0, makeRaw(16, 16, 3, 0x00).get()));
        UNSCOPED_INFO("no alpha channel reads as opaque");
        CHECK(rgb->getMinAlpha() == 255);

        LLPointer<LLImageGL> unread = new LLImageGL(/*usemipmaps=*/false);
        unread->setNeedsAlphaAndPickMask(false);
        REQUIRE(unread->createMetalTexture(0, makeRaw(16, 16, 4, 0xFF).get()));
        UNSCOPED_INFO("an alpha not analysed may be anything");
        CHECK(unread->getMinAlpha() == 0);
    }

// Original remaining image case 19: LLImageGL: a mask wanted after an upload reads alpha from the alpha channel
TEST_CASE_METHOD(NativeImages, "LLImageGL: a mask wanted after an upload reads alpha from the alpha channel", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 16, H = 16;
        // Red full and alpha clear: read from the first channel, it would look opaque.
        LLPointer<LLImageRaw> raw = makeRaw(W, H, 4, 0x00);
        for (size_t i = 0; i < (size_t)W * H; ++i)
        {
            raw->getData()[i * 4] = 0xFF;
        }
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        img->setNeedsAlphaAndPickMask(false);
        REQUIRE(img->createMetalTexture(0, raw.get()));
        UNSCOPED_INFO("no mask wanted reads as opaque");
        CHECK(img->getMaxAlpha() == 255);

        img->setNeedsAlphaAndPickMask(true);
        REQUIRE(img->createMetalTexture(0, raw.get()));
        CHECK(img->getMaxAlpha() == 0);
    }

// Original remaining image case 20: LLImageGL: an edit reaches its own texture while another unit is active
TEST_CASE_METHOD(NativeImages, "LLImageGL: an edit reaches its own texture while another native slot is populated", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 32, H = 32;
        LLPointer<LLImageGL> a = new LLImageGL(/*usemipmaps=*/false);
        LLPointer<LLImageGL> b = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(a->createMetalTexture(0, makeRaw(W, H, 4, 0x10).get()));
        REQUIRE(b->createMetalTexture(0, makeRaw(W, H, 4, 0x20).get()));

        // Partial update, as the glyph atlas does it. Forcing the slot 1 bind is what
        // leaves unit 1 active, cached or not.
        gGL.getTextureSlot(0)->bind(a.get());
        gGL.getTextureSlot(1)->bind(b.get(), false, /*forceBind=*/true);
        CHECK(a->uploadMetalSubImage(makeRaw(W, H, 4, 0xFF)->getData(),W,H,4,4,8,8));
        UNSCOPED_INFO("the update reached a");
        CHECK(readTexelRGBA(a, 5, 5) == 0xFFFFFFFFu);
        UNSCOPED_INFO("the update left b alone");
        CHECK(readTexelRGBA(b, 5, 5) == 0x20202020u);

        // Re-upload in place, through setImage.
        gGL.getTextureSlot(0)->bind(a.get());
        gGL.getTextureSlot(1)->bind(b.get(), false, /*forceBind=*/true);
        REQUIRE(a->createMetalTexture(0, makeRaw(W, H, 4, 0x77).get()));
        UNSCOPED_INFO("the re-upload reached a");
        CHECK(readTexelRGBA(a, 5, 5) == 0x77777777u);
        UNSCOPED_INFO("the re-upload left b alone");
        CHECK(readTexelRGBA(b, 5, 5) == 0x20202020u);
    }

// Original remaining image case 21: LLImageGL: a re-upload at the same discard level takes a new size or format
TEST_CASE_METHOD(NativeImages, "LLImageGL: a re-upload at the same discard level takes a new size or format", "[almetal][native-image-invariants]")
    {
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(32, 32, 4, 0x11).get()));
        REQUIRE(img->createMetalTexture(0, makeRaw(64, 64, 4, 0x22).get()));

        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().width==64);
        UNSCOPED_INFO("the resized texture holds the new pixels");
        CHECK(readTexelRGBA(img, 40, 40) == 0x22222222u);

        // Same size, an alpha channel the old storage has no room for.
        LLPointer<LLImageGL> rgb = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(rgb->createMetalTexture(0, makeRaw(16, 16, 3, 0x33).get()));
        REQUIRE(rgb->createMetalTexture(0, makeRaw(16, 16, 4, 0x44).get()));

        REQUIRE(rgb->getMetalTexture());
        CHECK(rgb->getMetalTexture()->descriptor().format==ALMetalPixelFormat::rgba8_unorm_srgb);
        UNSCOPED_INFO("the reformatted texture holds the new pixels");
        CHECK(readTexelRGBA(rgb, 3, 3) == 0x44444444u);
    }

// Original remaining image case 23: LLImageGL: generated mips fill the whole pyramid and sampling reaches it
TEST_CASE_METHOD(NativeImages, "LLImageGL: generated mips fill the whole pyramid and sampling reaches it", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 256, H = 256;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 4, 0x80).get()));

        const S32 last=LLImageGL::calcMipLevelCount(W,H)-1;
        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count==U32(last+1));
        U8 px[4] = { 0, 0, 0, 0 };
        readTexture(img,std::span(px),last);
        // This default RGBA image is sRGB. Storage remains 0x80 while
        // shader sampling decodes RGB to linear 0.21586 (55/255).
        CHECK(img->getMetalTexture()->descriptor().format==ALMetalPixelFormat::rgba8_unorm_srgb);
        CHECK(sample(*img,F32(last))==std::array<U8,4>{55,55,55,0x80});
        {
            INFO("the 1x1 level was generated");
            CHECK(px[0] == 0x80);
            CHECK(px[1] == 0x80);
            CHECK(px[2] == 0x80);
            CHECK(px[3] == 0x80);
        }
    }

// Original remaining image case 32: LLImageGL: integer formats allocate as themselves and hold their texels exactly
TEST_CASE_METHOD(NativeImages, "LLImageGL: integer formats allocate as themselves and hold their texels exactly", "[almetal][native-image-invariants]")
    {
        constexpr U16 W = 13, H = 7;
        constexpr size_t N = (size_t)W * H;

        std::vector<U32> uints(N);
        for (size_t i = 0; i < N; ++i)
        {
            uints[i] = 0x01000001u + (U32)i * 0x10001u;
        }
        LLPointer<LLImageGL> r32ui = new LLImageGL(W, H, 1, /*usemipmaps=*/true);
        r32ui->setExplicitFormat(GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);
        U64 before = LLImageGL::getTextureBytesAllocated();
        REQUIRE(r32ui->createMetalTexture(0, reinterpret_cast<const U8*>(uints.data())));
        UNSCOPED_INFO("R32UI accounted at four bytes a texel");
        CHECK((S64)(LLImageGL::getTextureBytesAllocated() - before) == (S64)N * 4);
        CHECK((S64)r32ui->getMipBytes(0) == (S64)N * 4);
        UNSCOPED_INFO("an integer texture asked for mips takes none");
        CHECK_FALSE(r32ui->getUseMipMaps());

        REQUIRE(r32ui->getMetalTexture());
        CHECK(r32ui->getMetalTexture()->descriptor().format==ALMetalPixelFormat::r32_uint);
        CHECK(r32ui->getMetalTexture()->descriptor().mipmap_level_count==1);
        std::vector<U32> got_uints(N, 0);
        readTexture(r32ui,std::span(got_uints));
        CHECK(got_uints == uints);

        // A partial update, its rows read from a wider source as the glyph atlas updates.
        std::vector<U32> patch(N);
        for (size_t i = 0; i < N; ++i)
        {
            patch[i] = 0xF0000000u + (U32)i;
        }
        CHECK(r32ui->uploadMetalSubImage(reinterpret_cast<const U8*>(patch.data()),W,H,2,1,4,3));
        readTexture(r32ui,std::span(got_uints));
        for (U32 y = 0; y < H; ++y)
        {
            for (U32 x = 0; x < W; ++x)
            {
                const size_t i = (size_t)y * W + x;
                const bool in_patch = x >= 2 && x < 6 && y >= 1 && y < 4;
                CAPTURE(x, y);
                REQUIRE(got_uints[i] ==
                              (in_patch ? patch[i] : uints[i]));
            }
        }

        std::vector<S16> shorts(N * 2);
        for (size_t i = 0; i < N; ++i)
        {
            shorts[i * 2]     = (S16)(-1 - (S32)i * 300);
            shorts[i * 2 + 1] = (S16)((S32)i * 251 - 12000);
        }
        LLPointer<LLImageGL> rg16i = new LLImageGL(W, H, 2, /*usemipmaps=*/false);
        rg16i->setExplicitFormat(GL_RG16I, GL_RG_INTEGER, GL_SHORT);
        before = LLImageGL::getTextureBytesAllocated();
        REQUIRE(rg16i->createMetalTexture(0, reinterpret_cast<const U8*>(shorts.data())));
        UNSCOPED_INFO("RG16I accounted at four bytes a texel");
        CHECK((S64)(LLImageGL::getTextureBytesAllocated() - before) == (S64)N * 4);
        REQUIRE(rg16i->getMetalTexture());
        CHECK(rg16i->getMetalTexture()->descriptor().format==ALMetalPixelFormat::rg16_sint);
        std::vector<S16> got_shorts(N * 2, 0);
        readTexture(rg16i,std::span(got_shorts));
        CHECK(got_shorts == shorts);

        std::vector<U32> wide(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            wide[i * 4]     = (U32)i;
            wide[i * 4 + 1] = 0x80000000u | (U32)i;
            wide[i * 4 + 2] = ~(U32)i;
            wide[i * 4 + 3] = 0xDEADBEEFu ^ (U32)i;
        }
        LLPointer<LLImageGL> rgba32ui = new LLImageGL(W, H, 4, /*usemipmaps=*/false);
        rgba32ui->setExplicitFormat(GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
        before = LLImageGL::getTextureBytesAllocated();
        REQUIRE(rgba32ui->createMetalTexture(0, reinterpret_cast<const U8*>(wide.data())));
        UNSCOPED_INFO("RGBA32UI accounted at sixteen bytes a texel");
        CHECK((S64)(LLImageGL::getTextureBytesAllocated() - before) == (S64)N * 16);
        REQUIRE(rgba32ui->getMetalTexture());
        CHECK(rgba32ui->getMetalTexture()->descriptor().format==ALMetalPixelFormat::rgba32_uint);
        std::vector<U32> got_wide(N * 4, 0);
        readTexture(rgba32ui,std::span(got_wide));
        CHECK(got_wide == wide);

        // What an LLImageRaw cannot hold, readBackRaw does not read: it has a byte a component.
        LLPointer<LLImageRaw> raw = new LLImageRaw();
        CHECK_FALSE(r32ui->readBackRaw(0, raw.get(), /*compressed_ok=*/false));
        CHECK_FALSE(r32ui->scaleDown(1));


    }

// Original remaining image case 33: LLImageGL: the size and channel tables cover the integer formats
TEST_CASE_METHOD(NativeImages, "LLImageGL: the size and channel tables cover the integer formats", "[almetal][native-image-invariants]")
    {
        const std::pair<GLenum, S32> bits[] =
        {
            { GL_R8I, 8 },      { GL_R8UI, 8 },      { GL_R16I, 16 },     { GL_R16UI, 16 },
            { GL_R32I, 32 },    { GL_R32UI, 32 },    { GL_RG8I, 16 },     { GL_RG8UI, 16 },
            { GL_RG16I, 32 },   { GL_RG16UI, 32 },   { GL_RG32I, 64 },    { GL_RG32UI, 64 },
            { GL_RGB8I, 24 },   { GL_RGB8UI, 24 },   { GL_RGB16I, 48 },   { GL_RGB16UI, 48 },
            { GL_RGB32I, 96 },  { GL_RGB32UI, 96 },  { GL_RGBA8I, 32 },   { GL_RGBA8UI, 32 },
            { GL_RGBA16I, 64 }, { GL_RGBA16UI, 64 }, { GL_RGBA32I, 128 }, { GL_RGBA32UI, 128 },
            { GL_RGB10_A2UI, 32 },
        };
        for (const auto& entry : bits)
        {
            INFO("format " << entry.first);
            REQUIRE(LLImageGL::dataFormatBits(entry.first) == entry.second);
            const S32 vram = entry.second == 24 ? 32 : entry.second == 48 ? 64 : entry.second == 96 ? 128 : entry.second;
            REQUIRE(LLImageGL::dataFormatVRAMBits(entry.first) == vram);
        }

        const std::pair<GLenum, S32> components[] =
        {
            { GL_RED_INTEGER, 1 }, { GL_RG_INTEGER, 2 },   { GL_RGB_INTEGER, 3 },
            { GL_BGR_INTEGER, 3 }, { GL_RGBA_INTEGER, 4 }, { GL_BGRA_INTEGER, 4 },
        };
        for (const auto& entry : components)
        {
            INFO("pixel format " << entry.first);
            REQUIRE(LLImageGL::dataFormatComponents(entry.first) == entry.second);
        }
    }

}
