#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct DecalInstance
{
    float4 origin;
    float4 normal;
    float4 tangent;
    float4 bitangent;
    float4 texcoords;
    float4 color;
    uint4 params;
};

struct DecalInstance_1
{
    float4 origin;
    float4 normal;
    float4 tangent;
    float4 bitangent;
    float4 texcoords;
    float4 color;
    uint4 params;
};

struct decalInstancesBlock
{
    DecalInstance_1 decalInstances[1];
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

struct localsBlock
{
    float4x4 model;
};

struct main0_out
{
    float3 outModelPosition [[user(locn0)]];
    float3 outModelNormal [[user(locn1)]];
    float2 outTexcoord [[user(locn2)]];
    float4 outColor [[user(locn3)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 inPosition [[attribute(0)]];
    uint inInstance [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _59 [[buffer(0)]], constant localsBlock& _86 [[buffer(1)]], const device decalInstancesBlock& _17 [[buffer(2)]])
{
    main0_out out = {};
    uint _24 = in.inInstance & 16777215u;
    DecalInstance instance;
    instance.origin = _17.decalInstances[_24].origin;
    instance.normal = _17.decalInstances[_24].normal;
    instance.tangent = _17.decalInstances[_24].tangent;
    instance.bitangent = _17.decalInstances[_24].bitangent;
    instance.texcoords = _17.decalInstances[_24].texcoords;
    instance.color = _17.decalInstances[_24].color;
    instance.params = _17.decalInstances[_24].params;
    uint age = uint(_59.ticks) - instance.params.x;
    uint lifetime = instance.params.y;
    float4 position = float4(in.inPosition, 1.0);
    out.outModelPosition = float3((_86.model * position).xyz);
    out.outModelNormal = fast::normalize(float3((_86.model * float4(instance.normal.xyz, 0.0)).xyz));
    float3 delta = in.inPosition - instance.origin.xyz;
    float2 st = ((float2(dot(delta, instance.tangent.xyz), dot(delta, instance.bitangent.xyz)) / float2(instance.origin.w)) * 0.5) + float2(0.5);
    out.outTexcoord = mix(instance.texcoords.xy, instance.texcoords.zw, st);
    out.outColor = instance.color;
    out.outColor.w *= (1.0 - fast::clamp(float(age) / float(lifetime), 0.0, 1.0));
    float4x4 _177 = _59.projection3D * _59.view;
    float4x4 _180 = _177 * _86.model;
    float4 _182 = _180 * position;
    out.gl_Position = _182;
    if ((in.inInstance >> uint(24)) != instance.params.z)
    {
        out.gl_Position = float4(2.0, 2.0, 2.0, 1.0);
    }
    return out;
}

