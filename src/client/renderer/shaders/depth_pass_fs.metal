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
    float4 clip_plane;
};

struct main0_in
{
    float3 model_position [[user(locn0)]];
};

fragment void main0(main0_in in [[stage_in]], constant uniforms_block& _16 [[buffer(0)]])
{
    bool _27 = any(_16.clip_plane.xyz != float3(0.0));
    bool _43;
    if (_27)
    {
        _43 = dot(in.model_position, _16.clip_plane.xyz) < _16.clip_plane.w;
    }
    else
    {
        _43 = _27;
    }
    if (_43)
    {
        discard_fragment();
    }
}

