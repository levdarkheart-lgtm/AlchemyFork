/** Conservative CPU classification over privately published source bounds. */
#include "almetalworldvisibility.h"
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>

ALMetalWorldCpuVisibilityResult alMetalClassifyWorldBoundsCPU(
    const ALMetalWorldConservativeBounds& bounds,const float* matrix) noexcept
{
    using Result=ALMetalWorldCpuVisibilityResult;
    // Bit tests survive the renderer's fast-math finite assumptions. Reject
    // subnormal inputs too: GPU input flushing could otherwise be amplified
    // by a large finite matrix coefficient before the dot-product error bound.
    const auto ordinary=[](float value) noexcept
    {
        const auto bits=std::bit_cast<std::uint32_t>(value)&0x7fffffffu;
        return bits<0x7f800000u && (bits==0 || bits>=0x00800000u);
    };
    if(!bounds.qualified || !matrix)return Result::unknown;
    for(unsigned i=0;i<16;++i)if(!ordinary(matrix[i]))return Result::unknown;
    for(unsigned axis=0;axis<4;++axis)
        if(!ordinary(bounds.minimum[axis]) || !ordinary(bounds.maximum[axis]) ||
           bounds.minimum[axis]>bounds.maximum[axis])return Result::unknown;

    // Evaluate affine support intervals rather than transforming eight
    // corners. Double is only the interval evaluator: this error still bounds
    // the actual FLOAT products/additions, different contraction orders,
    // native clip-Z conversion and flush-to-zero of intermediate products.
    constexpr double float_error=64.0*0x1p-24;
    constexpr double flush_error=16.0*FLT_MIN;
    double center[3]{},extent[3]{},absolute[3]{};
    for(unsigned axis=0;axis<3;++axis)
    {
        const double lower=bounds.minimum[axis],upper=bounds.maximum[axis];
        center[axis]=(lower+upper)*.5;extent[axis]=(upper-lower)*.5;
        absolute[axis]=std::max(std::abs(lower),std::abs(upper));
    }
    double clip[4]{},radius[4]{},error[4]{};
    for(unsigned component=0;component<4;++component)
    {
        clip[component]=matrix[12+component];
        double magnitude=std::abs(clip[component]);
        for(unsigned axis=0;axis<3;++axis)
        {
            const double coefficient=matrix[axis*4+component];
            clip[component]+=coefficient*center[axis];
            radius[component]+=std::abs(coefficient)*extent[axis];
            magnitude+=std::abs(coefficient)*absolute[axis];
        }
        error[component]=float_error*magnitude+flush_error;
        // Minimum absolute value over the entire affine interval is no
        // greater than at any corner. This can decline more ill-conditioned
        // boxes, but cannot relax the previous cancellation/overflow guard.
        const double minimum_absolute=std::max(0.0,std::abs(clip[component])-radius[component]);
        if(magnitude>static_cast<double>(FLT_MAX)/8.0 ||
           error[component]>std::max(1.0,minimum_absolute)/4096.0)
            return Result::unknown;
    }
    if(clip[3]-radius[3]<=error[3])return Result::unknown;
    bool wholly_inside=true;
    for(unsigned axis=0;axis<3;++axis)
    {
        const double uncertainty=error[axis]+error[3];
        for(double sign:{1.0,-1.0})
        {
            // Combine coefficients BEFORE taking absolute values. Adding
            // independent X/W radii would discard their shared-position
            // correlation and needlessly weaken the clip-plane test.
            const double distance=clip[3]+sign*clip[axis];
            double plane_radius=0;
            for(unsigned coordinate=0;coordinate<3;++coordinate)
                plane_radius+=std::abs(static_cast<double>(matrix[coordinate*4+3])+
                    sign*matrix[coordinate*4+axis])*extent[coordinate];
            if(distance+plane_radius < -uncertainty)return Result::outside;
            wholly_inside=wholly_inside && distance-plane_radius>uncertainty;
        }
    }
    return wholly_inside?Result::inside:Result::partial;
}
