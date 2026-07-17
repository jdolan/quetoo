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

struct light_t
{
    float4 origin;
    float4 color;
    float2 shadow;
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

struct voxel_light_data_block
{
    int voxel_light_data_elements[1];
};

struct voxel_light_indices_block
{
    int voxel_light_indices[1];
};

struct light_t_1
{
    float4 origin;
    float4 color;
    float2 shadow;
};

struct bsp_lights_block
{
    int num_bsp_lights;
    light_t_1 bsp_lights[1];
};

struct dynamic_lights_block
{
    int num_dynamic_lights;
    light_t_1 dynamic_lights[1];
};

struct decal_locals_block
{
    uint4 active_dynamic_lights[2];
};

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float3 in_model_position [[user(locn0)]];
    float3 in_model_normal [[user(locn1)]];
    float2 in_texcoord [[user(locn2)]];
    float4 in_color [[user(locn3)]];
    uint in_time [[user(locn4)]];
    uint in_lifetime [[user(locn5)]];
};

static inline __attribute__((always_inline))
int3 decal_voxel_xyz(thread const float3& position, constant uniforms_block& _56)
{
    float3 pos = position - _56.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_56.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _56)
{
    float3 color = (l.color.xyz * l.color.w) * _56.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_56.saturation));
}

static inline __attribute__((always_inline))
float3 decal_light(thread const light_t& light, thread const float3& normal, constant uniforms_block& _56, thread float3& in_model_position)
{
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
    return (light_color(param, _56) * atten) * lambert;
}

static inline __attribute__((always_inline))
bool dynamic_light_active(thread const spvUnsafeArray<uint4, 2>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _56 [[buffer(0)]], constant decal_locals_block& _294 [[buffer(1)]], const device bsp_lights_block& _257 [[buffer(2)]], const device dynamic_lights_block& _287 [[buffer(3)]], const device voxel_light_data_block& _218 [[buffer(4)]], const device voxel_light_indices_block& _246 [[buffer(5)]], texture2d<float> texture_diffusemap [[texture(0)]], sampler texture_diffusemapSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 diffuse = texture_diffusemap.sample(texture_diffusemapSmplr, in.in_texcoord);
    float3 normal = fast::normalize(in.in_model_normal);
    float3 light = float3(_56.ambient);
    float3 param = in.in_model_position;
    int3 voxel = decal_voxel_xyz(param, _56);
    int voxel_index = (((voxel.z * int(_56.voxels.size.y)) + voxel.y) * int(_56.voxels.size.x)) + voxel.x;
    int2 data = int2(_218.voxel_light_data_elements[(voxel_index * 2) + 0], _218.voxel_light_data_elements[(voxel_index * 2) + 1]);
    light_t param_1;
    for (int i = 0; i < data.y; i++)
    {
        int index = _246.voxel_light_indices[data.x + i];
        param_1.origin = _257.bsp_lights[index].origin;
        param_1.color = _257.bsp_lights[index].color;
        param_1.shadow = _257.bsp_lights[index].shadow;
        float3 param_2 = normal;
        light += decal_light(param_1, param_2, _56, in.in_model_position);
    }
    spvUnsafeArray<uint4, 2> param_3;
    light_t param_5;
    for (int j = 0; j < _287.num_dynamic_lights; j++)
    {
        param_3[0] = _294.active_dynamic_lights[0];
        param_3[1] = _294.active_dynamic_lights[1];
        int param_4 = j;
        if (dynamic_light_active(param_3, param_4))
        {
            param_5.origin = _287.dynamic_lights[j].origin;
            param_5.color = _287.dynamic_lights[j].color;
            param_5.shadow = _287.dynamic_lights[j].shadow;
            float3 param_6 = normal;
            light += decal_light(param_5, param_6, _56, in.in_model_position);
        }
    }
    out.out_color = diffuse * in.in_color;
    float4 _334 = out.out_color;
    float3 _336 = _334.xyz * light;
    out.out_color.x = _336.x;
    out.out_color.y = _336.y;
    out.out_color.z = _336.z;
    if (in.in_lifetime > 0u)
    {
        float age = float(uint(_56.ticks) - in.in_time);
        float fade = 1.0 - fast::clamp(age / float(in.in_lifetime), 0.0, 1.0);
        out.out_color.w *= fade;
    }
    return out;
}

