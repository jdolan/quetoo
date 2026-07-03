#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertex_data
{
    float2 texcoord;
};

struct main0_out
{
    float gl_FragDepth [[depth(any)]];
};

fragment main0_out main0(texture2d_ms<float> texture_depth_ms [[texture(0)]], sampler texture_depth_msSmplr [[sampler(0)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    out.gl_FragDepth = texture_depth_ms.read(uint2(int2(gl_FragCoord.xy)), 0).x;
    return out;
}

