/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __R_BSP_H__
#define __R_BSP_H__

const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_model_t *model);
boolean_t R_LeafInPvs(const r_bsp_leaf_t *leaf);

#ifdef __R_LOCAL_H__
const char *R_WorldspawnValue(const char *key);
boolean_t R_CullBox(const vec3_t mins, const vec3_t maxs);
boolean_t R_CullBspModel(const r_entity_t *e);
void R_DrawBspModel(const r_entity_t *e);
void R_DrawBspLights(void);
void R_DrawBspNormals(void);
void R_MarkSurfaces(void);
void R_MarkLeafs(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_BSP_H__ */
