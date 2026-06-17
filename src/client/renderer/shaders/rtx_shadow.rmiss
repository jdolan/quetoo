#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 1) rayPayloadInEXT float lit;
void main() { lit = 1.0; }
