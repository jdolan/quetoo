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

#define SURF_SLICK      0x2
#define SURF_SKY        0x4
#define SURF_LIQUID     0x8
#define SURF_BLEND_33   0x10
#define SURF_BLEND_66   0x20
#define SURF_BLEND_100  0x40
#define SURF_NO_DRAW    0x80
#define SURF_HINT       0x100
#define SURF_SKIP       0x200
#define SURF_ALPHA_TEST 0x400
#define SURF_PHONG      0x800
#define SURF_MATERIAL   0x1000

#define SURF_MASK_BLEND       (SURF_BLEND_33 | SURF_BLEND_66 | SURF_BLEND_100)
#define SURF_MASK_TRANSLUCENT (SURF_MASK_BLEND | SURF_ALPHA_TEST | SURF_MATERIAL)

#define STAGE_NONE          (0)
#define STAGE_TEXTURE       (1 << 0)
#define STAGE_BLEND         (1 << 1)
#define STAGE_COLOR         (1 << 2)
#define STAGE_SCROLL_S      (1 << 3)
#define STAGE_SCROLL_T      (1 << 4)
#define STAGE_SCALE_S       (1 << 5)
#define STAGE_SCALE_T       (1 << 6)
#define STAGE_ROTATE        (1 << 7)
#define STAGE_STRETCH       (1 << 8)
#define STAGE_PULSE         (1 << 9)
#define STAGE_ANIMATION     (1 << 10)
#define STAGE_ANIM_LERP     (1 << 11)
#define STAGE_TERRAIN       (1 << 12)
#define STAGE_DIRTMAP       (1 << 13)
#define STAGE_ENVMAP        (1 << 14)
#define STAGE_WARP          (1 << 15)
#define STAGE_LIGHTING      (1 << 16)
#define STAGE_LIGHTING_FLAT (1 << 17)
#define STAGE_EMISSIVE      (1 << 18)
#define STAGE_FLARE         (1 << 19)
#define STAGE_SHELL         (1 << 20)

#define STAGE_DRAW      (1 << 30)

const float PI = 3.141592653589793115997963468544185161590576171875;
const float TWO_PI = PI * 2.0;

const float DIRTMAP[8] = float[](0.125, 0.250, 0.375, 0.500, 0.625, 0.750, 0.875, 1.000);

/**
 * @brief Defines the shared sampler bindings for the lit-material shader family.
 * @remarks Fragment stages declare and sample the full family (material,
 * shadow atlas, voxel/sky, stage). Vertex stages only ever sample the voxel
 * caustics/occlusion and sky textures (see ambient_light(), vertex_caustics()
 * in light.glsl) -- they get their own compact 0..2 numbering here so their
 * descriptor set's bindings stay contiguous from zero, satisfying both
 * Vulkan's descriptor-type consistency and SDL_shadercross's contiguous-
 * binding requirement for MSL/DXIL translation. Fragment-only samplers
 * (material, shadow atlas, stage, stage next) are not declared at all for
 * vertex stages, rather than relying on dead-code elimination.
 */
#if defined(FRAGMENT_SHADER)
  #define BINDING_SAMPLER_MATERIAL        0
  #define BINDING_SAMPLER_SHADOW_ATLAS_0  1
  #define BINDING_SAMPLER_SHADOW_ATLAS_1  2
  #define BINDING_SAMPLER_SHADOW_ATLAS_2  3
  #define BINDING_SAMPLER_SHADOW_ATLAS_3  4
  #define BINDING_SAMPLER_SHADOW_ATLAS_4  5
  #define BINDING_SAMPLER_SHADOW_ATLAS_5  6
  #define BINDING_SAMPLER_VOXEL_CAUSTICS  7
  #define BINDING_SAMPLER_VOXEL_OCCLUSION 8
  #define BINDING_SAMPLER_SKY             9
  #define BINDING_SAMPLER_STAGE           10
  #define BINDING_SAMPLER_STAGE_NEXT      11
#elif defined(VERTEX_SHADER)
  #define BINDING_SAMPLER_VOXEL_CAUSTICS  0
  #define BINDING_SAMPLER_VOXEL_OCCLUSION 1
  #define BINDING_SAMPLER_SKY             2
#else
  #error "Define VERTEX_SHADER or FRAGMENT_SHADER when compiling shaders."
#endif

/**
 * @brief Defines the shared storage buffer bindings for lights and voxel clusters.
 * @remarks Including files must define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS first.
 */
#define BINDING_STORAGE_BSP_LIGHTS           (BINDING_STORAGE_NUM_ACTIVE_SAMPLERS + 0)
#define BINDING_STORAGE_DYNAMIC_LIGHTS       (BINDING_STORAGE_NUM_ACTIVE_SAMPLERS + 1)
#define BINDING_STORAGE_VOXEL_LIGHT_DATA     (BINDING_STORAGE_NUM_ACTIVE_SAMPLERS + 2)
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES  (BINDING_STORAGE_NUM_ACTIVE_SAMPLERS + 3)

#if defined(FRAGMENT_SHADER)
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_MATERIAL) uniform sampler2DArray texture_material;

