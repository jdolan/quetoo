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

struct vertexData
{
    float4 color;
};

struct main0_out
{
    float4 vertex0_color [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 inPosition [[attribute(0)]];
    float4 inColor [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _23 [[buffer(0)]])
{
    main0_out out = {};
    vertexData vertex0 = {};
    out.gl_Position = (_23.projection3D * _23.view) * float4(in.inPosition, 1.0);
    vertex0.color = in.inColor;
    out.vertex0_color = vertex0.color;
    return out;
}

