/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#version 450

/*
 * TODO(#864): bring-up world program — samples the material's diffuse layer
 * (layer 0 of the diffuse/normal/specular/tint 2D array). The sampler is bound
 * at fragment set 2 (SDL_gpu fragment sampler set), binding 0, so it lands on
 * fragment sampler slot 0 with no sparse-binding assumptions. When the full
 * bsp_fs is ported this moves to the fixed BINDING_MATERIAL map in uniforms.glsl.
 */

layout (set = 2, binding = 0) uniform sampler2DArray texture_material;

layout (location = 0) in vec2 in_diffusemap;

layout (location = 0) out vec4 out_color;

/**
 * @brief
 */
void main(void) {

  out_color = vec4(texture(texture_material, vec3(in_diffusemap, 0.0)).rgb, 1.0);
}
