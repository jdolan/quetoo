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

struct voxelLightIndicesBlock
{
    int voxelLightIndices[1];
};

struct dynamicLightsBlock
{
    int numDynamicLights;
    Light_1 dynamicLights[1];
};

struct spriteLocalsBlock
{
    uint4 activeDynamicLights[4];
};

struct SpriteInstance
{
    float4 center;
    float4 a;
    float4 b;
    float4 texcoords;
    float4 nextTexcoords;
    float4 color;
};

struct SpriteInstance_1
{
    float4 center;
    float4 a;
    float4 b;
    float4 texcoords;
    float4 nextTexcoords;
    float4 color;
};

struct spriteInstancesBlock
{
    SpriteInstance_1 spriteInstances[1];
};

constant spvUnsafeArray<float2, 4> _336 = spvUnsafeArray<float2, 4>({ float2(1.0, -1.0), float2(1.0), float2(-1.0, 1.0), float2(-1.0) });
constant spvUnsafeArray<float2, 4> _371 = spvUnsafeArray<float2, 4>({ float2(0.0), float2(1.0, 0.0), float2(1.0), float2(0.0, 1.0) });

struct main0_out
{
    float2 outDiffusemap [[user(locn0)]];
    float2 outNextDiffusemap [[user(locn1)]];
    float3 outColor [[user(locn2)]];
    float outLerp [[user(locn3)]];
    float outLighting [[user(locn4)]];
    float3 outDiffuse [[user(locn5)]];
    float4 gl_Position [[position, invariant]];
};

static inline __attribute__((always_inline))
int3 spriteVoxelXyz(thread const float3& position, constant uniformsBlock& _60)
{
    float3 pos = position - _60.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_60.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 lightColor(thread const Light& l, constant uniformsBlock& _60)
{
    float3 color = (l.color.xyz * l.color.w) * _60.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_60.saturation));
}

static inline __attribute__((always_inline))
float3 spriteLight(thread const Light& light, thread const float3& position, constant uniformsBlock& _60)
{
    float dist = distance(light.origin.xyz, position);
    float atten = fast::clamp(1.0 - (dist / light.origin.w), 0.0, 1.0);
    Light param = light;
    return lightColor(param, _60) * atten;
}

static inline __attribute__((always_inline))
bool dynamicLightActive(thread const spvUnsafeArray<uint4, 4>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 spriteLighting(thread const float3& position, constant uniformsBlock& _60, const device voxelLightDataBlock& _183, const device bspLightsBlock& _211, const device voxelLightIndicesBlock& _215, const device dynamicLightsBlock& _250, constant spriteLocalsBlock& _257)
{
    float3 diffuse = float3(0.0);
    float3 param = position;
    int3 voxel = spriteVoxelXyz(param, _60);
    int index = (((voxel.z * int(_60.voxels.size.y)) + voxel.y) * int(_60.voxels.size.x)) + voxel.x;
    int2 data = int2(_183.voxelLightDataElements[(index * 2) + 0], _183.voxelLightDataElements[(index * 2) + 1]);
    Light param_1;
    for (int i = 0; i < data.y; i++)
    {
        int _219 = data.x + i;
        param_1.origin = _211.bspLights[_215.voxelLightIndices[_219]].origin;
        param_1.color = _211.bspLights[_215.voxelLightIndices[_219]].color;
        param_1.tile = _211.bspLights[_215.voxelLightIndices[_219]].tile;
        float3 param_2 = position;
        diffuse += spriteLight(param_1, param_2, _60);
    }
    spvUnsafeArray<uint4, 4> param_3;
    Light param_5;
    for (int j = 0; j < _250.numDynamicLights; j++)
    {
        param_3[0] = _257.activeDynamicLights[0];
        param_3[1] = _257.activeDynamicLights[1];
        param_3[2] = _257.activeDynamicLights[2];
        param_3[3] = _257.activeDynamicLights[3];
        int param_4 = j;
        if (dynamicLightActive(param_3, param_4))
        {
            param_5.origin = _250.dynamicLights[j].origin;
            param_5.color = _250.dynamicLights[j].color;
            param_5.tile = _250.dynamicLights[j].tile;
            float3 param_6 = position;
            diffuse += spriteLight(param_5, param_6, _60);
        }
    }
    return diffuse;
}

vertex main0_out main0(constant uniformsBlock& _60 [[buffer(0)]], constant spriteLocalsBlock& _257 [[buffer(1)]], const device bspLightsBlock& _211 [[buffer(2)]], const device dynamicLightsBlock& _250 [[buffer(3)]], const device voxelLightDataBlock& _183 [[buffer(4)]], const device voxelLightIndicesBlock& _215 [[buffer(5)]], const device spriteInstancesBlock& _309 [[buffer(6)]], uint gl_VertexIndex [[vertex_id]])
{
    main0_out out = {};
    uint corner = uint(int(gl_VertexIndex)) & 3u;
    uint _312 = uint(int(gl_VertexIndex)) >> uint(2);
    SpriteInstance instance;
    instance.center = _309.spriteInstances[_312].center;
    instance.a = _309.spriteInstances[_312].a;
    instance.b = _309.spriteInstances[_312].b;
    instance.texcoords = _309.spriteInstances[_312].texcoords;
    instance.nextTexcoords = _309.spriteInstances[_312].nextTexcoords;
    instance.color = _309.spriteInstances[_312].color;
    float2 signs = _336[corner];
    float3 position = (instance.center.xyz + (instance.a.xyz * signs.x)) + (instance.b.xyz * signs.y);
    out.outDiffusemap = mix(instance.texcoords.xy, instance.texcoords.zw, _371[corner]);
    out.outNextDiffusemap = mix(instance.nextTexcoords.xy, instance.nextTexcoords.zw, _371[corner]);
    out.outColor = instance.color.xyz;
    out.outLerp = instance.center.w;
    out.outLighting = instance.a.w;
    float3 param = position;
    out.outDiffuse = spriteLighting(param, _60, _183, _211, _215, _250, _257);
    float4x4 _414 = _60.projection3D * _60.view;
    float4 _419 = float4(position, 1.0);
    float4 _420 = _414 * _419;
    out.gl_Position = _420;
    return out;
}

