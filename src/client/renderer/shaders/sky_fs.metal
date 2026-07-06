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

constant spvUnsafeArray<float, 8> _268 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float3 cubemap_coord [[user(locn0)]];
    float4 stage_color [[user(locn1)]];
};

static inline __attribute__((always_inline))
float2 direction_to_azimuthal_equidistant(thread const float3& direction)
{
    float theta = acos(fast::clamp(direction.z, -1.0, 1.0));
    float phi = precise::atan2(direction.y, direction.x);
    float r = theta / 3.1415927410125732421875;
    return float2(0.5) + ((float2(cos(phi), sin(phi)) * r) * 0.5);
}

static inline __attribute__((always_inline))
float2 transform_stage_uv(thread float2& uv, constant material_block& material, constant uniforms_block& _113)
{
    if ((material.flags & 128) == 128)
    {
        float2 center = uv - float2(0.5);
        float theta = ((float(_113.ticks) * 0.001000000047497451305389404296875) * material.rotate) * 6.283185482025146484375;
        center = float2x2(float2(cos(theta), -sin(theta)), float2(sin(theta), cos(theta))) * center;
        uv = center + float2(0.5);
    }
    if ((material.flags & 8) == 8)
    {
        uv.x += ((material.scroll.x * float(_113.ticks)) * 0.001000000047497451305389404296875);
    }
    if ((material.flags & 16) == 16)
    {
        uv.y += ((material.scroll.y * float(_113.ticks)) * 0.001000000047497451305389404296875);
    }
    if ((material.flags & 96) != 0)
    {
        float2 center_1 = uv - float2(0.5);
        float _198;
        if ((material.flags & 32) == 32)
        {
            _198 = material.scale.x;
        }
        else
        {
            _198 = 1.0;
        }
        float _211;
        if ((material.flags & 64) == 64)
        {
            _211 = material.scale.y;
        }
        else
        {
            _211 = 1.0;
        }
        center_1 /= float2(_198, _211);
        uv = center_1 + float2(0.5);
    }
    return uv;
}

static inline __attribute__((always_inline))
float4 sample_material_stage(thread const float2& texcoord, constant material_block& material, texture2d<float> texture_stage, sampler texture_stageSmplr, texture2d<float> texture_stage_next, sampler texture_stage_nextSmplr)
{
    if ((material.flags & 2048) == 2048)
    {
        return mix(texture_stage.sample(texture_stageSmplr, texcoord), texture_stage_next.sample(texture_stage_nextSmplr, texcoord), float4(material.lerp));
    }
    return texture_stage.sample(texture_stageSmplr, texcoord);
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _113 [[buffer(0)]], constant material_block& material [[buffer(1)]], texturecube<float> texture_sky [[texture(0)]], texture2d<float> texture_stage [[texture(1)]], texture2d<float> texture_stage_next [[texture(2)]], sampler texture_skySmplr [[sampler(0)]], sampler texture_stageSmplr [[sampler(1)]], sampler texture_stage_nextSmplr [[sampler(2)]])
{
    main0_out out = {};
    if (material.flags == 0)
    {
        out.out_color = texture_sky.sample(texture_skySmplr, fast::normalize(in.cubemap_coord));
    }
    else
    {
        float3 param = fast::normalize(in.cubemap_coord);
        float2 st = direction_to_azimuthal_equidistant(param);
        float2 param_1 = st;
        float2 _252 = transform_stage_uv(param_1, material, _113);
        st = _252;
        float2 param_2 = st;
        out.out_color = sample_material_stage(param_2, material, texture_stage, texture_stageSmplr, texture_stage_next, texture_stage_nextSmplr) * in.stage_color;
    }
    return out;
}

