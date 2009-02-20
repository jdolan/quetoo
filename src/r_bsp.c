/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "renderer.h"


vec3_t modelorg;  // relative to viewpoint


/*
 * R_CullBox
 *
 * Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
qboolean R_CullBox(const vec3_t mins, const vec3_t maxs){
	int i;

	if(!r_cull->value)
		return false;

	for(i = 0; i < 4; i++)
		if(BOX_ON_PLANE_SIDE(mins, maxs, &r_locals.frustum[i]) == SIDE_ON)
			return true;

	return false;
}


/*
 * R_CullBspModel
 *
 * Returns true if the specified entity is completely culled by the view
 * frustum, false otherwise.
 */
qboolean R_CullBspModel(const entity_t *e){
	vec3_t mins, maxs;
	int i;

	if(!e->model->nummodelsurfaces)  // no surfaces
		return true;

	if(e->angles[0] || e->angles[1] || e->angles[2]){
		for(i = 0; i < 3; i++){
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	return R_CullBox(mins, maxs);
}


/*
 * R_DrawBspModelSurfaces
 */
static void R_DrawBspModelSurfaces(const entity_t *e){
	cplane_t *plane;
	msurface_t *surf;
	float dot;
	int i;

	// temporarily swap the view frame so that the surface drawing
	// routines pickup only the bsp model's surfaces

	const int f = r_locals.frame;
	r_locals.frame = -2;

	surf = &e->model->surfaces[e->model->firstmodelsurface];

	for(i = 0; i < e->model->nummodelsurfaces; i++, surf++){

		plane = surf->plane;

		// find which side of the surf we are on
		if(AXIAL(plane))
			dot = modelorg[plane->type] - plane->dist;
		else
			dot = DotProduct(modelorg, plane->normal) - plane->dist;

		if(((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
				(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON))){
			// visible, flag for rendering
			surf->frame = r_locals.frame;
			surf->backframe = -1;
		}
		else {  // back-facing
			surf->frame = -1;
			surf->backframe = r_locals.frame;
		}
	}

	R_DrawOpaqueSurfaces(e->model->opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(e->model->opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(e->model->alpha_test_surfaces);

	R_EnableBlend(true);

	R_DrawBackSurfaces(e->model->back_surfaces);

	R_DrawMaterialSurfaces(e->model->material_surfaces);

	R_EnableFog(false);

	R_DrawBlendSurfaces(e->model->blend_surfaces);

	R_DrawBlendWarpSurfaces(e->model->blend_warp_surfaces);

	R_DrawFlareSurfaces(e->model->flare_surfaces);

	R_EnableFog(true);

	R_EnableBlend(false);

	r_locals.frame = f;  // undo the swap
}


/*
 * R_DrawBspModel
 */
void R_DrawBspModel(const entity_t *e){
	vec3_t forward, right, up;
	vec3_t temp;

	// set the relative origin, accounting for rotation if necessary
	VectorSubtract(r_view.origin, e->origin, modelorg);
	if(e->angles[0] || e->angles[1] || e->angles[2]){

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);

		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	R_ShiftLights(e->origin);

	glPushMatrix();

	R_RotateForEntity(e);

	R_DrawBspModelSurfaces(e);

	glPopMatrix();

	R_ShiftLights(vec3_origin);
}


/*
 * R_DrawBspNormals
 *
 * Developer tool for viewing BSP vertex normals.  Only Phong interpolated
 * surfaces show their normals when r_shownormals > 1.0.
 */
void R_DrawBspNormals(void){
	msurface_t *surf;
	GLfloat *vertex, *normal;
	vec3_t end;
	int i, j, k;

	if(!r_shownormals->value)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_ResetArrayState();  // default arrays

	glColor3f(1.0, 0.0, 0.0);

	k = 0;
	surf = r_worldmodel->surfaces;
	for(i = 0; i < r_worldmodel->numsurfaces; i++, surf++){

		if(surf->visframe != r_locals.visframe)
			continue;  // not visible

		if(surf->texinfo->flags & (SURF_SKY | SURF_WARP))
			continue;  // don't care

		if(r_shownormals->value > 1.0 && !(surf->texinfo->flags & SURF_PHONG))
			continue;  // don't care

		if(k > MAX_GL_ARRAY_LENGTH - 512){  // avoid overflows, draw in batches
			glDrawArrays(GL_LINES, 0, k / 3);
			k = 0;
		}

		for(j = 0; j < surf->numedges; j++){
			vertex = &r_worldmodel->verts[(surf->index + j) * 3];
			normal = &r_worldmodel->normals[(surf->index + j) * 3];

			VectorMA(vertex, 12.0, normal, end);

			memcpy(&r_state.vertex_array_3d[k], vertex, sizeof(vec3_t));
			memcpy(&r_state.vertex_array_3d[k + 3], end, sizeof(vec3_t));
			k += sizeof(vec3_t) / sizeof(vec_t) * 2;
		}
	}

	glDrawArrays(GL_LINES, 0, k / 3);

	R_EnableTexture(&texunit_diffuse, true);

	glColor4ubv(color_white);
}


/*
 * R_MarkSurfaces_
 *
 * Top-down BSP node recursion.  Nodes identified as within the PVS by
 * R_MarkLeafs are first frustum-culled; those which fail immediately
 * return.
 *
 * For the rest, the front-side child node is recursed.  Any surfaces marked
 * in that recursion must then pass a dot-product test to resolve sidedness.
 * Finally, the back-side child node is recursed.
 */
static void R_MarkSurfaces_(mnode_t *node){
	int i, side, sidebit;
	msurface_t *surf, **lsurf;
	cplane_t *plane;
	float dot;

	if(node->contents == CONTENTS_SOLID)
		return;  // solid

	if(node->visframe != r_locals.visframe)
		return;  // not in view

	if(R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;  // culled out

	// if leaf node, flag surfaces to draw this frame
	if(node->contents != CONTENTS_NODE){
		mleaf_t *leaf = (mleaf_t *)node;

		if(r_view.areabits){  // check for door connected areas
			if(!(r_view.areabits[leaf->area >> 3] & (1 << (leaf->area & 7))))
				return;  // not visible
		}

		lsurf = leaf->firstleafsurface;

		for(i = 0; i < leaf->numleafsurfaces; i++, lsurf++){
			(*lsurf)->visframe = r_locals.visframe;
		}

		return;
	}

	// otherwise, traverse down the apropriate sides of the node

	plane = node->plane;

	if(AXIAL(plane))
		dot = r_view.origin[plane->type] - plane->dist;
	else
		dot = DotProduct(r_view.origin, plane->normal) - plane->dist;

	if(dot >= 0){
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = MSURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_MarkSurfaces_(node->children[side]);

	// prune all marked surfaces to just those which are front-facing
	surf = r_worldmodel->surfaces + node->firstsurface;

	for(i = 0; i < node->numsurfaces; i++, surf++){

		if(surf->visframe == r_locals.visframe){  // it's been marked

			if((surf->flags & MSURF_PLANEBACK) != sidebit){  // but back-facing
				surf->frame = -1;
				surf->backframe = r_locals.frame;
			}
			else {  // draw it
				surf->frame = r_locals.frame;
				surf->backframe = -1;
			}
		}
	}

	// recurse down the back side
	R_MarkSurfaces_(node->children[!side]);
}


/*
 * R_MarkSurfaces
 *
 * Entry point for BSP recursion and surface-level visibility test.  This
 * is also where the infamous r_optimize strategy is implemented.
 */
void R_MarkSurfaces(void){
	static vec3_t oldorigin, oldangles;
	static int oldvisframe;
	static float oldfov;
	vec3_t o, a;

	VectorSubtract(r_view.origin, oldorigin, o);
	VectorSubtract(r_view.angles, oldangles, a);

	// only recurse after cluster change AND significant movement
	if(r_optimize->value &&
			(r_locals.visframe == oldvisframe) &&  // same pvs
			(r_view.fov_x == oldfov) &&  // same fov
			VectorLength(o) < 5.0 && VectorLength(a) < 2.0)  // little movement
		return;

	oldvisframe = r_locals.visframe;
	oldfov = r_view.fov_x;

	r_locals.frame++;

	if(r_locals.frame > 0xffff)  // avoid overflows, negatives are reserved
		r_locals.frame = 0;

	VectorCopy(r_view.origin, oldorigin);
	VectorCopy(r_view.angles, oldangles);

	R_ClearSkyBox();

	// flag all visible world surfaces
	R_MarkSurfaces_(r_worldmodel->nodes);
}


/*
 * R_LeafForPoint
 */
const mleaf_t *R_LeafForPoint(const vec3_t p, const model_t *model){
	const mnode_t *node;
	const cplane_t *plane;
	float dot;

	if(!model || !model->nodes){
		Com_Error(ERR_DROP, "R_LeafForPoint: Bad model.");
	}

	node = model->nodes;
	while(true){

		if(node->contents != CONTENTS_NODE)
			return (mleaf_t *)node;

		plane = node->plane;

		if(AXIAL(plane))
			dot = p[plane->type] - plane->dist;
		else
			dot = DotProduct(p, plane->normal) - plane->dist;

		if(dot > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	Com_Error(ERR_DROP, "R_LeafForPoint: NULL");
	return NULL;  // never reached
}


/*
 * R_DecompressVis
 */
static byte *R_DecompressVis(const byte *in){
	static byte decompressed[MAX_BSP_LEAFS / 8];
	int c;
	byte *out;
	int row;

	row = (r_worldmodel->vis->numclusters + 7) >> 3;
	out = decompressed;

	if(!in){  // no vis info, so make all visible
		while(row){
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if(*in){
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while(c){
			*out++ = 0;
			c--;
		}
	} while(out - decompressed < row);

	return decompressed;
}


/*
 * R_LeafInVis
 */
static inline qboolean R_LeafInVis(const mleaf_t *leaf, const byte *vis){
	int c;

	if(!vis)
		return true;

	if((c = leaf->cluster) == -1)
		return false;

	return vis[c >> 3] & (1 << (c & 7));
}


/*
 * R_MarkLeafs
 *
 * Mark the leafs that are in the PVS for the current cluster, creating the
 * recursion path for R_MarkSurfaces.  Leafs marked for the current cluster
 * will still be frustum-culled, and surfaces therein must still pass a
 * dot-product test in order to be marked as visible for the current frame.
 */
void R_MarkLeafs(void){
	byte *vis;
	mleaf_t *leaf;
	mnode_t *node;
	int i;

	if(r_lockvis->value)
		return;

	// resolve current cluster
	r_locals.cluster = R_LeafForPoint(r_view.origin, r_worldmodel)->cluster;

	if(!r_novis->value && (r_locals.oldcluster == r_locals.cluster))
		return;

	r_locals.oldcluster = r_locals.cluster;

	r_locals.visframe++;

	if(r_locals.visframe > 0xffff)  // avoid overflows, negatives are reserved
		r_locals.visframe = 0;

	if(r_novis->value || !r_worldmodel->vis){  // mark everything
		for(i = 0; i < r_worldmodel->numleafs; i++)
			r_worldmodel->leafs[i].visframe = r_locals.visframe;
		for(i = 0; i < r_worldmodel->numnodes; i++)
			r_worldmodel->nodes[i].visframe = r_locals.visframe;
		return;
	}

	// resolve pvs for the current cluster
	if(r_locals.cluster != -1)
		vis = R_DecompressVis((byte *)r_worldmodel->vis +
				r_worldmodel->vis->bitofs[r_locals.cluster][DVIS_PVS]);
	else
		vis = NULL;

	// recurse up the bsp from the visible leafs, marking a path via the nodes
	leaf = r_worldmodel->leafs;

	for(i = 0; i < r_worldmodel->numleafs; i++, leaf++){

		if(!R_LeafInVis(leaf, vis))
			continue;

		node = (mnode_t *)leaf;
		while(node){

			if(node->visframe == r_locals.visframe)
				break;

			node->visframe = r_locals.visframe;
			node = node->parent;
		}
	}
}