/**
 * @brief Declares the six shadow atlas face samplers.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_0) uniform sampler2DShadow texture_shadow_atlas_0;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_1) uniform sampler2DShadow texture_shadow_atlas_1;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_2) uniform sampler2DShadow texture_shadow_atlas_2;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_3) uniform sampler2DShadow texture_shadow_atlas_3;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_4) uniform sampler2DShadow texture_shadow_atlas_4;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SHADOW_ATLAS_5) uniform sampler2DShadow texture_shadow_atlas_5;
#endif

/**
 * @brief Declares the voxel caustics and occlusion volumes.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_VOXEL_CAUSTICS)  uniform sampler3D texture_voxel_caustics;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_VOXEL_OCCLUSION) uniform sampler3D texture_voxel_occlusion;

/**
 * @brief Declares the sky cubemap sampler.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SKY) uniform samplerCube texture_sky;

/**
 * @brief Declares the shared material and stage uniform block.
 * @remarks Field order must stay std140-compatible with r_material_uniforms_t.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_UNIFORMS_MATERIAL) uniform material_block {

  /**
   * @brief The stage color.
   */
  vec4 color;

  /**
   * @brief The stage texture coordinate origin for rotations and stretches.
   */
  vec2 st_origin;

  /**
   * @brief The stage stretch amplitude and frequency.
   */
  vec2 stretch;

  /**
   * @brief The stage scroll rate.
   */
  vec2 scroll;

  /**
   * @brief The stage scale factor.
   */
  vec2 scale;

  /**
   * @brief The stage terrain blending floor and ceiling.
   */
  vec2 terrain;

  /**
   * @brief The stage warp rate and intensity.
   */
  vec2 warp;

  /**
   * @brief The material surface flags (SURF_*).
   */
  int surface;

  /**
   * @brief The material alpha test threshold.
   */
  float alpha_test;

  /**
   * @brief The material roughness.
   */
  float roughness;

  /**
   * @brief The material hardness.
   */
  float hardness;

  /**
   * @brief The material specularity.
   */
  float specularity;

  /**
   * @brief The material parallax.
   */
  float parallax;

  /**
   * @brief The material self-shadowing.
   */
  float shadow;

  /**
   * @brief The stage flags.
   */
  int flags;

  /**
   * @brief The stage alpha pulse.
   */
  float pulse;

  /**
   * @brief Per-stage pulse drift offset in seconds.
   */
  float drift;

  /**
   * @brief The stage rotation rate in radians per second.
   */
  float rotate;

  /**
   * @brief The stage dirtmap intensity.
   */
  float dirtmap;

  /**
   * @brief The stage lighting intensity.
   */
  float lighting;

  /**
   * @brief The stage emissive intensity. Adds unlit stage color to output.
   */
  float emissive;

  /**
   * @brief The animation lerp factor [0, 1] between current and next frame.
   */
  float lerp;

  /**
   * @brief The stage shell radius.
   */
  float shell;

#if defined(MATERIAL_TINTS)
  /**
   * @brief Per-entity tint colors for player-skin colorization (mesh only).
   */
  vec4 tint_colors[3];
#endif
} material;

#if defined(FRAGMENT_SHADER)
/**
 * @brief The material stage texture, and its next animation frame.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_STAGE)      uniform sampler2D texture_stage;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_STAGE_NEXT) uniform sampler2D texture_stage_next;
#endif

/**
 * @brief Applies vertex-position adjustments for shell stages.
 */
void stage_transform(inout vec3 position, inout vec3 normal, inout vec3 tangent, inout vec3 bitangent) {

  if ((material.flags & STAGE_SHELL) == STAGE_SHELL) {
	  position += normal * material.shell;
  }
}

/**
 * @brief Applies per-stage vertex color and texture coordinate transforms.
 */
