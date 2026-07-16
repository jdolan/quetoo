#pragma clang diagnostic ignored "-Wmissing-prototypes"

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
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 in_diffusemap [[user(locn0)]];
    float2 in_next_diffusemap [[user(locn1)]];
    float3 in_color [[user(locn2)]];
    float in_lerp [[user(locn3)]];
    float in_lighting [[user(locn4)]];
    float3 in_diffuse [[user(locn5)]];
};

static inline __attribute__((always_inline))
float calc_depth(thread const float& z, constant uniforms_block& _24)
{
    return (2.0 * _24.depth_range.x) / ((_24.depth_range.y + _24.depth_range.x) - (z * (_24.depth_range.y - _24.depth_range.x)));
}

static inline __attribute__((always_inline))
float soften(constant uniforms_block& _24, texture2d<float> texture_depth_attachment, sampler texture_depth_attachmentSmplr, thread float4& gl_FragCoord)
{
    float4 depth_sample = texture_depth_attachment.sample(texture_depth_attachmentSmplr, (gl_FragCoord.xy / float2(_24.viewport.zw)));
    float param = depth_sample.x;
    float param_1 = gl_FragCoord.z;
    return smoothstep(0.0, 0.001599999959580600261688232421875, fast::clamp(calc_depth(param, _24) - calc_depth(param_1, _24), 0.0, 1.0));
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _24 [[buffer(0)]], texture2d<float> texture_diffusemap [[texture(0)]], texture2d<float> texture_next_diffusemap [[texture(1)]], texture2d<float> texture_depth_attachment [[texture(2)]], sampler texture_diffusemapSmplr [[sampler(0)]], sampler texture_next_diffusemapSmplr [[sampler(1)]], sampler texture_depth_attachmentSmplr [[sampler(2)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float3 texture_color = mix(texture_diffusemap.sample(texture_diffusemapSmplr, in.in_diffusemap).xyz, texture_next_diffusemap.sample(texture_next_diffusemapSmplr, in.in_next_diffusemap).xyz, float3(in.in_lerp));
    float3 color = in.in_color;
    if (in.in_lighting > 0.0)
    {
        color = mix(color, color * in.in_diffuse, float3(in.in_lighting));
    }
    out.out_color = float4((texture_color * color) * soften(_24, texture_depth_attachment, texture_depth_attachmentSmplr, gl_FragCoord), 1.0);
    return out;
}

