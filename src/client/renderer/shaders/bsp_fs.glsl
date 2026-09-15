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

/**
 * @brief BSP surfaces support parallax occlusion mapping and self-shadowing
 * from their heightmaps; mesh entities do not (see mesh_fs.glsl).
 */
#define PARALLAX_SELF_SHADOW

#include "uniforms.glsl"

// material.glsl declares the canonical BINDING_SAMPLER_MATERIAL..STAGE_NEXT
// family unconditionally; BSP additionally has its own liquid-warp and portal
// samplers after them (mesh/sky never set STAGE_WARP, and only BSP faces carry
// SURF_PORTAL). This stage samples all 12 plus those two (14 total), so storage
// bindings must follow those 14 -- see material.glsl's
// BINDING_STORAGE_NUM_ACTIVE_SAMPLERS comment.
#define BINDING_SAMPLER_WARP                 12
#define BINDING_SAMPLER_PORTAL               13
#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS  14
#define BINDING_UNIFORMS_MATERIAL            2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform bsp_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128];

  /**
   * @brief The layer of texture_portal this model's SURF_PORTAL faces sample, or -1 for none.
   */
  int portal_layer;
};

#include "light.glsl"

/**
 * @brief Warp texture for STAGE_WARP liquid surfaces.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_WARP) uniform sampler2D texture_warp;

/**
 * @brief The views rendered through SURF_PORTAL faces, one layer per portal.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_PORTAL) uniform sampler2DArray texture_portal;

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

// A float depth copy for the sprite pass to sample (soft particles); see r_framebuffer.c.
layout (location = 1) out float out_depth;

common_fragment_t fragment;

/**
 * @brief Applies parallax occlusion mapping to the fragment texcoord.
 */
void parallax_occlusion_mapping(in common_vertex_t vertex, inout common_fragment_t fragment) {

  fragment.parallax = vertex.diffusemap;

  if (material.parallax == 0.0 || fragment.texture_lod > 2.0 ||
      fragment.view_dist >= lighting_distance + LIGHTING_LOD_BLEND_DIST) {
    return;
  }

  float num_samples = mix(32.0, 8.0, min(fragment.texture_lod * 0.25, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(fragment.view_dir * mat3(vertex.tangent, vertex.bitangent, vertex.normal));
  dir.z = max(dir.z, 0.1);
  vec2 p = ((dir.xy * texel) / dir.z) * material.parallax * material.parallax;
  vec2 delta = p / num_samples;

  vec2 texcoord = vertex.diffusemap;
  vec2 prev_texcoord = vertex.diffusemap;

  float depth = 0.0;
  float layer = 1.0 / num_samples;
  float displacement = sample_material_displacement(texcoord, fragment.texture_lod);

  for (int i = 0; i < int(num_samples) && depth < displacement; i++) {
    depth += layer;
    prev_texcoord = texcoord;
    texcoord -= delta;
    displacement = sample_material_displacement(texcoord, fragment.texture_lod);
  }

  float a = displacement - depth;
  float b = sample_material_displacement(prev_texcoord, fragment.texture_lod) - depth + layer;

  fragment.parallax = mix(prev_texcoord, texcoord, a / (a - b));
}

/**
 * @brief Shades BSP base surfaces and material stages.
 */
void main(void) {

  out_depth = gl_FragCoord.z;

  // a portal face shows the view rendered from the point it targets. That view uses this one's
  // projection, so the two images coincide in screen space and the fragment reads straight
  // across. A portal view itself is given a layer of -1, so portals never recurse.
  //
  // This is the base pass only: a material whose stages draw the portal suppresses it with
  // SURF_MATERIAL, and each of those stages samples the portal for itself, through whatever
  // transforms it carries
  if (material.flags == STAGE_NONE && (material.surface & SURF_PORTAL) == SURF_PORTAL && portal_layer >= 0) {
    vec2 st = gl_FragCoord.xy / vec2(viewport.zw);
    out_color = vec4(texture(texture_portal, vec3(st, portal_layer)).rgb, 1.0);
    return;
  }

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.texture_lod = textureQueryLod(texture_material, vertex.diffusemap).x;

  parallax_occlusion_mapping(vertex, fragment);

  if (material.flags == STAGE_NONE) {

    fragment.diffuse_sample = sample_material_diffuse(fragment.parallax);

#ifdef ALPHA_TEST
    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuse_sample.a < material.alpha_test) {
        discard;
      }
    }
#endif

    out_color = fragment.diffuse_sample;

    out_color *= vertex.color;

    fragment_lighting_lod(vertex, fragment);

    out_color.rgb *= (fragment.ambient + fragment.diffuse);
    out_color.rgb += fragment.specular;

  } else {

    // a stage naming its material's own diffusemap draws what the face would have drawn, which
    // for a portal face is the portal. Its coordinates are then the screen's, since that is
    // where the portal's image lives, and every transform the stage carries -- warp, scroll,
    // rotate -- disturbs the view through the portal rather than a texture drawn over it
    bool portal = (material.flags & STAGE_PORTAL) == STAGE_PORTAL && portal_layer >= 0;

    vec2 st = portal ? gl_FragCoord.xy / vec2(viewport.zw) : fragment.parallax;

    if ((material.flags & STAGE_WARP) == STAGE_WARP) {

      // the ripple is sampled, and its amplitude given, in the face's own texture coordinates,
      // as it is for any other surface. A portal is read in screen coordinates, so the offset
      // is carried into them through the texcoord's screen derivative: the same material then
      // warps by the same amount of surface whatever the display's shape or resolution, and a
      // portal further away warps less of the screen, as it should
      vec2 texcoord = portal ? vertex.diffusemap : st;

      vec2 offset = (texture(texture_warp, texcoord + vec2(ticks * material.warp.x * 0.000125)).xy - 0.5) * material.warp.y;

      if (portal) {
        vec2 dx = dFdx(vertex.diffusemap);
        vec2 dy = dFdy(vertex.diffusemap);

        // invert the 2x2 mapping from screen pixels to texture coordinates
        float det = dx.x * dy.y - dy.x * dx.y;
        if (abs(det) > 1.0e-12) {
          vec2 pixels = vec2(dy.y * offset.x - dy.x * offset.y,
                             dx.x * offset.y - dx.y * offset.x) / det;

          st += pixels / vec2(viewport.zw);
        }
      } else {
        st += offset;
      }
    }

    if (portal) {
      fragment.diffuse_sample = vec4(texture(texture_portal, vec3(st, portal_layer)).rgb, 1.0);
    } else {
      fragment.diffuse_sample = sample_material_stage(st);
    }

    fragment.diffuse_sample *= vertex.color;

    out_color = fragment.diffuse_sample;

    if ((material.flags & STAGE_LIGHTING) == STAGE_LIGHTING) {
      fragment_lighting_lod(vertex, fragment);
      out_color.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      out_color.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      out_color.rgb += fragment.diffuse_sample.rgb * material.emissive;
    }
  }
}
