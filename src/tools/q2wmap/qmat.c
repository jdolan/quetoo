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
static int num_materials;


/*
 * AddMaterial
 */
static void AddMaterial(const char *name){
	int i;

	if(!name || !strcmp(name, "NULL"))
		return;

	for(i = 0; i < num_materials; i++){
		if(!strncmp(materials[i], name, sizeof(materials[0])))
			return;
	}

	strncpy(materials[num_materials], name, sizeof(materials[0]));
	num_materials++;
}


/*
 * CompareStrings
 *
 * A simple wrapper around strcmp so that it may act as a comparator.
 */
static int CompareStrings(const void *p1, const void *p2){
	return strcmp((const char *)p1, (const char *)p2);
}


/*
 * MAT_Main
 *
 * Loads the specified BSP file, resolves all materials referenced by it,
 * and generates a "stub" materials file.
 */
int MAT_Main(void){
	char path[MAX_QPATH];
	FILE *f;
	time_t start, end;
	int total_mat_time;
	int i;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Generating MAT]");
		SetConsoleTitle(title);
	#endif

	Com_Print("\n----- MAT -----\n\n");

	start = time(NULL);

	snprintf(path, sizeof(path), "materials/%s", Basename(bsp_name));
	strcpy(path + strlen(path) - 3, "mat");

	if((i = Fs_OpenFile(path, &f, FILE_READ)) > -1){
		Com_Print("Materials file %s exists, skipping...\n", path);
		Fs_CloseFile(f);
	}
	else {  // do it

		if((i = Fs_OpenFile(path, &f, FILE_WRITE)) == -1)
			Com_Error(ERR_FATAL, "Couldn't open %s for writing.\n", path);

		LoadBSPFileTexinfo(bsp_name);

		for(i = 0; i < d_bsp.num_texinfo; i++)  // resolve the materials
			AddMaterial(d_bsp.texinfo[i].texture);

		// sort them by name
		qsort(&materials[0], num_materials, sizeof(materials[0]), CompareStrings);

		for(i = 0; i < num_materials; i++){  // write the .mat definition

			fprintf(f,
					"{\n"
					"\tmaterial %s\n"
					"\tbump 1.0\n"
					"\thardness 1.0\n"
					"\tparallax 1.0\n"
					"\tspecular 1.0\n"
					"}\n",
					materials[i]);
		}

		Fs_CloseFile(f);

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
