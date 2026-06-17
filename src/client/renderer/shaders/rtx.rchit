#version 460
#extension GL_EXT_ray_tracing : require

struct Tri { vec4 normal; vec4 albedo; };
layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 2, set = 0, std430) readonly buffer Tris { Tri tris[]; };
layout(push_constant) uniform PC {
  vec4 eye; vec4 target; vec4 light; vec4 params;
} pc;

layout(location = 0) rayPayloadInEXT vec3 hit_value;
layout(location = 1) rayPayloadEXT float lit;
hitAttributeEXT vec2 attribs;

void main() {
  Tri t = tris[gl_PrimitiveID];
  vec3 N = normalize(t.normal.xyz);
  vec3 P = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
  if (dot(N, gl_WorldRayDirectionEXT) > 0.0) { N = -N; }

  vec3 to_light = pc.light.xyz - P;
  float dist = length(to_light);
  to_light /= dist;
  lit = 0.0;
  traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
    0xFF, 0, 0, 1, P + N * 1.0, 0.5, to_light, dist - 1.0, 1);
  float diffuse = max(dot(N, to_light), 0.0) * lit;
  vec3 to_eye = normalize(pc.eye.xyz - P);
  float head = max(dot(N, to_eye), 0.0);
  hit_value = t.albedo.xyz * (0.12 + 0.55 * diffuse + 0.30 * head);
}
