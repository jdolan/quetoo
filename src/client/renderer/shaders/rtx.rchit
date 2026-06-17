#version 460
#extension GL_EXT_ray_tracing : require

struct Tri { vec4 normal; vec4 albedo; };
struct Light { vec4 origin_radius; vec4 color_intensity; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 2, set = 0, std430) readonly buffer Tris { Tri tris[]; };
layout(binding = 3, set = 0, std430) readonly buffer Lights { Light lights[]; };
layout(push_constant) uniform PC {
  vec4 eye; vec4 target; vec4 light; vec4 params; // params.z = light count
} pc;

layout(location = 0) rayPayloadInEXT vec3 hit_value;
layout(location = 1) rayPayloadEXT float lit;
hitAttributeEXT vec2 attribs;

void main() {
  Tri t = tris[gl_PrimitiveID];
  vec3 N = normalize(t.normal.xyz);
  vec3 P = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
  if (dot(N, gl_WorldRayDirectionEXT) > 0.0) {
    N = -N;
  }

  vec3 diffuse = vec3(0.0);
  const int count = int(pc.params.z);
  // cap the per-pixel shadow-ray budget; sum diffuse from all lights regardless
  const int shadow_budget = 6;
  int shadows_cast = 0;

  for (int i = 0; i < count; i++) {
    vec3 lpos = lights[i].origin_radius.xyz;
    float radius = max(lights[i].origin_radius.w, 1.0);
    vec3 lcol = lights[i].color_intensity.rgb;
    float intensity = max(lights[i].color_intensity.a, 1.0);

    vec3 to_light = lpos - P;
    float dist = length(to_light);
    if (dist > radius) {
      continue;
    }
    to_light /= dist;

    float ndotl = max(dot(N, to_light), 0.0);
    if (ndotl <= 0.0) {
      continue;
    }

    // smooth distance attenuation
    float a = 1.0 - dist / radius;
    float atten = a * a;

    // trace a shadow ray for the strongest few contributors
    float shadow = 1.0;
    if (shadows_cast < shadow_budget) {
      shadows_cast++;
      lit = 0.0;
      traceRayEXT(tlas,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF, 0, 0, 1, P + N * 1.0, 1.0, to_light, dist - 2.0, 1);
      shadow = lit;
    }

    diffuse += lcol * (ndotl * atten * shadow * 0.02 * intensity);
  }

  // the diffuse textures average to dark linear values (~0.15); gamma-correct the
  // albedo to a perceptual value first, then apply an ambient floor plus the lights
  // so the real material colors are clearly readable.
  vec3 base = pow(clamp(t.albedo.xyz, 0.0, 1.0), vec3(1.0 / 2.2));
  vec3 color = base * (0.55 + diffuse * 3.0);
  hit_value = clamp(color, 0.0, 1.0);
}
