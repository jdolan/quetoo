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

#include "uniforms.glsl"

/*
 * Decal vertices carry only a position and a reference into the shared decal
 * instance buffer, which holds everything a decal contributes that does not vary
 * per vertex: the face basis it was clipped against, its atlas rect, its colour
 * and its lifespan. Texture coordinates are projected here rather than baked on
 * the CPU, so the geometry a block uploads is position plus one index.
 */
#define BINDING_STORAGE_DECAL_INSTANCES 0

/**
 * @brief One decal clipped to one face. Must match `r_decal_instance_t`.
 */
struct decal_instance_t {
  vec4 origin;
  vec4 normal;
  vec4 tangent;
  vec4 bitangent;
  vec4 texcoords;
  vec4 color;
  uvec4 params;
};

#define DECAL_TIME       0
#define DECAL_LIFETIME   1
#define DECAL_GENERATION 2

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_DECAL_INSTANCES) readonly buffer decal_instances_block {
  decal_instance_t decal_instances[];
};

layout (location = 0) in vec3 in_position;
layout (location = 1) in uint in_instance;

/**
 * @brief Per-draw model transform.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
};

layout (location = 0) out vec3 out_model_position;
layout (location = 1) out vec3 out_model_normal;
layout (location = 2) out vec2 out_texcoord;
layout (location = 3) out vec4 out_color;

invariant gl_Position;

/**
 * @brief Transforms decal vertices and forwards lighting inputs.
 */
void main(void) {

  const decal_instance_t instance = decal_instances[in_instance & 0xffffffu];

  const uint time = instance.params[DECAL_TIME];
  const uint lifetime = instance.params[DECAL_LIFETIME];
  const uint age = uint(ticks) - time;

  const vec4 position = vec4(in_position, 1.0);

  out_model_position = vec3(model * position);
  out_model_normal = normalize(vec3(model * vec4(instance.normal.xyz, 0.0)));

  const vec3 delta = in_position - instance.origin.xyz;
  const vec2 st = vec2(dot(delta, instance.tangent.xyz),
                       dot(delta, instance.bitangent.xyz)) / instance.origin.w * 0.5 + 0.5;

  out_texcoord = mix(instance.texcoords.xy, instance.texcoords.zw, st);

  out_color = instance.color;

  if (lifetime > 0u) {
    out_color.a *= 1.0 - clamp(float(age) / float(lifetime), 0.0, 1.0);
  }

  gl_Position = projection3D * view * model * position;

  /*
   * An instance slot is reclaimed once the ring wraps onto it, so a vertex can
   * outlive the decal it describes. Collapse the triangle rather than draw it
   * with whatever decal holds the slot now.
   */
  if ((in_instance >> 24) != instance.params[DECAL_GENERATION]) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
  }
}
