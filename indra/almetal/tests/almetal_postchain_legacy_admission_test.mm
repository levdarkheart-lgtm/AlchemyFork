/**
 * @file almetal_postchain_legacy_admission_test.mm
 * @brief Original CAS sampling and depth-of-field invariants on native owners.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
using Bytes = std::array<std::uint8_t, 4>;
using Color = std::array<double, 4>;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(arena); arena->beginFrame(1);
        renderer->setFrameData(std::move(arena));
    }
    template<typename T>
    std::unique_ptr<ALMetalRenderTarget> source(unsigned width, unsigned height,
        ALMetalPixelFormat format, const std::vector<T>& pixels, unsigned pixel_bytes)
    {
        ALMetalRenderTargetDescriptor d;
        d.width = width; d.height = height; d.colors[0].format = format;
        d.label = "Original post-chain admission input";
        auto target = ALMetalRenderTarget::create(*device, d, &error);
        INFO(error); REQUIRE(target);
        auto command = device->makeCommandBuffer("Original post-chain authored texels", &error);
        REQUIRE(command);
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(
            *command, 0, 0, {}, pixels.data(), width * pixel_bytes, &error));
        REQUIRE(command->commitAndWait(&error));
        return target;
    }
    template<typename T>
    std::vector<T> read(const ALMetalRenderTarget& target, unsigned elements_per_pixel)
    {
        const auto& d = target.descriptor();
        std::vector<T> result(d.width * d.height * elements_per_pixel);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(),
            d.width * elements_per_pixel * sizeof(T), &error));
        return result;
    }
};

struct LensCase
{
    const char* name;
    unsigned width, height, out_width, out_height;
    float k1 = 0, k2 = 0, scale = 1;
};
constexpr LensCase lens_cases[] = {
    {"one to one", 48, 32, 48, 32},
    {"barrel", 48, 32, 48, 32, -.2f},
    {"pincushion black corners", 48, 32, 48, 32, .3f, .05f},
    {"fit-scaled moustache", 48, 32, 48, 32, -.3f, .2f, .9f},
    {"resolution divisor", 24, 16, 48, 32},
    {"resolution multiplier", 96, 64, 48, 32},
    {"uneven distorted scale", 40, 27, 48, 32, -.15f},
};

// Original PostChainGL::pattern, authored directly into the actual RGB10_A2
// source. Alpha has no CAS meaning: its native pass deliberately writes zero.
std::vector<std::uint32_t> pattern(unsigned width, unsigned height)
{
    std::vector<std::uint32_t> result(width * height);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
        {
            std::uint32_t packed = 3u << 30u;
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                std::uint32_t hash = (x / ((channel % 3) + 1)) * 73856093u ^
                    (y / ((channel % 2) + 1)) * 19349663u ^ (channel + 1) * 83492791u;
                hash ^= hash >> 13; hash *= 0x5bd1e995u; hash ^= hash >> 15;
                const auto value = static_cast<std::uint32_t>(std::lround((hash & 0xffff) * (1023.0 / 65535.0)));
                packed |= value << (channel * 10);
            }
            result[y * width + x] = packed;
        }
    return result;
}

// GL_MIRRORED_REPEAT's integer texel rule, independent of the native sampler.
int mirror(int coordinate, int size)
{
    int repeat = coordinate < 0 ? -1 - coordinate : coordinate;
    repeat %= 2 * size;
    return repeat < size ? repeat : 2 * size - 1 - repeat;
}
Color sample(const std::vector<std::uint32_t>& pixels, unsigned width, unsigned height,
             double u, double v)
{
    const double sx = u * width - .5, sy = v * height - .5;
    const int x = static_cast<int>(std::floor(sx)), y = static_cast<int>(std::floor(sy));
    const double fx = sx - x, fy = sy - y;
    Color result{};
    for (int oy = 0; oy < 2; ++oy)
        for (int ox = 0; ox < 2; ++ox)
        {
            const auto packed = pixels[mirror(y + oy, height) * width + mirror(x + ox, width)];
            const double weight = (ox ? fx : 1 - fx) * (oy ? fy : 1 - fy);
            for (unsigned c = 0; c < 3; ++c)
                result[c] += ((packed >> (c * 10)) & 1023u) / 1023.0 * weight;
            result[3] += (packed >> 30u) / 3.0 * weight;
        }
    return result;
}
std::vector<Bytes> present(Fixture& f, const ALMetalRenderTarget& source, const LensCase& c)
{
    ALMetalWorldPresentationScene scene;
    scene.source = &source;
    scene.output_width = c.out_width; scene.output_height = c.out_height;
    scene.lens_distortion_amount = c.k1 != 0 || c.k2 != 0 ? 1.f : 0.f;
    scene.lens_distortion_k[0] = c.k1; scene.lens_distortion_k[1] = c.k2;
    scene.lens_distortion_scale = c.scale;
    auto result = f.renderer->renderPresentationPost(scene, &f.error);
    INFO(f.error); REQUIRE(result);
    REQUIRE(result->descriptor().width == c.out_width);
    REQUIRE(result->descriptor().height == c.out_height);
    return f.read<Bytes>(*result, 1);
}
void check_sampling(const std::vector<Bytes>& actual, const std::vector<std::uint32_t>& sharp,
                    const LensCase& c)
{
    REQUIRE(actual.size() == c.out_width * c.out_height);
    REQUIRE(sharp.size() == c.width * c.height);
    const bool identity = c.width == c.out_width && c.height == c.out_height && c.k1 == 0 && c.k2 == 0;
    for (unsigned y = 0; y < c.out_height; ++y)
        for (unsigned x = 0; x < c.out_width; ++x)
        {
            // Original postEffectUtilsF::applyLensDistortion + hard RGB mask.
            const double aspect = double(c.out_width) / c.out_height;
            const double ax = std::max(aspect, 1.0), ay = std::max(1 / aspect, 1.0);
            const double qx = ((x + .5) / c.out_width - .5) * ax;
            const double qy = ((y + .5) / c.out_height - .5) * ay;
            const double r2 = qx * qx + qy * qy;
            const double radial = 1 + r2 * (c.k1 + r2 * c.k2);
            const double u = qx * radial * c.scale / ax + .5;
            const double v = qy * radial * c.scale / ay + .5;
            const bool inside = (c.k1 == 0 && c.k2 == 0) || (u >= 0 && u <= 1 && v >= 0 && v <= 1);
            const auto expected = sample(sharp, c.width, c.height, u, v);
            const auto& pixel = actual[y * c.out_width + x];
            CAPTURE(c.name, x, y, u, v);
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                const int byte = static_cast<int>(std::lround((inside ? expected[channel] : 0) * 255));
                // The physical M1 filtering control bounds half/256 per axis
                // plus half/4095 component precision and half/255 output
                // rounding within two BGRA8 codes. Exact centers allow one.
                CHECK(std::abs(int(pixel[2 - channel]) - byte) <= (identity ? 1 : 2));
            }
            CHECK(pixel[3] == 0); // CAS writes no glow/coverage tag.
        }
}

struct DoFInput
{
    static constexpr unsigned width = 32, height = 24;
    std::unique_ptr<ALMetalRenderTarget> source;
    std::unique_ptr<ALMetalTexture> depth;
    std::vector<std::uint16_t> pixels;
    DoFInput(Fixture& f)
    {
        constexpr std::array<std::uint16_t, 7> levels{0x3000, 0x3400, 0x3800, 0x3c00, 0x4000, 0x4400, 0x4800};
        constexpr std::array<std::uint16_t, 4> glow{0, 0x3400, 0x3800, 0x3c00};
        pixels.resize(width * height * 4);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
            {
                const unsigned at = (y * width + x) * 4;
                pixels[at] = levels[(x * 3 + y) % levels.size()];
                pixels[at + 1] = levels[(x + y * 2 + 1) % levels.size()];
                pixels[at + 2] = levels[(x * 2 + y * 3 + 2) % levels.size()];
                pixels[at + 3] = glow[(x + y) % glow.size()]; // Untagged, partial and full glow.
            }
        source = f.source(width, height, ALMetalPixelFormat::rgba16_float, pixels, 8);
        ALMetalTextureDescriptor d;
        d.width = width; d.height = height; d.format = ALMetalPixelFormat::depth32_float;
        d.storage = ALMetalStorageMode::private_gpu; d.usage = ALMetalTextureUsage::shader_read;
        depth = ALMetalTexture::create(*f.device, d, &f.error); REQUIRE(depth);
        // Both inverse projections below reconstruct the same eye-space z=-2.
        const std::vector<float> values(width * height, .5f);
        auto command = f.device->makeCommandBuffer("Original DoF flat depth", &f.error); REQUIRE(command);
        REQUIRE(depth->upload(*command, 0, 0, {}, values.data(), width * sizeof(float), &f.error));
        REQUIRE(command->commitAndWait(&f.error));
    }
    ALMetalWorldDepthOfFieldScene scene(bool reverse) const
    {
        ALMetalWorldDepthOfFieldScene result;
        result.source = source.get(); result.depth_texture = depth.get();
        result.inverse_projection[0] = result.inverse_projection[5] = 1.f;
        result.inverse_projection[10] = reverse ? 2.f : 1.f;
        result.inverse_projection[14] = reverse ? -3.f : -2.f;
        result.inverse_projection[15] = 1.f;
        result.reverse_z = reverse;
        result.blur_constant = 24.f / 1.414f;
        result.tan_pixel_angle = result.magnification = 1.f;
        result.max_cof = 4.f; result.resolution_scale = .7f;
        return result;
    }
};

void check_blurred_glow(const std::vector<std::uint16_t>& actual,
                       const std::vector<std::uint16_t>& source)
{
    REQUIRE(actual.size() == source.size());
    unsigned defocused = 0;
    for (unsigned pixel = 0; pixel < actual.size() / 4; ++pixel)
    {
        CHECK(actual[pixel * 4 + 3] == source[pixel * 4 + 3]);
        for (unsigned channel = 0; channel < 3; ++channel)
            CHECK((actual[pixel * 4 + channel] & 0x7c00u) != 0x7c00u);
        defocused += actual[pixel * 4] != source[pixel * 4];
    }
    CHECK(defocused > actual.size() / 8);
}

double half_value(std::uint16_t word)
{
    const unsigned exponent = (word >> 10) & 31u, mantissa = word & 1023u;
    const double magnitude = exponent == 0 ? std::ldexp(double(mantissa), -24) :
        std::ldexp(double(mantissa + 1024), int(exponent) - 25);
    return word & 0x8000u ? -magnitude : magnitude;
}
}

TEST_CASE("Native CAS reaches the window through every original source size and lens case",
          "[almetal][postchain-legacy][cas-presentation]")
{
    Fixture f;
    for (const auto& c : lens_cases)
        for (float sharpness : {.4f, 1.f})
        {
            CAPTURE(c.name, sharpness);
            auto source = f.source(c.width, c.height, ALMetalPixelFormat::rgb10a2_unorm,
                                   pattern(c.width, c.height), 4);
            auto cas = f.renderer->renderCAS(*source, sharpness, &f.error);
            INFO(f.error); REQUIRE(cas);
            const auto sharpened = present(f, *cas, c);
            check_sampling(sharpened, f.read<std::uint32_t>(*cas, 1), c);
            const auto plain = present(f, *source, c);
            unsigned changed = 0;
            for (std::size_t pixel = 0; pixel < plain.size(); ++pixel)
                for (unsigned channel = 0; channel < 3; ++channel)
                    changed = std::max(changed, unsigned(std::abs(int(plain[pixel][channel]) - int(sharpened[pixel][channel]))));
            const bool identity = c.width == c.out_width && c.height == c.out_height && c.k1 == 0 && c.k2 == 0;
            const double legacy_tolerance = 1.0 / 1023 + (identity ? 0 : 1.0 / 256) + 1e-5;
            CHECK(changed / 255.0 > 4 * legacy_tolerance);
        }
}

TEST_CASE("Native CAS center samples retain the actual sharpened texel",
          "[almetal][postchain-legacy][cas-presentation]")
{
    Fixture f;
    constexpr LensCase c{"exact centers", 64, 40, 64, 40};
    auto source = f.source(c.width, c.height, ALMetalPixelFormat::rgb10a2_unorm,
                           pattern(c.width, c.height), 4);
    auto cas = f.renderer->renderCAS(*source, .6f, &f.error); REQUIRE(cas);
    check_sampling(present(f, *cas, c), f.read<std::uint32_t>(*cas, 1), c);
}

TEST_CASE("Native DoF gather and combine variants blur HDR color while preserving every glow tag",
          "[almetal][postchain-legacy][dof-invariants]")
{
    Fixture f;
    for (bool reverse : {false, true})
    {
        DoFInput input(f);
        for (bool precise : {false, true})
            for (bool shaped : {false, true})
                for (bool near : {false, true})
                {
                    CAPTURE(reverse, precise, shaped, near);
                    auto scene = input.scene(reverse);
                    scene.focal_distance = -.5f; // Every z=-2 texel lies beyond focus.
                    scene.rgba16_blur = precise; scene.near_blur = near;
                    scene.aperture_blades = shaped ? 6 : 0;
                    auto output = f.renderer->renderDepthOfField(scene, &f.error);
                    INFO(f.error); REQUIRE(output);
                    check_blurred_glow(f.read<std::uint16_t>(*output, 4), input.pixels);
                }
    }
}

TEST_CASE("Native DoF with near blur disabled leaves the entire foreground untouched",
          "[almetal][postchain-legacy][dof-invariants]")
{
    Fixture f;
    for (bool reverse : {false, true})
    {
        DoFInput input(f);
        for (bool precise : {false, true})
            for (bool shaped : {false, true})
            {
                CAPTURE(reverse, precise, shaped);
                auto scene = input.scene(reverse);
                scene.focal_distance = -1000.f; // All z=-2 texels are in front of focus.
                scene.near_blur = false; scene.rgba16_blur = precise;
                scene.aperture_blades = shaped ? 6 : 0;
                auto output = f.renderer->renderDepthOfField(scene, &f.error);
                INFO(f.error); REQUIRE(output);
                CHECK(f.read<std::uint16_t>(*output, 4) == input.pixels);
                // The same foreground must blur when explicitly enabled:
                // this positive control exercises both FRONT_BLUR gathers,
                // rather than accepting an implementation that always copies.
                scene.near_blur = true;
                auto near_output = f.renderer->renderDepthOfField(scene, &f.error);
                INFO(f.error); REQUIRE(near_output);
                check_blurred_glow(f.read<std::uint16_t>(*near_output, 4), input.pixels);
            }
    }
}

TEST_CASE("Native DoF circular and shaped gathers retain the original analytic impulse energy",
          "[almetal][postchain-legacy][dof-invariants][dof-impulse]")
{
    Fixture f;
    for (bool reverse : {false, true})
    {
        DoFInput input(f);
        std::fill(input.pixels.begin(), input.pixels.end(), 0);
        const unsigned center = ((DoFInput::height / 2) * DoFInput::width + DoFInput::width / 2) * 4;
        input.pixels[center] = 0x4400;     // HDR impulse RGB = (4,2,1).
        input.pixels[center + 1] = 0x4000;
        input.pixels[center + 2] = 0x3c00;
        input.pixels[center + 3] = 0x3800; // Independent glow tag = .5.
        input.source = f.source(DoFInput::width, DoFInput::height,
            ALMetalPixelFormat::rgba16_float, input.pixels, 8);
        for (bool precise : {false, true})
            for (bool shaped : {false, true})
                for (bool near : {false, true})
                    for (bool foreground : {false, true})
                    {
                        CAPTURE(reverse, precise, shaped, near, foreground);
                        auto scene = input.scene(reverse);
                        scene.focal_distance = foreground ? -1000.f : -.5f;
                        scene.max_cof = 2.f;
                        scene.resolution_scale = 1.f;
                        scene.rgba16_blur = precise; scene.near_blur = near;
                        scene.aperture_blades = shaped ? 6 : 0;
                        auto output = f.renderer->renderDepthOfField(scene, &f.error);
                        INFO(f.error); REQUIRE(output);
                        REQUIRE(output->descriptor().width == DoFInput::width);
                        REQUIRE(output->descriptor().height == DoFInput::height);
                        const auto actual = f.read<std::uint16_t>(*output, 4);
                        REQUIRE(actual.size() == input.pixels.size());
                        // Original two-ring gather: radius1 has int(3.7)=3
                        // taps; radius2 has int(7.4)=7. No ring tap reaches
                        // the center impulse. Far depth rejects its own outer
                        // ring, so center+3 black taps divide energy by4.
                        // Near circular accepts both rings: denominator11.
                        // With6 straight blades no outer tap at angles
                        // 2+i*2pi/7 is a polygon vertex, so all7 are clipped;
                        // all3 inner taps fit inside its .866 apothem.
                        const unsigned denominator = foreground && !near ? 1 :
                            foreground && near && !shaped ? 11 : 4;
                        for (unsigned channel = 0; channel < 3; ++channel)
                        {
                            const double expected = double(4u >> channel) / denominator;
                            const double value = half_value(actual[center + channel]);
                            if (foreground && !near)
                            {
                                CHECK(actual[center + channel] == input.pixels[center + channel]);
                                continue;
                            }
                            // Fixture acceptance budget for the gather and
                            // combine stores plus fast averaging: two 16F ULPs,
                            // or one compact R11/G11/B10 ULP plus one 16F ULP.
                            // Independent M1 constant/point controls observed
                            // downward stores at float-to-half boundaries.
                            // This is not a universal hardware error bound or
                            // the old sharp-copy architecture's 1e-4 comparison.
                            const int exponent = std::ilogb(expected);
                            const double half_ulp = std::ldexp(1.0, exponent - 10);
                            const double compact_ulp = std::ldexp(1.0, exponent - (channel < 2 ? 6 : 5));
                            const double bound = (precise ? 2 * half_ulp : compact_ulp + half_ulp) + 1e-6;
                            CAPTURE(channel, denominator, value, expected, bound);
                            CHECK(std::abs(value - expected) <= bound);
                        }
                        CHECK(actual[center + 3] == input.pixels[center + 3]);
                    }
    }
}
