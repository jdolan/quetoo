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
    float gl_FragDepth [[depth(any)]];
};

struct main0_in
{
    float3 in_position [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _22 [[buffer(0)]])
{
    main0_out out = {};
    float dist = length(in.in_position) / _22.depth_range.y;
    float bias0 = fast::clamp(dist * 0.00999999977648258209228515625, 1.0 / _22.depth_range.y, 8.0 / _22.depth_range.y);
    out.gl_FragDepth = fast::min(dist + bias0, 1.0);
    return out;
}

