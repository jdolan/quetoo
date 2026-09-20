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
 * The mesh fragment program: full per-fragment material lighting (normal maps,
 * Blinn-Phong specular, shadows, clustered voxel + dynamic lights), sharing the
 * BSP fragment stack and its descriptor layout (see bsp_fs). Unlike BSP surfaces,
 * mesh entity normal maps never carry a heightmap, so there is no parallax
 * occlusion mapping or parallax self-shadowing here -- fragment.parallax is
 * always just the plain diffusemap texcoord. Material stages and the EF_SHELL
 * overlay share this shader with the base pass via a runtime branch on
 * material.flags, exactly like bsp_fs -- see main() below.
 */

#define MATERIAL_TINTS

#include "uniforms.glsl"

#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS 12
#define BINDING_UNIFORMS_MATERIAL           2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform meshLocalsBlock {
  uvec4 activeDynamicLights[MAX_DYNAMIC_LIGHTS / 128];
};

#include "light.glsl"

layout (location = 0) in CommonVertex vertex;

layout (location = 0) out vec4 outColor;

layout (location = 1) out float outDepth;

CommonFragment fragment;

/**
 * @brief Computes per-fragment mesh lighting, blending down to vertex
 * lighting as distance from the camera increases (see fragmentLightingLod).
 */
void meshFragmentLighting(in CommonVertex vertex, inout CommonFragment fragment) {
  fragmentLightingLod(vertex, fragment);
}

/**
 * @brief Shades either the base mesh material pass or a material stage overlay.
 */
void main(void) {

  outDepth = gl_FragCoord.z;

  fragment.viewDir = normalize(-vertex.position);
  fragment.viewDist = length(vertex.position);

  fragment.parallax = vertex.diffusemap;

  if (material.flags == STAGE_NONE) {

    fragment.diffuseSample = sampleMaterialDiffuse(fragment.parallax);

#ifdef ALPHA_TEST
    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuseSample.a < material.alphaTest) {
        discard;
      }
    }
#endif

    vec4 tintmap = sampleMaterialTint(fragment.parallax);
    fragment.diffuseSample.rgb *= 1.0 - tintmap.a;
    fragment.diffuseSample.rgb += (material.tintColors[0] * tintmap.r).rgb * tintmap.a;
    fragment.diffuseSample.rgb += (material.tintColors[1] * tintmap.g).rgb * tintmap.a;
    fragment.diffuseSample.rgb += (material.tintColors[2] * tintmap.b).rgb * tintmap.a;

    outColor = fragment.diffuseSample * vertex.color;

    meshFragmentLighting(vertex, fragment);

    outColor.rgb *= (fragment.ambient + fragment.diffuse);
    outColor.rgb += fragment.specular;

  } else {

    fragment.diffuseSample = sampleMaterialStage(fragment.parallax) * vertex.color;

    outColor = fragment.diffuseSample;

    if ((material.flags & STAGE_LIGHTING) == STAGE_LIGHTING) {
      meshFragmentLighting(vertex, fragment);
      outColor.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      outColor.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      outColor.rgb += fragment.diffuseSample.rgb * material.emissive;
    }
  }
}
