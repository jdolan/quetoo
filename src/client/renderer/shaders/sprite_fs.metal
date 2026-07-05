#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct light_t
{
    float4 origin;
    float4 color;
    float4 shadow;
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

struct light_t_1
{
    float4 origin;
    float4 color;
    float4 shadow;
};

struct lights_block
{
    int num_lights;
    int num_bsp_lights;
    light_t_1 lights[1];
};

struct voxel_light_indices_block
{
    int voxel_light_indices[1];
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float3 in_position [[user(locn0)]];
    float2 in_diffusemap [[user(locn1)]];
    float2 in_next_diffusemap [[user(locn2)]];
    float3 in_color [[user(locn3)]];
    float in_lerp [[user(locn4)]];
    float in_lighting [[user(locn5)]];
};

static inline __attribute__((always_inline))
int3 sprite_voxel_xyz(thread const float3& position, constant uniforms_block& _50)
{
    float3 pos = position - _50.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_50.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _50)
{
    float3 color = (l.color.xyz * l.color.w) * _50.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_50.saturation));
}

static inline __attribute__((always_inline))
float3 sprite_lighting(constant uniforms_block& _50, thread float3& in_position, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device lights_block& _197, const device voxel_light_indices_block& _202)
{
    float3 diffuse = float3(0.0);
    float3 param = in_position;
    int3 voxel = sprite_voxel_xyz(param, _50);
    int2 data = texture_voxel_light_data.read(uint3(voxel), 0).xy;
    light_t light;
    for (int i = 0; i < data.y; i++)
    {
        int _206 = data.x + i;
        light.origin = _197.lights[_202.voxel_light_indices[_206]].origin;
        light.color = _197.lights[_202.voxel_light_indices[_206]].color;
        light.shadow = _197.lights[_202.voxel_light_indices[_206]].shadow;
        float dist = distance(light.origin.xyz, in_position);
        float atten = fast::clamp(1.0 - (dist / light.origin.w), 0.0, 1.0);
        light_t param_1 = light;
        diffuse += (light_color(param_1, _50) * atten);
    }
    return diffuse;
}

static inline __attribute__((always_inline))
float calc_depth(thread const float& z, constant uniforms_block& _50)
{
    return (2.0 * _50.depth_range.x) / ((_50.depth_range.y + _50.depth_range.x) - (z * (_50.depth_range.y - _50.depth_range.x)));
}

static inline __attribute__((always_inline))
float soften(constant uniforms_block& _50, texture2d<float> texture_depth_attachment, sampler texture_depth_attachmentSmplr, thread float4& gl_FragCoord)
{
    float4 depth_sample = texture_depth_attachment.sample(texture_depth_attachmentSmplr, (gl_FragCoord.xy / float2(_50.viewport.zw)));
    float param = depth_sample.x;
    float param_1 = gl_FragCoord.z;
    return smoothstep(0.0, 0.001599999959580600261688232421875, fast::clamp(calc_depth(param, _50) - calc_depth(param_1, _50), 0.0, 1.0));
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _50 [[buffer(0)]], const device lights_block& _197 [[buffer(1)]], const device voxel_light_indices_block& _202 [[buffer(2)]], texture2d<float> texture_diffusemap [[texture(0)]], texture2d<float> texture_next_diffusemap [[texture(1)]], texture2d<float> texture_depth_attachment [[texture(2)]], texture3d<int> texture_voxel_light_data [[texture(3)]], sampler texture_diffusemapSmplr [[sampler(0)]], sampler texture_next_diffusemapSmplr [[sampler(1)]], sampler texture_depth_attachmentSmplr [[sampler(2)]], sampler texture_voxel_light_dataSmplr [[sampler(3)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float3 texture_color = mix(texture_diffusemap.sample(texture_diffusemapSmplr, in.in_diffusemap).xyz, texture_next_diffusemap.sample(texture_next_diffusemapSmplr, in.in_next_diffusemap).xyz, float3(in.in_lerp));
    float3 color = in.in_color;
    if (in.in_lighting > 0.0)
    {
        color = mix(color, color * sprite_lighting(_50, in.in_position, texture_voxel_light_data, texture_voxel_light_dataSmplr, _197, _202), float3(in.in_lighting));
    }
    out.out_color = float4((texture_color * color) * soften(_50, texture_depth_attachment, texture_depth_attachmentSmplr, gl_FragCoord), 1.0);
    return out;
}

