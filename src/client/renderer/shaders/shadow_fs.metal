#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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
    float3 ambient;
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

struct main0_out
{
    float gl_FragDepth [[depth(any)]];
};

struct main0_in
{
    float3 inPosition [[user(locn0)]];
    float inLightRadius [[user(locn1), flat]];
};

fragment main0_out main0(main0_in in [[stage_in]])
{
    main0_out out = {};
    float dist = length(in.inPosition) / in.inLightRadius;
    float bias0 = fast::clamp(dist * 0.07999999821186065673828125, 1.0 / in.inLightRadius, 8.0 / in.inLightRadius);
    out.gl_FragDepth = fast::min(dist + bias0, 1.0);
    return out;
}

