/**
 * @file almetal_probe_candidates_test.cpp
 * @brief CPU conservative bounds and fail-open published-probe packet tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalprobecandidates.h"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>

namespace
{
using Sphere = ALMetalProbeCandidateSphere;
using View = ALMetalProbeCandidateView;
using Packet = ALMetalProbeCandidatePacket;

View orthographic()
{
    View view;
    view.width = 1024; view.height = 768;
    view.projection[0] = view.projection[5] = view.projection[10] = view.projection[15] = 1.f;
    view.raster_mapping_certified = true;
    return view;
}

View perspective()
{
    View view = orthographic();
    std::fill_n(view.projection, 16, 0.f);
    view.projection[0] = 1.3f; view.projection[5] = 1.7f;
    view.projection[10] = -1.02f; view.projection[11] = -1.f;
    view.projection[14] = -0.202f;
    return view;
}

bool full(const ALMetalProbeCandidateRect& rect, const View& view)
{
    return rect.x_min == 0 && rect.y_min == 0 &&
        rect.x_max == view.width && rect.y_max == view.height;
}

// Independent point oracle: only point projection, never reproduces AABB
// extrema, eye-space inflation or the producer's ratio/error intervals.
void checkPoint(const Sphere& sphere, const std::array<float, 3>& point,
    const View& view, const ALMetalProbeCandidateRect& rect)
{
    float squared = 0.f;
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        const float delta = point[axis] - sphere.center_radius[axis];
        squared = std::fma(delta, delta, squared);
    }
    if (!(squared <= sphere.center_radius[3] * sphere.center_radius[3])) return;
    float clip[4]{};
    for (unsigned row = 0; row < 4; ++row)
    {
        clip[row] = view.projection[12 + row];
        for (unsigned column = 0; column < 3; ++column)
            clip[row] = std::fma(view.projection[column * 4 + row], point[column], clip[row]);
    }
    if (!(clip[3] > 0.f)) return;
    const double x = (double(clip[0] / clip[3]) + 1.0) * view.width * 0.5;
    const double y = (double(clip[1] / clip[3]) + 1.0) * view.height * 0.5;
    if (!(x >= 0.0 && x < view.width && y >= 0.0 && y < view.height)) return;
    INFO("point " << point[0] << ',' << point[1] << ',' << point[2]);
    CHECK(x >= rect.x_min);
    CHECK(y >= rect.y_min);
    CHECK(x < rect.x_max);
    CHECK(y < rect.y_max);
}
}

TEST_CASE("Probe candidates validate count and retain the default slot", "[almetal][probe-candidates]")
{
    const View view = orthographic();
    std::array<Sphere, 256> probes{};
    auto packet = alMetalBuildProbeCandidates(probes, 0.f, view);
    REQUIRE(packet.config[3] == 1);
    CHECK(packet.config[0] == 1);
    CHECK(full(packet.bounds[0], view));
    packet = alMetalBuildProbeCandidates(probes, 256.f, view);
    CHECK(packet.config[0] == 256);
    CHECK(packet.config[1] == view.width);
    CHECK(packet.config[2] == view.height);
    for (float count : {-1.f, .5f, 257.f, 1.e30f,
             std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        CHECK(alMetalBuildProbeCandidates(probes, count, view).config[3] == 0);
    CHECK(alMetalBuildProbeCandidates({}, 0.f, view).config[3] == 0);
    CHECK(alMetalBuildProbeCandidates(std::span(probes).first(1), 2.f, view).config[3] == 0);
    auto invalid = view; invalid.raster_mapping_certified = false;
    CHECK(alMetalBuildProbeCandidates(probes, 2.f, invalid).config[3] == 0);
    invalid = view; invalid.width = 0;
    CHECK(alMetalBuildProbeCandidates(probes, 2.f, invalid).config[3] == 0);
    invalid = view; invalid.pixel_guard = 1.99;
    CHECK(alMetalBuildProbeCandidates(probes, 2.f, invalid).config[3] == 0);
    invalid = view; invalid.pixel_guard = std::numeric_limits<double>::infinity();
    CHECK(alMetalBuildProbeCandidates(probes, 2.f, invalid).config[3] == 0);
}

TEST_CASE("Probe candidate rectangles preserve stored row orientation and manual fallback", "[almetal][probe-candidates]")
{
    const View view = orthographic();
    std::array<Sphere, 4> probes{};
    probes[1] = {{.25f, .5f, -3.f, .0625f}, 0};
    probes[2] = probes[1]; probes[2].priority = 1;
    probes[3] = probes[1]; probes[3].priority = -1;
    const auto packet = alMetalBuildProbeCandidates(probes, 4.f, view);
    REQUIRE(packet.config[3] == 1);
    CHECK(full(packet.bounds[0], view));
    const auto rect = packet.bounds[1];
    CHECK_FALSE(full(rect, view));
    CHECK(rect.x_min > view.width / 2);
    CHECK(rect.y_min > view.height / 2); // positive source NDC Y -> larger stored row
    CHECK(full(packet.bounds[2], view));
    CHECK(full(packet.bounds[3], view));
    checkPoint(probes[1], {.25f, .5f, -3.f}, view, rect);
    probes[1] = {{3.f, 0.f, -3.f, .01f}, 0};
    const auto outside = alMetalBuildProbeCandidates(probes, 2.f, view).bounds[1];
    CHECK(outside.x_min == view.width);
    CHECK(outside.x_max == view.width); // safe empty screen intersection
}

TEST_CASE("Probe candidates fail open at camera and arithmetic boundaries", "[almetal][probe-candidates]")
{
    std::array<Sphere, 2> probes{};
    auto view = perspective();
    for (const Sphere& sphere : std::array<Sphere, 5>{{
        {{0.f, 0.f, 0.f, 1.f}, 0}, // crosses W=0
        {{0.f, 0.f, 2.f, .1f}, 0}, // entirely behind eye
        {{0.f, 0.f, -3.f, -1.f}, 0}, // shader squares negative radius
        {{0.f, 0.f, -3.f, std::numeric_limits<float>::max()}, 0},
        {{std::numeric_limits<float>::infinity(), 0.f, -3.f, 1.f}, 0}}})
    {
        probes[1] = sphere;
        CHECK(full(alMetalBuildProbeCandidates(probes, 2.f, view).bounds[1], view));
    }
    probes[1] = {{0.f, 0.f, -3.f, .1f}, 0};
    view.projection[0] = std::numeric_limits<float>::quiet_NaN();
    const auto packet = alMetalBuildProbeCandidates(probes, 2.f, view);
    CHECK(packet.config[3] == 1);
    CHECK(full(packet.bounds[1], view));
    view = perspective();
    view.projection[15] = 3.f; // box's W approaches/crosses 0 at z=-3
    view.projection[11] = 1.f;
    CHECK(full(alMetalBuildProbeCandidates(probes, 2.f, view).bounds[1], view));
}

TEST_CASE("Probe candidate padding contains f32 radius boundaries and underflow", "[almetal][probe-candidates]")
{
    std::array<Sphere, 2> probes{};
    const auto view = orthographic();
    for (float radius : {0.f, std::numeric_limits<float>::denorm_min(), 1.e-20f, .1f, 1.f})
    {
        probes[1] = {{.125f, -.25f, -1.f, radius}, 0};
        const auto packet = alMetalBuildProbeCandidates(probes, 2.f, view);
        REQUIRE(packet.config[3] == 1);
        for (unsigned axis = 0; axis < 3; ++axis)
            for (float direction : {-1.f, 1.f})
            {
                std::array<float, 3> point{.125f, -.25f, -1.f};
                point[axis] += direction * radius;
                checkPoint(probes[1], point, view, packet.bounds[1]);
                point[axis] = std::nextafter(point[axis], direction > 0
                    ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity());
                checkPoint(probes[1], point, view, packet.bounds[1]);
            }
    }
    // A zero-radius dot can flush tiny nonzero deltas to zero. A screen
    // magnification makes that domain visible; retain rather than false-cull.
    auto magnified = view;
    magnified.projection[0] = magnified.projection[5] = 1.e20f;
    probes[1] = {{0.f, 0.f, 0.f, 0.f}, 0};
    CHECK(full(alMetalBuildProbeCandidates(probes, 2.f, magnified).bounds[1], magnified));
}

TEST_CASE("Probe candidates contain randomized accepted projected sphere points", "[almetal][probe-candidates]")
{
    std::mt19937 rng(0x91b379u);
    std::uniform_real_distribution<float> unit(-1.f, 1.f), positive(0.f, 1.f);
    std::array<Sphere, 2> probes{};
    unsigned bounded = 0;
    for (unsigned sample = 0; sample < 500; ++sample)
    {
        auto view = sample % 3 ? perspective() : orthographic();
        view.width = 17 + rng() % 4000;
        view.height = 13 + rng() % 2400;
        view.projection[4] = unit(rng) * .3f; // skew, off-axis/asymmetric projection
        view.projection[8] = unit(rng) * .2f;
        view.projection[9] = unit(rng) * .2f;
        view.projection[12] = unit(rng) * .1f;
        view.projection[13] = unit(rng) * .1f;
        if (sample % 3)
        {
            view.projection[3] = unit(rng) * .05f;
            view.projection[7] = unit(rng) * .05f;
        }
        probes[1] = {{unit(rng) * 2.f, unit(rng) * 2.f,
                      -2.f - positive(rng) * 20.f, .001f + positive(rng)}, 0};
        const auto packet = alMetalBuildProbeCandidates(probes, 2.f, view);
        REQUIRE(packet.config[3] == 1);
        const auto& rect = packet.bounds[1];
        if (!full(rect, view)) ++bounded;
        for (unsigned n = 0; n < 80; ++n)
        {
            std::array<float, 3> direction{unit(rng), unit(rng), unit(rng)};
            const double length = std::sqrt(double(direction[0]) * direction[0] +
                double(direction[1]) * direction[1] + double(direction[2]) * direction[2]);
            if (!(length > 0.0)) continue;
            const float scale = probes[1].center_radius[3] *
                (n % 2 ? 1.f : positive(rng)) / float(length);
            std::array<float, 3> point;
            for (unsigned axis = 0; axis < 3; ++axis)
                point[axis] = probes[1].center_radius[axis] + direction[axis] * scale;
            checkPoint(probes[1], point, view, rect);
        }
    }
    CHECK(bounded > 450); // do not let an always-full implementation pass this oracle
}

namespace
{
// Independent test-only inverse, using pivoted elimination rather than any
// certifier intermediate or residual helper.
std::array<float, 16> inverseForTest(const float* projection)
{
    double rows[4][8]{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
        {
            rows[r][c] = projection[c * 4 + r];
            rows[r][c + 4] = r == c ? 1.0 : 0.0;
        }
    for (unsigned column = 0; column < 4; ++column)
    {
        unsigned pivot = column;
        for (unsigned r = column + 1; r < 4; ++r)
            if (std::abs(rows[r][column]) > std::abs(rows[pivot][column])) pivot = r;
        REQUIRE(std::abs(rows[pivot][column]) > 0.0);
        for (unsigned c = 0; c < 8; ++c) std::swap(rows[pivot][c], rows[column][c]);
        const double divisor = rows[column][column];
        for (double& value : rows[column]) value /= divisor;
        for (unsigned r = 0; r < 4; ++r)
            if (r != column)
            {
                const double multiplier = rows[r][column];
                for (unsigned c = 0; c < 8; ++c)
                    rows[r][c] -= multiplier * rows[column][c];
            }
    }
    std::array<float, 16> inverse{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c) inverse[c * 4 + r] = float(rows[r][c + 4]);
    return inverse;
}

View infinitePerspective(bool reverse)
{
    auto view = perspective();
    view.projection[10] = reverse ? 0.f : -1.f;
    view.projection[14] = reverse ? .1f : -.2f;
    return view;
}
}

TEST_CASE("BASE mapping certificate supports finite and infinite perspective depth", "[almetal][probe-candidates]")
{
    for (bool reverse : {false, true})
        for (bool infinite : {false, true})
        {
            auto view = infinite ? infinitePerspective(reverse) : perspective();
            if (reverse && !infinite)
            {
                const float near = .1f, far = 1000.f;
                view.projection[10] = near / (far - near);
                view.projection[14] = far * near / (far - near);
            }
            const auto inverse = inverseForTest(view.projection);
            const auto certificate = alMetalCertifyBaseProbeMapping(view.projection, inverse,
                reverse, view.width, view.height);
            REQUIRE(certificate.certified);
            CHECK(certificate.pixel_guard >= 2.0);
            CHECK(certificate.pixel_guard <= 16.0);
            CHECK(certificate.minimum_projected_denominator > 0.0);
            // Explicit infinite far plane is not rejected for inverseW=0:
            // the source clamp's common positive XYZ divisor cancels.
            if (infinite)
            {
                const float far_z = reverse ? 0.f : 1.f;
                CHECK(std::abs(inverse[11] * far_z + inverse[15]) < 1.e-6f);
            }
        }
}

TEST_CASE("BASE mapping certificate declines unsupported or unsafe inputs", "[almetal][probe-candidates]")
{
    auto view = perspective();
    const auto inverse = inverseForTest(view.projection);
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, inverse, false, 0, 768).certified);
    auto changed = inverse; changed[0] *= 2.f;
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, changed, false, 1024, 768).certified);
    changed = inverse; changed[0] = std::numeric_limits<float>::infinity();
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, changed, false, 1024, 768).certified);
    changed = inverse; std::fill(changed.begin(), changed.end(), 0.f);
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, changed, false, 1024, 768).certified);
    changed = inverse; changed[0] = std::numeric_limits<float>::max();
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, changed, false, 1024, 768).certified);
    for (unsigned offset : {12u, 13u, 15u})
    {
        auto affine = view; affine.projection[offset] = std::numeric_limits<float>::denorm_min();
        CHECK_FALSE(alMetalCertifyBaseProbeMapping(affine.projection, inverse, false, 1024, 768).certified);
    }
    const auto ortho = orthographic();
    const auto ortho_inverse = inverseForTest(ortho.projection);
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(ortho.projection, ortho_inverse, false, 1024, 768).certified);
    // A small but intentional mismatched XY transform needs a guard beyond
    // the supported 16-pixel envelope and must remain generic.
    changed = inverse; changed[0] *= 1.01f;
    CHECK_FALSE(alMetalCertifyBaseProbeMapping(view.projection, changed, false, 8192, 8192).certified);
}

TEST_CASE("BASE certificate guard contains independent f32 reconstructed raster positions", "[almetal][probe-candidates]")
{
    std::mt19937 rng(0x2ac913u);
    std::uniform_real_distribution<float> unit(-1.f, 1.f), positive(0.f, 1.f);
    unsigned certified = 0, checked = 0;
    for (unsigned sample = 0; sample < 100; ++sample)
    {
        const bool reverse = sample % 2 != 0;
        auto view = infinitePerspective(reverse);
        view.width = 17 + rng() % 4000; view.height = 13 + rng() % 2400;
        view.projection[0] = .2f + positive(rng) * 4.f;
        view.projection[5] = .2f + positive(rng) * 4.f;
        view.projection[4] = unit(rng) * .2f;
        view.projection[8] = unit(rng) * .5f;
        view.projection[9] = unit(rng) * .5f;
        const auto inverse = inverseForTest(view.projection);
        const auto certificate = alMetalCertifyBaseProbeMapping(view.projection, inverse,
            reverse, view.width, view.height);
        REQUIRE(certificate.certified); ++certified;
        for (unsigned point = 0; point < 100; ++point)
        {
            const double pixel_x = double(rng() % view.width) + .5;
            const double pixel_y = double(rng() % view.height) + .5;
            const float uv_x = float(pixel_x / view.width), uv_y = float(pixel_y / view.height);
            const float depth = point == 0 ? (reverse ? 0.f : 1.f) : positive(rng);
            const float clip[4] = {uv_x * 2.f - 1.f, uv_y * 2.f - 1.f,
                reverse ? depth : depth * 2.f - 1.f, 1.f};
            float u[4]{};
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    u[row] = std::fma(inverse[column * 4 + row], clip[column], u[row]);
            const float divisor = std::max(u[3], .000001f);
            float eye[3];
            for (unsigned axis = 0; axis < 3; ++axis) eye[axis] = u[axis] / divisor;
            // Double projection of the actual f32 eye position is the
            // mathematical coordinate bounded by the sphere packet producer.
            double projected[3]{};
            constexpr unsigned rows[3] = {0, 1, 3};
            for (unsigned axis = 0; axis < 3; ++axis)
                for (unsigned column = 0; column < 3; ++column)
                    projected[axis] += double(view.projection[column * 4 + rows[axis]]) * eye[column];
            REQUIRE(projected[2] > 0.0);
            const double x = (projected[0] / projected[2] + 1.0) * view.width * .5;
            const double y = (projected[1] / projected[2] + 1.0) * view.height * .5;
            CHECK(std::abs(x - pixel_x) <= certificate.pixel_guard);
            CHECK(std::abs(y - pixel_y) <= certificate.pixel_guard);
            ++checked;
        }
    }
    CHECK(certified == 100);
    CHECK(checked == 10000);
}
