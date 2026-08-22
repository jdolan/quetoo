#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct decal_instance_t
{
    float4 origin;
    float4 normal;
    float4 tangent;
    float4 bitangent;
    float4 texcoords;
    float4 color;
    uint4 params;
};

struct decal_instance_t_1
{
    float4 origin;
    float4 normal;
    float4 tangent;
    float4 bitangent;
    float4 texcoords;
    float4 color;
    uint4 params;
};

struct decal_instances_block
{
    decal_instance_t_1 decal_instances[1];
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
};

struct locals_block
{
    float4x4 model;
};

struct main0_out
{
    float3 out_model_position [[user(locn0)]];
    float3 out_model_normal [[user(locn1)]];
    float2 out_texcoord [[user(locn2)]];
    float4 out_color [[user(locn3)]];
    float4 gl_Position [[position, invariant]];
};

struct main0_in
{
    float3 in_position [[attribute(0)]];
    uint in_instance [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant uniforms_block& _66 [[buffer(0)]], constant locals_block& _88 [[buffer(1)]], const device decal_instances_block& _17 [[buffer(2)]])
{
    main0_out out = {};
    uint _24 = in.in_instance & 16777215u;
    decal_instance_t instance;
    instance.origin = _17.decal_instances[_24].origin;
    instance.normal = _17.decal_instances[_24].normal;
    instance.tangent = _17.decal_instances[_24].tangent;
    instance.bitangent = _17.decal_instances[_24].bitangent;
    instance.texcoords = _17.decal_instances[_24].texcoords;
    instance.color = _17.decal_instances[_24].color;
    instance.params = _17.decal_instances[_24].params;
    uint time = instance.params.x;
    uint lifetime = instance.params.y;
    uint age = uint(_66.ticks) - time;
    float4 position = float4(in.in_position, 1.0);
    out.out_model_position = float3((_88.model * position).xyz);
    out.out_model_normal = fast::normalize(float3((_88.model * float4(instance.normal.xyz, 0.0)).xyz));
    float3 delta = in.in_position - instance.origin.xyz;
    float2 st = ((float2(dot(delta, instance.tangent.xyz), dot(delta, instance.bitangent.xyz)) / float2(instance.origin.w)) * 0.5) + float2(0.5);
    out.out_texcoord = mix(instance.texcoords.xy, instance.texcoords.zw, st);
    out.out_color = instance.color;
    if (lifetime > 0u)
    {
        out.out_color.w *= (1.0 - fast::clamp(float(age) / float(lifetime), 0.0, 1.0));
    }
    float4x4 _184 = _66.projection3D * _66.view;
    float4x4 _187 = _184 * _88.model;
    float4 _189 = _187 * position;
    out.gl_Position = _189;
    if ((in.in_instance >> uint(24)) != instance.params.z)
    {
        out.gl_Position = float4(2.0, 2.0, 2.0, 1.0);
    }
    return out;
}

