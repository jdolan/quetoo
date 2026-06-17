#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

void main() {
  // background for rays that hit nothing
  payload = vec3(0.05, 0.05, 0.12);
}
