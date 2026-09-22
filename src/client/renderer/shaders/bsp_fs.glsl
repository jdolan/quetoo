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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#version 450

/**
 * @brief BSP surfaces support parallax occlusion mapping and self-shadowing
 * from their heightmaps; mesh entities do not (see mesh_fs.glsl).
 */
#define PARALLAX_SELF_SHADOW

#include "uniforms.glsl"

// material.glsl declares the canonical BINDING_SAMPLER_MATERIAL..STAGE_NEXT
// family unconditionally; BSP additionally has its own liquid-warp and subview
// samplers after them (mesh/sky never set STAGE_WARP, and only BSP faces show a
// subview). This stage samples all 12 plus those two (14 total), so storage
// bindings must follow those 14 -- see material.glsl's
// BINDING_STORAGE_NUM_ACTIVE_SAMPLERS comment.
#define BINDING_SAMPLER_WARP                 12
#define BINDING_SAMPLER_SUBVIEW              13
#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS  14
#define BINDING_UNIFORMS_MATERIAL            2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform bspLocalsBlock {

  /**
   * @brief The model matrix. Unused here, but both stages take the same block at the same slot.
   */
  mat4 model;

  uvec4 activeDynamicLights[MAX_DYNAMIC_LIGHTS / 128];

  /**
   * @brief The layer of textureSubviews this draw's faces sample, or -1 for none.
   */
  int subviewLayer;

  /**
   * @brief Whether that layer is stored mirrored in x, and so is read back flipped.
   */
  int subviewMirrored;
};

#include "light.glsl"

/**
 * @brief Warp texture for STAGE_WARP liquid surfaces.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_WARP) uniform sampler2D textureWarp;

/**
 * @brief The views rendered for the faces that show them, one layer per subview.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SUBVIEW) uniform sampler2DArray textureSubviews;

layout (location = 0) in CommonVertex vertex;

layout (location = 0) out vec4 outColor;

// A float depth copy for the sprite pass to sample (soft particles); see r_framebuffer.c.
layout (location = 1) out float outDepth;

CommonFragment fragment;

/**
 * @brief Applies parallax occlusion mapping to the fragment texcoord.
 */
void parallaxOcclusionMapping(in CommonVertex vertex, inout CommonFragment fragment) {

  fragment.parallax = vertex.diffusemap;

  if (material.parallax == 0.0 || fragment.texLod > 2.0 ||
      fragment.viewDist >= lightingDistance + LIGHTING_LOD_BLEND_DIST) {
    return;
  }

  float numSamples = mix(32.0, 8.0, min(fragment.texLod * 0.25, 1.0));

  vec2 texel = 1.0 / textureSize(textureMaterial, 0).xy;
  vec3 dir = normalize(fragment.viewDir * mat3(vertex.tangent, vertex.bitangent, vertex.normal));
  dir.z = max(dir.z, 0.1);
  vec2 p = ((dir.xy * texel) / dir.z) * material.parallax * material.parallax;
  vec2 delta = p / numSamples;

  vec2 texcoord = vertex.diffusemap;
  vec2 prevTexcoord = vertex.diffusemap;

  float depth = 0.0;
  float layer = 1.0 / numSamples;
  float displacement = sampleMaterialDisplacement(texcoord, fragment.texLod);

  for (int i = 0; i < int(numSamples) && depth < displacement; i++) {
    depth += layer;
    prevTexcoord = texcoord;
    texcoord -= delta;
    displacement = sampleMaterialDisplacement(texcoord, fragment.texLod);
  }

  float a = displacement - depth;
  float b = sampleMaterialDisplacement(prevTexcoord, fragment.texLod) - depth + layer;

  fragment.parallax = mix(prevTexcoord, texcoord, a / (a - b));
}

/**
 * @brief Shades BSP base surfaces and material stages.
 */
