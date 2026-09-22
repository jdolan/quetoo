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

struct Light
{
    float4 origin;
    float4 color;
    float2 tile;
};

struct CommonVertex
{
    float3 modelPosition;
    float3 modelNormal;
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

struct CommonFragment
{
    float3 viewDir;
    float viewDist;
    float texLod;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float3x3 tbn;
    float2 parallax;
    float4 diffuseSample;
    float3 normalSample;
    float4 specularSample;
    float3 ambient;
    float3 diffuse;
    float3 specular;
    float caustics;
    float2 shadowSinCos;
};

struct materialBlock
{
    float4 color;
    float2 stOrigin;
    float2 stretch;
    float2 scroll;
    float2 scale;
    float2 terrain;
    float2 warp;
    int surface;
    float alphaTest;
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

struct Voxels
{
    float4 mins;
    float4 maxs;
    float4 viewCoordinate;
    float4 size;
};

struct uniformsBlock
{
    int4 viewport;
    float4x4 projection3D;
    float4x4 view;
    float4x4 skyProjection;
    float4x4 lightProjection;
    Voxels voxels;
    float2 depthRange;
    int viewType;
    int ticks;
    packed_float3 ambient;
    float modulate;
    float saturation;
    float caustics;
    float ambientOcclusion;
    float lightingDistance;
    int editor;
    int developer;
    float2 padding;
};

struct voxelLightDataBlock
{
    int voxelLightDataElements[1];
};

struct voxelLightIndicesBlock
{
    int voxelLightIndices[1];
};

struct Light_1
{
    float4 origin;
    float4 color;
    float2 tile;
};

struct bspLightsBlock
{
    int numBspLights;
    Light_1 bspLights[1];
};

struct dynamicLightsBlock
{
    int numDynamicLights;
    Light_1 dynamicLights[1];
};

struct bspLocalsBlock
{
    float4x4 model;
    uint4 activeDynamicLights[4];
    int subviewLayer;
    int subviewMirrored;
};

constant spvUnsafeArray<float2, 16> _1064 = spvUnsafeArray<float2, 16>({ float2(0.2770744860172271728515625, 0.69514548778533935546875), float2(-0.59327852725982666015625, -0.1203283965587615966796875), float2(0.449474990367889404296875, 0.246909797191619873046875), float2(-0.1460638940334320068359375, -0.5679666996002197265625), float2(0.64004981517791748046875, -0.407194793224334716796875), float2(-0.3631913959980010986328125, 0.79357779026031494140625), float2(0.124885700643062591552734375, -0.897523820400238037109375), float2(-0.7720317840576171875, 0.443845808506011962890625), float2(0.88518059253692626953125, 0.1653372943401336669921875), float2(-0.52380120754241943359375, -0.726029574871063232421875), float2(0.3642682135105133056640625, 0.596805393695831298828125), float2(-0.833170115947723388671875, -0.33283460140228271484375), float2(0.552725970745086669921875, -0.698580920696258544921875), float2(-0.24071229994297027587890625, 0.3153156936168670654296875), float2(0.72694051265716552734375, -0.14306400716304779052734375), float2(-0.64446747303009033203125, 0.64446747303009033203125) });
constant spvUnsafeArray<float, 8> _2364 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float4 outColor [[color(0)]];
    float outDepth [[color(1)]];
};

struct main0_in
{
    float3 vertex0_modelPosition [[user(locn0)]];
    float3 vertex0_modelNormal [[user(locn1)]];
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
float sampleMaterialHeightmap(thread const float2& texcoord, thread const float& lod, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr)
{
    float3 _470 = float3(texcoord, 1.0);
    return textureMaterial.sample(textureMaterialSmplr, _470.xy, uint(rint(_470.z)), level(lod)).w;
}

static inline __attribute__((always_inline))
float sampleMaterialDisplacement(thread const float2& texcoord, thread const float& lod, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr)
{
    float2 param = texcoord;
    float param_1 = lod;
    return 1.0 - sampleMaterialHeightmap(param, param_1, textureMaterial, textureMaterialSmplr);
}

static inline __attribute__((always_inline))
void parallaxOcclusionMapping(thread const CommonVertex& vertex0, thread CommonFragment& fragment0, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material, constant uniformsBlock& _522)
{
    fragment0.parallax = vertex0.diffusemap;
    bool _1788 = material.parallax == 0.0;
    bool _1795;
    if (!_1788)
    {
        _1795 = fragment0.texLod > 2.0;
    }
    else
    {
        _1795 = _1788;
    }
    bool _1805;
    if (!_1795)
    {
        _1805 = fragment0.viewDist >= (_522.lightingDistance + 128.0);
    }
    else
    {
        _1805 = _1795;
    }
    if (_1805)
    {
        return;
    }
    float numSamples = mix(32.0, 8.0, fast::min(fragment0.texLod * 0.25, 1.0));
    float2 texel = float2(1.0) / float2(int3(textureMaterial.get_width(), textureMaterial.get_height(), textureMaterial.get_array_size()).xy);
    float3 dir = fast::normalize(fragment0.viewDir * float3x3(float3(vertex0.tangent), float3(vertex0.bitangent), float3(vertex0.normal)));
    dir.z = fast::max(dir.z, 0.100000001490116119384765625);
    float2 p = (((dir.xy * texel) / float2(dir.z)) * material.parallax) * material.parallax;
    float2 delta = p / float2(numSamples);
    float2 texcoord = vertex0.diffusemap;
    float2 prevTexcoord = vertex0.diffusemap;
    float depth = 0.0;
    float layer = 1.0 / numSamples;
    float2 param = texcoord;
    float param_1 = fragment0.texLod;
    float displacement = sampleMaterialDisplacement(param, param_1, textureMaterial, textureMaterialSmplr);
    for (int i = 0; (i < int(numSamples)) && (depth < displacement); i++)
    {
        depth += layer;
        prevTexcoord = texcoord;
        texcoord -= delta;
        float2 param_2 = texcoord;
        float param_3 = fragment0.texLod;
        displacement = sampleMaterialDisplacement(param_2, param_3, textureMaterial, textureMaterialSmplr);
    }
    float a = displacement - depth;
    float2 param_4 = prevTexcoord;
    float param_5 = fragment0.texLod;
    float b = (sampleMaterialDisplacement(param_4, param_5, textureMaterial, textureMaterialSmplr) - depth) + layer;
    fragment0.parallax = mix(prevTexcoord, texcoord, float2(a / (a - b)));
}

static inline __attribute__((always_inline))
float4 sampleMaterialDiffuse(thread const float2& texcoord, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr)
{
    float3 _354 = float3(texcoord, 0.0);
    return textureMaterial.sample(textureMaterialSmplr, _354.xy, uint(rint(_354.z)));
}

static inline __attribute__((always_inline))
float3 sampleMaterialNormal(thread const float2& texcoord, thread const float3x3& tbn, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material)
{
    float3 _363 = float3(texcoord, 1.0);
    float3 normalmap = (textureMaterial.sample(textureMaterialSmplr, _363.xy, uint(rint(_363.z))).xyz * 2.0) - float3(1.0);
    float3 roughness = float3(float2(material.roughness), 1.0);
    return fast::normalize(tbn * (normalmap * roughness));
}

static inline __attribute__((always_inline))
float saturate0(thread const float& x)
{
    return fast::clamp(x, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float toksvigGloss(thread const float3& normal, thread const float& power)
{
    float param = length(normal);
    float lenRcp = 1.0 / saturate0(param);
    return 1.0 / (1.0 + (power * (lenRcp - 1.0)));
}

static inline __attribute__((always_inline))
float4 sampleMaterialSpecular(thread const float2& texcoord, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material)
{
    float3 _395 = float3(texcoord, 2.0);
    float3 _401 = textureMaterial.sample(textureMaterialSmplr, _395.xy, uint(rint(_395.z))).xyz * material.hardness;
    float4 specularmap;
    specularmap.x = _401.x;
    specularmap.y = _401.y;
    specularmap.z = _401.z;
    float3 roughness = float3(float2(material.roughness), 1.0);
    float3 _420 = float3(texcoord, 1.0);
    float3 normalmap0 = ((textureMaterial.sample(textureMaterialSmplr, _420.xy, uint(rint(_420.z)), level(0.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float3 _433 = float3(texcoord, 1.0);
    float3 normalmap1 = ((textureMaterial.sample(textureMaterialSmplr, _433.xy, uint(rint(_433.z)), level(1.0)).xyz * 2.0) - float3(1.0)) * roughness;
    float power = powr(1.0 + material.specularity, 4.0);
    float3 param = normalmap0;
    float param_1 = power;
    float3 param_2 = normalmap1;
    float param_3 = power;
    specularmap.w = power * fast::min(toksvigGloss(param, param_1), toksvigGloss(param_2, param_3));
    return specularmap;
}

static inline __attribute__((always_inline))
float randomAngle(thread const float3& seed)
{
    return fract(sin(dot(seed, float3(12.98980045318603515625, 78.233001708984375, 45.16400146484375))) * 43758.546875) * 6.28318500518798828125;
}

static inline __attribute__((always_inline))
float voxelOcclusion(thread const float3& texcoord, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr)
{
    return textureVoxelOcclusion.sample(textureVoxelOcclusionSmplr, texcoord).x;
}

static inline __attribute__((always_inline))
float voxelExposure(thread const float3& texcoord, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr)
{
    return fast::max(0.25, textureVoxelOcclusion.sample(textureVoxelOcclusionSmplr, texcoord).y);
}

static inline __attribute__((always_inline))
float3 ambientLight(thread const CommonVertex& v, constant uniformsBlock& _522, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr, texturecube<float> textureSky, sampler textureSkySmplr)
{
    float3 param = v.voxel;
    float occlusion = voxelOcclusion(param, textureVoxelOcclusion, textureVoxelOcclusionSmplr);
    float3 param_1 = v.voxel;
    float exposure = voxelExposure(param_1, textureVoxelOcclusion, textureVoxelOcclusionSmplr);
    float3 sky = textureSky.sample(textureSkySmplr, fast::normalize(v.modelNormal), level(6.0)).xyz;
    return ((powr(float3(2.0) + sky, float3(2.0)) * exposure) * (1.0 - (occlusion * _522.ambientOcclusion))) * float3(_522.ambient);
}

static inline __attribute__((always_inline))
int3 voxelXyz(thread const float3& position, constant uniformsBlock& _522)
{
    float3 pos = position - _522.voxels.mins.xyz;
    int3 voxel = int3(floor((pos / float3(32.0)) + float3(0.001000000047497451305389404296875)));
    return clamp(voxel, int3(0), int3(_522.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxelLightData(thread const int3& voxel, constant uniformsBlock& _522, const device voxelLightDataBlock& _573)
{
    int index = (((voxel.z * int(_522.voxels.size.y)) + voxel.y) * int(_522.voxels.size.x)) + voxel.x;
    return int2(_573.voxelLightDataElements[(index * 2) + 0], _573.voxelLightDataElements[(index * 2) + 1]);
}

static inline __attribute__((always_inline))
int voxelLightIndex(thread const int& index, const device voxelLightIndicesBlock& _591)
{
    return _591.voxelLightIndices[index];
}

static inline __attribute__((always_inline))
float3 lightColor(thread const Light& l, constant uniformsBlock& _522)
{
    float3 color = (l.color.xyz * l.color.w) * _522.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_522.saturation));
}

static inline __attribute__((always_inline))
void cubemapFaceUv(thread const float3& dir, thread int& face, thread float2& faceUv, thread float& ma)
{
    float3 ad = abs(dir);
    bool _695 = ad.x >= ad.y;
    bool _703;
    if (_695)
    {
        _703 = ad.x >= ad.z;
    }
    else
    {
        _703 = _695;
    }
    float sc;
    float tc;
    if (_703)
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
        bool _732 = ad.y >= ad.x;
        bool _740;
        if (_732)
        {
            _740 = ad.y >= ad.z;
        }
        else
        {
            _740 = _732;
        }
        if (_740)
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
    faceUv = (float2(sc, tc) / float2(2.0 * ma)) + float2(0.5);
}

static inline __attribute__((always_inline))
float sampleShadowFace(thread const int& face, thread const float3& uvw, depth2d<float> textureShadowAtlas0, sampler textureShadowAtlas0Smplr, depth2d<float> textureShadowAtlas1, sampler textureShadowAtlas1Smplr, depth2d<float> textureShadowAtlas2, sampler textureShadowAtlas2Smplr, depth2d<float> textureShadowAtlas3, sampler textureShadowAtlas3Smplr, depth2d<float> textureShadowAtlas4, sampler textureShadowAtlas4Smplr, depth2d<float> textureShadowAtlas5, sampler textureShadowAtlas5Smplr)
{
    if (face == 0)
    {
        return textureShadowAtlas0.sample_compare(textureShadowAtlas0Smplr, uvw.xy, uvw.z);
    }
    else
    {
        if (face == 1)
        {
            return textureShadowAtlas1.sample_compare(textureShadowAtlas1Smplr, uvw.xy, uvw.z);
        }
        else
        {
            if (face == 2)
            {
                return textureShadowAtlas2.sample_compare(textureShadowAtlas2Smplr, uvw.xy, uvw.z);
            }
            else
            {
                if (face == 3)
                {
                    return textureShadowAtlas3.sample_compare(textureShadowAtlas3Smplr, uvw.xy, uvw.z);
                }
                else
                {
                    if (face == 4)
                    {
                        return textureShadowAtlas4.sample_compare(textureShadowAtlas4Smplr, uvw.xy, uvw.z);
                    }
                    else
                    {
                        return textureShadowAtlas5.sample_compare(textureShadowAtlas5Smplr, uvw.xy, uvw.z);
                    }
                }
            }
        }
    }
}

static inline __attribute__((always_inline))
float sampleShadowAtlas(thread const Light& light, thread const CommonVertex& v, thread const CommonFragment& f, thread const float& atten, depth2d<float> textureShadowAtlas0, sampler textureShadowAtlas0Smplr, depth2d<float> textureShadowAtlas1, sampler textureShadowAtlas1Smplr, depth2d<float> textureShadowAtlas2, sampler textureShadowAtlas2Smplr, depth2d<float> textureShadowAtlas3, sampler textureShadowAtlas3Smplr, depth2d<float> textureShadowAtlas4, sampler textureShadowAtlas4Smplr, depth2d<float> textureShadowAtlas5, sampler textureShadowAtlas5Smplr)
{
    if (light.tile.x < 0.0)
    {
        return 1.0;
    }
    float2 texSize = float2(int2(textureShadowAtlas0.get_width(), textureShadowAtlas0.get_height()));
    float tilePx = texSize.x / 32.0;
    float2 tileOrigin = light.tile / texSize;
    float tileUv = tilePx / texSize.x;
    float3 lightToFrag = v.modelPosition - light.origin.xyz;
    float distToLight = length(lightToFrag);
    float lightSize = light.origin.w * 3.0;
    float filterRadius = (lightSize * (distToLight / light.origin.w)) * 0.004999999888241291046142578125;
    float nDotL = fast::max(dot(v.modelNormal, fast::normalize(-lightToFrag)), 0.0);
    float3 offsetPosition = v.modelPosition + ((v.modelNormal * filterRadius) * (1.0 - nDotL));
    lightToFrag = offsetPosition - light.origin.xyz;
    distToLight = length(lightToFrag);
    float currentDepth = distToLight / light.origin.w;
    float3 param = lightToFrag;
    int param_1;
    float2 param_2;
    float param_3;
    cubemapFaceUv(param, param_1, param_2, param_3);
    int face = param_1;
    float2 fuv = param_2;
    float ma = param_3;
    fuv.y = 1.0 - fuv.y;
    float2 halfTexel = float2(0.5) / texSize;
    float2 tileMin = tileOrigin + halfTexel;
    float2 tileMax = (tileOrigin + float2(tileUv)) - halfTexel;
    float filterUv = filterRadius / (2.0 * fast::max(ma, 0.001000000047497451305389404296875));
    float importance = atten * fast::clamp(1.0 - (f.viewDist / 2048.0), 0.0, 1.0);
    int _985;
    if (importance > 0.300000011920928955078125)
    {
        _985 = 8;
    }
    else
    {
        _985 = (importance > 0.100000001490116119384765625) ? 4 : 2;
    }
    int numSamples = _985;
    float s = f.shadowSinCos.x;
    float c = f.shadowSinCos.y;
    float shadow = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        float2 rotated = float2((c * _1064[i].x) - (s * _1064[i].y), (s * _1064[i].x) + (c * _1064[i].y));
        float2 sampleFuv = fuv + (rotated * filterUv);
        float2 atlasUv = tileOrigin + (sampleFuv * float2(tileUv));
        atlasUv = fast::clamp(atlasUv, tileMin, tileMax);
        int param_4 = face;
        float3 param_5 = float3(atlasUv, currentDepth);
        shadow += sampleShadowFace(param_4, param_5, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr);
    }
    return shadow / float(numSamples);
}

static inline __attribute__((always_inline))
float parallaxSelfShadow(thread const float3& lightDir, thread const CommonVertex& v, thread const CommonFragment& f, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material)
{
    int maxSteps = int(mix(12.0, 2.0, fast::min(f.texLod * 0.5, 1.0)));
    float stepScale = mix(1.0, 4.0, fast::min(f.texLod * 0.5, 1.0));
    float2 texel = float2(1.0) / float2(int3(textureMaterial.get_width(), textureMaterial.get_height(), textureMaterial.get_array_size()).xy);
    float3 dir = fast::normalize(float3(dot(lightDir, v.tangent), dot(lightDir, v.bitangent), dot(lightDir, v.normal)));
    float3 delta = float3(dir.xy * texel, fast::max(dir.z * length(texel), 0.00999999977648258209228515625)) * stepScale;
    float2 param = f.parallax;
    float param_1 = f.texLod;
    float3 texcoord = float3(f.parallax, sampleMaterialHeightmap(param, param_1, textureMaterial, textureMaterialSmplr));
    float maxHeight = texcoord.z;
    int i = 0;
    for (;;)
    {
        bool _1365 = i < maxSteps;
        bool _1371;
        if (_1365)
        {
            _1371 = texcoord.z < 1.0;
        }
        else
        {
            _1371 = _1365;
        }
        if (_1371 && (maxHeight < 1.0))
        {
            texcoord += delta;
            float2 param_2 = texcoord.xy;
            float param_3 = f.texLod;
            maxHeight = fast::max(maxHeight, sampleMaterialHeightmap(param_2, param_3, textureMaterial, textureMaterialSmplr));
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
    float shadow = 1.0 - ((maxHeight - texcoord.z) * material.shadow);
    return fast::clamp(shadow, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float blinn(thread const float3& lightDir, thread const CommonFragment& f)
{
    return powr(fast::max(0.0, dot(fast::normalize(lightDir + f.viewDir), f.normalSample)), f.specularSample.w);
}

static inline __attribute__((always_inline))
float3 blinnPhong(thread const float3& lightColor_1, thread const float3& lightDir, thread const CommonFragment& f)
{
    float3 param = lightDir;
    CommonFragment param_1 = f;
    return (lightColor_1 * f.specularSample.xyz) * blinn(param, param_1);
}

static inline __attribute__((always_inline))
void fragmentLight(thread const CommonVertex& v, thread CommonFragment& f, thread const Light& light, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material, constant uniformsBlock& _522, depth2d<float> textureShadowAtlas0, sampler textureShadowAtlas0Smplr, depth2d<float> textureShadowAtlas1, sampler textureShadowAtlas1Smplr, depth2d<float> textureShadowAtlas2, sampler textureShadowAtlas2Smplr, depth2d<float> textureShadowAtlas3, sampler textureShadowAtlas3Smplr, depth2d<float> textureShadowAtlas4, sampler textureShadowAtlas4Smplr, depth2d<float> textureShadowAtlas5, sampler textureShadowAtlas5Smplr)
{
    float3 dir = light.origin.xyz - v.modelPosition;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return;
    }
    dir = fast::normalize(_522.view * float4(dir, 0.0)).xyz;
    bool isBlend = (material.surface & 112) != int(0u);
    bool isLiquid = (material.surface & 8) != int(0u);
    bool isStage = material.flags != 0;
    float lambert = dot(dir, f.normalSample);
    float _1462;
    if ((isBlend || isLiquid) || isStage)
    {
        _1462 = abs(lambert);
    }
    else
    {
        _1462 = fast::max(0.0, lambert);
    }
    lambert = _1462;
    if ((atten * lambert) <= 0.0)
    {
        return;
    }
    Light param = light;
    float3 color = lightColor(param, _522) * atten;
    Light param_1 = light;
    CommonVertex param_2 = v;
    CommonFragment param_3 = f;
    float param_4 = atten;
    float shadow = sampleShadowAtlas(param_1, param_2, param_3, param_4, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr);
    bool _1495 = !isStage;
    bool _1501;
    if (_1495)
    {
        _1501 = material.shadow > 0.0;
    }
    else
    {
        _1501 = _1495;
    }
    bool _1507;
    if (_1501)
    {
        _1507 = f.texLod < 2.0;
    }
    else
    {
        _1507 = _1501;
    }
    if (_1507)
    {
        float3 param_5 = dir;
        CommonVertex param_6 = v;
        CommonFragment param_7 = f;
        shadow *= parallaxSelfShadow(param_5, param_6, param_7, textureMaterial, textureMaterialSmplr, material);
    }
    if (shadow <= 0.0)
    {
        return;
    }
    f.diffuse += ((color * lambert) * shadow);
    float3 param_8 = color * shadow;
    float3 param_9 = dir;
    CommonFragment param_10 = f;
    f.specular += blinnPhong(param_8, param_9, param_10);
}

static inline __attribute__((always_inline))
bool dynamicLightActive(thread const spvUnsafeArray<uint4, 4>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 voxelCaustics(thread const float3& texcoord, constant uniformsBlock& _522, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr)
{
    float3 encoded = textureVoxelCaustics.sample(textureVoxelCausticsSmplr, texcoord).xyz;
    return ((encoded * 2.0) - float3(1.0)) * _522.caustics;
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
    float3 _251 = hash33(param);
    float3 param_1 = pi + float3(1.0, 0.0, 0.0);
    float3 _259 = hash33(param_1);
    float3 param_2 = pi + float3(0.0, 0.0, 1.0);
    float3 _270 = hash33(param_2);
    float3 param_3 = pi + float3(1.0, 0.0, 1.0);
    float3 _278 = hash33(param_3);
    float3 param_4 = pi + float3(0.0, 1.0, 0.0);
    float3 _292 = hash33(param_4);
    float3 param_5 = pi + float3(1.0, 1.0, 0.0);
    float3 _300 = hash33(param_5);
    float3 param_6 = pi + float3(0.0, 1.0, 1.0);
    float3 _311 = hash33(param_6);
    float3 param_7 = pi + float3(1.0);
    float3 _319 = hash33(param_7);
    return mix(mix(mix(dot(pf - float3(0.0), _251), dot(pf - float3(1.0, 0.0, 0.0), _259), w.x), mix(dot(pf - float3(0.0, 0.0, 1.0), _270), dot(pf - float3(1.0, 0.0, 1.0), _278), w.x), w.z), mix(mix(dot(pf - float3(0.0, 1.0, 0.0), _292), dot(pf - float3(1.0, 1.0, 0.0), _300), w.x), mix(dot(pf - float3(0.0, 1.0, 1.0), _311), dot(pf - float3(1.0), _319), w.x), w.z), w.y);
}

static inline __attribute__((always_inline))
void fragmentCaustics(thread const CommonVertex& v, thread CommonFragment& f, constant uniformsBlock& _522, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr)
{
    float3 param = v.voxel;
    float3 causticsSample = voxelCaustics(param, _522, textureVoxelCaustics, textureVoxelCausticsSmplr);
    float causticsStrength = length(causticsSample);
    if (causticsStrength == 0.0)
    {
        return;
    }
    float3 causticsDir = fast::normalize(float3x3(_522.view[0].xyz, _522.view[1].xyz, _522.view[2].xyz) * causticsSample);
    float facing = dot(v.normal, causticsDir);
    float backface = (facing < (-0.25)) ? 0.25 : 1.0;
    f.caustics = causticsStrength * backface;
    if (f.caustics == 0.0)
    {
        return;
    }
    float3 param_1 = (v.modelPosition * 0.0500000007450580596923828125) + float3((float(_522.ticks) / 1000.0) * 0.5);
    float _noise = noise3d(param_1);
    float thickness = 0.0199999995529651641845703125;
    float glow = 5.0;
    _noise = fast::clamp(powr((1.0 - abs(_noise)) + thickness, glow), 0.0, 1.0);
    float3 light = f.ambient + f.diffuse;
    f.diffuse += fast::max(float3(0.0), (light * f.caustics) * _noise);
}

static inline __attribute__((always_inline))
void fragmentLighting(thread const CommonVertex& v, thread CommonFragment& f, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material, constant uniformsBlock& _522, const device voxelLightDataBlock& _573, const device voxelLightIndicesBlock& _591, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr, depth2d<float> textureShadowAtlas0, sampler textureShadowAtlas0Smplr, depth2d<float> textureShadowAtlas1, sampler textureShadowAtlas1Smplr, depth2d<float> textureShadowAtlas2, sampler textureShadowAtlas2Smplr, depth2d<float> textureShadowAtlas3, sampler textureShadowAtlas3Smplr, depth2d<float> textureShadowAtlas4, sampler textureShadowAtlas4Smplr, depth2d<float> textureShadowAtlas5, sampler textureShadowAtlas5Smplr, texturecube<float> textureSky, sampler textureSkySmplr, const device bspLightsBlock& _1588, const device dynamicLightsBlock& _1618, constant bspLocalsBlock& _1625)
{
    CommonVertex param = v;
    f.ambient = ambientLight(param, _522, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureSky, textureSkySmplr);
    f.diffuse = float3(0.0);
    f.specular = float3(0.0);
    if (_522.editor == 0)
    {
        float3 param_1 = v.modelPosition;
        int3 voxelCoord = voxelXyz(param_1, _522);
        int3 param_2 = voxelCoord;
        int2 data = voxelLightData(param_2, _522, _573);
        Light param_6;
        for (int i = 0; i < data.y; i++)
        {
            int param_3 = data.x + i;
            int index = voxelLightIndex(param_3, _591);
            CommonVertex param_4 = v;
            CommonFragment param_5 = f;
            param_6.origin = _1588.bspLights[index].origin;
            param_6.color = _1588.bspLights[index].color;
            param_6.tile = _1588.bspLights[index].tile;
            fragmentLight(param_4, param_5, param_6, textureMaterial, textureMaterialSmplr, material, _522, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr);
            f = param_5;
        }
    }
    spvUnsafeArray<uint4, 4> param_7;
    Light param_11;
    for (int j = 0; j < _1618.numDynamicLights; j++)
    {
        param_7[0] = _1625.activeDynamicLights[0];
        param_7[1] = _1625.activeDynamicLights[1];
        param_7[2] = _1625.activeDynamicLights[2];
        param_7[3] = _1625.activeDynamicLights[3];
        int param_8 = j;
        if (dynamicLightActive(param_7, param_8))
        {
            CommonVertex param_9 = v;
            CommonFragment param_10 = f;
            param_11.origin = _1618.dynamicLights[j].origin;
            param_11.color = _1618.dynamicLights[j].color;
            param_11.tile = _1618.dynamicLights[j].tile;
            fragmentLight(param_9, param_10, param_11, textureMaterial, textureMaterialSmplr, material, _522, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr);
            f = param_10;
        }
    }
    CommonVertex param_12 = v;
    CommonFragment param_13 = f;
    fragmentCaustics(param_12, param_13, _522, textureVoxelCaustics, textureVoxelCausticsSmplr);
    f = param_13;
}

static inline __attribute__((always_inline))
void fragmentLightingLod(thread const CommonVertex& v, thread CommonFragment& f, texture2d_array<float> textureMaterial, sampler textureMaterialSmplr, constant materialBlock& material, constant uniformsBlock& _522, const device voxelLightDataBlock& _573, const device voxelLightIndicesBlock& _591, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr, depth2d<float> textureShadowAtlas0, sampler textureShadowAtlas0Smplr, depth2d<float> textureShadowAtlas1, sampler textureShadowAtlas1Smplr, depth2d<float> textureShadowAtlas2, sampler textureShadowAtlas2Smplr, depth2d<float> textureShadowAtlas3, sampler textureShadowAtlas3Smplr, depth2d<float> textureShadowAtlas4, sampler textureShadowAtlas4Smplr, depth2d<float> textureShadowAtlas5, sampler textureShadowAtlas5Smplr, texturecube<float> textureSky, sampler textureSkySmplr, const device bspLightsBlock& _1588, const device dynamicLightsBlock& _1618, constant bspLocalsBlock& _1625)
{
    float lightingLod = fast::clamp((f.viewDist - _522.lightingDistance) / 128.0, 0.0, 1.0);
    if (lightingLod >= 1.0)
    {
        f.ambient = v.ambient;
        f.diffuse = v.diffuse;
        f.specular = float3(0.0);
        return;
    }
    if ((material.flags & 131072) == 131072)
    {
        f.normalSample = fast::normalize(v.normal);
        f.specularSample = float4(f.diffuseSample.xyz, powr(1.0 + material.specularity, 4.0));
    }
    else
    {
        float2 param = f.parallax;
        float3x3 param_1 = float3x3(float3(v.tangent), float3(v.bitangent), float3(v.normal));
        f.normalSample = sampleMaterialNormal(param, param_1, textureMaterial, textureMaterialSmplr, material);
        float2 param_2 = f.parallax;
        f.specularSample = sampleMaterialSpecular(param_2, textureMaterial, textureMaterialSmplr, material);
    }
    float3 param_3 = v.modelPosition;
    float angle = randomAngle(param_3);
    f.shadowSinCos = float2(sin(angle), cos(angle));
    CommonVertex param_4 = v;
    CommonFragment param_5 = f;
    fragmentLighting(param_4, param_5, textureMaterial, textureMaterialSmplr, material, _522, _573, _591, textureVoxelCaustics, textureVoxelCausticsSmplr, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr, textureSky, textureSkySmplr, _1588, _1618, _1625);
    f = param_5;
    f.ambient = mix(f.ambient, v.ambient, float3(lightingLod));
    f.diffuse = mix(f.diffuse, v.diffuse, float3(lightingLod));
    f.specular *= (1.0 - lightingLod);
}

static inline __attribute__((always_inline))
float4 sampleMaterialStage(thread const float2& texcoord, constant materialBlock& material, texture2d<float> textureStage, sampler textureStageSmplr, texture2d<float> textureStageNext, sampler textureStageNextSmplr)
{
    if ((material.flags & 2048) == 2048)
    {
        return mix(textureStage.sample(textureStageSmplr, texcoord), textureStageNext.sample(textureStageNextSmplr, texcoord), float4(material.lerp));
    }
    return textureStage.sample(textureStageSmplr, texcoord);
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _522 [[buffer(0)]], constant bspLocalsBlock& _1625 [[buffer(1)]], constant materialBlock& material [[buffer(2)]], const device bspLightsBlock& _1588 [[buffer(3)]], const device dynamicLightsBlock& _1618 [[buffer(4)]], const device voxelLightDataBlock& _573 [[buffer(5)]], const device voxelLightIndicesBlock& _591 [[buffer(6)]], texture2d_array<float> textureMaterial [[texture(0)]], depth2d<float> textureShadowAtlas0 [[texture(1)]], depth2d<float> textureShadowAtlas1 [[texture(2)]], depth2d<float> textureShadowAtlas2 [[texture(3)]], depth2d<float> textureShadowAtlas3 [[texture(4)]], depth2d<float> textureShadowAtlas4 [[texture(5)]], depth2d<float> textureShadowAtlas5 [[texture(6)]], texture3d<float> textureVoxelCaustics [[texture(7)]], texture3d<float> textureVoxelOcclusion [[texture(8)]], texturecube<float> textureSky [[texture(9)]], texture2d<float> textureStage [[texture(10)]], texture2d<float> textureStageNext [[texture(11)]], texture2d<float> textureWarp [[texture(12)]], texture2d_array<float> textureSubviews [[texture(13)]], sampler textureMaterialSmplr [[sampler(0)]], sampler textureShadowAtlas0Smplr [[sampler(1)]], sampler textureShadowAtlas1Smplr [[sampler(2)]], sampler textureShadowAtlas2Smplr [[sampler(3)]], sampler textureShadowAtlas3Smplr [[sampler(4)]], sampler textureShadowAtlas4Smplr [[sampler(5)]], sampler textureShadowAtlas5Smplr [[sampler(6)]], sampler textureVoxelCausticsSmplr [[sampler(7)]], sampler textureVoxelOcclusionSmplr [[sampler(8)]], sampler textureSkySmplr [[sampler(9)]], sampler textureStageSmplr [[sampler(10)]], sampler textureStageNextSmplr [[sampler(11)]], sampler textureWarpSmplr [[sampler(12)]], sampler textureSubviewsSmplr [[sampler(13)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    CommonVertex vertex0 = {};
    vertex0.modelPosition = in.vertex0_modelPosition;
    vertex0.modelNormal = in.vertex0_modelNormal;
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
    out.outDepth = gl_FragCoord.z;
    bool _1952 = material.flags == 0;
    bool _1960;
    CommonFragment fragment0;
    if (_1952)
    {
        _1960 = (material.surface & 24576) != 0;
    }
    else
    {
        _1960 = _1952;
    }
    bool _1966;
    if (_1960)
    {
        _1966 = _1625.subviewLayer >= 0;
    }
    else
    {
        _1966 = _1960;
    }
    if (_1966)
    {
        float2 st = gl_FragCoord.xy / float2(_522.viewport.zw);
        if (_1625.subviewMirrored != 0)
        {
            st.x = 1.0 - st.x;
        }
        float3 _1997 = float3(st, float(_1625.subviewLayer));
        out.outColor = float4(textureSubviews.sample(textureSubviewsSmplr, _1997.xy, uint(rint(_1997.z))).xyz, 1.0);
        return out;
    }
    fragment0.viewDir = fast::normalize(-vertex0.position);
    fragment0.viewDist = length(vertex0.position);
    float2 _2025;
    _2025.x = textureMaterial.calculate_clamped_lod(textureMaterialSmplr, vertex0.diffusemap);
    _2025.y = textureMaterial.calculate_unclamped_lod(textureMaterialSmplr, vertex0.diffusemap);
    fragment0.texLod = _2025.x;
    CommonVertex param = vertex0;
    CommonFragment param_1 = fragment0;
    parallaxOcclusionMapping(param, param_1, textureMaterial, textureMaterialSmplr, material, _522);
    fragment0 = param_1;
    if (material.flags == 0)
    {
        float2 param_2 = fragment0.parallax;
        fragment0.diffuseSample = sampleMaterialDiffuse(param_2, textureMaterial, textureMaterialSmplr);
        if ((material.surface & 1024) == 1024)
        {
            if (fragment0.diffuseSample.w < material.alphaTest)
            {
                discard_fragment();
            }
        }
        out.outColor = fragment0.diffuseSample;
        out.outColor *= vertex0.color;
        CommonVertex param_3 = vertex0;
        CommonFragment param_4 = fragment0;
        fragmentLightingLod(param_3, param_4, textureMaterial, textureMaterialSmplr, material, _522, _573, _591, textureVoxelCaustics, textureVoxelCausticsSmplr, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr, textureSky, textureSkySmplr, _1588, _1618, _1625);
        fragment0 = param_4;
        float4 _2078 = out.outColor;
        float3 _2080 = _2078.xyz * (fragment0.ambient + fragment0.diffuse);
        out.outColor.x = _2080.x;
        out.outColor.y = _2080.y;
        out.outColor.z = _2080.z;
        float4 _2089 = out.outColor;
        float3 _2091 = _2089.xyz + fragment0.specular;
        out.outColor.x = _2091.x;
        out.outColor.y = _2091.y;
        out.outColor.z = _2091.z;
    }
    else
    {
        bool _2104 = (material.flags & 2097152) == 2097152;
        bool _2110;
        if (_2104)
        {
            _2110 = _1625.subviewLayer >= 0;
        }
        else
        {
            _2110 = _2104;
        }
        bool subview = _2110;
        bool _2118;
        if (subview)
        {
            _2118 = _1625.subviewMirrored != 0;
        }
        else
        {
            _2118 = subview;
        }
        bool mirrored = _2118;
        float2 _2121;
        if (subview)
        {
            _2121 = gl_FragCoord.xy / float2(_522.viewport.zw);
        }
        else
        {
            _2121 = fragment0.parallax;
        }
        float2 st_1 = _2121;
        if (mirrored)
        {
            st_1.x = 1.0 - st_1.x;
        }
        if ((material.flags & 32768) == 32768)
        {
            float2 _2151;
            if (subview)
            {
                _2151 = vertex0.diffusemap;
            }
            else
            {
                _2151 = st_1;
            }
            float2 texcoord = _2151;
            float2 offset = (textureWarp.sample(textureWarpSmplr, (texcoord + float2((float(_522.ticks) * material.warp.x) * 0.00012500000593718141317367553710938))).xy - float2(0.5)) * material.warp.y;
            if (subview)
            {
                float2 dx = dfdx(vertex0.diffusemap);
                float2 dy = dfdy(vertex0.diffusemap);
                float det = (dx.x * dy.y) - (dy.x * dx.y);
                if (abs(det) > 9.9999999600419720025001879548654e-13)
                {
                    float2 pixels = float2((dy.y * offset.x) - (dy.x * offset.y), (dx.x * offset.y) - (dx.y * offset.x)) / float2(det);
                    if (mirrored)
                    {
                        pixels.x = -pixels.x;
                    }
                    st_1 += (pixels / float2(_522.viewport.zw));
                }
            }
            else
            {
                st_1 += offset;
            }
        }
        if (subview)
        {
            float3 _2265 = float3(st_1, float(_1625.subviewLayer));
            fragment0.diffuseSample = float4(textureSubviews.sample(textureSubviewsSmplr, _2265.xy, uint(rint(_2265.z))).xyz, 1.0);
        }
        else
        {
            float2 param_5 = st_1;
            fragment0.diffuseSample = sampleMaterialStage(param_5, material, textureStage, textureStageSmplr, textureStageNext, textureStageNextSmplr);
        }
        fragment0.diffuseSample *= vertex0.color;
        out.outColor = fragment0.diffuseSample;
        if ((material.flags & 65536) == 65536)
        {
            CommonVertex param_6 = vertex0;
            CommonFragment param_7 = fragment0;
            fragmentLightingLod(param_6, param_7, textureMaterial, textureMaterialSmplr, material, _522, _573, _591, textureVoxelCaustics, textureVoxelCausticsSmplr, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureShadowAtlas0, textureShadowAtlas0Smplr, textureShadowAtlas1, textureShadowAtlas1Smplr, textureShadowAtlas2, textureShadowAtlas2Smplr, textureShadowAtlas3, textureShadowAtlas3Smplr, textureShadowAtlas4, textureShadowAtlas4Smplr, textureShadowAtlas5, textureShadowAtlas5Smplr, textureSky, textureSkySmplr, _1588, _1618, _1625);
            fragment0 = param_7;
            float4 _2309 = out.outColor;
            float3 _2311 = _2309.xyz * mix(float3(1.0), fragment0.ambient + fragment0.diffuse, float3(material.lighting));
            out.outColor.x = _2311.x;
            out.outColor.y = _2311.y;
            out.outColor.z = _2311.z;
            float4 _2323 = out.outColor;
            float3 _2325 = _2323.xyz + (fragment0.specular * material.lighting);
            out.outColor.x = _2325.x;
            out.outColor.y = _2325.y;
            out.outColor.z = _2325.z;
        }
        if ((material.flags & 262144) == 262144)
        {
            float4 _2346 = out.outColor;
            float3 _2348 = _2346.xyz + (fragment0.diffuseSample.xyz * material.emissive);
            out.outColor.x = _2348.x;
            out.outColor.y = _2348.y;
            out.outColor.z = _2348.z;
        }
    }
    return out;
}

