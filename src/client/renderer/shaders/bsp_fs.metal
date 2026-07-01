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
    int num_lights;
};

struct light_t_1
{
    float4 origin;
    float4 color;
    float4 shadow;
};

struct lights_block
{
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
    float2 in_diffusemap [[user(locn0)]];
    float3 in_model_position [[user(locn1)]];
    float3 in_model_normal [[user(locn2)]];
};

static inline __attribute__((always_inline))
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _49)
{
    float3 pos = position - _49.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_49.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _49)
{
    float3 color = (l.color.xyz * l.color.w) * _49.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_49.saturation));
}

static inline __attribute__((always_inline))
float3 bsp_light(thread const int& index, thread const float3& normal, constant uniforms_block& _49, const device lights_block& _106, thread float3& in_model_position)
{
    light_t light;
    light.origin = _106.lights[index].origin;
    light.color = _106.lights[index].color;
    light.shadow = _106.lights[index].shadow;
    float3 dir = light.origin.xyz - in_model_position;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return float3(0.0);
    }
    float lambert = fast::max(0.0, dot(normal, dir / float3(dist)));
    light_t param = light;
    return (light_color(param, _49) * atten) * lambert;
}

static inline __attribute__((always_inline))
float3 bsp_lighting(constant uniforms_block& _49, const device lights_block& _106, thread float3& in_model_position, thread float3& in_model_normal, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device voxel_light_indices_block& _200)
{
    float3 diffuse = float3(0.0);
    float3 normal = fast::normalize(in_model_normal);
    float3 param = in_model_position;
    int3 voxel = voxel_xyz(param, _49);
    int2 data = texture_voxel_light_data.read(uint3(voxel), 0).xy;
    for (int i = 0; i < data.y; i++)
    {
        int index = _200.voxel_light_indices[data.x + i];
        int param_1 = index;
        float3 param_2 = normal;
        diffuse += bsp_light(param_1, param_2, _49, _106, in_model_position);
    }
    return diffuse;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _49 [[buffer(0)]], const device lights_block& _106 [[buffer(1)]], const device voxel_light_indices_block& _200 [[buffer(2)]], texture2d_array<float> texture_material [[texture(0)]], texture3d<int> texture_voxel_light_data [[texture(1)]], sampler texture_materialSmplr [[sampler(0)]], sampler texture_voxel_light_dataSmplr [[sampler(1)]])
{
    main0_out out = {};
    float3 _232 = float3(in.in_diffusemap, 0.0);
    float4 diffuse = texture_material.sample(texture_materialSmplr, _232.xy, uint(rint(_232.z)));
    float3 light = float3(_49.ambient) + bsp_lighting(_49, _106, in.in_model_position, in.in_model_normal, texture_voxel_light_data, texture_voxel_light_dataSmplr, _200);
    out.out_color = float4(diffuse.xyz * light, 1.0);
    return out;
}

