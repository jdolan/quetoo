#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct voxels_t
{
    float4 mins;
    float4 maxs;
    float4 view_coordinate;
    float4 size;
};

struct uniforms_block
{
    int4 viewport;
    float4x4 projection3D;
    float4x4 view;
    float4x4 sky_projection;
    float4x4 light_projection;
    voxels_t voxels;
    float2 depth_range;
    int view_type;
    int ticks;
    float ambient;
    float modulate;
    float saturation;
    float caustics;
    float ambient_occlusion;
    float lighting_distance;
    int editor;
    int developer;
    int wireframe;
    int num_lights;
    int num_bsp_lights;
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 in_diffusemap [[user(locn0)]];
    float2 in_next_diffusemap [[user(locn1)]];
    float3 in_color [[user(locn2)]];
    float in_lerp [[user(locn3)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> texture_diffusemap [[texture(0)]], texture2d<float> texture_next_diffusemap [[texture(1)]], sampler texture_diffusemapSmplr [[sampler(0)]], sampler texture_next_diffusemapSmplr [[sampler(1)]])
{
    main0_out out = {};
    float3 texture_color = mix(texture_diffusemap.sample(texture_diffusemapSmplr, in.in_diffusemap).xyz, texture_next_diffusemap.sample(texture_next_diffusemapSmplr, in.in_next_diffusemap).xyz, float3(in.in_lerp));
    out.out_color = float4(texture_color * in.in_color, 1.0);
    return out;
}

