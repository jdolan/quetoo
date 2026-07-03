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
    float4 shadow;
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
    float lod;
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

struct material_block
{
    int surface;
    float alpha_test;
    float roughness;
    float hardness;
    float specularity;
    float parallax;
    float shadow;
};

struct voxel_light_indices_block
{
    int voxel_light_indices[1];
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

struct light_cull_block
{
    uint4 active_lights;
};

struct tint_block
{
    float4 tint_colors[3];
};

constant spvUnsafeArray<float2, 16> _901 = spvUnsafeArray<float2, 16>({ float2(0.2770744860172271728515625, 0.69514548778533935546875), float2(-0.59327852725982666015625, -0.1203283965587615966796875), float2(0.449474990367889404296875, 0.246909797191619873046875), float2(-0.1460638940334320068359375, -0.5679666996002197265625), float2(0.64004981517791748046875, -0.407194793224334716796875), float2(-0.3631913959980010986328125, 0.79357779026031494140625), float2(0.124885700643062591552734375, -0.897523820400238037109375), float2(-0.7720317840576171875, 0.443845808506011962890625), float2(0.88518059253692626953125, 0.1653372943401336669921875), float2(-0.52380120754241943359375, -0.726029574871063232421875), float2(0.3642682135105133056640625, 0.596805393695831298828125), float2(-0.833170115947723388671875, -0.33283460140228271484375), float2(0.552725970745086669921875, -0.698580920696258544921875), float2(-0.24071229994297027587890625, 0.3153156936168670654296875), float2(0.72694051265716552734375, -0.14306400716304779052734375), float2(-0.64446747303009033203125, 0.64446747303009033203125) });
constant spvUnsafeArray<float, 8> _1862 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float4 out_color [[color(0)]];
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
float sample_material_heightmap(thread const float2& texcoord, thread const float& lod, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _483 = float3(texcoord, 1.0);
    return texture_material.sample(texture_materialSmplr, _483.xy, uint(rint(_483.z)), level(lod)).w;
}

static inline __attribute__((always_inline))
float sample_material_displacement(thread const float2& texcoord, thread const float& lod, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float2 param = texcoord;
    float param_1 = lod;
    return 1.0 - sample_material_heightmap(param, param_1, texture_material, texture_materialSmplr);
}

static inline __attribute__((always_inline))
void parallax_occlusion_mapping(thread const common_vertex_t& vertex0, thread common_fragment_t& fragment0, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    fragment0.parallax = vertex0.diffusemap;
    bool _1497 = material.parallax == 0.0;
    bool _1504;
    if (!_1497)
    {
        _1504 = fragment0.lod > 4.0;
    }
    else
    {
        _1504 = _1497;
    }
    if (_1504)
    {
        return;
    }
    float num_samples = mix(32.0, 8.0, fast::min(fragment0.lod * 0.25, 1.0));
    float2 texel = float2(1.0) / float2(int3(texture_material.get_width(), texture_material.get_height(), texture_material.get_array_size()).xy);
    float3 dir = fast::normalize(fragment0.view_dir * float3x3(float3(vertex0.tangent), float3(vertex0.bitangent), float3(vertex0.normal)));
    dir.z = fast::max(dir.z, 0.100000001490116119384765625);
    float2 p = (((dir.xy * texel) / float2(dir.z)) * material.parallax) * material.parallax;
    float2 delta = p / float2(num_samples);
    float2 texcoord = vertex0.diffusemap;
    float2 prev_texcoord = vertex0.diffusemap;
    float depth = 0.0;
    float layer = 1.0 / num_samples;
    float2 param = texcoord;
    float param_1 = fragment0.lod;
    float displacement = sample_material_displacement(param, param_1, texture_material, texture_materialSmplr);
    for (int i = 0; (i < int(num_samples)) && (depth < displacement); i++)
    {
        depth += layer;
        prev_texcoord = texcoord;
        texcoord -= delta;
        float2 param_2 = texcoord;
        float param_3 = fragment0.lod;
        displacement = sample_material_displacement(param_2, param_3, texture_material, texture_materialSmplr);
    }
    float a = displacement - depth;
    float2 param_4 = prev_texcoord;
    float param_5 = fragment0.lod;
    float b = (sample_material_displacement(param_4, param_5, texture_material, texture_materialSmplr) - depth) + layer;
    fragment0.parallax = mix(prev_texcoord, texcoord, float2(a / (a - b)));
}

static inline __attribute__((always_inline))
float4 sample_material_diffuse(thread const float2& texcoord, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _370 = float3(texcoord, 0.0);
    return texture_material.sample(texture_materialSmplr, _370.xy, uint(rint(_370.z)));
}

static inline __attribute__((always_inline))
float4 sample_material_tint(thread const float2& texcoord, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _501 = float3(texcoord, 3.0);
    return texture_material.sample(texture_materialSmplr, _501.xy, uint(rint(_501.z)));
}

static inline __attribute__((always_inline))
float3 sample_material_normal(thread const float2& texcoord, thread const float3x3& tbn, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    float3 _379 = float3(texcoord, 1.0);
    float3 normalmap = (texture_material.sample(texture_materialSmplr, _379.xy, uint(rint(_379.z))).xyz * 2.0) - float3(1.0);
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
    float3 _409 = float3(texcoord, 2.0);
    float3 _415 = texture_material.sample(texture_materialSmplr, _409.xy, uint(rint(_409.z))).xyz * material.hardness;
    float4 specularmap;
    specularmap.x = _415.x;
    specularmap.y = _415.y;
    specularmap.z = _415.z;
    float3 roughness = float3(float2(material.roughness), 1.0);
    float3 _434 = float3(texcoord, 1.0);
    float3 normalmap0 = ((texture_material.sample(texture_materialSmplr, _434.xy, uint(rint(_434.z)), level(0.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float3 _447 = float3(texcoord, 1.0);
    float3 normalmap1 = ((texture_material.sample(texture_materialSmplr, _447.xy, uint(rint(_447.z)), level(1.0)).xyz * 2.0) - float3(1.0)) * roughness;
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
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _162)
{
    float3 pos = position - _162.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_162.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxel_light_data(thread const int3& voxel, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr)
{
    return texture_voxel_light_data.read(uint3(voxel), 0).xy;
}

static inline __attribute__((always_inline))
int voxel_light_index(thread const int& index, const device voxel_light_indices_block& _546)
{
    return _546.voxel_light_indices[index];
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _162)
{
    float3 color = (l.color.xyz * l.color.w) * _162.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_162.saturation));
}

static inline __attribute__((always_inline))
void cubemap_face_uv(thread const float3& dir, thread int& face, thread float2& face_uv, thread float& ma)
{
    float3 ad = abs(dir);
    bool _609 = ad.x >= ad.y;
    bool _617;
    if (_609)
    {
        _617 = ad.x >= ad.z;
    }
    else
    {
        _617 = _609;
    }
    float sc;
    float tc;
    if (_617)
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
        bool _646 = ad.y >= ad.x;
        bool _654;
        if (_646)
        {
            _654 = ad.y >= ad.z;
        }
        else
        {
            _654 = _646;
        }
        if (_654)
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
float sample_shadow_atlas(thread const light_t& light, thread const int& index, thread const common_vertex_t& v, thread const common_fragment_t& f, thread const float& atten, constant uniforms_block& _162, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr)
{
    float tile_uv = light.shadow.z;
    if (tile_uv == 0.0)
    {
        return 1.0;
    }
    float3 light_to_frag = v.model_position - light.origin.xyz;
    float dist_to_light = length(light_to_frag);
    float current_depth = dist_to_light / _162.depth_range.y;
    float3 param = light_to_frag;
    int param_1;
    float2 param_2;
    float param_3;
    cubemap_face_uv(param, param_1, param_2, param_3);
    int face = param_1;
    float2 fuv = param_2;
    float ma = param_3;
    fuv.y = 1.0 - fuv.y;
    int face_col = face - ((face / 3) * 3);
    int face_row = face / 3;
    float2 tile_origin = light.shadow.xy + float2(float(face_col) * tile_uv, float(face_row) * tile_uv);
    float2 half_texel = float2(0.5) / float2(int2(texture_shadow_atlas.get_width(), texture_shadow_atlas.get_height()));
    float2 tile_min = tile_origin + half_texel;
    float2 tile_max = (tile_origin + float2(tile_uv)) - half_texel;
    float light_size = light.origin.w * 3.0;
    float filter_radius = (light_size * (dist_to_light / light.origin.w)) * 0.004999999888241291046142578125;
    float filter_uv = filter_radius / (2.0 * fast::max(ma, 0.001000000047497451305389404296875));
    float importance = atten * fast::clamp(1.0 - (f.view_dist / 2048.0), 0.0, 1.0);
    int _822;
    if (importance > 0.300000011920928955078125)
    {
        _822 = 8;
    }
    else
    {
        _822 = (importance > 0.100000001490116119384765625) ? 4 : 2;
    }
    int num_samples = _822;
    float s = f.shadow_sin_cos.x;
    float c = f.shadow_sin_cos.y;
    float shadow = 0.0;
    for (int i = 0; i < num_samples; i++)
    {
        float2 rotated = float2((c * _901[i].x) - (s * _901[i].y), (s * _901[i].x) + (c * _901[i].y));
        float2 sample_fuv = fuv + (rotated * filter_uv);
        float2 atlas_uv = tile_origin + (sample_fuv * float2(tile_uv));
        atlas_uv = fast::clamp(atlas_uv, tile_min, tile_max);
        float3 _951 = float3(atlas_uv, current_depth);
        shadow += texture_shadow_atlas.sample_compare(texture_shadow_atlasSmplr, _951.xy, _951.z);
    }
    return shadow / float(num_samples);
}

static inline __attribute__((always_inline))
float parallax_self_shadow(thread const float3& light_dir, thread const common_vertex_t& v, thread const common_fragment_t& f, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    int max_steps = int(mix(16.0, 4.0, fast::min(f.lod * 0.3300000131130218505859375, 1.0)));
    float step_scale = mix(1.0, 2.5, fast::min(f.lod * 0.5, 1.0));
    float2 texel = float2(1.0) / float2(int3(texture_material.get_width(), texture_material.get_height(), texture_material.get_array_size()).xy);
    float3 dir = fast::normalize(float3(dot(light_dir, v.tangent), dot(light_dir, v.bitangent), dot(light_dir, v.normal)));
    float3 delta = float3(dir.xy * texel, fast::max(dir.z * length(texel), 0.00999999977648258209228515625)) * step_scale;
    float2 param = f.parallax;
    float param_1 = f.lod;
    float3 texcoord = float3(f.parallax, sample_material_heightmap(param, param_1, texture_material, texture_materialSmplr));
    float max_height = texcoord.z;
    int i = 0;
    for (;;)
    {
        bool _1165 = i < max_steps;
        bool _1171;
        if (_1165)
        {
            _1171 = texcoord.z < 1.0;
        }
        else
        {
            _1171 = _1165;
        }
        if (_1171)
        {
            texcoord += delta;
            float2 param_2 = texcoord.xy;
            float param_3 = f.lod;
            max_height = fast::max(max_height, sample_material_heightmap(param_2, param_3, texture_material, texture_materialSmplr));
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
    float shadow = 1.0 - ((max_height - texcoord.z) * material.shadow);
    return fast::clamp(shadow, 0.0, 1.0);
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
void fragment_light(thread const common_vertex_t& v, thread common_fragment_t& f, thread const int& index, constant uniforms_block& _162, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, const device lights_block& _1204)
{
    light_t light;
    light.origin = _1204.lights[index].origin;
    light.color = _1204.lights[index].color;
    light.shadow = _1204.lights[index].shadow;
    float3 dir = light.origin.xyz - v.model_position;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return;
    }
    dir = fast::normalize(_162.view * float4(dir, 0.0)).xyz;
    bool is_blend = (material.surface & 112) != int(0u);
    bool is_liquid = (material.surface & 8) != int(0u);
    bool is_stage = false;
    float lambert = dot(dir, f.normal_sample);
    float _1273;
    if ((is_blend || is_liquid) || is_stage)
    {
        _1273 = abs(lambert);
    }
    else
    {
        _1273 = fast::max(0.0, lambert);
    }
    lambert = _1273;
    if ((atten * lambert) <= 0.0)
    {
        return;
    }
    light_t param = light;
    float3 color = light_color(param, _162) * atten;
    light_t param_1 = light;
    int param_2 = index;
    common_vertex_t param_3 = v;
    common_fragment_t param_4 = f;
    float param_5 = atten;
    float shadow = sample_shadow_atlas(param_1, param_2, param_3, param_4, param_5, _162, texture_shadow_atlas, texture_shadow_atlasSmplr);
    if (!is_stage)
    {
        bool _1313 = f.lod < 4.0;
        bool _1319;
        if (_1313)
        {
            _1319 = material.shadow > 0.0;
        }
        else
        {
            _1319 = _1313;
        }
        if (_1319)
        {
            float3 param_6 = dir;
            common_vertex_t param_7 = v;
            common_fragment_t param_8 = f;
            shadow *= parallax_self_shadow(param_6, param_7, param_8, texture_material, texture_materialSmplr, material);
        }
    }
    if (shadow <= 0.0)
    {
        return;
    }
    f.diffuse += ((color * lambert) * shadow);
    float3 param_9 = color * shadow;
    float3 param_10 = dir;
    common_fragment_t param_11 = f;
    f.specular += blinn_phong(param_9, param_10, param_11);
}

static inline __attribute__((always_inline))
float3 voxel_caustics(thread const float3& texcoord, constant uniforms_block& _162, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 encoded = texture_voxel_caustics.sample(texture_voxel_causticsSmplr, texcoord).xyz;
    return ((encoded * 2.0) - float3(1.0)) * _162.caustics;
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
    float3 _267 = hash33(param);
    float3 param_1 = pi + float3(1.0, 0.0, 0.0);
    float3 _275 = hash33(param_1);
    float3 param_2 = pi + float3(0.0, 0.0, 1.0);
    float3 _286 = hash33(param_2);
    float3 param_3 = pi + float3(1.0, 0.0, 1.0);
    float3 _294 = hash33(param_3);
    float3 param_4 = pi + float3(0.0, 1.0, 0.0);
    float3 _308 = hash33(param_4);
    float3 param_5 = pi + float3(1.0, 1.0, 0.0);
    float3 _316 = hash33(param_5);
    float3 param_6 = pi + float3(0.0, 1.0, 1.0);
    float3 _327 = hash33(param_6);
    float3 param_7 = pi + float3(1.0);
    float3 _335 = hash33(param_7);
    return mix(mix(mix(dot(pf - float3(0.0), _267), dot(pf - float3(1.0, 0.0, 0.0), _275), w.x), mix(dot(pf - float3(0.0, 0.0, 1.0), _286), dot(pf - float3(1.0, 0.0, 1.0), _294), w.x), w.z), mix(mix(dot(pf - float3(0.0, 1.0, 0.0), _308), dot(pf - float3(1.0, 1.0, 0.0), _316), w.x), mix(dot(pf - float3(0.0, 1.0, 1.0), _327), dot(pf - float3(1.0), _335), w.x), w.z), w.y);
}

static inline __attribute__((always_inline))
void fragment_caustics(thread const common_vertex_t& v, thread common_fragment_t& f, constant uniforms_block& _162, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 param = v.voxel;
    float3 caustics_sample = voxel_caustics(param, _162, texture_voxel_caustics, texture_voxel_causticsSmplr);
    float caustics_strength = length(caustics_sample);
    if (caustics_strength == 0.0)
    {
        return;
    }
    float3 caustics_dir = fast::normalize(float3x3(_162.view[0].xyz, _162.view[1].xyz, _162.view[2].xyz) * caustics_sample);
    float facing = dot(v.normal, caustics_dir);
    float backface = (facing < (-0.25)) ? 0.25 : 1.0;
    f.caustics = caustics_strength * backface;
    if (f.caustics == 0.0)
    {
        return;
    }
    float3 param_1 = (v.model_position * 0.0500000007450580596923828125) + float3((float(_162.ticks) / 1000.0) * 0.5);
    float _noise = noise3d(param_1);
    float thickness = 0.0199999995529651641845703125;
    float glow = 5.0;
    _noise = fast::clamp(powr((1.0 - abs(_noise)) + thickness, glow), 0.0, 1.0);
    float3 light = f.ambient + f.diffuse;
    f.diffuse += fast::max(float3(0.0), (light * f.caustics) * _noise);
}

static inline __attribute__((always_inline))
void fragment_lighting(thread const common_vertex_t& v, thread common_fragment_t& f, constant uniforms_block& _162, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device voxel_light_indices_block& _546, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, const device lights_block& _1204, texturecube<float> texture_sky, sampler texture_skySmplr, constant light_cull_block& _1459)
{
    float3 param = v.voxel;
    float occlusion = voxel_occlusion(param, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 param_1 = v.voxel;
    float exposure = voxel_exposure(param_1, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 sky = texture_sky.sample(texture_skySmplr, fast::normalize(v.model_normal), level(6.0)).xyz;
    f.ambient = ((powr(float3(2.0) + sky, float3(2.0)) * exposure) * (1.0 - (occlusion * _162.ambient_occlusion))) * _162.ambient;
    f.diffuse = float3(0.0);
    f.specular = float3(0.0);
    if (_162.editor == 0)
    {
        float3 param_2 = v.model_position;
        int3 voxel_coord = voxel_xyz(param_2, _162);
        int3 param_3 = voxel_coord;
        int2 data = voxel_light_data(param_3, texture_voxel_light_data, texture_voxel_light_dataSmplr);
        for (int i = 0; i < data.y; i++)
        {
            int param_4 = data.x + i;
            int index = voxel_light_index(param_4, _546);
            common_vertex_t param_5 = v;
            common_fragment_t param_6 = f;
            int param_7 = index;
            fragment_light(param_5, param_6, param_7, _162, texture_material, texture_materialSmplr, material, texture_shadow_atlas, texture_shadow_atlasSmplr, _1204);
            f = param_6;
        }
    }
    int num_dynamic = _1204.num_lights - _1204.num_bsp_lights;
    for (int j = 0; j < num_dynamic; j++)
    {
        if ((_1459.active_lights[j >> 5] & (1u << uint(j & 31))) != 0u)
        {
            common_vertex_t param_8 = v;
            common_fragment_t param_9 = f;
            int param_10 = _1204.num_bsp_lights + j;
            fragment_light(param_8, param_9, param_10, _162, texture_material, texture_materialSmplr, material, texture_shadow_atlas, texture_shadow_atlasSmplr, _1204);
            f = param_9;
        }
    }
    common_vertex_t param_11 = v;
    common_fragment_t param_12 = f;
    fragment_caustics(param_11, param_12, _162, texture_voxel_caustics, texture_voxel_causticsSmplr);
    f = param_12;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _162 [[buffer(0)]], constant light_cull_block& _1459 [[buffer(1)]], constant material_block& material [[buffer(2)]], constant tint_block& _1714 [[buffer(3)]], const device lights_block& _1204 [[buffer(4)]], const device voxel_light_indices_block& _546 [[buffer(5)]], texture2d_array<float> texture_material [[texture(0)]], texture3d<int> texture_voxel_light_data [[texture(1)]], depth2d<float> texture_shadow_atlas [[texture(2)]], texture3d<float> texture_voxel_caustics [[texture(3)]], texture3d<float> texture_voxel_occlusion [[texture(4)]], texturecube<float> texture_sky [[texture(5)]], sampler texture_materialSmplr [[sampler(0)]], sampler texture_voxel_light_dataSmplr [[sampler(1)]], sampler texture_shadow_atlasSmplr [[sampler(2)]], sampler texture_voxel_causticsSmplr [[sampler(3)]], sampler texture_voxel_occlusionSmplr [[sampler(4)]], sampler texture_skySmplr [[sampler(5)]])
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
    common_fragment_t fragment0;
    fragment0.view_dir = fast::normalize(-vertex0.position);
    fragment0.view_dist = length(vertex0.position);
    float2 _1662;
    _1662.x = texture_material.calculate_clamped_lod(texture_materialSmplr, vertex0.diffusemap);
    _1662.y = texture_material.calculate_unclamped_lod(texture_materialSmplr, vertex0.diffusemap);
    fragment0.lod = _1662.x;
    common_vertex_t param = vertex0;
    common_fragment_t param_1 = fragment0;
    parallax_occlusion_mapping(param, param_1, texture_material, texture_materialSmplr, material);
    fragment0 = param_1;
    float2 param_2 = fragment0.parallax;
    fragment0.diffuse_sample = sample_material_diffuse(param_2, texture_material, texture_materialSmplr);
    if ((material.surface & 1024) == 1024)
    {
        if (fragment0.diffuse_sample.w < material.alpha_test)
        {
            discard_fragment();
        }
    }
    float2 param_3 = fragment0.parallax;
    float4 tintmap = sample_material_tint(param_3, texture_material, texture_materialSmplr);
    float4 _1702 = fragment0.diffuse_sample;
    float3 _1704 = _1702.xyz * (1.0 - tintmap.w);
    fragment0.diffuse_sample.x = _1704.x;
    fragment0.diffuse_sample.y = _1704.y;
    fragment0.diffuse_sample.z = _1704.z;
    float4 _1725 = fragment0.diffuse_sample;
    float3 _1727 = _1725.xyz + ((_1714.tint_colors[0] * tintmap.x).xyz * tintmap.w);
    fragment0.diffuse_sample.x = _1727.x;
    fragment0.diffuse_sample.y = _1727.y;
    fragment0.diffuse_sample.z = _1727.z;
    float4 _1744 = fragment0.diffuse_sample;
    float3 _1746 = _1744.xyz + ((_1714.tint_colors[1] * tintmap.y).xyz * tintmap.w);
    fragment0.diffuse_sample.x = _1746.x;
    fragment0.diffuse_sample.y = _1746.y;
    fragment0.diffuse_sample.z = _1746.z;
    float4 _1763 = fragment0.diffuse_sample;
    float3 _1765 = _1763.xyz + ((_1714.tint_colors[2] * tintmap.z).xyz * tintmap.w);
    fragment0.diffuse_sample.x = _1765.x;
    fragment0.diffuse_sample.y = _1765.y;
    fragment0.diffuse_sample.z = _1765.z;
    out.out_color = fragment0.diffuse_sample * vertex0.color;
    float2 param_4 = fragment0.parallax;
    float3x3 param_5 = float3x3(float3(vertex0.tangent), float3(vertex0.bitangent), float3(vertex0.normal));
    fragment0.normal_sample = sample_material_normal(param_4, param_5, texture_material, texture_materialSmplr, material);
    float2 param_6 = fragment0.parallax;
    fragment0.specular_sample = sample_material_specular(param_6, texture_material, texture_materialSmplr, material);
    float3 param_7 = vertex0.model_position;
    float angle = random_angle(param_7);
    fragment0.shadow_sin_cos = float2(sin(angle), cos(angle));
    common_vertex_t param_8 = vertex0;
    common_fragment_t param_9 = fragment0;
    fragment_lighting(param_8, param_9, _162, texture_material, texture_materialSmplr, material, texture_voxel_light_data, texture_voxel_light_dataSmplr, _546, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas, texture_shadow_atlasSmplr, _1204, texture_sky, texture_skySmplr, _1459);
    fragment0 = param_9;
    float4 _1832 = out.out_color;
    float3 _1834 = _1832.xyz * (fragment0.ambient + fragment0.diffuse);
    out.out_color.x = _1834.x;
    out.out_color.y = _1834.y;
    out.out_color.z = _1834.z;
    float4 _1844 = out.out_color;
    float3 _1846 = _1844.xyz + fragment0.specular;
    out.out_color.x = _1846.x;
    out.out_color.y = _1846.y;
    out.out_color.z = _1846.z;
    return out;
}

