#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct localsBlock
{
    float4x4 model;
    float4x4 lightView;
    float4 lightOrigin;
    float lerp;
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

struct main0_out
{
    float3 outPosition [[user(locn0)]];
    float outLightRadius [[user(locn1)]];
    float2 outDiffusemap [[user(locn2)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 inPosition [[attribute(0)]];
    float3 inNextPosition [[attribute(1)]];
    float2 inDiffusemap [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _71 [[buffer(0)]], constant localsBlock& _14 [[buffer(1)]])
{
    main0_out out = {};
    float3 position = float3((_14.model * float4(mix(in.inPosition, in.inNextPosition, float3(_14.lerp)), 1.0)).xyz) - _14.lightOrigin.xyz;
    out.outPosition = position;
    out.outLightRadius = _14.lightOrigin.w;
    out.outDiffusemap = in.inDiffusemap;
    float4x4 _78 = _71.lightProjection * _14.lightView;
    float4 _83 = float4(position, 1.0);
    float4 _84 = _78 * _83;
    out.gl_Position = _84;
    return out;
}

