#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct locals_block
{
    float4x4 projection2D;
};

struct vertex_data
{
    float2 diffusemap;
    float4 color;
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
    float2 vertex0_diffusemap [[user(locn0)]];
    float4 vertex0_color [[user(locn1)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float2 in_position [[attribute(0)]];
    float2 in_diffusemap [[attribute(1)]];
    float4 in_color [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant locals_block& _19 [[buffer(1)]])
{
    main0_out out = {};
    vertex_data vertex0 = {};
    out.gl_Position = _19.projection2D * float4(in.in_position, 0.0, 1.0);
    vertex0.diffusemap = in.in_diffusemap;
    vertex0.color = in.in_color;
    out.vertex0_diffusemap = vertex0.diffusemap;
    out.vertex0_color = vertex0.color;
    return out;
}

