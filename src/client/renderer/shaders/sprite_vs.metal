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
    float2 tile;
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

struct light_t_1
{
    float4 origin;
    float4 color;
    float2 tile;
};

struct bsp_lights_block
{
    int num_bsp_lights;
    light_t_1 bsp_lights[1];
};

struct voxel_light_indices_block
{
    int voxel_light_indices[1];
};

struct dynamic_lights_block
{
    int num_dynamic_lights;
    light_t_1 dynamic_lights[1];
};

struct sprite_locals_block
{
    uint4 active_dynamic_lights[4];
};

struct sprite_instance_t
{
    float4 center;
    float4 a;
    float4 b;
    float4 texcoords;
    float4 next_texcoords;
    float4 color;
};

struct sprite_instance_t_1
{
    float4 center;
    float4 a;
    float4 b;
    float4 texcoords;
    float4 next_texcoords;
    float4 color;
};

struct sprite_instances_block
{
    sprite_instance_t_1 sprite_instances[1];
};

constant spvUnsafeArray<float2, 4> _336 = spvUnsafeArray<float2, 4>({ float2(1.0, -1.0), float2(1.0), float2(-1.0, 1.0), float2(-1.0) });
constant spvUnsafeArray<float2, 4> _371 = spvUnsafeArray<float2, 4>({ float2(0.0), float2(1.0, 0.0), float2(1.0), float2(0.0, 1.0) });

struct main0_out
{
    float2 out_diffusemap [[user(locn0)]];
    float2 out_next_diffusemap [[user(locn1)]];
    float3 out_color [[user(locn2)]];
    float out_lerp [[user(locn3)]];
    float out_lighting [[user(locn4)]];
    float3 out_diffuse [[user(locn5)]];
    float4 gl_Position [[position, invariant]];
};

static inline __attribute__((always_inline))
int3 sprite_voxel_xyz(thread const float3& position, constant uniforms_block& _60)
{
    float3 pos = position - _60.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_60.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _60)
{
    float3 color = (l.color.xyz * l.color.w) * _60.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_60.saturation));
}

static inline __attribute__((always_inline))
float3 sprite_light(thread const light_t& light, thread const float3& position, constant uniforms_block& _60)
{
    float dist = distance(light.origin.xyz, position);
    float atten = fast::clamp(1.0 - (dist / light.origin.w), 0.0, 1.0);
    light_t param = light;
    return light_color(param, _60) * atten;
}

static inline __attribute__((always_inline))
bool dynamic_light_active(thread const spvUnsafeArray<uint4, 4>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 sprite_lighting(thread const float3& position, constant uniforms_block& _60, const device voxel_light_data_block& _183, const device bsp_lights_block& _211, const device voxel_light_indices_block& _215, const device dynamic_lights_block& _250, constant sprite_locals_block& _257)
{
    float3 diffuse = float3(0.0);
    float3 param = position;
    int3 voxel = sprite_voxel_xyz(param, _60);
    int index = (((voxel.z * int(_60.voxels.size.y)) + voxel.y) * int(_60.voxels.size.x)) + voxel.x;
    int2 data = int2(_183.voxel_light_data_elements[(index * 2) + 0], _183.voxel_light_data_elements[(index * 2) + 1]);
    light_t param_1;
    for (int i = 0; i < data.y; i++)
    {
        int _219 = data.x + i;
        param_1.origin = _211.bsp_lights[_215.voxel_light_indices[_219]].origin;
        param_1.color = _211.bsp_lights[_215.voxel_light_indices[_219]].color;
        param_1.tile = _211.bsp_lights[_215.voxel_light_indices[_219]].tile;
        float3 param_2 = position;
        diffuse += sprite_light(param_1, param_2, _60);
    }
    spvUnsafeArray<uint4, 4> param_3;
    light_t param_5;
    for (int j = 0; j < _250.num_dynamic_lights; j++)
    {
        param_3[0] = _257.active_dynamic_lights[0];
        param_3[1] = _257.active_dynamic_lights[1];
        param_3[2] = _257.active_dynamic_lights[2];
        param_3[3] = _257.active_dynamic_lights[3];
        int param_4 = j;
        if (dynamic_light_active(param_3, param_4))
        {
            param_5.origin = _250.dynamic_lights[j].origin;
            param_5.color = _250.dynamic_lights[j].color;
            param_5.tile = _250.dynamic_lights[j].tile;
            float3 param_6 = position;
            diffuse += sprite_light(param_5, param_6, _60);
        }
    }
    return diffuse;
}

vertex main0_out main0(constant uniforms_block& _60 [[buffer(0)]], constant sprite_locals_block& _257 [[buffer(1)]], const device bsp_lights_block& _211 [[buffer(2)]], const device dynamic_lights_block& _250 [[buffer(3)]], const device voxel_light_data_block& _183 [[buffer(4)]], const device voxel_light_indices_block& _215 [[buffer(5)]], const device sprite_instances_block& _309 [[buffer(6)]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    uint corner = uint(int(gl_VertexIndex)) & 3u;
    uint _312 = uint(int(gl_VertexIndex)) >> uint(2);
    sprite_instance_t instance;
    instance.center = _309.sprite_instances[_312].center;
    instance.a = _309.sprite_instances[_312].a;
    instance.b = _309.sprite_instances[_312].b;
    instance.texcoords = _309.sprite_instances[_312].texcoords;
    instance.next_texcoords = _309.sprite_instances[_312].next_texcoords;
    instance.color = _309.sprite_instances[_312].color;
    float2 signs = _336[corner];
    float3 position = (instance.center.xyz + (instance.a.xyz * signs.x)) + (instance.b.xyz * signs.y);
    out.out_diffusemap = mix(instance.texcoords.xy, instance.texcoords.zw, _371[corner]);
    out.out_next_diffusemap = mix(instance.next_texcoords.xy, instance.next_texcoords.zw, _371[corner]);
    out.out_color = instance.color.xyz;
    out.out_lerp = instance.center.w;
    out.out_lighting = instance.a.w;
    float3 param = position;
    out.out_diffuse = sprite_lighting(param, _60, _183, _211, _215, _250, _257);
    float4x4 _414 = _60.projection3D * _60.view;
    float4 _419 = float4(position, 1.0);
    float4 _420 = _414 * _419;
    out.gl_Position = _420;
    return out;
}

