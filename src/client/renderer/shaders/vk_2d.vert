#version 460

/**
 * @brief Vulkan 2D UI vertex shader. Mirrors draw_2d_vs.glsl but takes the
 * orthographic projection as a push constant and the texture index for the
 * bindless sampler array. Position arrives as 16-bit integers (see the
 * r_draw_2d_vertex_t vertex input), matching the OpenGL GL_SHORT attribute.
 */

layout(location = 0) in ivec2 in_position;
layout(location = 1) in vec2 in_diffusemap;
layout(location = 2) in vec4 in_color;

layout(push_constant) uniform push_constants {
  mat4 projection2D;
  uint texture_index;
} pc;

layout(location = 0) out vec2 out_diffusemap;
layout(location = 1) out vec4 out_color;

void main(void) {
  gl_Position = pc.projection2D * vec4(vec2(in_position), 0.0, 1.0);
  out_diffusemap = in_diffusemap;
  out_color = in_color;
}
