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

#include "bspfile.h"
#include "scriptlib.h"

bsp_file_t bsp_file;

/**
 * @brief Dumps info about current file
 */
static void PrintBSPFileSizes(void) {

	if (!num_entities) {
		ParseEntities();
	}

	Com_Verbose("%5i models       %7i\n", bsp_file.num_models,
	            (int32_t) (bsp_file.num_models * sizeof(bsp_model_t)));

	Com_Verbose("%5i brushes      %7i\n", bsp_file.num_brushes,
	            (int32_t) (bsp_file.num_brushes * sizeof(bsp_brush_t)));

	Com_Verbose("%5i brush_sides  %7i\n", bsp_file.num_brush_sides,
	            (int32_t) (bsp_file.num_brush_sides * sizeof(bsp_brush_side_t)));

	Com_Verbose("%5i planes       %7i\n", bsp_file.num_planes,
	            (int32_t) (bsp_file.num_planes * sizeof(bsp_plane_t)));

	Com_Verbose("%5i texinfo      %7i\n", bsp_file.num_texinfo,
	            (int32_t) (bsp_file.num_texinfo * sizeof(bsp_texinfo_t)));

	Com_Verbose("%5i entdata      %7i\n", num_entities, bsp_file.entity_string_size);

	Com_Verbose("\n");

	Com_Verbose("%5i vertexes     %7i\n", bsp_file.num_vertexes,
	            (int32_t) (bsp_file.num_vertexes * sizeof(bsp_vertex_t)));

	Com_Verbose("%5i normals      %7i\n", bsp_file.num_normals,
	            (int32_t) (bsp_file.num_normals * sizeof(bsp_normal_t)));

	Com_Verbose("%5i nodes        %7i\n", bsp_file.num_nodes,
	            (int32_t) (bsp_file.num_nodes * sizeof(bsp_node_t)));

	Com_Verbose("%5i faces        %7i\n", bsp_file.num_faces,
	            (int32_t) (bsp_file.num_faces * sizeof(bsp_face_t)));

	Com_Verbose("%5i leafs        %7i\n", bsp_file.num_leafs,
	            (int32_t) (bsp_file.num_leafs * sizeof(bsp_leaf_t)));

	Com_Verbose("%5i leaf_faces   %7i\n", bsp_file.num_leaf_faces,
	            (int32_t) (bsp_file.num_leaf_faces * sizeof(bsp_file.leaf_faces[0])));

	Com_Verbose("%5i leaf_brushes %7i\n", bsp_file.num_leaf_brushes,
	            (int32_t) (bsp_file.num_leaf_brushes * sizeof(bsp_file.leaf_brushes[0])));

	Com_Verbose("%5i surf_edges   %7i\n", bsp_file.num_face_edges,
	            (int32_t) (bsp_file.num_face_edges * sizeof(bsp_file.face_edges[0])));

	Com_Verbose("%5i edges        %7i\n", bsp_file.num_edges,
	            (int32_t) (bsp_file.num_edges * sizeof(bsp_edge_t)));

	Com_Verbose("      lightmap     %7i\n", bsp_file.lightmap_data_size);

	Com_Verbose("      vis          %7i\n", bsp_file.vis_data_size);
}

uint16_t num_entities;
entity_t entities[MAX_BSP_ENTITIES];

/**
 * @brief
 */
static void StripTrailing(char *e) {
	char *s;

	s = e + strlen(e) - 1;
	while (s >= e && *s <= 32) {
		*s = 0;
		s--;
	}
}

/**
 * @brief
 */
epair_t *ParseEpair(void) {
	epair_t *e;

	e = Mem_TagMalloc(sizeof(*e), MEM_TAG_EPAIR);

	if (strlen(token) >= MAX_BSP_ENTITY_KEY - 1) {
		Com_Error(ERROR_FATAL, "Token too long\n");
	}
	e->key = Mem_CopyString(token);
	GetToken(false);
	if (strlen(token) >= MAX_BSP_ENTITY_VALUE - 1) {
		Com_Error(ERROR_FATAL, "Token too long\n");
	}
	e->value = Mem_CopyString(token);

	// strip trailing spaces
	StripTrailing(e->key);
	StripTrailing(e->value);

	return e;
}

