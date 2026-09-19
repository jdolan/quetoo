#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertexData
{
    float2 texcoord;
};

struct main0_out
{
    float2 vertex0_texcoord [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float2 inPosition [[attribute(0)]];
    float2 inTexcoord [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    vertexData vertex0 = {};
    out.gl_Position = float4(in.inPosition, 0.0, 1.0);
    vertex0.texcoord = in.inTexcoord;
    out.vertex0_texcoord = vertex0.texcoord;
    return out;
}

