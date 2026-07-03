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

#pragma once

#define __R_LOCAL_H__

#include "renderer.h"
#include <Objectively/Vector.h>

/**
 * @brief The C-side descriptor slot map: the SDL_gpu bind/push slot arguments
 * the renderer passes to the bind and push-uniform calls.
 *
 * @remarks This DELIBERATELY duplicates the values in `shaders/bindings.glsl`
 * rather than #including it. C and GLSL only coincidentally share enough
 * preprocessor syntax to have shared that header, and the GLSL side carries
 * MATERIAL_STAGES / VERTEX_SHADER conditionals that have no meaning in C. Keep
 * these two declarations in sync by hand; see bindings.glsl for the full
 * rationale (fixed global sampler map, sparse-slot bind rule, storage buffers
 * kept contiguous after the samplers, C storage slots being zero-based within
 * their class). Only `BINDING_SAMPLER_SKY` (a GLSL layout binding) is mirrored
 * here, because the C side uses it to size a pipeline's `num_samplers`.
 */
enum {
	// Sampled-texture bind slots (SAMPLER_SET). Slots repeat across pipeline
	// families (each keeps its samplers dense from 0); a resource has one slot
	// within its family.
	SLOT_SAMPLER_MATERIAL            = 0, // lit family (bsp, mesh)
	SLOT_SAMPLER_VOXEL_LIGHT_DATA    = 1,
	SLOT_SAMPLER_SHADOW_ATLAS        = 2,
	SLOT_SAMPLER_VOXEL_CAUSTICS      = 3, // voxel caustics / occlusion volumes
	SLOT_SAMPLER_VOXEL_OCCLUSION     = 4,
	SLOT_SAMPLER_SKY_AMBIENT         = 5, // sky cubemap for image-based ambient
	SLOT_SAMPLER_STAGE               = 6, // material-stage texture / next-frame
	SLOT_SAMPLER_STAGE_NEXT          = 7,
	SLOT_SAMPLER_DIFFUSE             = 0, // unlit family (sprites, 2D)
	SLOT_SAMPLER_NEXT_DIFFUSE        = 1,
	SLOT_SAMPLER_DEPTH_ATTACHMENT    = 2, // scene depth (soft particles)
	SLOT_SAMPLER_SKY                 = 6, // sky: lone fixed high slot

	// The sky sampler's fixed GLSL layout(binding); mirrored so pipelines can
	// size num_samplers = BINDING_SAMPLER_SKY + 1.
	BINDING_SAMPLER_SKY              = 6,

	// Storage-buffer bind slots, zero-based within the storage class.
	SLOT_STORAGE_LIGHTS              = 0,
	SLOT_STORAGE_VOXEL_LIGHT_INDICES = 1,

	// Uniform-buffer push slots (UNIFORM_SET).
	SLOT_UNIFORMS_GLOBALS            = 0, // per-frame globals
	SLOT_UNIFORMS_LOCALS             = 1, // per-draw (model matrix / light cull)
	SLOT_UNIFORMS_MATERIAL           = 2,
	SLOT_UNIFORMS_STAGE_VERTEX       = 2, // material-stage params (vertex stage)
	SLOT_UNIFORMS_STAGE_FRAGMENT     = 3, // material-stage params (fragment stage)
};

/**
 * @brief The scene framebuffer color format: an HDR (unsigned float) target so
 * lighting can exceed 1.0 and feed bloom. All 3D pipelines render into it; the
 * present framebuffer stays the swapchain (LDR) format, and R_DrawPost composites
 * the scene into it (adding bloom, clamping to LDR).
 */
#define R_SCENE_COLOR_FORMAT SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT
