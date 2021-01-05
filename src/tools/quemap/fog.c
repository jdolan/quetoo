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

#include "fog.h"
#include "light.h"

GArray *fogs;

/**
 * @brief
 */
_Bool PointInsideFog(const vec3_t point, const fog_t *fog) {

	if (Vec3_BoxIntersect(point, point, fog->mins, fog->maxs)) {

		for (guint i = 0; i < fog->brushes->len; i++) {
			const cm_bsp_brush_t *brush = g_ptr_array_index(fog->brushes, i);
			if (Cm_PointInsideBrush(point, brush)) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief
 */
void FreeFog(void) {

	if (!fogs) {
		return;
	}

	g_array_free(fogs, true);
	fogs = NULL;
}

/**
 * @brief
 */
static void FogForEntity(const cm_entity_t *entity) {

	const char *class_name = Cm_EntityValue(entity, "classname")->string;
	if (!g_strcmp0(class_name, "worldspawn")) {

		if (Cm_EntityValue(entity, "fog_absorption")->parsed ||
			Cm_EntityValue(entity, "fog_color")->parsed ||
			Cm_EntityValue(entity, "fog_density")->parsed ||
			Cm_EntityValue(entity, "fog_noise")->parsed) {

			fog_t fog = {};
			fog.type = FOG_GLOBAL;
			fog.entity = entity;
			
			if (Cm_EntityValue(entity, "fog_absorption")->parsed & ENTITY_FLOAT) {
				fog.absorption = Cm_EntityValue(entity, "fog_absorption")->value;
			} else {
				fog.absorption = FOG_ABSORPTION;
			}

			const cm_entity_t *color = Cm_EntityValue(entity, "fog_color");
			if (color->parsed & ENTITY_VEC3) {
				fog.color = color->vec3;
			} else {
				fog.color = FOG_COLOR;
			}

			fog.density = Cm_EntityValue(entity, "fog_density")->value ?: FOG_DENSITY;
			fog.noise = Cm_EntityValue(entity, "fog_noise")->value ?: FOG_NOISE;

			g_array_append_val(fogs, fog);
		}
	} else if (!g_strcmp0(class_name, "misc_fog")) {

		fog_t fog = {};
		fog.type = FOG_VOLUME;
		fog.entity = entity;

		if (Cm_EntityValue(entity, "absorption")->parsed & ENTITY_FLOAT) {
			fog.absorption = Cm_EntityValue(entity, "absorption")->value;
		} else {
			fog.absorption = FOG_ABSORPTION;
		}

		if (Cm_EntityValue(entity, "_color")->parsed & ENTITY_VEC3) {
			fog.color = Cm_EntityValue(entity, "_color")->vec3;
		} else {
			fog.color = FOG_COLOR;
		}

		fog.density = Cm_EntityValue(entity, "density")->value ?: FOG_DENSITY;
		fog.noise = Cm_EntityValue(entity, "noise")->value ?: FOG_NOISE;

		fog.brushes = Cm_EntityBrushes(entity);

		fog.mins = Vec3_Mins();
		fog.maxs = Vec3_Maxs();

		for (guint i = 0; i < fog.brushes->len; i++) {
			const cm_bsp_brush_t *brush = g_ptr_array_index(fog.brushes, i);

			fog.mins = Vec3_Minf(fog.mins, brush->mins);
			fog.maxs = Vec3_Maxf(fog.maxs, brush->maxs);
		}

		g_array_append_val(fogs, fog);
	}
}

/**
 * @brief
 */
void BuildFog(void) {

	FreeFog();

	fogs = g_array_new(false, false, sizeof(fog_t));

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (size_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		FogForEntity(*entity);
	}

	Com_Verbose("Fog lighting for %d sources\n", fogs->len);
}
