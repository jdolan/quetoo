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
    int num_bsp_lights;
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

struct light_cull_block
{
    uint4 active_lights;
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
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _65)
{
    float3 pos = position - _65.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_65.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _65)
{
    float3 color = (l.color.xyz * l.color.w) * _65.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_65.saturation));
}

static inline __attribute__((always_inline))
void cubemap_face_uv(thread const float3& dir, thread int& face, thread float2& face_uv)
{
    float3 ad = abs(dir);
    bool _127 = ad.x >= ad.y;
    bool _136;
    if (_127)
    {
        _136 = ad.x >= ad.z;
    }
    else
    {
        _136 = _127;
    }
    float ma;
    float sc;
    float tc;
    if (_136)
    {
        ma = ad.x;
        if (dir.x > 0.0)
        {
            face = 0;
            sc = -dir.z;
            tc = -dir.y;
        }
        else
        {
            face = 1;
            sc = dir.z;
            tc = -dir.y;
        }
    }
    else
    {
        bool _167 = ad.y >= ad.x;
        bool _175;
        if (_167)
        {
            _175 = ad.y >= ad.z;
        }
        else
        {
            _175 = _167;
        }
        if (_175)
        {
            ma = ad.y;
            if (dir.y > 0.0)
            {
                face = 2;
                sc = dir.x;
                tc = dir.z;
            }
            else
            {
                face = 3;
                sc = dir.x;
                tc = -dir.z;
            }
        }
        else
        {
            ma = ad.z;
            if (dir.z > 0.0)
            {
                face = 4;
                sc = dir.x;
                tc = -dir.y;
            }
            else
            {
                face = 5;
                sc = -dir.x;
                tc = -dir.y;
            }
        }
    }
    face_uv = (float2(sc, tc) / float2(2.0 * fast::max(ma, 9.9999997473787516355514526367188e-05))) + float2(0.5);
}

static inline __attribute__((always_inline))
float2 shadow_atlas_uv(thread const light_t& light, thread const float3& light_to_frag, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr)
{
    float tile_uv = light.shadow.z;
    float3 param = light_to_frag;
    int param_1;
    float2 param_2;
    cubemap_face_uv(param, param_1, param_2);
    int face = param_1;
    float2 fuv = param_2;
    fuv.y = 1.0 - fuv.y;
    int face_col = face - ((face / 3) * 3);
    int face_row = face / 3;
    float2 tile_origin = light.shadow.xy + float2(float(face_col) * tile_uv, float(face_row) * tile_uv);
    float2 half_texel = float2(0.5) / float2(int2(texture_shadow_atlas.get_width(), texture_shadow_atlas.get_height()));
    return fast::clamp(tile_origin + (fuv * tile_uv), tile_origin + half_texel, (tile_origin + float2(tile_uv)) - half_texel);
}

static inline __attribute__((always_inline))
float sample_shadow_atlas(thread const light_t& light, constant uniforms_block& _65, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, thread float3& in_model_position)
{
    if (light.shadow.z == 0.0)
    {
        return 1.0;
    }
    float3 light_to_frag = in_model_position - light.origin.xyz;
    float current_depth = length(light_to_frag) / _65.depth_range.y;
    light_t param = light;
    float3 param_1 = light_to_frag;
    float2 uv = shadow_atlas_uv(param, param_1, texture_shadow_atlas, texture_shadow_atlasSmplr);
    float3 _331 = float3(uv, current_depth);
    return texture_shadow_atlas.sample_compare(texture_shadow_atlasSmplr, _331.xy, _331.z);
}

static inline __attribute__((always_inline))
float3 bsp_light(thread const int& index, thread const float3& normal, constant uniforms_block& _65, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, thread float3& in_model_position, const device lights_block& _341)
{
    light_t light;
    light.origin = _341.lights[index].origin;
    light.color = _341.lights[index].color;
    light.shadow = _341.lights[index].shadow;
    float3 dir = light.origin.xyz - in_model_position;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return float3(0.0);
    }
    float lambert = fast::max(0.0, dot(normal, dir / float3(dist)));
    if (lambert <= 0.0)
    {
        return float3(0.0);
    }
    light_t param = light;
    light_t param_1 = light;
    return ((light_color(param, _65) * atten) * lambert) * sample_shadow_atlas(param_1, _65, texture_shadow_atlas, texture_shadow_atlasSmplr, in_model_position);
}

static inline __attribute__((always_inline))
float3 bsp_lighting(constant uniforms_block& _65, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, thread float3& in_model_position, const device lights_block& _341, thread float3& in_model_normal, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device voxel_light_indices_block& _436, constant light_cull_block& _473)
{
    float3 diffuse = float3(0.0);
    float3 normal = fast::normalize(in_model_normal);
    float3 param = in_model_position;
    int3 voxel = voxel_xyz(param, _65);
    int2 data = texture_voxel_light_data.read(uint3(voxel), 0).xy;
    for (int i = 0; i < data.y; i++)
    {
        int index = _436.voxel_light_indices[data.x + i];
        int param_1 = index;
        float3 param_2 = normal;
        diffuse += bsp_light(param_1, param_2, _65, texture_shadow_atlas, texture_shadow_atlasSmplr, in_model_position, _341);
    }
    int num_dynamic = _65.num_lights - _65.num_bsp_lights;
    for (int j = 0; j < num_dynamic; j++)
    {
        if ((_473.active_lights[j >> 5] & (1u << uint(j & 31))) != 0u)
        {
            int param_3 = _65.num_bsp_lights + j;
            float3 param_4 = normal;
            diffuse += bsp_light(param_3, param_4, _65, texture_shadow_atlas, texture_shadow_atlasSmplr, in_model_position, _341);
        }
    }
    return diffuse;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _65 [[buffer(0)]], constant light_cull_block& _473 [[buffer(1)]], const device lights_block& _341 [[buffer(2)]], const device voxel_light_indices_block& _436 [[buffer(3)]], texture2d_array<float> texture_material [[texture(0)]], texture3d<int> texture_voxel_light_data [[texture(1)]], depth2d<float> texture_shadow_atlas [[texture(2)]], sampler texture_materialSmplr [[sampler(0)]], sampler texture_voxel_light_dataSmplr [[sampler(1)]], sampler texture_shadow_atlasSmplr [[sampler(2)]])
{
    main0_out out = {};
    float3 _513 = float3(in.in_diffusemap, 0.0);
    float4 diffuse = texture_material.sample(texture_materialSmplr, _513.xy, uint(rint(_513.z)));
    float3 light = float3(_65.ambient) + bsp_lighting(_65, texture_shadow_atlas, texture_shadow_atlasSmplr, in.in_model_position, _341, in.in_model_normal, texture_voxel_light_data, texture_voxel_light_dataSmplr, _436, _473);
    out.out_color = float4(diffuse.xyz * light, 1.0);
    return out;
}

