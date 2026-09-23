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
    int wireframe;
    int padding;
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

struct decalLocalsBlock
{
    uint4 activeDynamicLights[4];
};

struct main0_out
{
    float4 outColor [[color(0)]];
};

struct main0_in
{
    float3 inModelPosition [[user(locn0)]];
    float3 inModelNormal [[user(locn1)]];
    float2 inTexcoord [[user(locn2)]];
    float4 inColor [[user(locn3)]];
};

static inline __attribute__((always_inline))
int3 decalVoxelXyz(thread const float3& position, constant uniformsBlock& _56)
{
    float3 pos = position - _56.voxels.mins.xyz;
    int3 voxel = int3(floor(pos / float3(32.0)));
    return clamp(voxel, int3(0), int3(_56.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
float3 lightColor(thread const Light& l, constant uniformsBlock& _56)
{
    float3 color = (l.color.xyz * l.color.w) * _56.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_56.saturation));
}

static inline __attribute__((always_inline))
float3 decalLight(thread const Light& light, thread const float3& normal, constant uniformsBlock& _56, thread float3& inModelPosition)
{
    float3 dir = light.origin.xyz - inModelPosition;
    float dist = length(dir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return float3(0.0);
    }
    float lambert = fast::max(0.0, dot(normal, dir / float3(dist)));
    Light param = light;
    return (lightColor(param, _56) * atten) * lambert;
}

static inline __attribute__((always_inline))
bool dynamicLightActive(thread const spvUnsafeArray<uint4, 4>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _56 [[buffer(0)]], constant decalLocalsBlock& _295 [[buffer(1)]], const device bspLightsBlock& _258 [[buffer(2)]], const device dynamicLightsBlock& _288 [[buffer(3)]], const device voxelLightDataBlock& _219 [[buffer(4)]], const device voxelLightIndicesBlock& _247 [[buffer(5)]], texture2d<float> textureDiffusemap [[texture(0)]], sampler textureDiffusemapSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 diffuse = textureDiffusemap.sample(textureDiffusemapSmplr, in.inTexcoord);
    float3 normal = fast::normalize(in.inModelNormal);
    float3 light = float3(_56.ambient);
    float3 param = in.inModelPosition;
    int3 voxel = decalVoxelXyz(param, _56);
    int voxelIndex = (((voxel.z * int(_56.voxels.size.y)) + voxel.y) * int(_56.voxels.size.x)) + voxel.x;
    int2 data = int2(_219.voxelLightDataElements[(voxelIndex * 2) + 0], _219.voxelLightDataElements[(voxelIndex * 2) + 1]);
    Light param_1;
    for (int i = 0; i < data.y; i++)
    {
        int index = _247.voxelLightIndices[data.x + i];
        param_1.origin = _258.bspLights[index].origin;
        param_1.color = _258.bspLights[index].color;
        param_1.tile = _258.bspLights[index].tile;
        float3 param_2 = normal;
        light += decalLight(param_1, param_2, _56, in.inModelPosition);
    }
    spvUnsafeArray<uint4, 4> param_3;
    Light param_5;
    for (int j = 0; j < _288.numDynamicLights; j++)
    {
        param_3[0] = _295.activeDynamicLights[0];
        param_3[1] = _295.activeDynamicLights[1];
        param_3[2] = _295.activeDynamicLights[2];
        param_3[3] = _295.activeDynamicLights[3];
        int param_4 = j;
        if (dynamicLightActive(param_3, param_4))
        {
            param_5.origin = _288.dynamicLights[j].origin;
            param_5.color = _288.dynamicLights[j].color;
            param_5.tile = _288.dynamicLights[j].tile;
            float3 param_6 = normal;
            light += decalLight(param_5, param_6, _56, in.inModelPosition);
        }
    }
    out.outColor = diffuse * in.inColor;
    float4 _339 = out.outColor;
    float3 _341 = _339.xyz * light;
    out.outColor.x = _341.x;
    out.outColor.y = _341.y;
    out.outColor.z = _341.z;
    return out;
}

