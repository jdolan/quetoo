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
};

struct main0_out
{
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_corner [[attribute(0)]];
    float3 in_mins [[attribute(1)]];
    float3 in_maxs [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _33 [[buffer(0)]])
{
    main0_out out = {};
    float3 position = mix(in.in_mins, in.in_maxs, in.in_corner);
    float4x4 _41 = _33.projection3D * _33.view;
    float4 _47 = float4(position, 1.0);
    float4 _48 = _41 * _47;
    out.gl_Position = _48;
    return out;
}

