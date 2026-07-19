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
    float in_light_radius [[user(locn1), flat]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float dist = length(in.in_position) / in.in_light_radius;
    float bias0 = fast::clamp(dist * 0.07999999821186065673828125, 1.0 / in.in_light_radius, 8.0 / in.in_light_radius);
    out.gl_FragDepth = fast::min(dist + bias0, 1.0);
    return out;
}

