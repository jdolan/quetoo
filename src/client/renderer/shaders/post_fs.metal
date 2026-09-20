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

struct vertexData
{
    float2 texcoord;
};

struct localsBlock
{
    int postStage;
    float bloom;
    float bloomThreshold;
};

constant spvUnsafeArray<float, 3> _96 = spvUnsafeArray<float, 3>({ 0.0, 1.384615421295166015625, 3.23076915740966796875 });
constant spvUnsafeArray<float, 3> _108 = spvUnsafeArray<float, 3>({ 0.2270270287990570068359375, 0.3162162303924560546875, 0.0702702701091766357421875 });

struct main0_out
{
    float4 outColor [[color(0)]];
};

struct main0_in
{
    float2 vertex0_texcoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
void bloomExtract(thread float4& outColor, texture2d<float> textureColorAttachment, sampler textureColorAttachmentSmplr, thread vertexData& vertex0, constant localsBlock& _35)
{
    outColor = float4(fast::max(textureColorAttachment.sample(textureColorAttachmentSmplr, vertex0.texcoord).xyz - float3(_35.bloomThreshold), float3(0.0)), 1.0);
}

static inline __attribute__((always_inline))
void bloomBlur(thread float4& outColor, thread vertexData& vertex0, constant localsBlock& _35, texture2d<float> textureBloomAttachment, sampler textureBloomAttachmentSmplr)
{
    float2 texel = float2(1.0) / float2(int2(textureBloomAttachment.get_width(), textureBloomAttachment.get_height()));
    outColor = textureBloomAttachment.sample(textureBloomAttachmentSmplr, vertex0.texcoord) * 0.2270270287990570068359375;
    if (_35.postStage == 1)
    {
        for (int i = 1; i < 3; i++)
        {
            outColor += (textureBloomAttachment.sample(textureBloomAttachmentSmplr, (vertex0.texcoord + float2(texel.x * _96[i], 0.0))) * _108[i]);
            outColor += (textureBloomAttachment.sample(textureBloomAttachmentSmplr, (vertex0.texcoord - float2(texel.x * _96[i], 0.0))) * _108[i]);
        }
    }
    else
    {
        for (int i_1 = 1; i_1 < 3; i_1++)
        {
            outColor += (textureBloomAttachment.sample(textureBloomAttachmentSmplr, (vertex0.texcoord + float2(0.0, texel.y * _96[i_1]))) * _108[i_1]);
            outColor += (textureBloomAttachment.sample(textureBloomAttachmentSmplr, (vertex0.texcoord - float2(0.0, texel.y * _96[i_1]))) * _108[i_1]);
        }
    }
    outColor.w = 1.0;
}

static inline __attribute__((always_inline))
void tonemap(thread float4& outColor, texture2d<float> textureColorAttachment, sampler textureColorAttachmentSmplr, thread vertexData& vertex0, constant localsBlock& _35, texture2d<float> textureBloomAttachment, sampler textureBloomAttachmentSmplr)
{
    float3 color = textureColorAttachment.sample(textureColorAttachmentSmplr, vertex0.texcoord).xyz;
    float3 glow = textureBloomAttachment.sample(textureBloomAttachmentSmplr, vertex0.texcoord).xyz;
    color += (glow * _35.bloom);
    outColor = float4(fast::clamp(color, float3(0.0), float3(1.0)), 1.0);
}

fragment main0_out main0(main0_in in [[stage_in]], constant localsBlock& _35 [[buffer(0)]], texture2d<float> textureColorAttachment [[texture(0)]], texture2d<float> textureBloomAttachment [[texture(1)]], sampler textureColorAttachmentSmplr [[sampler(0)]], sampler textureBloomAttachmentSmplr [[sampler(1)]])
{
    main0_out out = {};
    vertexData vertex0 = {};
    vertex0.texcoord = in.vertex0_texcoord;
    if (_35.postStage == 0)
    {
        bloomExtract(out.outColor, textureColorAttachment, textureColorAttachmentSmplr, vertex0, _35);
    }
    else
    {
        bool _228 = _35.postStage == 1;
        bool _235;
        if (!_228)
        {
            _235 = _35.postStage == 2;
        }
        else
        {
            _235 = _228;
        }
        if (_235)
        {
            bloomBlur(out.outColor, vertex0, _35, textureBloomAttachment, textureBloomAttachmentSmplr);
        }
        else
        {
            tonemap(out.outColor, textureColorAttachment, textureColorAttachmentSmplr, vertex0, _35, textureBloomAttachment, textureBloomAttachmentSmplr);
        }
    }
    return out;
}

