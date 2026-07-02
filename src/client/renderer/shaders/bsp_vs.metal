#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct locals_block
{
    float4x4 model;
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
    int wireframe;
    int num_lights;
    int num_bsp_lights;
};

struct main0_out
{
    float2 out_diffusemap [[user(locn0)]];
    float3 out_model_position [[user(locn1)]];
    float3 out_model_normal [[user(locn2)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float3 in_normal [[attribute(1)]];
    float2 in_diffusemap [[attribute(4)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _68 [[buffer(0)]], constant locals_block& _30 [[buffer(1)]])
{
    main0_out out = {};
    float4 position = float4(in.in_position, 1.0);
    out.out_diffusemap = in.in_diffusemap;
    out.out_model_position = float3((_30.model * position).xyz);
    out.out_model_normal = fast::normalize(float3((_30.model * float4(in.in_normal, 0.0)).xyz));
    float4x4 _75 = _68.projection3D * _68.view;
    float4x4 _78 = _75 * _30.model;
    float4 _80 = _78 * position;
    out.gl_Position = _80;
    return out;
}