/**
 * @brief
 */
static _Bool ParseEntity(void) {
	epair_t *e;
	entity_t *mapent;

	if (!GetToken(true)) {
		return false;
	}

	if (g_strcmp0(token, "{")) {
		Com_Error(ERROR_FATAL, "\"{\" not found\n");
	}

	if (num_entities == MAX_BSP_ENTITIES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES\n");
	}

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if (!GetToken(true)) {
			Com_Error(ERROR_FATAL, "EOF without closing brace\n");
		}
		if (!g_strcmp0(token, "}")) {
			break;
		}
		e = ParseEpair();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (true);

	return true;
}

/**
 * @brief Parses the d_bsp.entity_string string into entities
 */
void ParseEntities(void) {

	ParseFromMemory(bsp_file.entity_string, bsp_file.entity_string_size);

	num_entities = 0;
	while (ParseEntity()) {
	}
}

/**
 * @brief Generates the entdata string from all the entities
 */
void UnparseEntities(void) {
	char *buf, *end;
	epair_t *ep;
	char line[2096];
	int32_t i;
	char key[1024], value[1024];

	Bsp_AllocLump(&bsp_file, BSP_LUMP_ENTITIES, MAX_BSP_ENT_STRING);
	buf = bsp_file.entity_string;
	end = buf;
	*end = 0;

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep) {
			continue; // ent got removed
		}

		strcat(end, "{\n");
		end += 2;

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			strcpy(key, ep->key);
			StripTrailing(key);
			strcpy(value, ep->value);
			StripTrailing(value);

			sprintf(line, "\"%s\" \"%s\"\n", key, value);
			strcat(end, line);
			end += strlen(line);
		}
		strcat(end, "}\n");
		end += 2;

		if (end > buf + MAX_BSP_ENT_STRING) {
			Com_Error(ERROR_FATAL, "MAX_BSP_ENT_STRING\n");
		}
	}

	bsp_file.entity_string_size = (int32_t) (ptrdiff_t) (end - buf + 1);
}

void SetKeyValue(entity_t *ent, const char *key, const char *value) {
	epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next) {
		if (!g_strcmp0(ep->key, key)) {
			Mem_Free(ep->value);
			ep->value = Mem_CopyString(value);
			return;
		}
	}

	ep = Mem_TagMalloc(sizeof(*ep), MEM_TAG_EPAIR);
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = Mem_CopyString(key);
	ep->value = Mem_CopyString(value);
}

const char *ValueForKey(const entity_t *ent, const char *key) {
	const epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!g_strcmp0(ep->key, key)) {
			return ep->value;
		}
	return "";
}

vec_t FloatForKey(const entity_t *ent, const char *key) {
	const char *k;

	k = ValueForKey(ent, key);
	return atof(k);
}

void VectorForKey(const entity_t *ent, const char *key, vec3_t vec) {
	const char *k;

	k = ValueForKey(ent, key);

	if (sscanf(k, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3) {
		VectorClear(vec);
	}
}

int32_t LoadBSPFile(const char *filename, const bsp_lump_id_t lumps) {

	memset(&bsp_file, 0, sizeof(bsp_file));

	bsp_header_t *file;

	if (Fs_Load(filename, (void **) &file) == -1) {
		Com_Error(ERROR_FATAL, "Invalid BSP file at %s\n", filename);
	}

	const int32_t version = Bsp_Verify(file);

	if (!version) {
		Fs_Free(file);
		Com_Error(ERROR_FATAL, "Invalid BSP file at %s\n", filename);
	}

	Bsp_LoadLumps(file, &bsp_file, lumps);
	Fs_Free(file);

	return version;
}

void WriteBSPFile(const char *filename, const int32_t version) {
	file_t *file = Fs_OpenWrite(filename);
	Bsp_Write(file, &bsp_file, version);
	Fs_Close(file);

	if (verbose) {
		PrintBSPFileSizes();
	}
}