void main(void) {

  outDepth = gl_FragCoord.z;

  // a subview face shows a second view of the world: through the point a portal targets, or
  // mirrored about the face's own plane. That view uses this one's projection, so the two images
  // coincide in screen space and the fragment reads straight across. A subview is itself given a
  // layer of -1, so subviews never recurse.
  //
  // This is the base pass only: a material whose stages draw the subview suppresses it with
  // SURF_MATERIAL, and each of those stages samples it for itself, through whatever transforms
  // it carries
  if (material.flags == STAGE_NONE && (material.surface & SURF_MASK_SUBVIEW) != 0 && subviewLayer >= 0) {
    vec2 st = gl_FragCoord.xy / vec2(viewport.zw);

    // a mirrored layer is drawn by a camera whose x axis is the mirror of this one's, so it is
    // stored flipped left to right and read back the same way
    if (subviewMirrored != 0) {
      st.x = 1.0 - st.x;
    }

    outColor = vec4(texture(textureSubviews, vec3(st, subviewLayer)).rgb, 1.0);
    return;
  }

  fragment.viewDir = normalize(-vertex.position);
  fragment.viewDist = length(vertex.position);
  fragment.texLod = textureQueryLod(textureMaterial, vertex.diffusemap).x;

  parallaxOcclusionMapping(vertex, fragment);

  if (material.flags == STAGE_NONE) {

    fragment.diffuseSample = sampleMaterialDiffuse(fragment.parallax);

#ifdef ALPHA_TEST
    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuseSample.a < material.alphaTest) {
        discard;
      }
    }
#endif

    outColor = fragment.diffuseSample;

    outColor *= vertex.color;

    fragmentLightingLod(vertex, fragment);

    outColor.rgb *= (fragment.ambient + fragment.diffuse);
    outColor.rgb += fragment.specular;

  } else {

    // a stage naming its material's own diffusemap draws what the face would have drawn, which
    // for such a face is its subview. Its coordinates are then the screen's, since that is
    // where the subview's image lives, and every transform the stage carries -- warp, scroll,
    // rotate -- disturbs the view itself rather than a texture drawn over it
    bool subview = (material.flags & STAGE_SUBVIEW) == STAGE_SUBVIEW && subviewLayer >= 0;

    // a mirrored layer is stored flipped left to right; see the base pass above
    bool mirrored = subview && subviewMirrored != 0;

    vec2 st = subview ? gl_FragCoord.xy / vec2(viewport.zw) : fragment.parallax;

    if (mirrored) {
      st.x = 1.0 - st.x;
    }

    if ((material.flags & STAGE_WARP) == STAGE_WARP) {

      // the ripple is sampled, and its amplitude given, in the face's own texture coordinates,
      // as it is for any other surface. A subview is read in screen coordinates, so the offset
      // is carried into them through the texcoord's screen derivative: the same material then
      // warps by the same amount of surface whatever the display's shape or resolution, and a
      // subview further away warps less of the screen, as it should
      vec2 texcoord = subview ? vertex.diffusemap : st;

      vec2 offset = (texture(textureWarp, texcoord + vec2(ticks * material.warp.x * 0.000125)).xy - 0.5) * material.warp.y;

      if (subview) {
        vec2 dx = dFdx(vertex.diffusemap);
        vec2 dy = dFdy(vertex.diffusemap);

        // invert the 2x2 mapping from screen pixels to texture coordinates
        float det = dx.x * dy.y - dy.x * dx.y;
        if (abs(det) > 1.0e-12) {
          vec2 pixels = vec2(dy.y * offset.x - dy.x * offset.y,
                             dx.x * offset.y - dx.y * offset.x) / det;

          // the offset is in this view's screen pixels, and a mirrored layer runs the other way
          if (mirrored) {
            pixels.x = -pixels.x;
          }

          st += pixels / vec2(viewport.zw);
        }
      } else {
        st += offset;
      }
    }

    if (subview) {
      fragment.diffuseSample = vec4(texture(textureSubviews, vec3(st, subviewLayer)).rgb, 1.0);
    } else {
      fragment.diffuseSample = sampleMaterialStage(st);
    }

    fragment.diffuseSample *= vertex.color;

    outColor = fragment.diffuseSample;

    if ((material.flags & STAGE_LIGHTING) == STAGE_LIGHTING) {
      fragmentLightingLod(vertex, fragment);
      outColor.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      outColor.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      outColor.rgb += fragment.diffuseSample.rgb * material.emissive;
    }
  }
}
