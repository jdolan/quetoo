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
    int wireframe;
    int padding;
};

constant spvUnsafeArray<float, 8> _275 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
{
    float4 outColor [[color(0)]];
};

struct main0_in
{
    float3 cubemapCoord [[user(locn0)]];
    float4 stageColor [[user(locn1)]];
};

static inline __attribute__((always_inline))
float2 directionToAzimuthalEquidistant(thread const float3& direction)
{
    float theta = acos(fast::clamp(direction.z, -1.0, 1.0));
    float phi = precise::atan2(direction.y, direction.x);
    float r = theta / 3.1415927410125732421875;
    return float2(0.5) + ((float2(cos(phi), sin(phi)) * r) * 0.5);
}

static inline __attribute__((always_inline))
float2 transformStageUv(thread float2& uv, constant materialBlock& material, constant uniformsBlock& _113)
{
    if ((material.flags & 128) == 128)
    {
        float2 center = uv - float2(0.5);
        float theta = ((float(_113.ticks) * 0.001000000047497451305389404296875) * material.rotate) * 6.283185482025146484375;
        center = float2x2(float2(cos(theta), -sin(theta)), float2(sin(theta), cos(theta))) * center;
        uv = center + float2(0.5);
    }
    if ((material.flags & 8) == 8)
    {
        uv.x += ((material.scroll.x * float(_113.ticks)) * 0.001000000047497451305389404296875);
    }
    if ((material.flags & 16) == 16)
    {
        uv.y += ((material.scroll.y * float(_113.ticks)) * 0.001000000047497451305389404296875);
    }
    if ((material.flags & 96) != 0)
    {
        float2 center_1 = uv - float2(0.5);
        float _198;
        if ((material.flags & 32) == 32)
        {
            _198 = material.scale.x;
        }
        else
        {
            _198 = 1.0;
        }
        float _211;
        if ((material.flags & 64) == 64)
        {
            _211 = material.scale.y;
        }
        else
        {
            _211 = 1.0;
        }
        center_1 /= float2(_198, _211);
        uv = center_1 + float2(0.5);
    }
    return uv;
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

fragment main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _113 [[buffer(0)]], constant materialBlock& material [[buffer(1)]], texturecube<float> textureSky [[texture(9)]], texture2d<float> textureStage [[texture(10)]], texture2d<float> textureStageNext [[texture(11)]], sampler textureSkySmplr [[sampler(9)]], sampler textureStageSmplr [[sampler(10)]], sampler textureStageNextSmplr [[sampler(11)]])
{
    main0_out out = {};
    if (_113.wireframe != 0)
    {
        out.outColor = float4(1.0);
        return out;
    }
    if (material.flags == 0)
    {
        out.outColor = textureSky.sample(textureSkySmplr, fast::normalize(in.cubemapCoord));
    }
    else
    {
        float3 param = fast::normalize(in.cubemapCoord);
        float2 st = directionToAzimuthalEquidistant(param);
        float2 param_1 = st;
        float2 _259 = transformStageUv(param_1, material, _113);
        st = _259;
        float2 param_2 = st;
        out.outColor = sampleMaterialStage(param_2, material, textureStage, textureStageSmplr, textureStageNext, textureStageNextSmplr) * in.stageColor;
    }
    return out;
}

