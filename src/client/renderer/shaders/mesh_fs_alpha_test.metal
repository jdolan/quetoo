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

struct common_fragment_t
{
    float3 view_dir;
    float view_dist;
    float texture_lod;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float3x3 tbn;
    float2 parallax;
    float4 diffuse_sample;
    float3 normal_sample;
    float4 specular_sample;
    float3 ambient;
    float3 diffuse;
    float3 specular;
    float caustics;
    float2 shadow_sin_cos;
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
    float4 tint_colors[3];
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

struct mesh_locals_block
{
    uint4 active_dynamic_lights[2];
};

constant spvUnsafeArray<float2, 16> _1042 = spvUnsafeArray<float2, 16>({ float2(0.2770744860172271728515625, 0.69514548778533935546875), float2(-0.59327852725982666015625, -0.1203283965587615966796875), float2(0.449474990367889404296875, 0.246909797191619873046875), float2(-0.1460638940334320068359375, -0.5679666996002197265625), float2(0.64004981517791748046875, -0.407194793224334716796875), float2(-0.3631913959980010986328125, 0.79357779026031494140625), float2(0.124885700643062591552734375, -0.897523820400238037109375), float2(-0.7720317840576171875, 0.443845808506011962890625), float2(0.88518059253692626953125, 0.1653372943401336669921875), float2(-0.52380120754241943359375, -0.726029574871063232421875), float2(0.3642682135105133056640625, 0.596805393695831298828125), float2(-0.833170115947723388671875, -0.33283460140228271484375), float2(0.552725970745086669921875, -0.698580920696258544921875), float2(-0.24071229994297027587890625, 0.3153156936168670654296875), float2(0.72694051265716552734375, -0.14306400716304779052734375), float2(-0.64446747303009033203125, 0.64446747303009033203125) });
constant spvUnsafeArray<float, 8> _1903 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float4 out_color [[color(0)]];
    float out_depth [[color(1)]];
};

struct main0_in
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
};

static inline __attribute__((always_inline))
float4 sample_material_diffuse(thread const float2& texcoord, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _341 = float3(texcoord, 0.0);
    return texture_material.sample(texture_materialSmplr, _341.xy, uint(rint(_341.z)));
}

static inline __attribute__((always_inline))
float4 sample_material_tint(thread const float2& texcoord, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _489 = float3(texcoord, 3.0);
    return texture_material.sample(texture_materialSmplr, _489.xy, uint(rint(_489.z)));
}

static inline __attribute__((always_inline))
float3 sample_material_normal(thread const float2& texcoord, thread const float3x3& tbn, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    float3 _350 = float3(texcoord, 1.0);
    float3 normalmap = (texture_material.sample(texture_materialSmplr, _350.xy, uint(rint(_350.z))).xyz * 2.0) - float3(1.0);
    float3 roughness = float3(float2(material.roughness), 1.0);
    return fast::normalize(tbn * (normalmap * roughness));
}

