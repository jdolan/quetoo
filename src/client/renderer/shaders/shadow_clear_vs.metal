#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    float2 position = float2(float((int(gl_VertexIndex) << 1) & 2), float(int(gl_VertexIndex) & 2));
    out.gl_Position = float4((position * 2.0) - float2(1.0), 1.0, 1.0);
    return out;
}

