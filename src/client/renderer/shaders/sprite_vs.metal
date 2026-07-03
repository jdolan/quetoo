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
};

struct main0_out
{
    float2 out_diffusemap [[user(locn0)]];
    float2 out_next_diffusemap [[user(locn1)]];
    float3 out_color [[user(locn2)]];
    float out_lerp [[user(locn3)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float2 in_diffusemap [[attribute(1)]];
    float2 in_next_diffusemap [[attribute(2)]];
    float4 in_color [[attribute(3)]];
    float2 in_lerp_lighting [[attribute(4)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _44 [[buffer(0)]])
{
    main0_out out = {};
    out.out_diffusemap = in.in_diffusemap;
    out.out_next_diffusemap = in.in_next_diffusemap;
    out.out_color = in.in_color.xyz;
    out.out_lerp = in.in_lerp_lighting.x;
    float4x4 _52 = _44.projection3D * _44.view;
    float4 _60 = float4(in.in_position, 1.0);
    float4 _61 = _52 * _60;
    out.gl_Position = _61;
    return out;
}

