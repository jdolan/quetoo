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

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float3 cubemap_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texturecube<float> texture_sky [[texture(0)]], sampler texture_skySmplr [[sampler(0)]])
{
    main0_out out = {};
    out.out_color = texture_sky.sample(texture_skySmplr, fast::normalize(in.cubemap_coord));
    return out;
}