static inline __attribute__((always_inline))
float saturate0(thread const float& x)
{
    return fast::clamp(x, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float toksvig_gloss(thread const float3& normal, thread const float& power)
{
    float param = length(normal);
    float len_rcp = 1.0 / saturate0(param);
    return 1.0 / (1.0 + (power * (len_rcp - 1.0)));
}

static inline __attribute__((always_inline))
float4 sample_material_specular(thread const float2& texcoord, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    float3 _384 = float3(texcoord, 2.0);
    float3 _390 = texture_material.sample(texture_materialSmplr, _384.xy, uint(rint(_384.z))).xyz * material.hardness;
    float4 specularmap;
    specularmap.x = _390.x;
    specularmap.y = _390.y;
    specularmap.z = _390.z;
    float3 roughness = float3(float2(material.roughness), 1.0);
    float3 _409 = float3(texcoord, 1.0);
    float3 normalmap0 = ((texture_material.sample(texture_materialSmplr, _409.xy, uint(rint(_409.z)), level(0.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float3 _422 = float3(texcoord, 1.0);
    float3 normalmap1 = ((texture_material.sample(texture_materialSmplr, _422.xy, uint(rint(_422.z)), level(1.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float power = powr(1.0 + material.specularity, 4.0);
    float3 param = normalmap0;
    float param_1 = power;
    float3 param_2 = normalmap1;
    float param_3 = power;
    specularmap.w = power * fast::min(toksvig_gloss(param, param_1), toksvig_gloss(param_2, param_3));
    return specularmap;
}

static inline __attribute__((always_inline))
float random_angle(thread const float3& seed)
{
    return fract(sin(dot(seed, float3(12.98980045318603515625, 78.233001708984375, 45.16400146484375))) * 43758.546875) * 6.28318500518798828125;
}

static inline __attribute__((always_inline))
float voxel_occlusion(thread const float3& texcoord, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr)
{
    return texture_voxel_occlusion.sample(texture_voxel_occlusionSmplr, texcoord).x;
}

static inline __attribute__((always_inline))
float voxel_exposure(thread const float3& texcoord, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr)
{
    return fast::max(0.25, texture_voxel_occlusion.sample(texture_voxel_occlusionSmplr, texcoord).y);
}

static inline __attribute__((always_inline))
float3 ambient_light(thread const common_vertex_t& v, constant uniforms_block& _500, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, texturecube<float> texture_sky, sampler texture_skySmplr)
{
    float3 param = v.voxel;
    float occlusion = voxel_occlusion(param, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 param_1 = v.voxel;
    float exposure = voxel_exposure(param_1, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 sky = texture_sky.sample(texture_skySmplr, fast::normalize(v.model_normal), level(6.0)).xyz;
    return ((powr(float3(2.0) + sky, float3(2.0)) * exposure) * (1.0 - (occlusion * _500.ambient_occlusion))) * _500.ambient;
}

static inline __attribute__((always_inline))
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _500)
{
    float3 pos = position - _500.voxels.mins.xyz;
    int3 voxel = int3(floor((pos / float3(32.0)) + float3(0.001000000047497451305389404296875)));
    return clamp(voxel, int3(0), int3(_500.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxel_light_data(thread const int3& voxel, constant uniforms_block& _500, const device voxel_light_data_block& _551)
{
    int index = (((voxel.z * int(_500.voxels.size.y)) + voxel.y) * int(_500.voxels.size.x)) + voxel.x;
    return int2(_551.voxel_light_data_elements[(index * 2) + 0], _551.voxel_light_data_elements[(index * 2) + 1]);
}

static inline __attribute__((always_inline))
int voxel_light_index(thread const int& index, const device voxel_light_indices_block& _569)
{
    return _569.voxel_light_indices[index];
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _500)
{
    float3 color = (l.color.xyz * l.color.w) * _500.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_500.saturation));
}

static inline __attribute__((always_inline))
void cubemap_face_uv(thread const float3& dir, thread int& face, thread float2& face_uv, thread float& ma)
{
    float3 ad = abs(dir);
    bool _673 = ad.x >= ad.y;
    bool _681;
    if (_673)
    {
        _681 = ad.x >= ad.z;
    }
    else
    {
        _681 = _673;
    }
    float sc;
    float tc;
    if (_681)
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
        bool _710 = ad.y >= ad.x;
        bool _718;
        if (_710)
        {
            _718 = ad.y >= ad.z;
        }
        else
        {
            _718 = _710;
        }
        if (_718)
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
    face_uv = (float2(sc, tc) / float2(2.0 * ma)) + float2(0.5);
}

static inline __attribute__((always_inline))
float sample_shadow_face(thread const int& face, thread const float3& uvw, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr)
{
    if (face == 0)
    {
        return texture_shadow_atlas_0.sample_compare(texture_shadow_atlas_0Smplr, uvw.xy, uvw.z);
    }
    else
    {
        if (face == 1)
        {
            return texture_shadow_atlas_1.sample_compare(texture_shadow_atlas_1Smplr, uvw.xy, uvw.z);
        }
        else
        {
            if (face == 2)
            {
                return texture_shadow_atlas_2.sample_compare(texture_shadow_atlas_2Smplr, uvw.xy, uvw.z);
            }
            else
            {
                if (face == 3)
                {
                    return texture_shadow_atlas_3.sample_compare(texture_shadow_atlas_3Smplr, uvw.xy, uvw.z);
                }
                else
                {
                    if (face == 4)
                    {
                        return texture_shadow_atlas_4.sample_compare(texture_shadow_atlas_4Smplr, uvw.xy, uvw.z);
                    }
                    else
                    {
                        return texture_shadow_atlas_5.sample_compare(texture_shadow_atlas_5Smplr, uvw.xy, uvw.z);
                    }
                }
            }
        }
    }
}

static inline __attribute__((always_inline))
float sample_shadow_atlas(thread const light_t& light, thread const common_vertex_t& v, thread const common_fragment_t& f, thread const float& atten, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr)
{
    if (light.shadow.x < 0.0)
    {
        return 1.0;
    }
    float2 texture_size = float2(int2(texture_shadow_atlas_0.get_width(), texture_shadow_atlas_0.get_height()));
    float tile_px = texture_size.x / 32.0;
    float2 tile_origin = light.shadow / texture_size;
    float tile_uv = tile_px / texture_size.x;
    float3 light_to_frag = v.model_position - light.origin.xyz;
    float dist_to_light = length(light_to_frag);
    float light_size = light.origin.w * 3.0;
    float filter_radius = (light_size * (dist_to_light / light.origin.w)) * 0.004999999888241291046142578125;
    float n_dot_l = fast::max(dot(v.model_normal, fast::normalize(-light_to_frag)), 0.0);
    float3 offset_position = v.model_position + ((v.model_normal * filter_radius) * (1.0 - n_dot_l));
    light_to_frag = offset_position - light.origin.xyz;
    dist_to_light = length(light_to_frag);
    float current_depth = dist_to_light / light.origin.w;
    float3 param = light_to_frag;
    int param_1;
    float2 param_2;
    float param_3;
    cubemap_face_uv(param, param_1, param_2, param_3);
    int face = param_1;
    float2 fuv = param_2;
    float ma = param_3;
    fuv.y = 1.0 - fuv.y;
    float2 half_texel = float2(0.5) / texture_size;
    float2 tile_min = tile_origin + half_texel;
    float2 tile_max = (tile_origin + float2(tile_uv)) - half_texel;
    float filter_uv = filter_radius / (2.0 * fast::max(ma, 0.001000000047497451305389404296875));
    float importance = atten * fast::clamp(1.0 - (f.view_dist / 2048.0), 0.0, 1.0);
    int _963;
    if (importance > 0.300000011920928955078125)
    {
        _963 = 8;
    }
    else
    {
        _963 = (importance > 0.100000001490116119384765625) ? 4 : 2;
    }
    int num_samples = _963;
    float s = f.shadow_sin_cos.x;
    float c = f.shadow_sin_cos.y;
    float shadow = 0.0;
    for (int i = 0; i < num_samples; i++)
    {
        float2 rotated = float2((c * _1042[i].x) - (s * _1042[i].y), (s * _1042[i].x) + (c * _1042[i].y));
        float2 sample_fuv = fuv + (rotated * filter_uv);
        float2 atlas_uv = tile_origin + (sample_fuv * float2(tile_uv));
        atlas_uv = fast::clamp(atlas_uv, tile_min, tile_max);
        int param_4 = face;
        float3 param_5 = float3(atlas_uv, current_depth);
        shadow += sample_shadow_face(param_4, param_5, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr);
    }
    return shadow / float(num_samples);
}

static inline __attribute__((always_inline))
float blinn(thread const float3& light_dir, thread const common_fragment_t& f)
{
    return powr(fast::max(0.0, dot(fast::normalize(light_dir + f.view_dir), f.normal_sample)), f.specular_sample.w);
}

static inline __attribute__((always_inline))
float3 blinn_phong(thread const float3& light_color_1, thread const float3& light_dir, thread const common_fragment_t& f)
{
    float3 param = light_dir;
    common_fragment_t param_1 = f;
    return (light_color_1 * f.specular_sample.xyz) * blinn(param, param_1);
}

static inline __attribute__((always_inline))
void fragment_light(thread const common_vertex_t& v, thread common_fragment_t& f, thread const light_t& light, constant material_block& material, constant uniforms_block& _500, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr)
{
    float3 dir = light.origin.xyz - v.model_position;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return;
    }
    dir = fast::normalize(_500.view * float4(dir, 0.0)).xyz;
    bool is_blend = (material.surface & 112) != int(0u);
    bool is_liquid = (material.surface & 8) != int(0u);
    bool is_stage = material.flags != 0;
    float lambert = dot(dir, f.normal_sample);
    float _1324;
    if ((is_blend || is_liquid) || is_stage)
    {
        _1324 = abs(lambert);
    }
    else
    {
        _1324 = fast::max(0.0, lambert);
    }
    lambert = _1324;
    if ((atten * lambert) <= 0.0)
    {
        return;
    }
    light_t param = light;
    float3 color = light_color(param, _500) * atten;
    light_t param_1 = light;
    common_vertex_t param_2 = v;
    common_fragment_t param_3 = f;
    float param_4 = atten;
    float shadow = sample_shadow_atlas(param_1, param_2, param_3, param_4, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr);
    if (shadow <= 0.0)
    {
        return;
    }
    f.diffuse += ((color * lambert) * shadow);
    float3 param_5 = color * shadow;
    float3 param_6 = dir;
    common_fragment_t param_7 = f;
    f.specular += blinn_phong(param_5, param_6, param_7);
}

static inline __attribute__((always_inline))
bool dynamic_light_active(thread const spvUnsafeArray<uint4, 2>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 voxel_caustics(thread const float3& texcoord, constant uniforms_block& _500, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 encoded = texture_voxel_caustics.sample(texture_voxel_causticsSmplr, texcoord).xyz;
    return ((encoded * 2.0) - float3(1.0)) * _500.caustics;
}

static inline __attribute__((always_inline))
float3 hash33(thread float3& p)
{
    p = fract(p * float3(0.103100001811981201171875, 0.113689996302127838134765625, 0.13786999881267547607421875));
    p += float3(dot(p, p.yxz + float3(19.1900005340576171875)));
    return float3(-1.0) + (fract(float3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x)) * 2.0);
}

static inline __attribute__((always_inline))
float noise3d(thread const float3& p)
{
    float3 pi = floor(p);
    float3 pf = p - pi;
    float3 w = (pf * pf) * (float3(3.0) - (pf * 2.0));
    float3 param = pi + float3(0.0);
    float3 _238 = hash33(param);
    float3 param_1 = pi + float3(1.0, 0.0, 0.0);
    float3 _246 = hash33(param_1);
    float3 param_2 = pi + float3(0.0, 0.0, 1.0);
    float3 _257 = hash33(param_2);
    float3 param_3 = pi + float3(1.0, 0.0, 1.0);
    float3 _265 = hash33(param_3);
    float3 param_4 = pi + float3(0.0, 1.0, 0.0);
    float3 _279 = hash33(param_4);
    float3 param_5 = pi + float3(1.0, 1.0, 0.0);
    float3 _287 = hash33(param_5);
    float3 param_6 = pi + float3(0.0, 1.0, 1.0);
    float3 _298 = hash33(param_6);
    float3 param_7 = pi + float3(1.0);
    float3 _306 = hash33(param_7);
    return mix(mix(mix(dot(pf - float3(0.0), _238), dot(pf - float3(1.0, 0.0, 0.0), _246), w.x), mix(dot(pf - float3(0.0, 0.0, 1.0), _257), dot(pf - float3(1.0, 0.0, 1.0), _265), w.x), w.z), mix(mix(dot(pf - float3(0.0, 1.0, 0.0), _279), dot(pf - float3(1.0, 1.0, 0.0), _287), w.x), mix(dot(pf - float3(0.0, 1.0, 1.0), _298), dot(pf - float3(1.0), _306), w.x), w.z), w.y);
}

static inline __attribute__((always_inline))
void fragment_caustics(thread const common_vertex_t& v, thread common_fragment_t& f, constant uniforms_block& _500, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 param = v.voxel;
    float3 caustics_sample = voxel_caustics(param, _500, texture_voxel_caustics, texture_voxel_causticsSmplr);
    float caustics_strength = length(caustics_sample);
    if (caustics_strength == 0.0)
    {
        return;
    }
    float3 caustics_dir = fast::normalize(float3x3(_500.view[0].xyz, _500.view[1].xyz, _500.view[2].xyz) * caustics_sample);
    float facing = dot(v.normal, caustics_dir);
    float backface = (facing < (-0.25)) ? 0.25 : 1.0;
    f.caustics = caustics_strength * backface;
    if (f.caustics == 0.0)
    {
        return;
    }
    float3 param_1 = (v.model_position * 0.0500000007450580596923828125) + float3((float(_500.ticks) / 1000.0) * 0.5);
    float _noise = noise3d(param_1);
    float thickness = 0.0199999995529651641845703125;
    float glow = 5.0;
    _noise = fast::clamp(powr((1.0 - abs(_noise)) + thickness, glow), 0.0, 1.0);
    float3 light = f.ambient + f.diffuse;
    f.diffuse += fast::max(float3(0.0), (light * f.caustics) * _noise);
}

static inline __attribute__((always_inline))
void fragment_lighting(thread const common_vertex_t& v, thread common_fragment_t& f, constant material_block& material, constant uniforms_block& _500, const device voxel_light_data_block& _551, const device voxel_light_indices_block& _569, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr, texturecube<float> texture_sky, sampler texture_skySmplr, const device bsp_lights_block& _1425, const device dynamic_lights_block& _1455, constant mesh_locals_block& _1462)
{
    common_vertex_t param = v;
    f.ambient = ambient_light(param, _500, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_sky, texture_skySmplr);
    f.diffuse = float3(0.0);
    f.specular = float3(0.0);
    if (_500.editor == 0)
    {
        float3 param_1 = v.model_position;
        int3 voxel_coord = voxel_xyz(param_1, _500);
        int3 param_2 = voxel_coord;
        int2 data = voxel_light_data(param_2, _500, _551);
        light_t param_6;
        for (int i = 0; i < data.y; i++)
        {
            int param_3 = data.x + i;
            int index = voxel_light_index(param_3, _569);
            common_vertex_t param_4 = v;
            common_fragment_t param_5 = f;
            param_6.origin = _1425.bsp_lights[index].origin;
            param_6.color = _1425.bsp_lights[index].color;
            param_6.shadow = _1425.bsp_lights[index].shadow;
            fragment_light(param_4, param_5, param_6, material, _500, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr);
            f = param_5;
        }
    }
    spvUnsafeArray<uint4, 2> param_7;
    light_t param_11;
    for (int j = 0; j < _1455.num_dynamic_lights; j++)
    {
        param_7[0] = _1462.active_dynamic_lights[0];
        param_7[1] = _1462.active_dynamic_lights[1];
        int param_8 = j;
        if (dynamic_light_active(param_7, param_8))
        {
            common_vertex_t param_9 = v;
            common_fragment_t param_10 = f;
            param_11.origin = _1455.dynamic_lights[j].origin;
            param_11.color = _1455.dynamic_lights[j].color;
            param_11.shadow = _1455.dynamic_lights[j].shadow;
            fragment_light(param_9, param_10, param_11, material, _500, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr);
            f = param_10;
        }
    }
    common_vertex_t param_12 = v;
    common_fragment_t param_13 = f;
    fragment_caustics(param_12, param_13, _500, texture_voxel_caustics, texture_voxel_causticsSmplr);
    f = param_13;
}

static inline __attribute__((always_inline))
void fragment_lighting_lod(thread const common_vertex_t& v, thread common_fragment_t& f, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, constant uniforms_block& _500, const device voxel_light_data_block& _551, const device voxel_light_indices_block& _569, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr, texturecube<float> texture_sky, sampler texture_skySmplr, const device bsp_lights_block& _1425, const device dynamic_lights_block& _1455, constant mesh_locals_block& _1462)
{
    float lighting_lod = fast::clamp((f.view_dist - _500.lighting_distance) / 128.0, 0.0, 1.0);
    if (lighting_lod >= 1.0)
    {
        f.ambient = v.ambient;
        f.diffuse = v.diffuse;
        f.specular = float3(0.0);
        return;
    }
    if ((material.flags & 131072) == 131072)
    {
        f.normal_sample = fast::normalize(v.normal);
        f.specular_sample = float4(f.diffuse_sample.xyz, powr(1.0 + material.specularity, 4.0));
    }
    else
    {
        float2 param = f.parallax;
        float3x3 param_1 = float3x3(float3(v.tangent), float3(v.bitangent), float3(v.normal));
        f.normal_sample = sample_material_normal(param, param_1, texture_material, texture_materialSmplr, material);
        float2 param_2 = f.parallax;
        f.specular_sample = sample_material_specular(param_2, texture_material, texture_materialSmplr, material);
    }
    float3 param_3 = v.model_position;
    float angle = random_angle(param_3);
    f.shadow_sin_cos = float2(sin(angle), cos(angle));
    common_vertex_t param_4 = v;
    common_fragment_t param_5 = f;
    fragment_lighting(param_4, param_5, material, _500, _551, _569, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr, texture_sky, texture_skySmplr, _1425, _1455, _1462);
    f = param_5;
    f.ambient = mix(f.ambient, v.ambient, float3(lighting_lod));
    f.diffuse = mix(f.diffuse, v.diffuse, float3(lighting_lod));
    f.specular *= (1.0 - lighting_lod);
}

static inline __attribute__((always_inline))
void mesh_fragment_lighting(thread const common_vertex_t& vertex0, thread common_fragment_t& fragment0, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, constant uniforms_block& _500, const device voxel_light_data_block& _551, const device voxel_light_indices_block& _569, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas_0, sampler texture_shadow_atlas_0Smplr, depth2d<float> texture_shadow_atlas_1, sampler texture_shadow_atlas_1Smplr, depth2d<float> texture_shadow_atlas_2, sampler texture_shadow_atlas_2Smplr, depth2d<float> texture_shadow_atlas_3, sampler texture_shadow_atlas_3Smplr, depth2d<float> texture_shadow_atlas_4, sampler texture_shadow_atlas_4Smplr, depth2d<float> texture_shadow_atlas_5, sampler texture_shadow_atlas_5Smplr, texturecube<float> texture_sky, sampler texture_skySmplr, const device bsp_lights_block& _1425, const device dynamic_lights_block& _1455, constant mesh_locals_block& _1462)
{
    if (_500.view_type == 2)
    {
        fragment0.ambient = vertex0.ambient;
        fragment0.diffuse = vertex0.diffuse;
        fragment0.specular = float3(0.0);
        return;
    }
    common_vertex_t param = vertex0;
    common_fragment_t param_1 = fragment0;
    fragment_lighting_lod(param, param_1, texture_material, texture_materialSmplr, material, _500, _551, _569, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr, texture_sky, texture_skySmplr, _1425, _1455, _1462);
    fragment0 = param_1;
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

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _500 [[buffer(0)]], constant mesh_locals_block& _1462 [[buffer(1)]], constant material_block& material [[buffer(2)]], const device bsp_lights_block& _1425 [[buffer(3)]], const device dynamic_lights_block& _1455 [[buffer(4)]], const device voxel_light_data_block& _551 [[buffer(5)]], const device voxel_light_indices_block& _569 [[buffer(6)]], texture2d_array<float> texture_material [[texture(0)]], depth2d<float> texture_shadow_atlas_0 [[texture(1)]], depth2d<float> texture_shadow_atlas_1 [[texture(2)]], depth2d<float> texture_shadow_atlas_2 [[texture(3)]], depth2d<float> texture_shadow_atlas_3 [[texture(4)]], depth2d<float> texture_shadow_atlas_4 [[texture(5)]], depth2d<float> texture_shadow_atlas_5 [[texture(6)]], texture3d<float> texture_voxel_caustics [[texture(7)]], texture3d<float> texture_voxel_occlusion [[texture(8)]], texturecube<float> texture_sky [[texture(9)]], texture2d<float> texture_stage [[texture(10)]], texture2d<float> texture_stage_next [[texture(11)]], sampler texture_materialSmplr [[sampler(0)]], sampler texture_shadow_atlas_0Smplr [[sampler(1)]], sampler texture_shadow_atlas_1Smplr [[sampler(2)]], sampler texture_shadow_atlas_2Smplr [[sampler(3)]], sampler texture_shadow_atlas_3Smplr [[sampler(4)]], sampler texture_shadow_atlas_4Smplr [[sampler(5)]], sampler texture_shadow_atlas_5Smplr [[sampler(6)]], sampler texture_voxel_causticsSmplr [[sampler(7)]], sampler texture_voxel_occlusionSmplr [[sampler(8)]], sampler texture_skySmplr [[sampler(9)]], sampler texture_stageSmplr [[sampler(10)]], sampler texture_stage_nextSmplr [[sampler(11)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    common_vertex_t vertex0 = {};
    vertex0.model_position = in.vertex0_model_position;
    vertex0.model_normal = in.vertex0_model_normal;
    vertex0.position = in.vertex0_position;
    vertex0.normal = in.vertex0_normal;
    vertex0.tangent = in.vertex0_tangent;
    vertex0.bitangent = in.vertex0_bitangent;
    vertex0.diffusemap = in.vertex0_diffusemap;
    vertex0.voxel = in.vertex0_voxel;
    vertex0.color = in.vertex0_color;
    vertex0.ambient = in.vertex0_ambient;
    vertex0.diffuse = in.vertex0_diffuse;
    vertex0.caustics = in.vertex0_caustics;
    out.out_depth = gl_FragCoord.z;
    common_fragment_t fragment0;
    fragment0.view_dir = fast::normalize(-vertex0.position);
    fragment0.view_dist = length(vertex0.position);
    fragment0.parallax = vertex0.diffusemap;
    if (material.flags == 0)
    {
        float2 param = fragment0.parallax;
        fragment0.diffuse_sample = sample_material_diffuse(param, texture_material, texture_materialSmplr);
        if ((material.surface & 1024) == 1024)
        {
            if (fragment0.diffuse_sample.w < material.alpha_test)
            {
                discard_fragment();
            }
        }
        float2 param_1 = fragment0.parallax;
        float4 tintmap = sample_material_tint(param_1, texture_material, texture_materialSmplr);
        float4 _1698 = fragment0.diffuse_sample;
        float3 _1700 = _1698.xyz * (1.0 - tintmap.w);
        fragment0.diffuse_sample.x = _1700.x;
        fragment0.diffuse_sample.y = _1700.y;
        fragment0.diffuse_sample.z = _1700.z;
        float4 _1718 = fragment0.diffuse_sample;
        float3 _1720 = _1718.xyz + ((material.tint_colors[0] * tintmap.x).xyz * tintmap.w);
        fragment0.diffuse_sample.x = _1720.x;
        fragment0.diffuse_sample.y = _1720.y;
        fragment0.diffuse_sample.z = _1720.z;
        float4 _1737 = fragment0.diffuse_sample;
        float3 _1739 = _1737.xyz + ((material.tint_colors[1] * tintmap.y).xyz * tintmap.w);
        fragment0.diffuse_sample.x = _1739.x;
        fragment0.diffuse_sample.y = _1739.y;
        fragment0.diffuse_sample.z = _1739.z;
        float4 _1756 = fragment0.diffuse_sample;
        float3 _1758 = _1756.xyz + ((material.tint_colors[2] * tintmap.z).xyz * tintmap.w);
        fragment0.diffuse_sample.x = _1758.x;
        fragment0.diffuse_sample.y = _1758.y;
        fragment0.diffuse_sample.z = _1758.z;
        out.out_color = fragment0.diffuse_sample * vertex0.color;
        if (_500.view_type == 2)
        {
            fragment0.ambient = float3(0.66600000858306884765625);
            fragment0.diffuse = float3(0.0);
            fragment0.specular = float3(0.0);
        }
        else
        {
            common_vertex_t param_2 = vertex0;
            common_fragment_t param_3 = fragment0;
            mesh_fragment_lighting(param_2, param_3, texture_material, texture_materialSmplr, material, _500, _551, _569, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr, texture_sky, texture_skySmplr, _1425, _1455, _1462);
            fragment0 = param_3;
        }
        float4 _1794 = out.out_color;
        float3 _1796 = _1794.xyz * (fragment0.ambient + fragment0.diffuse);
        out.out_color.x = _1796.x;
        out.out_color.y = _1796.y;
        out.out_color.z = _1796.z;
        float4 _1805 = out.out_color;
        float3 _1807 = _1805.xyz + fragment0.specular;
        out.out_color.x = _1807.x;
        out.out_color.y = _1807.y;
        out.out_color.z = _1807.z;
    }
    else
    {
        float2 param_4 = fragment0.parallax;
        fragment0.diffuse_sample = sample_material_stage(param_4, material, texture_stage, texture_stageSmplr, texture_stage_next, texture_stage_nextSmplr) * vertex0.color;
        out.out_color = fragment0.diffuse_sample;
        if ((material.flags & 65536) == 65536)
        {
            common_vertex_t param_5 = vertex0;
            common_fragment_t param_6 = fragment0;
            mesh_fragment_lighting(param_5, param_6, texture_material, texture_materialSmplr, material, _500, _551, _569, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas_0, texture_shadow_atlas_0Smplr, texture_shadow_atlas_1, texture_shadow_atlas_1Smplr, texture_shadow_atlas_2, texture_shadow_atlas_2Smplr, texture_shadow_atlas_3, texture_shadow_atlas_3Smplr, texture_shadow_atlas_4, texture_shadow_atlas_4Smplr, texture_shadow_atlas_5, texture_shadow_atlas_5Smplr, texture_sky, texture_skySmplr, _1425, _1455, _1462);
            fragment0 = param_6;
            float4 _1848 = out.out_color;
            float3 _1850 = _1848.xyz * mix(float3(1.0), fragment0.ambient + fragment0.diffuse, float3(material.lighting));
            out.out_color.x = _1850.x;
            out.out_color.y = _1850.y;
            out.out_color.z = _1850.z;
            float4 _1862 = out.out_color;
            float3 _1864 = _1862.xyz + (fragment0.specular * material.lighting);
            out.out_color.x = _1864.x;
            out.out_color.y = _1864.y;
            out.out_color.z = _1864.z;
        }
        if ((material.flags & 262144) == 262144)
        {
            float4 _1885 = out.out_color;
            float3 _1887 = _1885.xyz + (fragment0.diffuse_sample.xyz * material.emissive);
            out.out_color.x = _1887.x;
            out.out_color.y = _1887.y;
            out.out_color.z = _1887.z;
        }
    }
    return out;
}

