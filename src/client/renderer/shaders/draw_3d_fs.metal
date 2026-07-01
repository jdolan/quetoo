#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertex_data
{
    float4 color;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float4 vertex0_color [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    vertex_data vertex0 = {};
    vertex0.color = in.vertex0_color;
    out.out_color = vertex0.color;
    return out;
}

