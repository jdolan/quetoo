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
    int debug_voxel_lights;
};

struct main0_out
{
    float3 out_model_position [[user(locn0)]];
    float3 out_model_normal [[user(locn1)]];
    float2 out_texcoord [[user(locn2)]];
    float4 out_color [[user(locn3)]];
    uint out_time [[user(locn4)]];
    uint out_lifetime [[user(locn5)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float3 in_normal [[attribute(1)]];
    float2 in_texcoord [[attribute(2)]];
    float4 in_color [[attribute(3)]];
    uint in_time [[attribute(4)]];
    uint in_lifetime [[attribute(5)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _81 [[buffer(0)]], constant locals_block& _24 [[buffer(1)]])
{
    main0_out out = {};
    float4 position = float4(in.in_position, 1.0);
    out.out_model_position = float3((_24.model * position).xyz);
    out.out_model_normal = fast::normalize(float3((_24.model * float4(in.in_normal, 0.0)).xyz));
    out.out_texcoord = in.in_texcoord;
    out.out_color = in.in_color;
    out.out_time = in.in_time;
    out.out_lifetime = in.in_lifetime;
    float4x4 _88 = _81.projection3D * _81.view;
    float4x4 _91 = _88 * _24.model;
    float4 _93 = _91 * position;
    out.gl_Position = _93;
    return out;
}