void stage_vertex(in vec3 in_position, inout common_vertex_t vertex) {
  int envmap = material.flags & STAGE_ENVMAP;

  if (envmap != 0) {
    vec3 view_dir = normalize(vertex.position);
    vec3 reflect_dir = reflect(view_dir, normalize(vertex.normal));
    vertex.diffusemap = vec2(0.5 + reflect_dir.y * 0.5, 0.5 - reflect_dir.z * 0.5);
  }

  if ((material.flags & STAGE_STRETCH) == STAGE_STRETCH) {
	  float p = 1.0 + sin(ticks * .001 * material.stretch.y * PI) * material.stretch.x * .5;

	  mat2 matrix;
	  vec2 translate;
	  matrix[0][0] = p;
	  matrix[1][0] = 0;
	  translate[0] = material.st_origin.x - material.st_origin.x * p;

	  matrix[0][1] = 0;
	  matrix[1][1] = p;
	  translate[1] = material.st_origin.y - material.st_origin.y * p;

	  vertex.diffusemap[0] = vertex.diffusemap[0] * matrix[0][0] + vertex.diffusemap[1] * matrix[1][0] + translate[0];
	  vertex.diffusemap[1] = vertex.diffusemap[0] * matrix[0][1] + vertex.diffusemap[1] * matrix[1][1] + translate[1];
  }

  if ((material.flags & STAGE_ROTATE) == STAGE_ROTATE) {
	  float theta = ticks * 0.001 * material.rotate * TWO_PI;
    vec2 st_origin = material.st_origin;
    if (envmap != 0) {
      st_origin = vec2(0.5);
    }

	  vertex.diffusemap = vertex.diffusemap - st_origin;
	  vertex.diffusemap = mat2(cos(theta), -sin(theta), sin(theta),  cos(theta)) * vertex.diffusemap;
	  vertex.diffusemap = vertex.diffusemap + st_origin;
  }

  if (envmap != 0) {
    if ((material.flags & (STAGE_SCALE_S | STAGE_SCALE_T)) != 0) {
      vec2 scale = vec2(
        (material.flags & STAGE_SCALE_S) == STAGE_SCALE_S ? material.scale.s : 1.0,
        (material.flags & STAGE_SCALE_T) == STAGE_SCALE_T ? material.scale.t : 1.0
      );
      vec2 centered = vertex.diffusemap - vec2(0.5);
      centered /= max(abs(scale), vec2(0.0001));
      vertex.diffusemap = centered + vec2(0.5);
    }

    if ((material.flags & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
      vertex.diffusemap.s += material.scroll.s * ticks * 0.001;
    }

    if ((material.flags & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
      vertex.diffusemap.t += material.scroll.t * ticks * 0.001;
    }
  } else {
    if ((material.flags & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
      vertex.diffusemap.s += material.scroll.s * ticks * 0.001;
    }

    if ((material.flags & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
      vertex.diffusemap.t += material.scroll.t * ticks * 0.001;
    }

    if ((material.flags & STAGE_SCALE_S) == STAGE_SCALE_S) {
      vertex.diffusemap.s *= material.scale.s;
    }

    if ((material.flags & STAGE_SCALE_T) == STAGE_SCALE_T) {
      vertex.diffusemap.t *= material.scale.t;
    }
  }

  if ((material.flags & STAGE_COLOR) == STAGE_COLOR) {
	  vertex.color = material.color;
  }

  if ((material.flags & STAGE_PULSE) == STAGE_PULSE) {
	  vertex.color.a *= (sin((ticks * .001 + material.drift) * material.pulse * PI) + 1.0) * .5;
  }

  if ((material.flags & STAGE_TERRAIN) == STAGE_TERRAIN) {
	  float z = clamp(in_position.z, material.terrain.x, material.terrain.y);
	  vertex.color.a *= (z - material.terrain.x) / (material.terrain.y - material.terrain.x);
  }

  if ((material.flags & STAGE_DIRTMAP) == STAGE_DIRTMAP) {
	  int index = int(in_position.x) + int(in_position.y) + int(in_position.z);
	  vertex.color.a *= DIRTMAP[index % DIRTMAP.length()] * material.dirtmap;
  }
}

#if defined(FRAGMENT_SHADER)
/**
 * @brief Samples the diffuse material layer.
 */
vec4 sample_material_diffuse(in vec2 texcoord) {
  return texture(texture_material, vec3(texcoord, 0));
}

/**
 * @brief Samples and transforms the material normal map.
 */
vec3 sample_material_normal(in vec2 texcoord, in mat3 tbn) {
  vec3 normalmap = texture(texture_material, vec3(texcoord, 1)).xyz * 2.0 - 1.0;
  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  return normalize(tbn * (normalmap * roughness));
}

/**
 * @brief Samples the specular map with Toksvig filtering.
 */
vec4 sample_material_specular(in vec2 texcoord) {
  vec4 specularmap;
  specularmap.rgb = texture(texture_material, vec3(texcoord, 2)).rgb * material.hardness;

  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  vec3 normalmap0 = (textureLod(texture_material, vec3(texcoord, 1), 0.0).xyz * 2.0 - 1.0) * roughness;
  vec3 normalmap1 = (textureLod(texture_material, vec3(texcoord, 1), 1.0).xyz * 2.0 - 1.0) * roughness;

  float power = pow(1.0 + material.specularity, 4.0);
  specularmap.w = power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));

  return specularmap;
}

/**
 * @brief Samples the material heightmap.
 */
float sample_material_heightmap(in vec2 texcoord, in float lod) {
  return textureLod(texture_material, vec3(texcoord, 1), lod).w;
}

/**
 * @brief Samples the material displacement map.
 */
float sample_material_displacement(in vec2 texcoord, in float lod) {
  return 1.0 - sample_material_heightmap(texcoord, lod);
}

/**
 * @brief Samples the active material stage texture.
 */
vec4 sample_material_stage(in vec2 texcoord) {
  if ((material.flags & STAGE_ANIM_LERP) == STAGE_ANIM_LERP) {
    return mix(texture(texture_stage, texcoord), texture(texture_stage_next, texcoord), material.lerp);
  }
  return texture(texture_stage, texcoord);
}

/**
 * @brief Samples the material tint map.
 */
vec4 sample_material_tint(in vec2 texcoord) {
  return texture(texture_material, vec3(texcoord, 3));
}
#endif
