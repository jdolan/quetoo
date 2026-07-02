#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float gl_FragDepth [[depth(any)]];
};

struct main0_in
{
    float in_dist [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    out.gl_FragDepth = fast::min(in.in_dist + 0.000600000028498470783233642578125, 1.0);
    return out;
}

