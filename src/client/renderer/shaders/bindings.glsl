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

#ifndef _BINDINGS_GLSL_
#define _BINDINGS_GLSL_

/*
 * The one, fixed, global descriptor binding map, included by both the GLSL shaders
 * and the C renderer so a resource has the SAME binding in every pipeline: voxel
 * light data is always BINDING_SAMPLER_VOXEL_LIGHT_DATA, whether in bsp, mesh, or
 * sprite. shadercross preserves the binding number (binding N -> Metal [[texture(N)]]),
 * so a shader may declare only the samplers it uses at their fixed slots. HOWEVER,
 * SDL_gpu validates that EVERY slot in [0, num_samplers) is bound at draw time (it
 * checks presence, not which slots the shader samples), so a program that uses a
 * high fixed slot must still bind the leading unused slots (with any texture) and
 * declare num_samplers large enough to cover its highest slot.
 *
 * BINDING_* are the GLSL `layout(binding = ...)` values (shader side). SLOT_* are
 * the SDL_gpu bind/push slot arguments (C side). They coincide for samplers and
 * uniform buffers. Storage buffers share the sampler descriptor set, and -- unlike
 * samplers, whose Metal texture indices are independent so sparse bindings survive
 * shadercross -- their Metal [[buffer(n)]] index is packed right after the samplers.
 * So storage BINDING_* must be contiguous with (immediately follow) the samplers a
 * program declares, or the shadercross index desyncs from SDL_gpu's runtime slot.
 * The C side binds storage by a zero-based slot WITHIN the storage class, so
 * SLOT_STORAGE_* differs from BINDING_STORAGE_* -- only their relative order matters.
 */

/*
 * Sampled textures (SAMPLER_SET). Because SDL_gpu requires every slot in
 * [0, num_samplers) to be bound at draw, each program family keeps its samplers
 * dense from 0 so no filler binds are needed; slot numbers may therefore repeat
 * ACROSS families (separate pipelines) while a resource keeps ONE binding WITHIN
 * its family.
 *
 * Lit-geometry family (bsp, mesh): material / voxel-data / shadow at [0, 2].
 */
#define BINDING_SAMPLER_MATERIAL            0
#define BINDING_SAMPLER_VOXEL_LIGHT_DATA    1
#define BINDING_SAMPLER_SHADOW_ATLAS        2

/*
 * Unlit textured family (sprites, 2D): diffuse and its animation next-frame,
 * plus the scene depth attachment (soft particles) at the following slot.
 */
#define BINDING_SAMPLER_DIFFUSE             0
#define BINDING_SAMPLER_NEXT_DIFFUSE        1
#define BINDING_SAMPLER_DEPTH_ATTACHMENT    2

/*
 * Sky: a lone samplerCube, at a fixed high slot to exercise the fixed-map path.
 * The leading unused slots are filled with the same binding (see R_DrawSky).
 */
#define BINDING_SAMPLER_SKY                 6

#define SLOT_SAMPLER_MATERIAL               BINDING_SAMPLER_MATERIAL
#define SLOT_SAMPLER_VOXEL_LIGHT_DATA       BINDING_SAMPLER_VOXEL_LIGHT_DATA
#define SLOT_SAMPLER_SHADOW_ATLAS           BINDING_SAMPLER_SHADOW_ATLAS
#define SLOT_SAMPLER_DIFFUSE                BINDING_SAMPLER_DIFFUSE
#define SLOT_SAMPLER_NEXT_DIFFUSE           BINDING_SAMPLER_NEXT_DIFFUSE
#define SLOT_SAMPLER_DEPTH_ATTACHMENT       BINDING_SAMPLER_DEPTH_ATTACHMENT
#define SLOT_SAMPLER_SKY                    BINDING_SAMPLER_SKY

/*
 * Material stage samplers (lit family, MATERIAL_STAGES variant only): the stage
 * texture and its animation next-frame, immediately after the base lit samplers.
 */
#if defined(MATERIAL_STAGES)
#define BINDING_SAMPLER_STAGE               3
#define BINDING_SAMPLER_STAGE_NEXT          4
#endif

// C-side slots are unconditional (the C renderer includes this without MATERIAL_STAGES).
#define SLOT_SAMPLER_STAGE                  3
#define SLOT_SAMPLER_STAGE_NEXT             4

/*
 * Storage buffers, contiguous immediately after the lit-geometry samplers: [0, 2]
 * for the base lit programs, or [0, 4] for the MATERIAL_STAGES variant (which adds
 * the two stage samplers). The C-side storage slot (0, 1) is unchanged either way.
 */
#if defined(MATERIAL_STAGES)
#define BINDING_STORAGE_LIGHTS              5
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES 6
#else
#define BINDING_STORAGE_LIGHTS              3
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES 4
#endif

#define SLOT_STORAGE_LIGHTS                 0
#define SLOT_STORAGE_VOXEL_LIGHT_INDICES    1

/*
 * Uniform buffers (UNIFORM_SET). The per-draw "locals" slot holds the model matrix
 * in the vertex stage and the active-lights bitmask in the fragment stage. Globals
 * and locals are declared in uniforms.glsl as BINDING_UNIFORMS / BINDING_LOCALS;
 * the material parameters follow them.
 */
#define BINDING_UNIFORMS_MATERIAL           2

#define SLOT_UNIFORMS_GLOBALS               0
#define SLOT_UNIFORMS_LOCALS                1
#define SLOT_UNIFORMS_MATERIAL              BINDING_UNIFORMS_MATERIAL

/*
 * Per-stage parameters (MATERIAL_STAGES variant only). Kept contiguous within each
 * stage's uniform set: the vertex stage has no material UBO, so the stage UBO
 * follows globals(0)+locals(1) at 2; the fragment stage follows the material UBO at 3.
 */
#if defined(MATERIAL_STAGES)
#if defined(VERTEX_SHADER)
#define BINDING_UNIFORMS_STAGE              2
#else
#define BINDING_UNIFORMS_STAGE              3
#endif
#endif

// C-side push slots are unconditional (see above).
#define SLOT_UNIFORMS_STAGE_VERTEX          2
#define SLOT_UNIFORMS_STAGE_FRAGMENT        3

#endif // _BINDINGS_GLSL_
