#version 460
#extension GL_EXT_ray_tracing : require

// shadow ray payload: set to 1.0 only when the ray reaches the light unobstructed
layout(location = 1) rayPayloadInEXT float lit;

void main() {
  lit = 1.0;
}
