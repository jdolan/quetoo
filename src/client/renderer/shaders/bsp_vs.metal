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
};

struct locals_block
{
    float4x4 model;
};

struct common_vertex_t
{
    float3 model_position;
    float3 model_normal;
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 diffusemap;
    float3 voxel;
    float4 color;
    float3 ambient;
    float3 diffuse;
    float caustics;
};

constant spvUnsafeArray<float, 8> _193 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float3 vertex0_model_position [[user(locn0)]];
    float3 vertex0_model_normal [[user(locn1)]];
    float3 vertex0_position [[user(locn2)]];
    float3 vertex0_normal [[user(locn3)]];
    float3 vertex0_tangent [[user(locn4)]];
    float3 vertex0_bitangent [[user(locn5)]];
    float2 vertex0_diffusemap [[user(locn6)]];
    float3 vertex0_voxel [[user(locn7)]];
    float4 vertex0_color [[user(locn8)]];
    float3 vertex0_ambient [[user(locn9)]];
    float3 vertex0_diffuse [[user(locn10)]];
    float vertex0_caustics [[user(locn11)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    float3 in_normal [[attribute(1)]];
    float3 in_tangent [[attribute(2)]];
    float3 in_bitangent [[attribute(3)]];
    float2 in_diffusemap [[attribute(4)]];
    float4 in_color [[attribute(5)]];
};

static inline __attribute__((always_inline))
float3 voxel_uvw(thread const float3& position, constant uniforms_block& _22)
{
    return (position - _22.voxels.mins.xyz) / (_22.voxels.maxs.xyz - _22.voxels.mins.xyz);
}

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _22 [[buffer(0)]], constant locals_block& _49 [[buffer(1)]])
{
    main0_out out = {};
    common_vertex_t vertex0 = {};
    float4x4 view_model = _22.view * _49.model;
    float4 position = float4(in.in_position, 1.0);
    float4 normal = float4(in.in_normal, 0.0);
    float4 tangent = float4(in.in_tangent, 0.0);
    float4 bitangent = float4(in.in_bitangent, 0.0);
    vertex0.model_position = float3((_49.model * position).xyz);
    vertex0.model_normal = fast::normalize(float3((_49.model * normal).xyz));
    vertex0.position = float3((view_model * position).xyz);
    vertex0.normal = fast::normalize(float3((view_model * normal).xyz));
    vertex0.tangent = fast::normalize(float3((view_model * tangent).xyz));
    vertex0.bitangent = fast::normalize(float3((view_model * bitangent).xyz));
    vertex0.diffusemap = in.in_diffusemap;
    float3 param = float3((_49.model * position).xyz);
    vertex0.voxel = voxel_uvw(param, _22);
    vertex0.color = in.in_color;
    float4x4 _178 = _22.projection3D * view_model;
    float4 _180 = _178 * position;
    out.gl_Position = _180;
    out.vertex0_model_position = vertex0.model_position;
    out.vertex0_model_normal = vertex0.model_normal;
    out.vertex0_position = vertex0.position;
    out.vertex0_normal = vertex0.normal;
    out.vertex0_tangent = vertex0.tangent;
    out.vertex0_bitangent = vertex0.bitangent;
    out.vertex0_diffusemap = vertex0.diffusemap;
    out.vertex0_voxel = vertex0.voxel;
    out.vertex0_color = vertex0.color;
    out.vertex0_ambient = vertex0.ambient;
    out.vertex0_diffuse = vertex0.diffuse;
    out.vertex0_caustics = vertex0.caustics;
    return out;
}

