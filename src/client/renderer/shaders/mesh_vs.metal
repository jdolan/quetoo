#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct locals_block
{
    float4x4 model;
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
    int wireframe;
    int num_lights;
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
    float3 in_next_position [[attribute(5)]];
    float3 in_next_normal [[attribute(6)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _84 [[buffer(0)]], constant locals_block& _19 [[buffer(1)]])
{
    main0_out out = {};
    float4 position = float4(mix(in.in_position, in.in_next_position, float3(_19.lerp)), 1.0);
    float4 normal = float4(mix(in.in_normal, in.in_next_normal, float3(_19.lerp)), 0.0);
    out.out_diffusemap = in.in_diffusemap;
    out.out_model_position = float3((_19.model * position).xyz);
    out.out_model_normal = fast::normalize(float3((_19.model * normal).xyz));
    float4x4 _90 = _84.projection3D * _84.view;
    float4x4 _93 = _90 * _19.model;
    float4 _95 = _93 * position;
    out.gl_Position = _95;
    return out;
}

