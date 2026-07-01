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
};

struct locals_block
{
    float4x4 model;
};

struct main0_out
{
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _17 [[buffer(0)]], constant locals_block& _24 [[buffer(1)]])
{
    main0_out out = {};
    float4x4 view_model = _17.view * _24.model;
    float4x4 _39 = _17.projection3D * view_model;
    float4 _48 = float4(in.in_position, 1.0);
    float4 _49 = _39 * _48;
    out.gl_Position = _49;
    return out;
}

