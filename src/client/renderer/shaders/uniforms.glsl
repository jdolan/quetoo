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

#define VIEW_UNKNOWN      0
#define VIEW_MAIN         1
#define VIEW_PLAYER_MODEL 2

#define BSP_VOXEL_SIZE    32.0

// BSP Contents flags
#define CONTENTS_NONE   0x0
#define CONTENTS_SOLID  0x1
#define CONTENTS_WINDOW 0x2
#define CONTENTS_LAVA   0x8
#define CONTENTS_SLIME  0x10
#define CONTENTS_WATER  0x20
#define CONTENTS_MIST   0x40

#define CONTENTS_MASK_SOLID  (CONTENTS_SOLID | CONTENTS_WINDOW)
#define CONTENTS_MASK_LIQUID (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)

/**
 * @brief The voxels struct.
 */
struct voxels_t {
  /**
   * @brief The voxels mins, in world space.
   */
  vec4 mins;

  /**
   * @brief The voxels maxs, in world space.
   */
  vec4 maxs;

  /**
   * @brief The view origin, in voxel space.
   */
  vec4 view_coordinate;

  /**
   * @brief The grid size, in voxels.
   */
  vec4 size;
};

/**
 * @brief The uniforms block.
 */
layout (std140) uniform uniforms_block {
  /**
   * @brief The viewport (x, y, w, h) in device pixels.
   */
  ivec4 viewport;

  /**
   * @brief The 2D projection matrix.
   */
  mat4 projection2D;

  /**
   * @brief The 3D projection matrix.
   */
  mat4 projection3D;

  /**
   * @brief The view matrix.
   */
  mat4 view;

  /**
   * @brief The projection matrix for environment cubemaps.
   */
  mat4 sky_projection;

  /**
   * @brief The projection matrix for shadow projection.
   */
  mat4 light_projection;

  /**
   * @brief The voxels.
   */
  voxels_t voxels;

  /**
   * @brief The depth range, in world units.
   */
  vec2 depth_range;

  /**
   * @brief The view type, e.g. VIEW_MAIN.
   */
  int view_type;

  /**
   * @brief The renderer time, in milliseconds.
   */
  int ticks;

  /**
   * @brief The ambient scalar.
   */
  float ambient;

  /**
   * @brief The modulate scalar.
   */
  float modulate;

  /**
   * @brief The caustics scalar.
   */
  float caustics;

  /**
   * @brief The volumetric fog density scalar.
   */
  float fog_density;

  /**
   * @brief The number of volumetric fog samples per fragment (quality).
   */
  float fog_samples;

  /**
   * @brief Distance threshold for switching to vertex fog.
   */
  float fog_distance;

  /**
   * @brief Distance threshold for switching to vertex lighting.
   */
  float lighting_distance;

  /**
   * @brief The editor flags, used for in-game lighting and more.
   */
  int editor;

  /**
   * @brief The developer flags, used for shader development tweaking.
   */
  int developer;

  /**
   * @brief The wireframe mode flag.
   */
  int wireframe;
};

/**
 * @brief The light struct.
 */
struct light_t {
  /**
   * @brief The light origin in model space, and radius.
   */
  vec4 origin;

  /**
   * @brief The light color and intensity.
   */
  vec4 color;
};

#define MAX_BSP_LIGHTS 256
#define MAX_DYNAMIC_LIGHTS 64
#define MAX_LIGHTS (MAX_BSP_LIGHTS + MAX_DYNAMIC_LIGHTS)

/**
 * @brief The lights uniform block.
 */
layout (std140) uniform lights_block {
  /**
   * @brief The light sources for the current frame.
   */
  light_t lights[MAX_LIGHTS];
};

/**
 * @brief The -1 terminated array of active light indexes for the current render operation.
 */
uniform int active_lights[MAX_LIGHTS];

/**
 * @brief The diffusemap texture, for non-material passes such as sprites.
 */
uniform sampler2D texture_diffusemap;
uniform sampler2D texture_next_diffusemap;

/**
 * @brief The material primary texture.
 */
uniform sampler2DArray texture_material;

/**
 * @brief The material secondary texture.
 */
uniform sampler2D texture_stage;

/**
 * @brief The warp texture, for liquids.
 */
uniform sampler2D texture_warp;

/**
 * @brief The voxel textures.
 */
uniform sampler3D texture_voxel_data;
uniform isampler3D texture_voxel_light_data;
uniform isamplerBuffer texture_voxel_light_indices;

/**
 * @brief The sky cubemap texture.
 */
uniform samplerCube texture_sky;

/**
 * @brief The shadow cubemap array texture.
 */
uniform samplerCubeArrayShadow texture_shadow_cubemap_array;

/**
 * @brief The framebuffer attachment textures.
 */
uniform sampler2D texture_color_attachment;
uniform sampler2D texture_depth_attachment_copy;
