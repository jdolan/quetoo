#version 460
#extension GL_EXT_ray_tracing : require

struct Tri { vec4 normal; vec4 albedo; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 2, set = 0, std430) readonly buffer Tris { Tri tris[]; };

layout(location = 0) rayPayloadInEXT vec3 hit_value;
layout(location = 1) rayPayloadEXT float lit;
hitAttributeEXT vec2 attribs;

void main() {
  Tri t = tris[gl_PrimitiveID];
  vec3 N = normalize(t.normal.xyz);
  // face the normal toward the viewer so the visible surface shades correctly
  if (dot(N, gl_WorldRayDirectionEXT) > 0.0) {
    N = -N;
  }
  vec3 P = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

  const vec3 light_pos = vec3(2.5, 4.5, 2.0);
  vec3 to_light = light_pos - P;
  float dist = length(to_light);
  to_light /= dist;

  // trace a shadow ray toward the light (miss shader index 1 sets lit = 1.0)
  lit = 0.0;
  traceRayEXT(tlas,
    gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
    0xFF, 0, 0, 1, P + N * 0.002, 0.001, to_light, dist - 0.01, 1);

  float ndotl = max(dot(N, to_light), 0.0);
  float diffuse = ndotl * lit;
  hit_value = t.albedo.xyz * (0.18 + 0.82 * diffuse);
}
