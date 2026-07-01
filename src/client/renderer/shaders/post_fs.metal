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

struct vertex_data
{
    float2 texcoord;
};

struct locals_block
{
    int post_stage;
    float bloom;
    float bloom_threshold;
};

struct voxels_t
{
    float4 mins;
    float4 maxs;
    float4 view_coordinate;
    float4 size;
};

struct uniforms_block
{
    int4 viewport;
    float4x4 projection3D;
    float4x4 view;
    float4x4 sky_projection;
    float4x4 light_projection;
    voxels_t voxels;
    float2 depth_range;
    int view_type;
    int ticks;
    float ambient;
    float modulate;
    float saturation;
    float caustics;
    float ambient_occlusion;
    float lighting_distance;
    int editor;
    int developer;
    int wireframe;
    int num_lights;
};

constant spvUnsafeArray<float, 3> _96 = spvUnsafeArray<float, 3>({ 0.0, 1.384615421295166015625, 3.23076915740966796875 });
constant spvUnsafeArray<float, 3> _108 = spvUnsafeArray<float, 3>({ 0.2270270287990570068359375, 0.3162162303924560546875, 0.0702702701091766357421875 });

struct main0_out
{
    float4 out_color [[color(0)]];
};

struct main0_in
{
    float2 vertex0_texcoord [[user(locn0)]];
};

static inline __attribute__((always_inline))
void bloom_extract(thread float4& out_color, texture2d<float> texture_color_attachment, sampler texture_color_attachmentSmplr, thread vertex_data& vertex0, constant locals_block& _35)
{
    out_color = float4(fast::max(texture_color_attachment.sample(texture_color_attachmentSmplr, vertex0.texcoord).xyz - float3(_35.bloom_threshold), float3(0.0)), 1.0);
}

static inline __attribute__((always_inline))
void bloom_blur(thread float4& out_color, thread vertex_data& vertex0, constant locals_block& _35, texture2d<float> texture_bloom_attachment, sampler texture_bloom_attachmentSmplr)
{
    float2 texel = float2(1.0) / float2(int2(texture_bloom_attachment.get_width(), texture_bloom_attachment.get_height()));
    out_color = texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, vertex0.texcoord) * 0.2270270287990570068359375;
    if (_35.post_stage == 1)
    {
        for (int i = 1; i < 3; i++)
        {
            out_color += (texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, (vertex0.texcoord + float2(texel.x * _96[i], 0.0))) * _108[i]);
            out_color += (texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, (vertex0.texcoord - float2(texel.x * _96[i], 0.0))) * _108[i]);
        }
    }
    else
    {
        for (int i_1 = 1; i_1 < 3; i_1++)
        {
            out_color += (texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, (vertex0.texcoord + float2(0.0, texel.y * _96[i_1]))) * _108[i_1]);
            out_color += (texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, (vertex0.texcoord - float2(0.0, texel.y * _96[i_1]))) * _108[i_1]);
        }
    }
    out_color.w = 1.0;
}

static inline __attribute__((always_inline))
void tonemap(thread float4& out_color, texture2d<float> texture_color_attachment, sampler texture_color_attachmentSmplr, thread vertex_data& vertex0, constant locals_block& _35, texture2d<float> texture_bloom_attachment, sampler texture_bloom_attachmentSmplr)
{
    float3 color = texture_color_attachment.sample(texture_color_attachmentSmplr, vertex0.texcoord).xyz;
    float3 glow = texture_bloom_attachment.sample(texture_bloom_attachmentSmplr, vertex0.texcoord).xyz;
    color += (glow * _35.bloom);
    out_color = float4(fast::clamp(color, float3(0.0), float3(1.0)), 1.0);
}

fragment main0_out main0(main0_in in [[stage_in]], constant locals_block& _35 [[buffer(1)]], texture2d<float> texture_color_attachment [[texture(12)]], texture2d<float> texture_bloom_attachment [[texture(15)]], sampler texture_color_attachmentSmplr [[sampler(12)]], sampler texture_bloom_attachmentSmplr [[sampler(15)]])
{
    main0_out out = {};
    vertex_data vertex0 = {};
    vertex0.texcoord = in.vertex0_texcoord;
    if (_35.post_stage == 0)
    {
        bloom_extract(out.out_color, texture_color_attachment, texture_color_attachmentSmplr, vertex0, _35);
    }
    else
    {
        bool _228 = _35.post_stage == 1;
        bool _235;
        if (!_228)
        {
            _235 = _35.post_stage == 2;
        }
        else
        {
            _235 = _228;
        }
        if (_235)
        {
            bloom_blur(out.out_color, vertex0, _35, texture_bloom_attachment, texture_bloom_attachmentSmplr);
        }
        else
        {
            tonemap(out.out_color, texture_color_attachment, texture_color_attachmentSmplr, vertex0, _35, texture_bloom_attachment, texture_bloom_attachmentSmplr);
        }
    }
    return out;
}

