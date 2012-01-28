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

#include "qlight.h"

/*
 *
 * every surface must be divided into at least two patches each axis
 *
 */

patch_t *face_patches[MAX_BSP_FACES];
int num_patches;

vec3_t face_offset[MAX_BSP_FACES];	// for rotating bmodels

boolean_t extra_samples;

vec3_t ambient;

float brightness = 1.0;
float saturation = 1.0;
float contrast = 1.0;

float surface_scale = 0.4;
float entity_scale = 1.0;


/*
 * Light_PointInLeafnum
 */
static int Light_PointInLeafnum(const vec3_t point){
	int nodenum;

	nodenum = 0;
	while(nodenum >= 0){
		d_bsp_node_t *node = &d_bsp.nodes[nodenum];
		d_bsp_plane_t *plane = &d_bsp.planes[node->plane_num];
		vec_t dist = DotProduct(point, plane->normal) - plane->dist;
		if(dist > 0)
			nodenum = node->children[0];
		else
			nodenum = node->children[1];
	}

	return -nodenum - 1;
}


/*
 * Light_PointInLeaf
 */
d_bsp_leaf_t *Light_PointInLeaf(const vec3_t point){
	const int num = Light_PointInLeafnum(point);
	return &d_bsp.leafs[num];
}


/*
 * PvsForOrigin
 */
boolean_t PvsForOrigin(const vec3_t org, byte *pvs){
	d_bsp_leaf_t *leaf;

	if(!d_bsp.vis_data_size){
		memset(pvs, 0xff, (d_bsp.num_leafs + 7) / 8);
		return true;
	}

	leaf = Light_PointInLeaf(org);
	if(leaf->cluster == -1)
		return false;  // in solid leaf

	DecompressVis(d_bsp.vis_data + d_vis->bit_offsets[leaf->cluster][DVIS_PVS], pvs);
	return true;
}

// we use the c_model_t collision detection facilities for lighting
static int num_cmodels;
static c_model_t *cmodels[MAX_BSP_MODELS];

/*
 * Light_Trace
 */
void Light_Trace(c_trace_t *trace, const vec3_t start, const vec3_t end, int mask){
	float frac;
	int i;

	frac = 9999.0;

	// and any BSP submodels, too
	for(i = 0; i < num_cmodels; i++){
		const c_trace_t tr = Cm_BoxTrace(start, end, vec3_origin, vec3_origin,
				cmodels[i]->head_node, mask);

		if(tr.fraction < frac){
			frac = tr.fraction;
			*trace = tr;
		}
	}
}




/*
 * LightWorld
 */
static void LightWorld(void){
	int i;

	if(d_bsp.num_nodes == 0 || d_bsp.num_faces == 0)
		Com_Error(ERR_FATAL, "LightWorld: Empty map\n");

	// load the map for tracing
	cmodels[0] = Cm_LoadBsp(bsp_name, &i);
	num_cmodels = Cm_NumModels();

	for(i = 1; i < num_cmodels; i++){
		cmodels[i] = Cm_Model(va("*%d", i));
	}

	// turn each face into a single patch
	BuildPatches();

	// subdivide patches to a maximum dimension
	SubdividePatches();

	// create lights out of patches and entities
	BuildLights();

	// patches are no longer needed
	FreePatches();

	// build per-vertex normals for phong shading
	BuildVertexNormals();

	// build initial facelights
	RunThreadsOn(d_bsp.num_faces, true, BuildFacelights);

	// finalize it and write it out
	d_bsp.lightmap_data_size = 0;
	RunThreadsOn(d_bsp.num_faces, true, FinalLightFace);
}


/*
 * LIGHT_Main
 */
int LIGHT_Main(void){
	time_t start, end;
	int total_light_time;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Compiling LIGHT]");
		SetConsoleTitle(title);
	#endif

	Com_Print("\n----- LIGHT -----\n\n");

	start = time(NULL);

	LoadBSPFile(bsp_name);

	if(!d_bsp.vis_data_size)
		Com_Error(ERR_FATAL, "No vis information\n");

	ParseEntities();

	CalcTextureReflectivity();

	LightWorld();

	WriteBSPFile(bsp_name);

	end = time(NULL);
	total_light_time = (int)(end - start);
	Com_Print("\nLIGHT Time: ");
	if(total_light_time > 59)
		Com_Print("%d Minutes ", total_light_time / 60);
	Com_Print("%d Seconds\n", total_light_time % 60);

	return 0;
}
