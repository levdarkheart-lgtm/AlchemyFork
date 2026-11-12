// Local experiment admission through the real LLImageJ2C implementation.
#include "linden_common.h"
#include "llimagej2c.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <vector>

namespace {
LLPointer<LLImageJ2C> losslessTexture(S32 components, std::vector<U8>& pixels)
{
    LLPointer<LLImageRaw> raw = new LLImageRaw(64,32,components);
    pixels.resize(64*32*components);
    for (S32 y=0;y<32;++y)
        for (S32 x=0;x<64;++x)
            for (S32 c=0;c<components;++c)
                pixels[(y*64+x)*components+c] = U8(x*7+y*13+c*41);
    std::copy(pixels.begin(),pixels.end(),raw->getData());
    LLPointer<LLImageJ2C> j2c = new LLImageJ2C;
    j2c->setReversible(true);
    REQUIRE(j2c->encode(raw,0.0f));
    j2c->setDiscardLevel(0);
    return j2c;
}
}

TEST_CASE("OpenHTJ2K experiment reports the decoder without changing disk encoding identity", "[alopenhtj2k]")
{
    const auto engine=LLImageJ2C::getEngineInfo(), cache=LLImageJ2C::getCacheInfo();
    CHECK(engine.find("OpenHTJ2K 0.27.0 decode; ")==0);
    CHECK(cache.find("OpenJPEG")==0);
    CHECK(cache.size()<32);
    CHECK(engine!=cache);
}

TEST_CASE("OpenHTJ2K experiment preserves bottom-up primary and baked auxiliary channels", "[alopenhtj2k]")
{
    for (S32 components : {1,2,3,4,5,8}) {
        CAPTURE(components);
        std::vector<U8> pixels;
        auto j2c=losslessTexture(components,pixels);
        for (S32 first : {0,4}) {
            if (first>=components) continue;
            CAPTURE(first);
            j2c->setDiscardLevel(0);
            LLPointer<LLImageRaw> out=new LLImageRaw;
            REQUIRE(j2c->decodeChannels(out,0.0f,first,4));
            REQUIRE(out->getWidth()==64);
            REQUIRE(out->getHeight()==32);
            const S32 count=std::min(4,components-first);
            REQUIRE(out->getComponents()==count);
            std::vector<U8> expected(64*32*count);
            for (size_t p=0;p<64*32;++p)
                for (S32 c=0;c<count;++c)
                    expected[p*count+c]=pixels[p*components+first+c];
            CHECK(std::equal(expected.begin(),expected.end(),out->getData()));
        }
    }
}

TEST_CASE("OpenHTJ2K experiment rejects an invalid channel and can decode a later valid request", "[alopenhtj2k]")
{
    std::vector<U8> pixels;
    auto j2c=losslessTexture(4,pixels);
    LLPointer<LLImageRaw> out=new LLImageRaw;
    CHECK_FALSE(j2c->decodeChannels(out,0.0f,8,4));
    CHECK(out->getData()==nullptr);
    j2c->setDiscardLevel(0);
    REQUIRE(j2c->decode(out,0.0f));
    REQUIRE(out->getData()!=nullptr);
    CHECK(std::equal(pixels.begin(),pixels.end(),out->getData()));
}
