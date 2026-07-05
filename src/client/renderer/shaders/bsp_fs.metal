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
    int debug_voxel_lights;
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

constant spvUnsafeArray<float2, 16> _930 = spvUnsafeArray<float2, 16>({ float2(0.2770744860172271728515625, 0.69514548778533935546875), float2(-0.59327852725982666015625, -0.1203283965587615966796875), float2(0.449474990367889404296875, 0.246909797191619873046875), float2(-0.1460638940334320068359375, -0.5679666996002197265625), float2(0.64004981517791748046875, -0.407194793224334716796875), float2(-0.3631913959980010986328125, 0.79357779026031494140625), float2(0.124885700643062591552734375, -0.897523820400238037109375), float2(-0.7720317840576171875, 0.443845808506011962890625), float2(0.88518059253692626953125, 0.1653372943401336669921875), float2(-0.52380120754241943359375, -0.726029574871063232421875), float2(0.3642682135105133056640625, 0.596805393695831298828125), float2(-0.833170115947723388671875, -0.33283460140228271484375), float2(0.552725970745086669921875, -0.698580920696258544921875), float2(-0.24071229994297027587890625, 0.3153156936168670654296875), float2(0.72694051265716552734375, -0.14306400716304779052734375), float2(-0.64446747303009033203125, 0.64446747303009033203125) });
constant spvUnsafeArray<float, 8> _2001 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

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
float sample_material_heightmap(thread const float2& texcoord, thread const float& lod, texture2d_array<float> texture_material, sampler texture_materialSmplr)
{
    float3 _485 = float3(texcoord, 1.0);
    return texture_material.sample(texture_materialSmplr, _485.xy, uint(rint(_485.z)), level(lod)).w;
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
    bool _1564 = material.parallax == 0.0;
    bool _1571;
    if (!_1564)
    {
        _1571 = fragment0.lod > 4.0;
    }
    else
    {
        _1571 = _1564;
    }
    if (_1571)
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
    float3 _374 = float3(texcoord, 0.0);
    return texture_material.sample(texture_materialSmplr, _374.xy, uint(rint(_374.z)));
}

