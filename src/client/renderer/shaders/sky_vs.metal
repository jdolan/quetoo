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
    float3 cubemap_coord [[user(locn0)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _28 [[buffer(0)]])
{
    main0_out out = {};
    float4 position = float4(in.in_position, 1.0);
    out.cubemap_coord = float3((_28.sky_projection * position).xyz);
    float4x4 _52 = _28.projection3D * _28.view;
    float4 _54 = _52 * position;
    out.gl_Position = _54;
    return out;
}

