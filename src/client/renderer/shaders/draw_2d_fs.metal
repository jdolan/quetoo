#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct vertex_data
{
    float2 diffusemap;
    float4 color;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 vertex0_diffusemap [[user(locn0)]];
    float4 vertex0_color [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> texture_diffusemap [[texture(0)]], sampler texture_diffusemapSmplr [[sampler(0)]])
{
    main0_out out = {};
    vertex_data vertex0 = {};
    vertex0.diffusemap = in.vertex0_diffusemap;
    vertex0.color = in.vertex0_color;
    out.out_color = vertex0.color * texture_diffusemap.sample(texture_diffusemapSmplr, vertex0.diffusemap);
    return out;
}

