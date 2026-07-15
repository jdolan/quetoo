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

// Implementation of signed integer mod accurate to SPIR-V specification
template<typename Tx, typename Ty>
inline Tx spvSMod(Tx x, Ty y)
{
    Tx remainder = x - y * (x / y);
    return select(Tx(remainder + y), remainder, remainder == 0 || (x >= 0) == (y >= 0));
}

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

struct light_t
{
    float4 origin;
    float4 color;
    float2 shadow;
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

struct bsp_locals_block
{
    float4x4 model;
    uint4 active_dynamic_lights[2];
};

constant spvUnsafeArray<float, 8> _527 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

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
void stage_transform(thread float3& position, thread const float3& normal, thread const float3& tangent, thread const float3& bitangent, constant material_block& material)
{
    if ((material.flags & 1048576) == 1048576)
    {
        position += (normal * material.shell);
    }
}

static inline __attribute__((always_inline))
float3 voxel_uvw(thread const float3& position, constant uniforms_block& _156)
{
    return (position - _156.voxels.mins.xyz) / (_156.voxels.maxs.xyz - _156.voxels.mins.xyz);
}

static inline __attribute__((always_inline))
void stage_vertex(thread const float3& in_position, thread common_vertex_t& vertex0, constant material_block& material, constant uniforms_block& _156)
{
    int envmap = material.flags & 16384;
    if (envmap != 0)
    {
        float3 view_dir = fast::normalize(vertex0.position);
        float3 reflect_dir = reflect(view_dir, fast::normalize(vertex0.normal));
        vertex0.diffusemap = float2(0.5 + (reflect_dir.y * 0.5), 0.5 - (reflect_dir.z * 0.5));
    }
    if ((material.flags & 256) == 256)
    {
        float p = 1.0 + ((sin(((float(_156.ticks) * 0.001000000047497451305389404296875) * material.stretch.y) * 3.1415927410125732421875) * material.stretch.x) * 0.5);
        float2x2 matrix;
        matrix[0].x = p;
        matrix[1].x = 0.0;
        float2 translate;
        translate.x = material.st_origin.x - (material.st_origin.x * p);
        matrix[0].y = 0.0;
        matrix[1].y = p;
        translate.y = material.st_origin.y - (material.st_origin.y * p);
        vertex0.diffusemap.x = ((vertex0.diffusemap.x * matrix[0].x) + (vertex0.diffusemap.y * matrix[1].x)) + translate.x;
        vertex0.diffusemap.y = ((vertex0.diffusemap.x * matrix[0].y) + (vertex0.diffusemap.y * matrix[1].y)) + translate.y;
    }
    if ((material.flags & 128) == 128)
    {
        float theta = ((float(_156.ticks) * 0.001000000047497451305389404296875) * material.rotate) * 6.283185482025146484375;
        float2 st_origin = material.st_origin;
        if (envmap != 0)
        {
            st_origin = float2(0.5);
        }
        vertex0.diffusemap -= st_origin;
        vertex0.diffusemap = float2x2(float2(cos(theta), -sin(theta)), float2(sin(theta), cos(theta))) * vertex0.diffusemap;
        vertex0.diffusemap += st_origin;
    }
    if (envmap != 0)
    {
        if ((material.flags & 96) != 0)
        {
            float _303;
            if ((material.flags & 32) == 32)
            {
                _303 = material.scale.x;
            }
            else
            {
                _303 = 1.0;
            }
            float _316;
            if ((material.flags & 64) == 64)
            {
                _316 = material.scale.y;
            }
            else
            {
                _316 = 1.0;
            }
            float2 scale = float2(_303, _316);
            float2 centered = vertex0.diffusemap - float2(0.5);
            centered /= fast::max(abs(scale), float2(9.9999997473787516355514526367188e-05));
            vertex0.diffusemap = centered + float2(0.5);
        }
        if ((material.flags & 8) == 8)
        {
            vertex0.diffusemap.x += ((material.scroll.x * float(_156.ticks)) * 0.001000000047497451305389404296875);
        }
        if ((material.flags & 16) == 16)
        {
            vertex0.diffusemap.y += ((material.scroll.y * float(_156.ticks)) * 0.001000000047497451305389404296875);
        }
    }
    else
    {
        if ((material.flags & 8) == 8)
        {
            vertex0.diffusemap.x += ((material.scroll.x * float(_156.ticks)) * 0.001000000047497451305389404296875);
        }
        if ((material.flags & 16) == 16)
        {
            vertex0.diffusemap.y += ((material.scroll.y * float(_156.ticks)) * 0.001000000047497451305389404296875);
        }
        if ((material.flags & 32) == 32)
        {
            vertex0.diffusemap.x *= material.scale.x;
        }
        if ((material.flags & 64) == 64)
        {
            vertex0.diffusemap.y *= material.scale.y;
        }
    }
    if ((material.flags & 4) == 4)
    {
        vertex0.color = material.color;
    }
    if ((material.flags & 512) == 512)
    {
        vertex0.color.w *= ((sin((((float(_156.ticks) * 0.001000000047497451305389404296875) + material.drift) * material.pulse) * 3.1415927410125732421875) + 1.0) * 0.5);
    }
    if ((material.flags & 4096) == 4096)
    {
        float z = fast::clamp(in_position.z, material.terrain.x, material.terrain.y);
        vertex0.color.w *= ((z - material.terrain.x) / (material.terrain.y - material.terrain.x));
    }
    if ((material.flags & 8192) == 8192)
    {
        int index = (int(in_position.x) + int(in_position.y)) + int(in_position.z);
        vertex0.color.w *= (_527[spvSMod(index, 8)] * material.dirtmap);
    }
}

static inline __attribute__((always_inline))
float voxel_occlusion(thread const float3& texcoord)
{
    return 0.0;
}

static inline __attribute__((always_inline))
float voxel_exposure(thread const float3& texcoord)
{
    return 1.0;
}

static inline __attribute__((always_inline))
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _156)
{
    float3 pos = position - _156.voxels.mins.xyz;
    int3 voxel = int3(floor((pos / float3(32.0)) + float3(0.001000000047497451305389404296875)));
    return clamp(voxel, int3(0), int3(_156.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxel_light_data(thread const int3& voxel, constant uniforms_block& _156, const device voxel_light_data_block& _603)
{
    int index = (((voxel.z * int(_156.voxels.size.y)) + voxel.y) * int(_156.voxels.size.x)) + voxel.x;
    return int2(_603.voxel_light_data_elements[(index * 2) + 0], _603.voxel_light_data_elements[(index * 2) + 1]);
}

static inline __attribute__((always_inline))
int voxel_light_index(thread const int& index, const device voxel_light_indices_block& _620)
{
    return _620.voxel_light_indices[index];
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _156)
{
    float3 color = (l.color.xyz * l.color.w) * _156.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_156.saturation));
}

static inline __attribute__((always_inline))
float3 vertex_light(thread const common_vertex_t& v, thread const light_t& light, constant material_block& material, constant uniforms_block& _156)
{
    float3 light_dir = light.origin.xyz - v.model_position;
    float dist = length(light_dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return float3(0.0);
    }
    light_dir = fast::normalize(light_dir);
    float lambert = dot(v.model_normal, light_dir);
    float _714;
    if ((material.surface & 120) != int(0u))
    {
        _714 = abs(lambert);
    }
    else
    {
        _714 = fast::max(0.0, lambert);
    }
    lambert = _714;
    light_t param = light;
    return (light_color(param, _156) * atten) * lambert;
}

static inline __attribute__((always_inline))
bool dynamic_light_active(thread const spvUnsafeArray<uint4, 2>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 voxel_caustics(thread const float3& texcoord)
{
    return float3(0.0);
}

static inline __attribute__((always_inline))
void vertex_caustics(thread common_vertex_t& v)
{
    float3 param = v.voxel;
    v.caustics = length(voxel_caustics(param));
}

static inline __attribute__((always_inline))
void vertex_lighting(thread common_vertex_t& v, constant material_block& material, constant uniforms_block& _156, const device voxel_light_data_block& _603, const device voxel_light_indices_block& _620, const device bsp_lights_block& _799, const device dynamic_lights_block& _830, constant bsp_locals_block& _837)
{
    float3 param = v.voxel;
    float occlusion = voxel_occlusion(param);
    float3 param_1 = v.voxel;
    float exposure = voxel_exposure(param_1);
    v.ambient = (float3(_156.ambient) * exposure) * (1.0 - (occlusion * _156.ambient_occlusion));
    v.diffuse = float3(0.0);
    if (_156.editor == 0)
    {
        float3 param_2 = v.model_position;
        int3 voxel_coord = voxel_xyz(param_2, _156);
        int3 param_3 = voxel_coord;
        int2 data = voxel_light_data(param_3, _156, _603);
        light_t param_6;
        for (int i = 0; i < data.y; i++)
        {
            int param_4 = data.x + i;
            int index = voxel_light_index(param_4, _620);
            common_vertex_t param_5 = v;
            param_6.origin = _799.bsp_lights[index].origin;
            param_6.color = _799.bsp_lights[index].color;
            param_6.shadow = _799.bsp_lights[index].shadow;
            v.diffuse += vertex_light(param_5, param_6, material, _156);
        }
    }
    spvUnsafeArray<uint4, 2> param_7;
    light_t param_10;
    for (int j = 0; j < _830.num_dynamic_lights; j++)
    {
        param_7[0] = _837.active_dynamic_lights[0];
        param_7[1] = _837.active_dynamic_lights[1];
        int param_8 = j;
        if (dynamic_light_active(param_7, param_8))
        {
            common_vertex_t param_9 = v;
            param_10.origin = _830.dynamic_lights[j].origin;
            param_10.color = _830.dynamic_lights[j].color;
            param_10.shadow = _830.dynamic_lights[j].shadow;
            v.diffuse += vertex_light(param_9, param_10, material, _156);
        }
    }
    common_vertex_t param_11 = v;
    vertex_caustics(param_11);
    v = param_11;
}

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _156 [[buffer(0)]], constant bsp_locals_block& _837 [[buffer(1)]], constant material_block& material [[buffer(2)]], const device bsp_lights_block& _799 [[buffer(3)]], const device dynamic_lights_block& _830 [[buffer(4)]], const device voxel_light_data_block& _603 [[buffer(5)]], const device voxel_light_indices_block& _620 [[buffer(6)]])
{
    main0_out out = {};
    common_vertex_t vertex0 = {};
    float4x4 view_model = _156.view * _837.model;
    float4 position = float4(in.in_position, 1.0);
    float4 normal = float4(in.in_normal, 0.0);
    float4 tangent = float4(in.in_tangent, 0.0);
    float4 bitangent = float4(in.in_bitangent, 0.0);
    float3 param = position.xyz;
    float3 param_1 = normal.xyz;
    float3 param_2 = tangent.xyz;
    float3 param_3 = bitangent.xyz;
    stage_transform(param, param_1, param_2, param_3, material);
    position.x = param.x;
    position.y = param.y;
    position.z = param.z;
    normal.x = param_1.x;
    normal.y = param_1.y;
    normal.z = param_1.z;
    tangent.x = param_2.x;
    tangent.y = param_2.y;
    tangent.z = param_2.z;
    bitangent.x = param_3.x;
    bitangent.y = param_3.y;
    bitangent.z = param_3.z;
    vertex0.model_position = float3((_837.model * position).xyz);
    vertex0.model_normal = fast::normalize(float3((_837.model * normal).xyz));
    vertex0.position = float3((view_model * position).xyz);
    vertex0.normal = fast::normalize(float3((view_model * normal).xyz));
    vertex0.tangent = fast::normalize(float3((view_model * tangent).xyz));
    vertex0.bitangent = fast::normalize(float3((view_model * bitangent).xyz));
    vertex0.diffusemap = in.in_diffusemap;
    float3 param_4 = float3((_837.model * position).xyz);
    vertex0.voxel = voxel_uvw(param_4, _156);
    vertex0.color = in.in_color;
    float3 param_5 = in.in_position;
    common_vertex_t param_6 = vertex0;
    stage_vertex(param_5, param_6, material, _156);
    vertex0 = param_6;
    common_vertex_t param_7 = vertex0;
    vertex_lighting(param_7, material, _156, _603, _620, _799, _830, _837);
    vertex0 = param_7;
    float4x4 _1048 = _156.projection3D * view_model;
    float4 _1050 = _1048 * position;
    out.gl_Position = _1050;
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

