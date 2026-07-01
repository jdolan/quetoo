#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 out_color [[color(0)]];
};

fragment main0_out main0()
{
    main0_out out = {};
    out.out_color = float4(1.0, 0.0, 0.0, 1.0);
    return out;
}

