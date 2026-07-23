#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 hit_value;
void main() {
  float t = clamp(gl_WorldRayDirectionEXT.z * 0.5 + 0.5, 0.0, 1.0);
  hit_value = mix(vec3(0.10, 0.11, 0.13), vec3(0.18, 0.22, 0.32), t);
}
