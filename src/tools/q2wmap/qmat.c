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

#include "qbsp.h"


static char materials[MAX_BSP_TEXINFO][32];
static int32_t num_materials;


/*
 * @brief
 */
static void AddMaterial(const char *name){
	int32_t i;

	if(!name || !strcmp(name, "NULL"))
		return;

	for(i = 0; i < num_materials; i++){
		if(!strncmp(materials[i], name, sizeof(materials[0])))
			return;
	}

	g_strlcpy(materials[num_materials], name, sizeof(materials[0]));
	num_materials++;
}


/*
 * @brief A simple wrapper around strcmp so that it may act as a comparator.
 */
static int32_t CompareStrings(const void *p1, const void *p2){
	return strcmp((const char *)p1, (const char *)p2);
}


/*
 * @brief Loads the specified BSP file, resolves all materials referenced by it,
 * and generates a "stub" materials file.
 */
int32_t MAT_Main(void){
	char path[MAX_QPATH];
	file_t *f;
	time_t start, end;
	int32_t total_mat_time;
	int32_t i;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Generating MAT]");
		SetConsoleTitle(title);
	#endif

	Com_Print("\n----- MAT -----\n\n");

	start = time(NULL);

	g_snprintf(path, sizeof(path), "materials/%s", Basename(bsp_name));
	strcpy(path + strlen(path) - 3, "mat");

	if(Fs_Exists(path)){
		Com_Print("Materials file %s exists, skipping...\n", path);
	}
	else {  // do it

		if(!(f = Fs_OpenWrite(path)))
			Com_Error(ERR_FATAL, "Couldn't open %s for writing.\n", path);

		LoadBSPFileTexinfo(bsp_name);

		for(i = 0; i < d_bsp.num_texinfo; i++)  // resolve the materials
			AddMaterial(d_bsp.texinfo[i].texture);

		// sort them by name
		qsort(&materials[0], num_materials, sizeof(materials[0]), CompareStrings);

		for(i = 0; i < num_materials; i++){  // write the .mat definition

			Fs_Print(f,
					"{\n"
					"\tmaterial %s\n"
					"\tbump 1.0\n"
					"\thardness 1.0\n"
					"\tparallax 1.0\n"
					"\tspecular 1.0\n"
					"}\n",
					materials[i]);
		}

		Fs_Close(f);

		Com_Print("Generated %d materials\n", num_materials);
	}

	end = time(NULL);
	total_mat_time = (int)(end - start);

	Com_Print("\nMaterials time: ");
	if(total_mat_time > 59)
		Com_Print("%d Minutes ", total_mat_time / 60);
	Com_Print("%d Seconds\n", total_mat_time % 60);

	return 0;
}
