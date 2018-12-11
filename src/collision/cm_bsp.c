#include "cm_local.h"

/**
 * @brief Metadata for BSP lumps
 */
typedef struct {
	size_t size_ofs; // offset to size
	size_t data_ofs; // offset to ptr
	size_t type_size; // size of the data we're pointed to
	size_t max_count; // the max size of this lump (in elements, not bytes)
} bsp_lump_meta_t;

#ifndef BSP_SIZEOF
#define BSP_SIZEOF(T, F) \
	sizeof(*((T *) 0)->F)
#endif

#define BSP_LUMP_NUM_STRUCT(n, m) { \
	.size_ofs = offsetof(bsp_file_t, num_ ## n), \
	.data_ofs = offsetof(bsp_file_t, n), \
	.type_size = BSP_SIZEOF(bsp_file_t, n), \
	.max_count = m \
}

#define BSP_LUMP_SIZE_STRUCT(n, m) { \
	.size_ofs = offsetof(bsp_file_t, n ## _size), \
	.data_ofs = offsetof(bsp_file_t, n),\
	.type_size = sizeof(byte), \
	.max_count = m \
}

#define BSP_LUMP_SKIP { 0, 0, 0, 0 }

static bsp_lump_meta_t bsp_lump_meta[BSP_TOTAL_LUMPS] = {
	BSP_LUMP_SIZE_STRUCT(entity_string, MAX_BSP_ENT_STRING),
	BSP_LUMP_NUM_STRUCT(planes, MAX_BSP_PLANES),
	BSP_LUMP_NUM_STRUCT(vertexes, MAX_BSP_VERTS),
	BSP_LUMP_SIZE_STRUCT(vis_data, MAX_BSP_VISIBILITY),
	BSP_LUMP_NUM_STRUCT(nodes, MAX_BSP_NODES),
	BSP_LUMP_NUM_STRUCT(texinfo, MAX_BSP_TEXINFO),
	BSP_LUMP_NUM_STRUCT(faces, MAX_BSP_FACES),
	BSP_LUMP_SIZE_STRUCT(lightmap_data, MAX_BSP_LIGHTING),
	BSP_LUMP_NUM_STRUCT(leafs, MAX_BSP_LEAFS),
	BSP_LUMP_NUM_STRUCT(leaf_faces, MAX_BSP_LEAF_FACES),
	BSP_LUMP_NUM_STRUCT(leaf_brushes, MAX_BSP_LEAF_BRUSHES),
	BSP_LUMP_NUM_STRUCT(edges, MAX_BSP_EDGES),
	BSP_LUMP_NUM_STRUCT(face_edges, MAX_BSP_FACE_EDGES),
	BSP_LUMP_NUM_STRUCT(models, MAX_BSP_MODELS),
	BSP_LUMP_NUM_STRUCT(brushes, MAX_BSP_BRUSHES),
	BSP_LUMP_NUM_STRUCT(brush_sides, MAX_BSP_BRUSH_SIDES),
	BSP_LUMP_SKIP,
	BSP_LUMP_NUM_STRUCT(areas, MAX_BSP_AREAS),
	BSP_LUMP_NUM_STRUCT(area_portals, MAX_BSP_AREA_PORTALS)
};

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
/**
 * @brief Table of swap functions.
 */
typedef void (*Bsp_SwapFunction) (void *lump, const int32_t num);

/**
 * @brief Swap function.
 */
