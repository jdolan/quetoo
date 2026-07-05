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

struct vertex_data
{
    float4 color;
};

struct main0_out
{
    float4 vertex0_color [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float4 in_color [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _22 [[buffer(0)]])
{
    main0_out out = {};
    vertex_data vertex0 = {};
    out.gl_Position = (_22.projection3D * _22.view) * float4(in.in_position, 1.0);
    vertex0.color = in.in_color;
    out.vertex0_color = vertex0.color;
    return out;
}

