#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertexData
{
    float4 color;
};

struct main0_out
{
    float4 outColor [[color(0)]];
};

struct main0_in
{
    float4 vertex0_color [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    vertexData vertex0 = {};
    vertex0.color = in.vertex0_color;
    out.outColor = vertex0.color;
    return out;
}

