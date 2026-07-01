#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 in_diffusemap [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d_array<float> texture_material [[texture(0)]], sampler texture_materialSmplr [[sampler(0)]])
{
    main0_out out = {};
    float3 _23 = float3(in.in_diffusemap, 0.0);
    out.out_color = float4(texture_material.sample(texture_materialSmplr, _23.xy, uint(rint(_23.z))).xyz, 1.0);
    return out;
}

