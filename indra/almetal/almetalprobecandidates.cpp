/**
 * @file almetalprobecandidates.cpp
 * @brief Conservative CPU producer for per-view probe candidate bounds.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalprobecandidates.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double float_margin = 64.0 * std::numeric_limits<float>::epsilon();

ALMetalProbeCandidateRect fullView(const ALMetalProbeCandidateView& view) noexcept
{
    return {0, 0, view.width, view.height};
}

bool finiteProjection(const ALMetalProbeCandidateView& view) noexcept
{
    return std::all_of(std::begin(view.projection), std::end(view.projection),
        [](float value) { return std::isfinite(value); });
}

ALMetalProbeCandidateRect sphereBounds(const ALMetalProbeCandidateSphere& probe,
    const ALMetalProbeCandidateView& view) noexcept
{
    const auto full = fullView(view);
    if (probe.priority != 0) return full;
    for (float value : probe.center_radius)
        if (!std::isfinite(value)) return full;
    const double radius = probe.center_radius[3];
    // A negative radius still participates through radius*radius in the
    // original shader. Never convert it to an empty influence volume.
    if (radius < 0.0 || radius > std::sqrt(std::numeric_limits<float>::max()) / 8.0)
        return full;

    double lower[3], upper[3];
    const double underflow_guard = 8.0 * std::sqrt(std::numeric_limits<float>::min());
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        const double center = probe.center_radius[axis];
        // Shader subtraction/dot/radius-square use f32 (possibly FMA/FTZ).
        // Inflate in eye space BEFORE projection, including the zero-radius
        // subnormal-square case, rather than relying on screen padding alone.
        const double extent = radius + float_margin * (std::abs(center) + radius)
            + underflow_guard;
        lower[axis] = center - extent;
        upper[axis] = center + extent;
        if (!std::isfinite(lower[axis]) || !std::isfinite(upper[axis]) ||
            std::max(std::abs(lower[axis]), std::abs(upper[axis])) >
                std::numeric_limits<float>::max() / 8.0)
            return full;
    }

    double minimum[2] = {std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()};
    double maximum[2] = {-std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()};
    // A linear-fractional coordinate over a convex box with strictly positive
    // W attains its extrema at vertices. If W crosses/approaches zero, retain
    // the full view instead of attempting near-plane clipping or false cull.
    for (unsigned corner = 0; corner < 8; ++corner)
    {
        const double point[4] = {
            (corner & 1) ? upper[0] : lower[0],
            (corner & 2) ? upper[1] : lower[1],
            (corner & 4) ? upper[2] : lower[2], 1.0};
        double clip[3]{}, errors[3]{};
        constexpr unsigned rows[3] = {0, 1, 3};
        for (unsigned output = 0; output < 3; ++output)
        {
            double magnitude = 0.0;
            for (unsigned column = 0; column < 4; ++column)
            {
                const double term = double(view.projection[column * 4 + rows[output]])
                    * point[column];
                clip[output] += term;
                magnitude += std::abs(term);
            }
            if (!std::isfinite(clip[output]) || !std::isfinite(magnitude) ||
                magnitude > std::numeric_limits<float>::max() / 8.0)
                return full;
            errors[output] = float_margin * magnitude +
                32.0 * std::numeric_limits<float>::min();
        }
        const double w_low = clip[2] - errors[2];
        const double w_high = clip[2] + errors[2];
        if (!(w_low > errors[2]) || !std::isfinite(w_high)) return full;
        for (unsigned axis = 0; axis < 2; ++axis)
        {
            const double a = clip[axis] - errors[axis];
            const double b = clip[axis] + errors[axis];
            const double values[] = {a / w_low, a / w_high, b / w_low, b / w_high};
            for (double value : values)
            {
                if (!std::isfinite(value)) return full;
                const double division_guard = float_margin * std::abs(value);
                minimum[axis] = std::min(minimum[axis], value - division_guard);
                maximum[axis] = std::max(maximum[axis], value + division_guard);
            }
        }
    }
    const auto boundary = [](double ndc, std::uint32_t extent, double guard,
                              bool upper_bound) noexcept
    {
        double pixel = (ndc + 1.0) * (double(extent) * 0.5);
        pixel += upper_bound ? guard : -guard;
        pixel = std::nextafter(pixel, upper_bound
            ? std::numeric_limits<double>::infinity()
            : -std::numeric_limits<double>::infinity());
        pixel = std::clamp(upper_bound ? std::ceil(pixel) : std::floor(pixel),
            0.0, double(extent));
        return static_cast<std::uint32_t>(pixel);
    };
    return {boundary(minimum[0], view.width, view.pixel_guard, false),
            boundary(minimum[1], view.height, view.pixel_guard, false),
            boundary(maximum[0], view.width, view.pixel_guard, true),
            boundary(maximum[1], view.height, view.pixel_guard, true)};
}
}

ALMetalProbeCandidatePacket alMetalBuildProbeCandidates(
    std::span<const ALMetalProbeCandidateSphere> probes,
    float published_count, const ALMetalProbeCandidateView& view) noexcept
{
    ALMetalProbeCandidatePacket packet;
    packet.config[1] = view.width;
    packet.config[2] = view.height;
    // Validate before any float-to-integer conversion or source indexing.
    if (!std::isfinite(published_count) || published_count < 0.f ||
        published_count > float(packet.capacity) ||
        std::floor(published_count) != published_count ||
        !view.width || !view.height || !view.raster_mapping_certified ||
        !std::isfinite(view.pixel_guard) || view.pixel_guard < 2.0)
        return packet;
    const auto count = std::max(std::uint32_t(1), static_cast<std::uint32_t>(published_count));
    if (probes.size() < count) return packet;
    packet.config[0] = count;
    packet.config[3] = 1;
    packet.bounds[0] = fullView(view); // Default is a fallback, not a local sphere.
    const bool projection_finite = finiteProjection(view);
    for (std::uint32_t index = 1; index < count; ++index)
        packet.bounds[index] = projection_finite ? sphereBounds(probes[index], view) : fullView(view);
    return packet;
}

ALMetalProbeBaseMappingCertificate alMetalCertifyBaseProbeMapping(
    std::span<const float, 16> projection,
    std::span<const float, 16> inverse_projection,
    bool reverse_z, std::uint32_t width, std::uint32_t height) noexcept
{
    ALMetalProbeBaseMappingCertificate result;
    if (!width || !height || projection[12] != 0.f ||
        projection[13] != 0.f || projection[15] != 0.f)
        return result;
    for (unsigned i = 0; i < 16; ++i)
        if (!std::isfinite(projection[i]) || !std::isfinite(inverse_projection[i]))
            return result;

    // The source forms q.xy=2*uv-1 and q.z=reverse?depth:2*depth-1.
    // Include its f32 endpoint rounding in the interval, independently of the
    // two-pixel fullscreen interpolation/raster budget below.
    constexpr double input_guard = 8.0 * std::numeric_limits<float>::epsilon();
    const double q_low[4] = {-1.0 - input_guard, -1.0 - input_guard,
        (reverse_z ? 0.0 : -1.0) - input_guard, 1.0};
    const double q_high[4] = {1.0 + input_guard, 1.0 + input_guard,
        1.0 + input_guard, 1.0};
    double q_magnitude[4], inverse_magnitude[4]{}, inverse_error[4]{};
    for (unsigned column = 0; column < 4; ++column)
        q_magnitude[column] = std::max(std::abs(q_low[column]), std::abs(q_high[column]));
    constexpr double ftz_guard = 32.0 * std::numeric_limits<float>::min();
    constexpr double finite_limit = double(std::numeric_limits<float>::max()) / 128.0;
    for (unsigned row = 0; row < 4; ++row)
    {
        for (unsigned column = 0; column < 4; ++column)
            inverse_magnitude[row] += std::abs(double(inverse_projection[column * 4 + row]))
                * q_magnitude[column];
        inverse_error[row] = float_margin * inverse_magnitude[row] + ftz_guard;
        if (!std::isfinite(inverse_magnitude[row]) ||
            inverse_magnitude[row] + inverse_error[row] > finite_limit)
            return result;
    }

    // BASE clamps inverse W then divides XYZ by the SAME positive k. With
    // P_X/Y/W fourth column zero, k cancels algebraically, including at the
    // infinite far plane. Do not require inverseW>0 or silently ignore clamp.
    const double k_min = double(0.000001f);
    const double k_max = std::max(k_min, inverse_magnitude[3] + inverse_error[3]);
    // MSL fast division accuracy is specified for normal denominators through
    // 2^126. Keep a factor-two endpoint margin for reciprocal lowering/FTZ;
    // finite_limit is stricter still. Reject possible division overflow too.
    if (!(k_min >= std::numeric_limits<float>::min()) ||
        !(k_max <= std::ldexp(1.0, 125)))
        return result;
    double homogeneous_error[3]{};
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        const double magnitude = inverse_magnitude[axis] + inverse_error[axis];
        if (magnitude / k_min > finite_limit) return result;
        // Let u=fl(I*q), e=fl(u.xyz/k), h=e*k. Then h-I*q is bounded
        // without a 1/inverseW amplification. MSL Table8.2 permits <=2.5ULP
        // division in the admitted domain; 64*EPS includes reciprocal/FMA
        // lowering, either supported rounding mode, and normal-result error.
        // The absolute term covers permitted input/output subnormal FTZ.
        homogeneous_error[axis] = inverse_error[axis] + float_margin * magnitude
            + ftz_guard * k_max;
        if (!std::isfinite(homogeneous_error[axis])) return result;
    }

    double product[4][4]{}; // P*I, in ordinary row/column notation
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            for (unsigned inner = 0; inner < 4; ++inner)
                product[row][column] += double(projection[inner * 4 + row])
                    * double(inverse_projection[column * 4 + inner]);
    const double scale = product[3][3];
    if (!std::isfinite(scale) || !(scale > ftz_guard)) return result;
    // A strict induced-norm residual below one proves the paired product (and
    // hence both square matrices) nonsingular. Use 1/16 as a fail-open
    // applicability bound; a valid ordinary inverse is much closer to I.
    double residual_norm = 0.0;
    for (unsigned row = 0; row < 4; ++row)
    {
        double row_sum = 0.0;
        for (unsigned column = 0; column < 4; ++column)
            row_sum += std::abs(product[row][column] / scale - (row == column ? 1.0 : 0.0));
        if (!std::isfinite(row_sum)) return result;
        residual_norm = std::max(residual_norm, row_sum);
    }
    if (!(residual_norm < 1.0 / 16.0)) return result;

    double projected_error[3]{};
    constexpr unsigned rows[3] = {0, 1, 3};
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        const unsigned row = rows[axis];
        for (unsigned column = 0; column < 4; ++column)
            projected_error[axis] += std::abs(product[row][column] -
                (column == row ? scale : 0.0)) * q_magnitude[column];
        for (unsigned inner = 0; inner < 3; ++inner)
            projected_error[axis] += std::abs(double(projection[inner * 4 + row]))
                * homogeneous_error[inner];
        if (!std::isfinite(projected_error[axis])) return result;
    }
    const double denominator = scale - projected_error[2];
    if (!(denominator > scale * 0.5)) return result;
    for (unsigned axis = 0; axis < 2; ++axis)
        result.ndc_error[axis] = (projected_error[axis] +
            q_magnitude[axis] * projected_error[2]) / denominator + input_guard;
    const double guard = 2.0 + std::max(result.ndc_error[0] * double(width) * 0.5,
                                       result.ndc_error[1] * double(height) * 0.5);
    if (!std::isfinite(guard) || guard > 16.0) return ALMetalProbeBaseMappingCertificate{};
    result.pixel_guard = std::nextafter(guard, std::numeric_limits<double>::infinity());
    result.minimum_projected_denominator = denominator;
    result.certified = true;
    return result;
}
