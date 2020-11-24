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

#include "cg_types.h"

#ifdef __CG_LOCAL_H__

typedef struct cg_entity_s cg_entity_t;

typedef void (*EntityInit)(cg_entity_t *self);
typedef void (*EntityThink)(cg_entity_t *self);

/**
 * @brief The entity class type.
 */
typedef struct {
	/**
	 * @brief The entity class name.
	 */
	const char *class_name;

	/**
	 * @brief The initialization function, called once per level.
	 */
	EntityInit Init;

	/**
	 * @brief The think function, called once per client frame.
	 */
	EntityThink Think;

	/**
	 * @brief The size of the opaque data.
	 */
	size_t data_size;

} cg_entity_class_t;

/**
 * @brief The entity instance type.
 */
struct cg_entity_s {

	/**
	 * @brief The entity class.
	 */
	const cg_entity_class_t *clazz;

	/**
	 * @brief The backing entity definition.
	 */
	const cm_entity_t *def;

	/**
	 * @brief The entity origin.
	 */
	vec3_t origin;

	/**
	 * @brief For PVS and PHS culling.
	 */
	const r_bsp_leaf_t *leaf;

	/**
	 * @brief The entity frequency and randomness.
	 */
	float hz, drift;

	/**
	 * @brief Timestamp for next emission.
	 */
	uint32_t next_think;

	/**
	 * @brief Opaque, type-specific data.
	 */
	void *data;
};

cl_entity_t *Cg_Self(void);
_Bool Cg_IsSelf(const cl_entity_t *ent);
_Bool Cg_IsDucking(const cl_entity_t *ent);
void Cg_TraverseStep(cl_entity_step_t *step, uint32_t time, float height);
void Cg_InterpolateStep(cl_entity_step_t *step);
void Cg_Interpolate(const cl_frame_t *frame);
void Cg_AddEntities(const cl_frame_t *frame);
void Cg_LoadEntities(void);
void Cg_FreeEntities(void);
#endif /* __CG_ENTITY_H__ */
