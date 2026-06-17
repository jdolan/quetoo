#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

struct Tri { vec4 normal; vec4 albedo; vec4 uv0_uv1; vec4 uv2_tex; };
struct Light { vec4 origin_radius; vec4 color_intensity; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 2, set = 0, std430) readonly buffer Tris { Tri tris[]; };
layout(binding = 3, set = 0, std430) readonly buffer Lights { Light lights[]; };
layout(binding = 0, set = 1) uniform sampler2D textures[];
layout(push_constant) uniform PC {
  vec4 eye; vec4 target; vec4 light; vec4 params; // params.z = light count
} pc;

layout(location = 0) rayPayloadInEXT vec3 hit_value;
layout(location = 1) rayPayloadEXT float lit;
hitAttributeEXT vec2 attribs;

// PCG hash based RNG, seeded per pixel + accumulation frame for soft shadows
float rand(inout uint seed) {
  seed = seed * 747796405u + 2891336453u;
  uint r = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
  r = (r >> 22u) ^ r;
  return float(r) / 4294967295.0;
}

void main() {
  const uint frame = uint(pc.params.w);
  uint seed = (gl_LaunchIDEXT.x * 6151u + gl_LaunchIDEXT.y * 3079u + frame * 12289u + 7u) | 1u;
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

    // trace a shadow ray for the strongest few contributors. Jitter the sampled
    // point on the light within a small sphere so the shadow edge is a soft
    // penumbra; the per-frame seed makes successive accumulated frames converge.
    float shadow = 1.0;
    if (shadows_cast < shadow_budget) {
      shadows_cast++;

      const float soft = min(radius * 0.1, 24.0);
      const vec3 jit = (vec3(rand(seed), rand(seed), rand(seed)) * 2.0 - 1.0) * soft;
      const vec3 sample_pos = lpos + jit;
      vec3 sdir = sample_pos - P;
      const float sdist = length(sdir);
      sdir /= sdist;

      lit = 0.0;
      traceRayEXT(tlas,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF, 0, 0, 1, P + N * 1.0, 1.0, sdir, sdist - 2.0, 1);
      shadow = lit;
    }

    diffuse += lcol * (ndotl * atten * shadow * 0.02 * intensity);
  }

  // sample the material's diffuse texture with barycentric-interpolated UVs, or
  // fall back to the averaged material color when the material has no texture
  vec3 albedo;
  if (t.uv2_tex.w > 0.5) {
    const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec2 uv = bary.x * t.uv0_uv1.xy + bary.y * t.uv0_uv1.zw + bary.z * t.uv2_tex.xy;
    albedo = texture(textures[nonuniformEXT(uint(t.uv2_tex.z))], uv).rgb;
  } else {
    albedo = t.albedo.xyz;
  }

  // gamma-correct the albedo to a perceptual value, then apply an ambient floor
  // plus the lights so the material is clearly readable.
  vec3 base = pow(clamp(albedo, 0.0, 1.0), vec3(1.0 / 2.2));
  vec3 color = base * (0.55 + diffuse * 3.0);
  hit_value = clamp(color, 0.0, 1.0);
}
