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

struct localsBlock
{
    float4x4 model;
};

struct main0_out
{
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 inPosition [[attribute(0)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _18 [[buffer(0)]], constant localsBlock& _25 [[buffer(1)]])
{
    main0_out out = {};
    float4x4 viewModel = _18.view * _25.model;
    float4x4 _40 = _18.projection3D * viewModel;
    float4 _48 = float4(in.inPosition, 1.0);
    float4 _49 = _40 * _48;
    out.gl_Position = _49;
    return out;
}

