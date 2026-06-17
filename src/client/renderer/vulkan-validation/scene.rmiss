#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hit_value;

void main() {
  // simple vertical sky gradient
  float t = clamp(gl_WorldRayDirectionEXT.y * 0.5 + 0.5, 0.0, 1.0);
  hit_value = mix(vec3(0.55, 0.6, 0.7), vec3(0.15, 0.25, 0.5), t);
}
