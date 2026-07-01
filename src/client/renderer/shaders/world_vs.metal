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

struct locals_block
{
    float4x4 model;
};

struct main0_out
{
    float2 out_diffusemap [[user(locn0)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float2 in_diffusemap [[attribute(4)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _27 [[buffer(0)]], constant locals_block& _38 [[buffer(1)]])
{
    main0_out out = {};
    out.out_diffusemap = in.in_diffusemap;
    float4x4 _35 = _27.projection3D * _27.view;
    float4x4 _41 = _35 * _38.model;
    float4 _50 = float4(in.in_position, 1.0);
    float4 _51 = _41 * _50;
    out.gl_Position = _51;
    return out;
}

