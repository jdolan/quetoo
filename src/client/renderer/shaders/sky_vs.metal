#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
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

struct material_block
{
    float4 color;
    float2 st_origin;
    float2 stretch;
    float2 scroll;
    float2 scale;
    float2 terrain;
    float2 warp;
    int surface;
    float alpha_test;
    float roughness;
    float hardness;
    float specularity;
    float parallax;
    float shadow;
    int flags;
    float pulse;
    float drift;
    float rotate;
    float dirtmap;
    float lighting;
    float emissive;
    float lerp;
    float shell;
};

constant spvUnsafeArray<float, 8> _118 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float3 cubemap_coord [[user(locn0)]];
    float4 stage_color [[user(locn1)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _28 [[buffer(0)]], constant material_block& material [[buffer(1)]])
{
    main0_out out = {};
    float4 position = float4(in.in_position, 1.0);
    out.cubemap_coord = float3((_28.sky_projection * position).xyz);
    out.stage_color = float4(1.0);
    if ((material.flags & 4) == 4)
    {
        out.stage_color = material.color;
    }
    if ((material.flags & 512) == 512)
    {
        out.stage_color.w *= ((sin((((float(_28.ticks) * 0.001000000047497451305389404296875) + material.drift) * material.pulse) * 3.1415927410125732421875) + 1.0) * 0.5);
    }
    float4x4 _105 = _28.projection3D * _28.view;
    float4 _107 = _105 * position;
    out.gl_Position = _107;
    return out;
}

