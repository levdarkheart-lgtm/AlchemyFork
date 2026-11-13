/**
 * @file alwebrtc_exception_abi_test.cpp
 * @brief Real voice-wrapper linkage must preserve the system C++ exception ABI.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alaudioechobuffer.h"
#include "../llwebrtc.h"
#include "llimagej2c.h"

#include <catch2/catch_test_macros.hpp>

#include <cxxabi.h>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
void exerciseLinkedVoiceWrapper()
{
    // These out-of-line calls pull the actual production archive member.
    // No audio device, WebRTC session, viewer or window is started.
    llwebrtc::ALAudioEchoBuffer ring;
    ring.setEnabled(true);
    const float samples[] = {1.f, 2.f};
    const float* channels[] = {samples};
    ring.write(channels, 1, 2, 48000);
    int16_t output[2]{};
    REQUIRE(ring.read(output, 2, 1, 48000));
    CHECK(output[0] == 1);
    CHECK(output[1] == 2);

    // Also retain the other production wrapper member. Referencing its real
    // entry point exercises the viewer's archive combination without calling
    // WebRTC initialization, opening devices or creating any session.
    using Init = void (*)(llwebrtc::LLWebRTCLogCallback*);
    Init volatile linked_init = &llwebrtc::init;
    CHECK(linked_init != nullptr);
}

struct ExceptionResult
{
    bool caught_base = false;
    std::string message;
    std::string native_type;
};

template<typename Action>
ExceptionResult captureAsStandardException(Action&& action)
{
    ExceptionResult result;
    try
    {
        std::forward<Action>(action)();
    }
    catch (const std::exception& error)
    {
        result.caught_base = true;
        result.message = error.what();
    }
    catch (...)
    {
        const auto* type = abi::__cxa_current_exception_type();
        result.native_type = type ? type->name() : "unknown";
    }
    return result;
}

LLPointer<LLImageJ2C> fiveChannelTexture(std::vector<U8>& pixels)
{
    constexpr U16 width = 64, height = 32;
    constexpr S8 components = 5;
    LLPointer<LLImageRaw> raw = new LLImageRaw(width, height, components);
    pixels.resize(width * height * components);
    for (U16 y = 0; y < height; ++y)
        for (U16 x = 0; x < width; ++x)
            for (S8 channel = 0; channel < components; ++channel)
                pixels[(y * width + x) * components + channel] =
                    U8(x * 7 + y * 13 + channel * 41);
    std::copy(pixels.begin(), pixels.end(), raw->getData());
    LLPointer<LLImageJ2C> image = new LLImageJ2C;
    image->setReversible(true);
    REQUIRE(image->encode(raw, 0.f));
    image->setDiscardLevel(0);
    return image;
}
}

TEST_CASE("llwebrtc: linked voice wrapper preserves runtime_error base catches",
          "[llwebrtc][exception-abi]")
{
    exerciseLinkedVoiceWrapper();
    const auto result = captureAsStandardException([]
    {
        throw std::runtime_error("PPD-090 standard exception contract");
    });
    INFO("Unmatched native exception type: " << result.native_type);
    REQUIRE(result.caught_base);
    CHECK(result.message == "PPD-090 standard exception contract");
}

TEST_CASE("llwebrtc: linked voice wrapper preserves standard-library base catches",
          "[llwebrtc][exception-abi]")
{
    exerciseLinkedVoiceWrapper();
    const auto result = captureAsStandardException([]
    {
        const std::string value("x");
        (void)value.at(2);
    });
    INFO("Unmatched native exception type: " << result.native_type);
    REQUIRE(result.caught_base);
    CHECK_FALSE(result.message.empty());
}

TEST_CASE("llwebrtc: linked voice wrapper preserves five-channel decode failure and recovery",
          "[llwebrtc][exception-abi][alopenhtj2k]")
{
    if (LLImageJ2C::getEngineInfo().find("OpenHTJ2K 0.27.0 decode; ") != 0)
        SKIP("This codec-boundary regression requires the OpenHTJ2K build");

    exerciseLinkedVoiceWrapper();
    std::vector<U8> pixels;
    auto image = fiveChannelTexture(pixels);
    // Preserve the actual primary RGBA and separately requested bake channel.
    for (const S32 first : {0, 4})
    {
        CAPTURE(first);
        image->setDiscardLevel(0);
        LLPointer<LLImageRaw> output = new LLImageRaw;
        REQUIRE(image->decodeChannels(output, 0.f, first, 4));
        REQUIRE(output->getWidth() == 64);
        REQUIRE(output->getHeight() == 32);
        const S32 channels = std::min(4, 5 - first);
        REQUIRE(output->getComponents() == channels);
        std::vector<U8> expected(64 * 32 * channels);
        for (std::size_t pixel = 0; pixel < 64 * 32; ++pixel)
            for (S32 channel = 0; channel < channels; ++channel)
                expected[pixel * channels + channel] =
                    pixels[pixel * 5 + first + channel];
        CHECK(std::equal(expected.begin(), expected.end(), output->getData()));
    }

    image->setDiscardLevel(0);
    LLPointer<LLImageRaw> rejected = new LLImageRaw;
    bool returned = false, decoded = true;
    // This deliberately enters the real codec's runtime_error path. The
    // production boundary must handle it even with both voice-wrapper objects
    // linked, returning an ordinary image failure instead of escaping.
    const auto escaped = captureAsStandardException([&]
    {
        decoded = image->decodeChannels(rejected, 0.f, 8, 4);
        returned = true;
    });
    INFO("Escaped exception message: " << escaped.message);
    INFO("Unmatched native exception type: " << escaped.native_type);
    REQUIRE(returned);
    CHECK_FALSE(decoded);
    CHECK(rejected->getData() == nullptr);
    CHECK_FALSE(LLImage::getLastThreadError().empty());

    image->setDiscardLevel(0);
    LLPointer<LLImageRaw> recovered = new LLImageRaw;
    REQUIRE(image->decode(recovered, 0.f));
    REQUIRE(recovered->getComponents() == 4);
    std::vector<U8> expected(64 * 32 * 4);
    for (std::size_t pixel = 0; pixel < 64 * 32; ++pixel)
        std::copy_n(pixels.begin() + pixel * 5, 4, expected.begin() + pixel * 4);
    CHECK(std::equal(expected.begin(), expected.end(), recovered->getData()));
}
