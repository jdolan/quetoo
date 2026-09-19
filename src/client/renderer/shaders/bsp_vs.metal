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

struct Light
{
    float4 origin;
    float4 color;
    float2 tile;
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
    int portalLayer;
};

constant spvUnsafeArray<float, 8> _532 = spvUnsafeArray<float, 8>({ 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 });

struct main0_out
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
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 inPosition [[attribute(0)]];
    float3 inNormal [[attribute(1)]];
    float3 inTangent [[attribute(2)]];
    float3 inBitangent [[attribute(3)]];
    float2 inDiffusemap [[attribute(4)]];
    float4 inColor [[attribute(5)]];
};

static inline __attribute__((always_inline))
void stageTransform(thread float3& position, thread const float3& normal, thread const float3& tangent, thread const float3& bitangent, constant materialBlock& material)
{
    if ((material.flags & 1048576) == 1048576)
    {
        position += (normal * material.shell);
    }
}

static inline __attribute__((always_inline))
float3 voxelUvw(thread const float3& position, constant uniformsBlock& _161)
{
    return (position - _161.voxels.mins.xyz) / (_161.voxels.maxs.xyz - _161.voxels.mins.xyz);
}

static inline __attribute__((always_inline))
void stageVertex(thread const float3& inPosition, thread CommonVertex& vertex0, constant materialBlock& material, constant uniformsBlock& _161)
{
    int envmap = material.flags & 16384;
    if (envmap != 0)
    {
        float3 viewDir = fast::normalize(vertex0.position);
        float3 reflectDir = reflect(viewDir, fast::normalize(vertex0.normal));
        vertex0.diffusemap = float2(0.5 + (reflectDir.y * 0.5), 0.5 - (reflectDir.z * 0.5));
    }
    if ((material.flags & 256) == 256)
    {
        float p = 1.0 + ((sin(((float(_161.ticks) * 0.001000000047497451305389404296875) * material.stretch.y) * 3.1415927410125732421875) * material.stretch.x) * 0.5);
        float2x2 matrix;
        matrix[0].x = p;
        matrix[1].x = 0.0;
        float2 translate;
        translate.x = material.stOrigin.x - (material.stOrigin.x * p);
        matrix[0].y = 0.0;
        matrix[1].y = p;
        translate.y = material.stOrigin.y - (material.stOrigin.y * p);
        vertex0.diffusemap.x = ((vertex0.diffusemap.x * matrix[0].x) + (vertex0.diffusemap.y * matrix[1].x)) + translate.x;
        vertex0.diffusemap.y = ((vertex0.diffusemap.x * matrix[0].y) + (vertex0.diffusemap.y * matrix[1].y)) + translate.y;
    }
    if ((material.flags & 128) == 128)
    {
        float theta = ((float(_161.ticks) * 0.001000000047497451305389404296875) * material.rotate) * 6.283185482025146484375;
        float2 stOrigin = material.stOrigin;
        if (envmap != 0)
        {
            stOrigin = float2(0.5);
        }
        vertex0.diffusemap -= stOrigin;
        vertex0.diffusemap = float2x2(float2(cos(theta), -sin(theta)), float2(sin(theta), cos(theta))) * vertex0.diffusemap;
        vertex0.diffusemap += stOrigin;
    }
    if (envmap != 0)
    {
        if ((material.flags & 96) != 0)
        {
            float _308;
            if ((material.flags & 32) == 32)
            {
                _308 = material.scale.x;
            }
            else
            {
                _308 = 1.0;
            }
            float _321;
            if ((material.flags & 64) == 64)
            {
                _321 = material.scale.y;
            }
            else
            {
                _321 = 1.0;
            }
            float2 scale = float2(_308, _321);
            float2 centered = vertex0.diffusemap - float2(0.5);
            centered /= fast::max(abs(scale), float2(9.9999997473787516355514526367188e-05));
            vertex0.diffusemap = centered + float2(0.5);
        }
        if ((material.flags & 8) == 8)
        {
            vertex0.diffusemap.x += ((material.scroll.x * float(_161.ticks)) * 0.001000000047497451305389404296875);
        }
        if ((material.flags & 16) == 16)
        {
            vertex0.diffusemap.y += ((material.scroll.y * float(_161.ticks)) * 0.001000000047497451305389404296875);
        }
    }
    else
    {
        if ((material.flags & 8) == 8)
        {
            vertex0.diffusemap.x += ((material.scroll.x * float(_161.ticks)) * 0.001000000047497451305389404296875);
        }
        if ((material.flags & 16) == 16)
        {
            vertex0.diffusemap.y += ((material.scroll.y * float(_161.ticks)) * 0.001000000047497451305389404296875);
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
        vertex0.color.w *= ((sin((((float(_161.ticks) * 0.001000000047497451305389404296875) + material.drift) * material.pulse) * 3.1415927410125732421875) + 1.0) * 0.5);
    }
    if ((material.flags & 4096) == 4096)
    {
        float z = fast::clamp(inPosition.z, material.terrain.x, material.terrain.y);
        vertex0.color.w *= ((z - material.terrain.x) / (material.terrain.y - material.terrain.x));
    }
    if ((material.flags & 8192) == 8192)
    {
        int index = (int(inPosition.x) + int(inPosition.y)) + int(inPosition.z);
        vertex0.color.w *= (_532[spvSMod(index, 8)] * material.dirtmap);
    }
}

static inline __attribute__((always_inline))
float voxelOcclusion(thread const float3& texcoord, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr)
{
    return textureVoxelOcclusion.sample(textureVoxelOcclusionSmplr, texcoord, level(0.0)).x;
}

static inline __attribute__((always_inline))
float voxelExposure(thread const float3& texcoord, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr)
{
    return fast::max(0.25, textureVoxelOcclusion.sample(textureVoxelOcclusionSmplr, texcoord, level(0.0)).y);
}

static inline __attribute__((always_inline))
float3 ambientLight(thread const CommonVertex& v, constant uniformsBlock& _161, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr, texturecube<float> textureSky, sampler textureSkySmplr)
{
    float3 param = v.voxel;
    float occlusion = voxelOcclusion(param, textureVoxelOcclusion, textureVoxelOcclusionSmplr);
    float3 param_1 = v.voxel;
    float exposure = voxelExposure(param_1, textureVoxelOcclusion, textureVoxelOcclusionSmplr);
    float3 sky = textureSky.sample(textureSkySmplr, fast::normalize(v.modelNormal), level(6.0)).xyz;
    return ((powr(float3(2.0) + sky, float3(2.0)) * exposure) * (1.0 - (occlusion * _161.ambientOcclusion))) * float3(_161.ambient);
}

static inline __attribute__((always_inline))
int3 voxelXyz(thread const float3& position, constant uniformsBlock& _161)
{
    float3 pos = position - _161.voxels.mins.xyz;
    int3 voxel = int3(floor((pos / float3(32.0)) + float3(0.001000000047497451305389404296875)));
    return clamp(voxel, int3(0), int3(_161.voxels.size.xyz) - int3(1));
}

static inline __attribute__((always_inline))
int2 voxelLightData(thread const int3& voxel, constant uniformsBlock& _161, const device voxelLightDataBlock& _608)
{
    int index = (((voxel.z * int(_161.voxels.size.y)) + voxel.y) * int(_161.voxels.size.x)) + voxel.x;
    return int2(_608.voxelLightDataElements[(index * 2) + 0], _608.voxelLightDataElements[(index * 2) + 1]);
}

static inline __attribute__((always_inline))
int voxelLightIndex(thread const int& index, const device voxelLightIndicesBlock& _625)
{
    return _625.voxelLightIndices[index];
}

static inline __attribute__((always_inline))
float3 lightColor(thread const Light& l, constant uniformsBlock& _161)
{
    float3 color = (l.color.xyz * l.color.w) * _161.modulate;
    float luma = dot(color, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return mix(float3(luma), color, float3(_161.saturation));
}

static inline __attribute__((always_inline))
float3 vertexLight(thread const CommonVertex& v, thread const Light& light, constant materialBlock& material, constant uniformsBlock& _161)
{
    float3 lightDir = light.origin.xyz - v.modelPosition;
    float dist = length(lightDir);
    float radius = light.origin.w;
    float atten = fast::clamp(1.0 - (dist / radius), 0.0, 1.0);
    if (atten <= 0.0)
    {
        return float3(0.0);
    }
    lightDir = fast::normalize(lightDir);
    float lambert = dot(v.modelNormal, lightDir);
    float _789;
    if ((material.surface & 120) != int(0u))
    {
        _789 = abs(lambert);
    }
    else
    {
        _789 = fast::max(0.0, lambert);
    }
    lambert = _789;
    Light param = light;
    return (lightColor(param, _161) * atten) * lambert;
}

static inline __attribute__((always_inline))
bool dynamicLightActive(thread const spvUnsafeArray<uint4, 4>& mask, thread const int& j)
{
    return (mask[j >> 7][(j >> 5) & 3] & (1u << uint(j & 31))) != 0u;
}

static inline __attribute__((always_inline))
float3 voxelCaustics(thread const float3& texcoord, constant uniformsBlock& _161, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr)
{
    float3 encoded = textureVoxelCaustics.sample(textureVoxelCausticsSmplr, texcoord, level(0.0)).xyz;
    return ((encoded * 2.0) - float3(1.0)) * _161.caustics;
}

static inline __attribute__((always_inline))
void vertexCaustics(thread CommonVertex& v, constant uniformsBlock& _161, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr)
{
    float3 param = v.voxel;
    v.caustics = length(voxelCaustics(param, _161, textureVoxelCaustics, textureVoxelCausticsSmplr));
}

static inline __attribute__((always_inline))
void vertexLighting(thread CommonVertex& v, constant materialBlock& material, constant uniformsBlock& _161, const device voxelLightDataBlock& _608, const device voxelLightIndicesBlock& _625, texture3d<float> textureVoxelCaustics, sampler textureVoxelCausticsSmplr, texture3d<float> textureVoxelOcclusion, sampler textureVoxelOcclusionSmplr, texturecube<float> textureSky, sampler textureSkySmplr, const device bspLightsBlock& _854, const device dynamicLightsBlock& _885, constant bspLocalsBlock& _892)
{
    CommonVertex param = v;
    v.ambient = ambientLight(param, _161, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureSky, textureSkySmplr);
    v.diffuse = float3(0.0);
    if (_161.editor == 0)
    {
        float3 param_1 = v.modelPosition;
        int3 voxelCoord = voxelXyz(param_1, _161);
        int3 param_2 = voxelCoord;
        int2 data = voxelLightData(param_2, _161, _608);
        Light param_5;
        for (int i = 0; i < data.y; i++)
        {
            int param_3 = data.x + i;
            int index = voxelLightIndex(param_3, _625);
            CommonVertex param_4 = v;
            param_5.origin = _854.bspLights[index].origin;
            param_5.color = _854.bspLights[index].color;
            param_5.tile = _854.bspLights[index].tile;
            v.diffuse += vertexLight(param_4, param_5, material, _161);
        }
    }
    spvUnsafeArray<uint4, 4> param_6;
    Light param_9;
    for (int j = 0; j < _885.numDynamicLights; j++)
    {
        param_6[0] = _892.activeDynamicLights[0];
        param_6[1] = _892.activeDynamicLights[1];
        param_6[2] = _892.activeDynamicLights[2];
        param_6[3] = _892.activeDynamicLights[3];
        int param_7 = j;
        if (dynamicLightActive(param_6, param_7))
        {
            CommonVertex param_8 = v;
            param_9.origin = _885.dynamicLights[j].origin;
            param_9.color = _885.dynamicLights[j].color;
            param_9.tile = _885.dynamicLights[j].tile;
            v.diffuse += vertexLight(param_8, param_9, material, _161);
        }
    }
    CommonVertex param_10 = v;
    vertexCaustics(param_10, _161, textureVoxelCaustics, textureVoxelCausticsSmplr);
    v = param_10;
}

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _161 [[buffer(0)]], constant bspLocalsBlock& _892 [[buffer(1)]], constant materialBlock& material [[buffer(2)]], const device bspLightsBlock& _854 [[buffer(3)]], const device dynamicLightsBlock& _885 [[buffer(4)]], const device voxelLightDataBlock& _608 [[buffer(5)]], const device voxelLightIndicesBlock& _625 [[buffer(6)]], texture3d<float> textureVoxelCaustics [[texture(0)]], texture3d<float> textureVoxelOcclusion [[texture(1)]], texturecube<float> textureSky [[texture(2)]], sampler textureVoxelCausticsSmplr [[sampler(0)]], sampler textureVoxelOcclusionSmplr [[sampler(1)]], sampler textureSkySmplr [[sampler(2)]])
{
    main0_out out = {};
    CommonVertex vertex0 = {};
    float4x4 viewModel = _161.view * _892.model;
    float4 position = float4(in.inPosition, 1.0);
    float4 normal = float4(in.inNormal, 0.0);
    float4 tangent = float4(in.inTangent, 0.0);
    float4 bitangent = float4(in.inBitangent, 0.0);
    float3 param = position.xyz;
    float3 param_1 = normal.xyz;
    float3 param_2 = tangent.xyz;
    float3 param_3 = bitangent.xyz;
    stageTransform(param, param_1, param_2, param_3, material);
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
    vertex0.modelPosition = float3((_892.model * position).xyz);
    vertex0.modelNormal = fast::normalize(float3((_892.model * normal).xyz));
    vertex0.position = float3((viewModel * position).xyz);
    vertex0.normal = fast::normalize(float3((viewModel * normal).xyz));
    vertex0.tangent = fast::normalize(float3((viewModel * tangent).xyz));
    vertex0.bitangent = fast::normalize(float3((viewModel * bitangent).xyz));
    vertex0.diffusemap = in.inDiffusemap;
    float3 param_4 = float3((_892.model * position).xyz);
    vertex0.voxel = voxelUvw(param_4, _161);
    vertex0.color = in.inColor;
    float3 param_5 = in.inPosition;
    CommonVertex param_6 = vertex0;
    stageVertex(param_5, param_6, material, _161);
    vertex0 = param_6;
    CommonVertex param_7 = vertex0;
    vertexLighting(param_7, material, _161, _608, _625, textureVoxelCaustics, textureVoxelCausticsSmplr, textureVoxelOcclusion, textureVoxelOcclusionSmplr, textureSky, textureSkySmplr, _854, _885, _892);
    vertex0 = param_7;
    float4x4 _1107 = _161.projection3D * viewModel;
    float4 _1109 = _1107 * position;
    out.gl_Position = _1109;
    out.vertex0_modelPosition = vertex0.modelPosition;
    out.vertex0_modelNormal = vertex0.modelNormal;
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

