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
 * @brief One decal clipped to one face. Must match `RenderDecalInstance`.
 */
struct DecalInstance {
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

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_DECAL_INSTANCES) readonly buffer decalInstancesBlock {
  DecalInstance decalInstances[];
};

layout (location = 0) in vec3 inPosition;
layout (location = 1) in uint inInstance;

/**
 * @brief Per-draw model transform.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform localsBlock {
  mat4 model;
};

layout (location = 0) out vec3 outModelPosition;
layout (location = 1) out vec3 outModelNormal;
layout (location = 2) out vec2 outTexcoord;
layout (location = 3) out vec4 outColor;

invariant gl_Position;

/**
 * @brief Transforms decal vertices and forwards lighting inputs.
 */
void main(void) {

  const DecalInstance instance = decalInstances[inInstance & 0xffffffu];

  const uint age = uint(ticks) - instance.params[DECAL_TIME];
  const uint lifetime = instance.params[DECAL_LIFETIME];

  const vec4 position = vec4(inPosition, 1.0);

  outModelPosition = vec3(model * position);
  outModelNormal = normalize(vec3(model * vec4(instance.normal.xyz, 0.0)));

  const vec3 delta = inPosition - instance.origin.xyz;
  const vec2 st = vec2(dot(delta, instance.tangent.xyz),
                       dot(delta, instance.bitangent.xyz)) / instance.origin.w * 0.5 + 0.5;

  outTexcoord = mix(instance.texcoords.xy, instance.texcoords.zw, st);

  outColor = instance.color;
  outColor.a *= 1.0 - clamp(float(age) / float(lifetime), 0.0, 1.0);

  gl_Position = projection3D * view * model * position;

  /*
   * An instance slot is reclaimed once the ring wraps onto it, so a vertex can
   * outlive the decal it describes. Collapse the triangle rather than draw it
   * with whatever decal holds the slot now.
   */
  if ((inInstance >> 24) != instance.params[DECAL_GENERATION]) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
  }
}