static void Bsp_SwapPlanes(void *lump, const int32_t num) {

	bsp_plane_t *plane = (bsp_plane_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		for (int32_t j = 0; j < 3; j++) {
			plane->normal[j] = LittleFloat(plane->normal[j]);
		}

		plane->dist = LittleFloat(plane->dist);
		plane->type = LittleLong(plane->type);

		plane++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVertexes(void *lump, const int32_t num) {

	bsp_vertex_t *vertex = (bsp_vertex_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		for (int32_t j = 0; j < 3; j++) {
			vertex->point[j] = LittleFloat(vertex->point[j]);
			vertex->normal[j] = LittleFloat(vertex->normal[j]);
		}

		vertex++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVis(void *lump, const int32_t num) {

	bsp_vis_t *vis = (bsp_vis_t *) lump;

	// visibility
	int32_t j = vis->num_clusters;

	vis->num_clusters = LittleLong(vis->num_clusters);

	for (int32_t i = 0; i < j; i++) {

		vis->bit_offsets[i][0] = LittleLong(vis->bit_offsets[i][0]);
		vis->bit_offsets[i][1] = LittleLong(vis->bit_offsets[i][1]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapNodes(void *lump, const int32_t num) {

	bsp_node_t *node = (bsp_node_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		node->plane_num = LittleLong(node->plane_num);

		for (int32_t j = 0; j < 3; j++) {
			node->mins[j] = LittleShort(node->mins[j]);
			node->maxs[j] = LittleShort(node->maxs[j]);
		}

		node->children[0] = LittleLong(node->children[0]);
		node->children[1] = LittleLong(node->children[1]);
		node->first_face = LittleShort(node->first_face);
		node->num_faces = LittleShort(node->num_faces);

		node++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapTexinfos(void *lump, const int32_t num) {

	bsp_texinfo_t *texinfo = (bsp_texinfo_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		for (int32_t j = 0; j < 4; j++) {
			texinfo->vecs[0][j] = LittleFloat(texinfo->vecs[0][j]);
			texinfo->vecs[1][j] = LittleFloat(texinfo->vecs[1][j]);
		}

		texinfo->flags = LittleLong(texinfo->flags);
		texinfo->value = LittleLong(texinfo->value);
		texinfo->next_texinfo = LittleLong(texinfo->next_texinfo);

		texinfo++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapFaces(void *lump, const int32_t num) {

	bsp_face_t *face = (bsp_face_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		face->texinfo = LittleShort(face->texinfo);
		face->plane_num = LittleShort(face->plane_num);
		face->side = LittleShort(face->side);
		face->lightmap_offset = LittleLong(face->lightmap_offset);
		face->first_face_edge = LittleLong(face->first_face_edge);
		face->num_face_edges = LittleShort(face->num_face_edges);

		face++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafs(void *lump, const int32_t num) {

	bsp_leaf_t *leaf = (bsp_leaf_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		leaf->contents = LittleLong(leaf->contents);
		leaf->cluster = LittleShort(leaf->cluster);
		leaf->area = LittleShort(leaf->area);

		for (int32_t j = 0; j < 3; j++) {
			leaf->mins[j] = LittleShort(leaf->mins[j]);
			leaf->maxs[j] = LittleShort(leaf->maxs[j]);
		}

		leaf->first_leaf_face = LittleShort(leaf->first_leaf_face);
		leaf->num_leaf_faces = LittleShort(leaf->num_leaf_faces);
		leaf->first_leaf_brush = LittleShort(leaf->first_leaf_brush);
		leaf->num_leaf_brushes = LittleShort(leaf->num_leaf_brushes);

		leaf++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafFaces(void *lump, const int32_t num) {

	uint16_t *leaf_face = (uint16_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		leaf_face[i] = LittleShort(leaf_face[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafBrushes(void *lump, const int32_t num) {

	uint16_t *leaf_brush = (uint16_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		leaf_brush[i] = LittleShort(leaf_brush[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapEdges(void *lump, const int32_t num) {

	bsp_edge_t *edge = (bsp_edge_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		edge->v[0] = LittleShort(edge->v[0]);
		edge->v[1] = LittleShort(edge->v[1]);

		edge++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapFaceEdges(void *lump, const int32_t num) {

	int32_t *face_edge = (int32_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		face_edge[i] = LittleLong(face_edge[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapModels(void *lump, const int32_t num) {

	bsp_model_t *model = (bsp_model_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		model->first_face = LittleLong(model->first_face);
		model->num_faces = LittleLong(model->num_faces);
		model->head_node = LittleLong(model->head_node);

		for (int32_t j = 0; j < 3; j++) {
			model->mins[j] = LittleFloat(model->mins[j]);
			model->maxs[j] = LittleFloat(model->maxs[j]);
			model->origin[j] = LittleFloat(model->origin[j]);
		}

		model++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushes(void *lump, const int32_t num) {

	bsp_brush_t *brush = (bsp_brush_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		brush->first_brush_side = LittleLong(brush->first_brush_side);
		brush->num_sides = LittleLong(brush->num_sides);
		brush->contents = LittleLong(brush->contents);

		brush++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushSides(void *lump, const int32_t num) {

	bsp_brush_side_t *brush_side = (bsp_brush_side_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		brush_side->plane_num = LittleShort(brush_side->plane_num);
		brush_side->surf_num = LittleShort(brush_side->surf_num);

		brush_side++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapAreas(void *lump, const int32_t num) {

	bsp_area_t *area = (bsp_area_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		area->num_area_portals = LittleLong(area->num_area_portals);
		area->first_area_portal = LittleLong(area->first_area_portal);

		area++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapAreaPortals(void *lump, const int32_t num) {

	bsp_area_portal_t *area_portal = (bsp_area_portal_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		area_portal->portal_num = LittleLong(area_portal->portal_num);
		area_portal->other_area = LittleLong(area_portal->other_area);

		area_portal++;
	}
}

static Bsp_SwapFunction bsp_swap_funcs[BSP_TOTAL_LUMPS] = {
	NULL,
	Bsp_SwapPlanes,
	Bsp_SwapVertexes,
	Bsp_SwapVis,
	Bsp_SwapNodes,
	Bsp_SwapTexinfos,
	Bsp_SwapFaces,
	NULL,
	Bsp_SwapLeafs,
	Bsp_SwapLeafFaces,
	Bsp_SwapLeafBrushes,
	Bsp_SwapEdges,
	Bsp_SwapFaceEdges,
	Bsp_SwapModels,
	Bsp_SwapBrushes,
	Bsp_SwapBrushSides,
	NULL,
	Bsp_SwapAreas,
	Bsp_SwapAreaPortals
};
#endif

/**
 * @brief Calculates the effective size of the BSP file.
 */
int64_t Bsp_Size(const bsp_header_t *file) {
	int64_t total = 0;

	for (bsp_lump_id_t i = 0; i < BSP_TOTAL_LUMPS; i++) {

		total += LittleLong(file->lumps[i].file_len);
	}

	return total;
}

/**
 * @brief Verifies that a file is indeed a BSP file and returns
 * the version number. Returns -1 if it is not a valid BSP.
 */
int32_t Bsp_Verify(const bsp_header_t *file) {

	if (LittleLong(file->ident) != BSP_IDENT) {
		return -1;
	}

	if (LittleLong(file->version) != BSP_VERSION) {
		return -1;
	}

	return BSP_VERSION;
}

/**
 * @brief Read the lump length/offset from the BSP file.
 */
static void Bsp_GetLumpPosition(const bsp_header_t *file, const bsp_lump_id_t lump_id, d_bsp_lump_t *lump) {

	*lump = file->lumps[lump_id];
	lump->file_len = LittleLong(lump->file_len);
	lump->file_ofs = LittleLong(lump->file_ofs);
}

/**
 * @brief Set if the lump is valid but the ptrs do not exist.
 */
#define LUMP_SKIPPED	(int32_t *) (ptrdiff_t) -1u

/**
 * @brief Convenience to calculate bsp_file offset in bytes
 */
#define BSP_BYTE_OFFSET(bsp, bytes) \
	(((byte *) bsp) + bytes)

/**
 * @brief Get the lump offset data for the specified lump. Returns false if the
 * lump is not valid. num and data will be filled with the pointer to the lump's
 * count and data pointers in memory. They may be empty. If num is LUMP_SKIPPED,
 * the lump is a valid lump but not stored/used by the library.
 */
static _Bool Bsp_GetLumpOffsets(const bsp_file_t *bsp, const bsp_lump_id_t lump_id, int32_t **num, void ***data) {

	if (lump_id >= BSP_TOTAL_LUMPS) {
		return false;
	}

	bsp_lump_meta_t *meta = &bsp_lump_meta[lump_id];

	if (!meta->type_size) {

		if (num) {
			*num = LUMP_SKIPPED;
		}

	} else {

		if (num) {
			*num = (int32_t *) BSP_BYTE_OFFSET(bsp, meta->size_ofs);
		}

		if (data) {
			*data = (void **) BSP_BYTE_OFFSET(bsp, meta->data_ofs);
		}
	}

	return true;
}

/**
 * @brief Check whether the specified lump is loaded in memory or not.
 */
_Bool Bsp_LumpLoaded(const bsp_file_t *bsp, const bsp_lump_id_t lump_id) {

	return bsp->loaded_lumps & (bsp_lump_id_t) (1 << lump_id);
}

/**
 * @brief Unloads the specified lump from memory.
 */
void Bsp_UnloadLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id) {

	if (!Bsp_LumpLoaded(bsp, lump_id)) {
		return;
	}

	int32_t *lump_count;
	void **lump_data;

	if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
		Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
	}

	// lump is valid but we're skipping it
	if (lump_count == LUMP_SKIPPED) {
		return;
	}

	// free memory
	if (*lump_data) {
		Mem_Free(*lump_data);
		*lump_data = NULL;
	}

	*lump_count = 0;

	bsp->loaded_lumps &= ~((bsp_lump_id_t) (1 << lump_id));
}

/**
 * @brief Unloads the specified lumps from memory.
 */
void Bsp_UnloadLumps(bsp_file_t *bsp, const bsp_lump_id_t lump_bits) {

	for (bsp_lump_id_t i = BSP_LUMP_ENTITIES; i < BSP_TOTAL_LUMPS; i++) {

		if (lump_bits & (bsp_lump_id_t) (i << 1)) {
			Bsp_UnloadLump(bsp, i);
		}
	}
}

/**
 * @brief Load a lump into memory from the specified BSP file. Returns false
 * if an error occured during the load that is recoverable.
 */
_Bool Bsp_LoadLump(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_id) {

	int32_t *lump_count;
	void **lump_data;

	if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
		Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
	}

	// lump is valid but we're skipping it
	if (lump_count == LUMP_SKIPPED) {
		return true;
	}

	// unload the lump if we're already loaded
	Bsp_UnloadLump(bsp, lump_id);

	// find the lump in the file
	d_bsp_lump_t lump;
	Bsp_GetLumpPosition(file, lump_id, &lump);

	const size_t lump_type_size = bsp_lump_meta[lump_id].type_size;

	if (lump.file_len % lump_type_size) {
		Com_Error(ERROR_DROP, "Lump (%i) size (%i) doesn't match expected data type (%" PRIuPTR ")\n", lump_id, lump.file_len,
		          lump_type_size);
	}

	*lump_count = lump.file_len / lump_type_size;

	if (*lump_count >= (int32_t) bsp_lump_meta[lump_id].max_count) {
		Com_Error(ERROR_DROP, "Lump (%i) count (%i) exceeds max (%" PRIuPTR ")\n", lump_id, *lump_count,
		          bsp_lump_meta[lump_id].max_count);
	}

	*lump_data = Mem_TagMalloc(lump.file_len, MEM_TAG_BSP | (lump_id << 16));

	// blit the data into memory
	if (lump.file_ofs && lump.file_len) {

		const byte *file_lump = ((byte *) file) + lump.file_ofs;

		memcpy(*lump_data, file_lump, lump.file_len);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
		// swap the lump if required
		if (bsp_swap_funcs[lump_id]) {
			bsp_swap_funcs[lump_id](*lump_data, *lump_count);
		}
#endif
	}

	bsp->loaded_lumps |= (bsp_lump_id_t) (1 << lump_id);

	return true;
}

/**
 * @brief Loads the specified lumps into memory. If a failure occurs at any point during
 * loading, it will stop trying to load more and return false.
 */
_Bool Bsp_LoadLumps(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_bits) {

	for (bsp_lump_id_t i = BSP_LUMP_ENTITIES; i < BSP_TOTAL_LUMPS; i++) {

		if (lump_bits & (bsp_lump_id_t) (1 << i)) {
			if (!Bsp_LoadLump(file, bsp, i)) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Allocates data for the specified lump in the BSP. If the lump is already loaded,
 * the data will either be expanded or truncated to the specified count. Note that "count"
 * is not in bytes, but rather the number of components to allocate - this depends on the
 * lump_id (for instance, the VERTEXES lump will allocate count * bsp_vertex_t). This
 * will count as loading a lump as well. Since realloc is used here, be careful that you
 * aren't storing a pointer to the old lump data. The lump size pointer (as in, num_x or x_size)
 * won't be modified by this function call, so be careful!
 */
void Bsp_AllocLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id, const size_t count) {

	// mark as loaded
	if (!Bsp_LumpLoaded(bsp, lump_id)) {
		bsp->loaded_lumps |= (bsp_lump_id_t) (1 << lump_id);
	}

	int32_t *lump_count;
	void **lump_data;

	if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
		Com_Error(ERROR_DROP, "Tried to allocate an invalid lump (%i)\n", lump_id);
	}

	// lump is valid but we're skipping it
	if (lump_count == LUMP_SKIPPED) {
		return;
	}

	// calculate size
	const size_t lump_type_size = bsp_lump_meta[lump_id].type_size;

	*lump_data = Mem_Realloc(*lump_data, lump_type_size * count);
}

/**
 * @brief Writes the specified BSP to the file. This will write from the current
 * position of the file.
 */
void Bsp_Write(file_t *file, const bsp_file_t *bsp) {

	// create the header
	bsp_header_t header;

	header.ident = LittleLong(BSP_IDENT);
	header.version = LittleLong(BSP_VERSION);

	// store where we are, write what we got
	int64_t header_pos = Fs_Tell(file);
	Fs_Write(file, &header, sizeof(header), 1);

	// write out the lumps now

	int64_t current_position = Fs_Tell(file);
	memset(header.lumps, 0, sizeof(header.lumps));

	for (bsp_lump_id_t i = 0; i < BSP_TOTAL_LUMPS; i++) {
		int32_t *lump_count;
		void **lump_data;
		const size_t lump_type_size = bsp_lump_meta[i].type_size;

		Bsp_GetLumpOffsets(bsp, i, &lump_count, &lump_data);

		// lump is valid but we're skipping it
		if (lump_count == LUMP_SKIPPED || *lump_count == 0) {
			continue;
		}

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
		// swap lump to disk endianness
		if (bsp_swap_funcs[i]) {
			bsp_swap_funcs[i](*lump_data, *lump_count);
		}
#endif

		// write and increase position for next lump
		const int64_t len = Fs_Write(file, *lump_data, lump_type_size, *lump_count);
		const int64_t lump_size = (int32_t) (len * lump_type_size);

		header.lumps[i].file_len = LittleLong((int32_t) lump_size);
		header.lumps[i].file_ofs = LittleLong((int32_t) current_position);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
		// swap back to memory endianness
		if (bsp_swap_funcs[i]) {
			bsp_swap_funcs[i](*lump_data, *lump_count);
		}
#endif

		current_position += lump_size;
	}

	// go back and write the finished header
	Fs_Seek(file, header_pos);
	Fs_Write(file, &header, sizeof(header), 1);

	// return to where we were
	Fs_Seek(file, current_position);
}

/**
 * @brief
 */
void Bsp_DecompressVis(const bsp_file_t *bsp, const byte *in, byte *out) {
	const int32_t row = (bsp->vis_data.vis->num_clusters + 7) >> 3;
	byte *out_p = out;

	if (!in || !bsp->vis_data_size) { // no vis info, so make all visible
		for (int32_t i = 0; i < row; i++) {
			*out_p++ = 0xff;
		}
	} else {
		do {
			if (*in) {
				*out_p++ = *in++;
				continue;
			}

			int32_t c = in[1];
			in += 2;
			if ((out_p - out) + c > row) {
				c = (int32_t) (row - (out_p - out));
				Com_Warn("Overrun\n");
			}
			while (c) {
				*out_p++ = 0;
				c--;
			}
		} while (out_p - out < row);
	}
}

/**
 * @brief
 */
int32_t Bsp_CompressVis(const bsp_file_t *bsp, const byte *vis, byte *dest) {
	int32_t j;
	int32_t rep;
	int32_t visrow;
	byte *dest_p;

	dest_p = dest;
	visrow = (bsp->vis_data.vis->num_clusters + 7) >> 3;

	for (j = 0; j < visrow; j++) {
		*dest_p++ = vis[j];
		if (vis[j]) {
			continue;
		}

		rep = 1;
		for (j++; j < visrow; j++)
			if (vis[j] || rep == 0xff) {
				break;
			} else {
				rep++;
			}
		*dest_p++ = rep;
		j--;
	}

	return (int32_t) (ptrdiff_t) (dest_p - dest);
}
