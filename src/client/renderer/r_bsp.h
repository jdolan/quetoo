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

vec_t R_DistanceToSurface(const vec3_t p, const r_bsp_face_t *surf);
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_bsp_model_t *bsp);
_Bool R_LeafVisible(const r_bsp_leaf_t *leaf);
_Bool R_LeafHearable(const r_bsp_leaf_t *leaf);
_Bool R_CullBox(const vec3_t mins, const vec3_t maxs);
_Bool R_CullSphere(const vec3_t point, const vec_t radius);

#ifdef __R_LOCAL_H__
_Bool R_CullBspInlineModel(const r_entity_t *e);
void R_DrawBspInlineModels(const r_entities_t *ents);
void R_AddBspInlineModelFlares(const r_entities_t *ents);
void R_DrawBspLeafs(void);
void R_DrawBspNormals(void);
void R_DrawBspLights(void);
void R_MarkBspFaces(void);
void R_UpdateVis(void);
#endif /* __R_LOCAL_H__ */