static inline __attribute__((always_inline))
float3 sample_material_normal(thread const float2& texcoord, thread const float3x3& tbn, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material)
{
    float3 _383 = float3(texcoord, 1.0);
    float3 normalmap = (texture_material.sample(texture_materialSmplr, _383.xy, uint(rint(_383.z))).xyz * 2.0) - float3(1.0);
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
    float3 _413 = float3(texcoord, 2.0);
    float3 _418 = texture_material.sample(texture_materialSmplr, _413.xy, uint(rint(_413.z))).xyz * material.hardness;
    float4 specularmap;
    specularmap.x = _418.x;
    specularmap.y = _418.y;
    specularmap.z = _418.z;
    float3 roughness = float3(float2(material.roughness), 1.0);
    float3 _437 = float3(texcoord, 1.0);
    float3 normalmap0 = ((texture_material.sample(texture_materialSmplr, _437.xy, uint(rint(_437.z)), level(0.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float3 _450 = float3(texcoord, 1.0);
    float3 normalmap1 = ((texture_material.sample(texture_materialSmplr, _450.xy, uint(rint(_450.z)), level(1.0)).xyz * 2.0) - float3(1.0)) * roughness;
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
int3 voxel_xyz(thread const float3& position, constant uniforms_block& _166)
{
    float3 pos = position - _166.voxels.mins.xyz;
    int3 voxel = int3(floor((pos / float3(32.0)) + float3(0.001000000047497451305389404296875)));
    return clamp(voxel, int3(0), int3(_166.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxel_light_data(thread const int3& voxel, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr)
{
    return texture_voxel_light_data.read(uint3(voxel), 0).xy;
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
int voxel_light_index(thread const int& index, const device voxel_light_indices_block& _576)
{
    return _576.voxel_light_indices[index];
}

static inline __attribute__((always_inline))
float3 light_color(thread const light_t& l, constant uniforms_block& _166)
{
    float3 color = (l.color.xyz * l.color.w) * _166.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_166.saturation));
}

static inline __attribute__((always_inline))
void cubemap_face_uv(thread const float3& dir, thread int& face, thread float2& face_uv, thread float& ma)
{
    float3 ad = abs(dir);
    bool _637 = ad.x >= ad.y;
    bool _645;
    if (_637)
    {
        _645 = ad.x >= ad.z;
    }
    else
    {
        _645 = _637;
    }
    float sc;
    float tc;
    if (_645)
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
        bool _674 = ad.y >= ad.x;
        bool _682;
        if (_674)
        {
            _682 = ad.y >= ad.z;
        }
        else
        {
            _682 = _674;
        }
        if (_682)
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
float sample_shadow_atlas(thread const light_t& light, thread const int& index, thread const common_vertex_t& v, thread const common_fragment_t& f, thread const float& atten, constant uniforms_block& _166, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr)
{
    float tile_uv = light.shadow.z;
    if (tile_uv == 0.0)
    {
        return 1.0;
    }
    float3 light_to_frag = v.model_position - light.origin.xyz;
    float dist_to_light = length(light_to_frag);
    float current_depth = dist_to_light / _166.depth_range.y;
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
    int _851;
    if (importance > 0.300000011920928955078125)
    {
        _851 = 8;
    }
    else
    {
        _851 = (importance > 0.100000001490116119384765625) ? 4 : 2;
    }
    int num_samples = _851;
    float s = f.shadow_sin_cos.x;
    float c = f.shadow_sin_cos.y;
    float shadow = 0.0;
    for (int i = 0; i < num_samples; i++)
    {
        float2 rotated = float2((c * _930[i].x) - (s * _930[i].y), (s * _930[i].x) + (c * _930[i].y));
        float2 sample_fuv = fuv + (rotated * filter_uv);
        float2 atlas_uv = tile_origin + (sample_fuv * float2(tile_uv));
        atlas_uv = fast::clamp(atlas_uv, tile_min, tile_max);
        float3 _980 = float3(atlas_uv, current_depth);
        shadow += texture_shadow_atlas.sample_compare(texture_shadow_atlasSmplr, _980.xy, _980.z);
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
        bool _1192 = i < max_steps;
        bool _1198;
        if (_1192)
        {
            _1198 = texcoord.z < 1.0;
        }
        else
        {
            _1198 = _1192;
        }
        if (_1198)
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
void fragment_light(thread const common_vertex_t& v, thread common_fragment_t& f, thread const int& index, constant uniforms_block& _166, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, const device lights_block& _1232)
{
    light_t light;
    light.origin = _1232.lights[index].origin;
    light.color = _1232.lights[index].color;
    light.shadow = _1232.lights[index].shadow;
    float3 dir = light.origin.xyz - v.model_position;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return;
    }
    dir = fast::normalize(_166.view * float4(dir, 0.0)).xyz;
    bool is_blend = (material.surface & 112) != int(0u);
    bool is_liquid = (material.surface & 8) != int(0u);
    bool is_stage = material.flags != 0;
    float lambert = dot(dir, f.normal_sample);
    float _1303;
    if ((is_blend || is_liquid) || is_stage)
    {
        _1303 = abs(lambert);
    }
    else
    {
        _1303 = fast::max(0.0, lambert);
    }
    lambert = _1303;
    if ((atten * lambert) <= 0.0)
    {
        return;
    }
    light_t param = light;
    float3 color = light_color(param, _166) * atten;
    light_t param_1 = light;
    int param_2 = index;
    common_vertex_t param_3 = v;
    common_fragment_t param_4 = f;
    float param_5 = atten;
    float shadow = sample_shadow_atlas(param_1, param_2, param_3, param_4, param_5, _166, texture_shadow_atlas, texture_shadow_atlasSmplr);
    if (!is_stage)
    {
        bool _1343 = f.lod < 4.0;
        bool _1349;
        if (_1343)
        {
            _1349 = material.shadow > 0.0;
        }
        else
        {
            _1349 = _1343;
        }
        if (_1349)
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
float3 voxel_caustics(thread const float3& texcoord, constant uniforms_block& _166, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 encoded = texture_voxel_caustics.sample(texture_voxel_causticsSmplr, texcoord).xyz;
    return ((encoded * 2.0) - float3(1.0)) * _166.caustics;
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
    float3 _271 = hash33(param);
    float3 param_1 = pi + float3(1.0, 0.0, 0.0);
    float3 _279 = hash33(param_1);
    float3 param_2 = pi + float3(0.0, 0.0, 1.0);
    float3 _290 = hash33(param_2);
    float3 param_3 = pi + float3(1.0, 0.0, 1.0);
    float3 _298 = hash33(param_3);
    float3 param_4 = pi + float3(0.0, 1.0, 0.0);
    float3 _312 = hash33(param_4);
    float3 param_5 = pi + float3(1.0, 1.0, 0.0);
    float3 _320 = hash33(param_5);
    float3 param_6 = pi + float3(0.0, 1.0, 1.0);
    float3 _331 = hash33(param_6);
    float3 param_7 = pi + float3(1.0);
    float3 _339 = hash33(param_7);
    return mix(mix(mix(dot(pf - float3(0.0), _271), dot(pf - float3(1.0, 0.0, 0.0), _279), w.x), mix(dot(pf - float3(0.0, 0.0, 1.0), _290), dot(pf - float3(1.0, 0.0, 1.0), _298), w.x), w.z), mix(mix(dot(pf - float3(0.0, 1.0, 0.0), _312), dot(pf - float3(1.0, 1.0, 0.0), _320), w.x), mix(dot(pf - float3(0.0, 1.0, 1.0), _331), dot(pf - float3(1.0), _339), w.x), w.z), w.y);
}

static inline __attribute__((always_inline))
void fragment_caustics(thread const common_vertex_t& v, thread common_fragment_t& f, constant uniforms_block& _166, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr)
{
    float3 param = v.voxel;
    float3 caustics_sample = voxel_caustics(param, _166, texture_voxel_caustics, texture_voxel_causticsSmplr);
    float caustics_strength = length(caustics_sample);
    if (caustics_strength == 0.0)
    {
        return;
    }
    float3 caustics_dir = fast::normalize(float3x3(_166.view[0].xyz, _166.view[1].xyz, _166.view[2].xyz) * caustics_sample);
    float facing = dot(v.normal, caustics_dir);
    float backface = (facing < (-0.25)) ? 0.25 : 1.0;
    f.caustics = caustics_strength * backface;
    if (f.caustics == 0.0)
    {
        return;
    }
    float3 param_1 = (v.model_position * 0.0500000007450580596923828125) + float3((float(_166.ticks) / 1000.0) * 0.5);
    float _noise = noise3d(param_1);
    float thickness = 0.0199999995529651641845703125;
    float glow = 5.0;
    _noise = fast::clamp(powr((1.0 - abs(_noise)) + thickness, glow), 0.0, 1.0);
    float3 light = f.ambient + f.diffuse;
    f.diffuse += fast::max(float3(0.0), (light * f.caustics) * _noise);
}

static inline __attribute__((always_inline))
void fragment_lighting(thread const common_vertex_t& v, thread common_fragment_t& f, constant uniforms_block& _166, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device voxel_light_indices_block& _576, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, const device lights_block& _1232, texturecube<float> texture_sky, sampler texture_skySmplr, constant light_cull_block& _1526)
{
    if (_166.debug_voxel_lights != 0)
    {
        float3 param = v.model_position;
        int3 voxel_coord = voxel_xyz(param, _166);
        int3 param_1 = voxel_coord;
        int2 data = voxel_light_data(param_1, texture_voxel_light_data, texture_voxel_light_dataSmplr);
        float hash = fract(sin(float(data.x) * 12.98980045318603515625) * 43758.546875);
        f.ambient = float3(hash, float(data.y) / 8.0, float(data.y == 0));
        f.diffuse = float3(0.0);
        f.specular = float3(0.0);
        return;
    }
    float3 param_2 = v.voxel;
    float occlusion = voxel_occlusion(param_2, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 param_3 = v.voxel;
    float exposure = voxel_exposure(param_3, texture_voxel_occlusion, texture_voxel_occlusionSmplr);
    float3 sky = texture_sky.sample(texture_skySmplr, fast::normalize(v.model_normal), level(6.0)).xyz;
    f.ambient = ((powr(float3(2.0) + sky, float3(2.0)) * exposure) * (1.0 - (occlusion * _166.ambient_occlusion))) * _166.ambient;
    f.diffuse = float3(0.0);
    f.specular = float3(0.0);
    if (_166.editor == 0)
    {
        float3 param_4 = v.model_position;
        int3 voxel_coord_1 = voxel_xyz(param_4, _166);
        int3 param_5 = voxel_coord_1;
        int2 data_1 = voxel_light_data(param_5, texture_voxel_light_data, texture_voxel_light_dataSmplr);
        for (int i = 0; i < data_1.y; i++)
        {
            int param_6 = data_1.x + i;
            int index = voxel_light_index(param_6, _576);
            common_vertex_t param_7 = v;
            common_fragment_t param_8 = f;
            int param_9 = index;
            fragment_light(param_7, param_8, param_9, _166, texture_material, texture_materialSmplr, material, texture_shadow_atlas, texture_shadow_atlasSmplr, _1232);
            f = param_8;
        }
    }
    int num_dynamic = _1232.num_lights - _1232.num_bsp_lights;
    for (int j = 0; j < num_dynamic; j++)
    {
        if ((_1526.active_lights[j >> 5] & (1u << uint(j & 31))) != 0u)
        {
            common_vertex_t param_10 = v;
            common_fragment_t param_11 = f;
            int param_12 = _1232.num_bsp_lights + j;
            fragment_light(param_10, param_11, param_12, _166, texture_material, texture_materialSmplr, material, texture_shadow_atlas, texture_shadow_atlasSmplr, _1232);
            f = param_11;
        }
    }
    common_vertex_t param_13 = v;
    common_fragment_t param_14 = f;
    fragment_caustics(param_13, param_14, _166, texture_voxel_caustics, texture_voxel_causticsSmplr);
    f = param_14;
}

static inline __attribute__((always_inline))
void bsp_fragment_lighting(thread const common_vertex_t& vertex0, thread common_fragment_t& fragment0, constant uniforms_block& _166, texture2d_array<float> texture_material, sampler texture_materialSmplr, constant material_block& material, texture3d<int> texture_voxel_light_data, sampler texture_voxel_light_dataSmplr, const device voxel_light_indices_block& _576, texture3d<float> texture_voxel_caustics, sampler texture_voxel_causticsSmplr, texture3d<float> texture_voxel_occlusion, sampler texture_voxel_occlusionSmplr, depth2d<float> texture_shadow_atlas, sampler texture_shadow_atlasSmplr, const device lights_block& _1232, texturecube<float> texture_sky, sampler texture_skySmplr, constant light_cull_block& _1526)
{
    if (fragment0.view_dist >= _166.lighting_distance)
    {
        fragment0.ambient = vertex0.ambient;
        fragment0.diffuse = vertex0.diffuse;
        fragment0.specular = float3(0.0);
        return;
    }
    float2 param = fragment0.parallax;
    float3x3 param_1 = float3x3(float3(vertex0.tangent), float3(vertex0.bitangent), float3(vertex0.normal));
    fragment0.normal_sample = sample_material_normal(param, param_1, texture_material, texture_materialSmplr, material);
    float2 param_2 = fragment0.parallax;
    fragment0.specular_sample = sample_material_specular(param_2, texture_material, texture_materialSmplr, material);
    float3 param_3 = vertex0.model_position;
    float angle = random_angle(param_3);
    fragment0.shadow_sin_cos = float2(sin(angle), cos(angle));
    common_vertex_t param_4 = vertex0;
    common_fragment_t param_5 = fragment0;
    fragment_lighting(param_4, param_5, _166, texture_material, texture_materialSmplr, material, texture_voxel_light_data, texture_voxel_light_dataSmplr, _576, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas, texture_shadow_atlasSmplr, _1232, texture_sky, texture_skySmplr, _1526);
    fragment0 = param_5;
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

fragment main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _166 [[buffer(0)]], constant light_cull_block& _1526 [[buffer(1)]], constant material_block& material [[buffer(2)]], const device lights_block& _1232 [[buffer(3)]], const device voxel_light_indices_block& _576 [[buffer(4)]], texture2d_array<float> texture_material [[texture(0)]], texture3d<int> texture_voxel_light_data [[texture(1)]], depth2d<float> texture_shadow_atlas [[texture(2)]], texture3d<float> texture_voxel_caustics [[texture(3)]], texture3d<float> texture_voxel_occlusion [[texture(4)]], texturecube<float> texture_sky [[texture(5)]], texture2d<float> texture_stage [[texture(6)]], texture2d<float> texture_stage_next [[texture(7)]], texture2d<float> texture_warp [[texture(8)]], sampler texture_materialSmplr [[sampler(0)]], sampler texture_voxel_light_dataSmplr [[sampler(1)]], sampler texture_shadow_atlasSmplr [[sampler(2)]], sampler texture_voxel_causticsSmplr [[sampler(3)]], sampler texture_voxel_occlusionSmplr [[sampler(4)]], sampler texture_skySmplr [[sampler(5)]], sampler texture_stageSmplr [[sampler(6)]], sampler texture_stage_nextSmplr [[sampler(7)]], sampler texture_warpSmplr [[sampler(8)]], float4 gl_FragCoord [[position]])
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
    if (_166.wireframe != 0)
    {
        out.out_color = float4(0.800000011920928955078125, 0.800000011920928955078125, 0.800000011920928955078125, 1.0);
        return out;
    }
    fragment0.view_dir = fast::normalize(-vertex0.position);
    fragment0.view_dist = length(vertex0.position);
    float2 _1808;
    _1808.x = texture_material.calculate_clamped_lod(texture_materialSmplr, vertex0.diffusemap);
    _1808.y = texture_material.calculate_unclamped_lod(texture_materialSmplr, vertex0.diffusemap);
    fragment0.lod = _1808.x;
    common_vertex_t param = vertex0;
    common_fragment_t param_1 = fragment0;
    parallax_occlusion_mapping(param, param_1, texture_material, texture_materialSmplr, material);
    fragment0 = param_1;
    if (material.flags == 0)
    {
        float2 param_2 = fragment0.parallax;
        fragment0.diffuse_sample = sample_material_diffuse(param_2, texture_material, texture_materialSmplr);
        if ((material.surface & 1024) == 1024)
        {
            if (fragment0.diffuse_sample.w < material.alpha_test)
            {
                discard_fragment();
            }
        }
        out.out_color = fragment0.diffuse_sample;
        out.out_color *= vertex0.color;
        common_vertex_t param_3 = vertex0;
        common_fragment_t param_4 = fragment0;
        bsp_fragment_lighting(param_3, param_4, _166, texture_material, texture_materialSmplr, material, texture_voxel_light_data, texture_voxel_light_dataSmplr, _576, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas, texture_shadow_atlasSmplr, _1232, texture_sky, texture_skySmplr, _1526);
        fragment0 = param_4;
        float4 _1861 = out.out_color;
        float3 _1863 = _1861.xyz * (fragment0.ambient + fragment0.diffuse);
        out.out_color.x = _1863.x;
        out.out_color.y = _1863.y;
        out.out_color.z = _1863.z;
        float4 _1872 = out.out_color;
        float3 _1874 = _1872.xyz + fragment0.specular;
        out.out_color.x = _1874.x;
        out.out_color.y = _1874.y;
        out.out_color.z = _1874.z;
    }
    else
    {
        float2 st = fragment0.parallax;
        if ((material.flags & 32768) == 32768)
        {
            st += ((texture_warp.sample(texture_warpSmplr, (st + float2((float(_166.ticks) * material.warp.x) * 0.00012500000593718141317367553710938))).xy - float2(0.5)) * material.warp.y);
        }
        float2 param_5 = st;
        fragment0.diffuse_sample = sample_material_stage(param_5, material, texture_stage, texture_stageSmplr, texture_stage_next, texture_stage_nextSmplr) * vertex0.color;
        out.out_color = fragment0.diffuse_sample;
        if ((material.flags & 65536) == 65536)
        {
            common_vertex_t param_6 = vertex0;
            common_fragment_t param_7 = fragment0;
            bsp_fragment_lighting(param_6, param_7, _166, texture_material, texture_materialSmplr, material, texture_voxel_light_data, texture_voxel_light_dataSmplr, _576, texture_voxel_caustics, texture_voxel_causticsSmplr, texture_voxel_occlusion, texture_voxel_occlusionSmplr, texture_shadow_atlas, texture_shadow_atlasSmplr, _1232, texture_sky, texture_skySmplr, _1526);
            fragment0 = param_7;
            float4 _1946 = out.out_color;
            float3 _1948 = _1946.xyz * mix(float3(1.0), fragment0.ambient + fragment0.diffuse, float3(material.lighting));
            out.out_color.x = _1948.x;
            out.out_color.y = _1948.y;
            out.out_color.z = _1948.z;
            float4 _1960 = out.out_color;
            float3 _1962 = _1960.xyz + (fragment0.specular * material.lighting);
            out.out_color.x = _1962.x;
            out.out_color.y = _1962.y;
            out.out_color.z = _1962.z;
        }
        if ((material.flags & 262144) == 262144)
        {
            float4 _1983 = out.out_color;
            float3 _1985 = _1983.xyz + (fragment0.diffuse_sample.xyz * material.emissive);
            out.out_color.x = _1985.x;
            out.out_color.y = _1985.y;
            out.out_color.z = _1985.z;
        }
    }
    return out;
}

