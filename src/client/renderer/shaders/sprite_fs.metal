#pragma clang diagnostic ignored "-Wmissing-prototypes"

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

struct main0_out
{
    float4 outColor [[color(0)]];
};

struct main0_in
{
    float2 inDiffusemap [[user(locn0)]];
    float2 inNextDiffusemap [[user(locn1)]];
    float3 inColor [[user(locn2)]];
    float inLerp [[user(locn3)]];
    float inLighting [[user(locn4)]];
    float3 inDiffuse [[user(locn5)]];
};

static inline __attribute__((always_inline))
float calcDepth(thread const float& z, constant uniformsBlock& _25)
{
    return (2.0 * _25.depthRange.x) / ((_25.depthRange.y + _25.depthRange.x) - (z * (_25.depthRange.y - _25.depthRange.x)));
}

static inline __attribute__((always_inline))
float soften(constant uniformsBlock& _25, texture2d<float> textureDepthAttachment, sampler textureDepthAttachmentSmplr, thread float4& gl_FragCoord)
{
    float4 depthSample = textureDepthAttachment.sample(textureDepthAttachmentSmplr, (gl_FragCoord.xy / float2(_25.viewport.zw)));
    float param = depthSample.x;
    float param_1 = gl_FragCoord.z;
    return smoothstep(0.0, 0.001599999959580600261688232421875, fast::clamp(calcDepth(param, _25) - calcDepth(param_1, _25), 0.0, 1.0));
}

fragment main0_out main0(main0_in in [[stage_in]], constant uniformsBlock& _25 [[buffer(0)]], texture2d<float> textureDiffusemap [[texture(0)]], texture2d<float> textureNextDiffusemap [[texture(1)]], texture2d<float> textureDepthAttachment [[texture(2)]], sampler textureDiffusemapSmplr [[sampler(0)]], sampler textureNextDiffusemapSmplr [[sampler(1)]], sampler textureDepthAttachmentSmplr [[sampler(2)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float3 textureColor = mix(textureDiffusemap.sample(textureDiffusemapSmplr, in.inDiffusemap).xyz, textureNextDiffusemap.sample(textureNextDiffusemapSmplr, in.inNextDiffusemap).xyz, float3(in.inLerp));
    float3 color = in.inColor;
    if (in.inLighting > 0.0)
    {
        color = mix(color, color * in.inDiffuse, float3(in.inLighting));
    }
    float _132;
    if (_25.viewType == 3)
    {
        _132 = 1.0;
    }
    else
    {
        _132 = soften(_25, textureDepthAttachment, textureDepthAttachmentSmplr, gl_FragCoord);
    }
    float softness = _132;
    out.outColor = float4((textureColor * color) * softness, 1.0);
    return out;
}

