#version 460
#extension GL_EXT_nonuniform_qualifier : require

/**
 * @brief Vulkan 2D UI fragment shader. Mirrors draw_2d_fs.glsl, sampling the
 * per-batch texture from the bindless combined-image-sampler array (the white
 * texture is used for untextured fills, so color * white == color).
 */

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform push_constants {
  mat4 projection2D;
  uint texture_index;
} pc;

layout(location = 0) in vec2 in_diffusemap;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main(void) {
  out_color = in_color * texture(textures[nonuniformEXT(pc.texture_index)], in_diffusemap);
}
