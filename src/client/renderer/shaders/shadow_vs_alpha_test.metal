#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct locals_block
{
    float4x4 model;
    float4x4 light_view;
    float4 light_origin;
    float lerp;
};

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
};

struct main0_out
{
    float3 out_position [[user(locn0)]];
    float out_light_radius [[user(locn1)]];
    float2 out_diffusemap [[user(locn2)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float3 in_next_position [[attribute(1)]];
    float2 in_diffusemap [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _71 [[buffer(0)]], constant locals_block& _14 [[buffer(1)]])
{
    main0_out out = {};
    float3 position = float3((_14.model * float4(mix(in.in_position, in.in_next_position, float3(_14.lerp)), 1.0)).xyz) - _14.light_origin.xyz;
    out.out_position = position;
    out.out_light_radius = _14.light_origin.w;
    out.out_diffusemap = in.in_diffusemap;
    float4x4 _78 = _71.light_projection * _14.light_view;
    float4 _83 = float4(position, 1.0);
    float4 _84 = _78 * _83;
    out.gl_Position = _84;
    return out;
}

