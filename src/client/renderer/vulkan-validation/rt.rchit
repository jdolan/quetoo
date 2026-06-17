#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;
hitAttributeEXT vec2 attribs;

void main() {
  // ray-traced barycentric color proves the closest-hit shader ran on a real hit
  vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
  payload = bary;
}
