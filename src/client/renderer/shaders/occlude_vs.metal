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
    int wireframe;
    int padding;
};

struct main0_out
{
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 inCorner [[attribute(0)]];
    float3 inMins [[attribute(1)]];
    float3 inMaxs [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _33 [[buffer(0)]])
{
    main0_out out = {};
    float3 position = mix(in.inMins, in.inMaxs, in.inCorner);
    float4x4 _41 = _33.projection3D * _33.view;
    float4 _47 = float4(position, 1.0);
    float4 _48 = _41 * _47;
    out.gl_Position = _48;
    return out;
}

