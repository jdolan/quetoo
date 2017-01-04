#include "cm_local.h"

/**
 * @brief Stores positions to offsets in bsp_file_t to various lump data
 */
typedef struct {
	size_t num_offset; // offset to size
	size_t ptr_offset; // offset to ptr
	size_t ptr_size; // size of the data we're pointed to
} bsp_lump_ofs_t;

#ifndef member_sizeof
	#define BSP_SIZEOF(T, F) \
		sizeof(*((T *) 0)->F)
#endif

#define BSP_LUMP_NUM_STRUCT(n) \
	{ .num_offset = offsetof(bsp_file_t, num_ ## n), .ptr_offset = offsetof(bsp_file_t, n), .ptr_size = BSP_SIZEOF(bsp_file_t, n) }
#define BSP_LUMP_SIZE_STRUCT(n) \
	{ .num_offset = offsetof(bsp_file_t, n ## _size), .ptr_offset = offsetof(bsp_file_t, n), .ptr_size = sizeof(byte) }
#define BSP_LUMP_SKIP \
	{ 0, 0, 0 }

static bsp_lump_ofs_t bsp_lump_ofs[BSP_LUMPS] = {
	BSP_LUMP_SIZE_STRUCT(entity_string),
	BSP_LUMP_NUM_STRUCT(planes),
	BSP_LUMP_NUM_STRUCT(vertexes),
	BSP_LUMP_SIZE_STRUCT(vis_data),
	BSP_LUMP_NUM_STRUCT(nodes),
	BSP_LUMP_NUM_STRUCT(texinfo),
	BSP_LUMP_NUM_STRUCT(faces),
	BSP_LUMP_SIZE_STRUCT(lightmap_data),
	BSP_LUMP_NUM_STRUCT(leafs),
	BSP_LUMP_NUM_STRUCT(leaf_faces),
	BSP_LUMP_NUM_STRUCT(leaf_brushes),
	BSP_LUMP_NUM_STRUCT(edges),
	BSP_LUMP_NUM_STRUCT(face_edges),
	BSP_LUMP_NUM_STRUCT(models),
	BSP_LUMP_NUM_STRUCT(brushes),
	BSP_LUMP_NUM_STRUCT(brush_sides),
	BSP_LUMP_SKIP,
	BSP_LUMP_NUM_STRUCT(areas),
	BSP_LUMP_NUM_STRUCT(area_portals),
	BSP_LUMP_NUM_STRUCT(normals)
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

	d_bsp_plane_t *plane = (d_bsp_plane_t *) lump;

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

	d_bsp_vertex_t *vertex = (d_bsp_vertex_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		for (int32_t j = 0; j < 3; j++) {
			vertex->point[j] = LittleFloat(vertex->point[j]);
		}

		vertex++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVis(void *lump, const int32_t num) {

	d_bsp_vis_t *vis = (d_bsp_vis_t *) lump;

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

	d_bsp_node_t *node = (d_bsp_node_t *) lump;

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

	d_bsp_texinfo_t *texinfo = (d_bsp_texinfo_t *) lump;

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

	d_bsp_face_t *face = (d_bsp_face_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		face->texinfo = LittleShort(face->texinfo);
		face->plane_num = LittleShort(face->plane_num);
		face->side = LittleShort(face->side);
		face->light_ofs = LittleLong(face->light_ofs);
		face->first_edge = LittleLong(face->first_edge);
		face->num_edges = LittleShort(face->num_edges);

		face++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafs(void *lump, const int32_t num) {

	d_bsp_leaf_t *leaf = (d_bsp_leaf_t *) lump;

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

	d_bsp_edge_t *edge = (d_bsp_edge_t *) lump;
	
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

	d_bsp_model_t *model = (d_bsp_model_t *) lump;
	
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

	d_bsp_brush_t *brush = (d_bsp_brush_t *) lump;
	
	for (int32_t i = 0; i < num; i++) {
		
		brush->first_side = LittleLong(brush->first_side);
		brush->num_sides = LittleLong(brush->num_sides);
		brush->contents = LittleLong(brush->contents);

		brush++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushSides(void *lump, const int32_t num) {

	d_bsp_brush_side_t *brush_side = (d_bsp_brush_side_t *) lump;
	
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

	d_bsp_area_t *area = (d_bsp_area_t *) lump;
	
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

	d_bsp_area_portal_t *area_portal = (d_bsp_area_portal_t *) lump;
	
	for (int32_t i = 0; i < num; i++) {
		
		area_portal->portal_num = LittleLong(area_portal->portal_num);
		area_portal->other_area = LittleLong(area_portal->other_area);

		area_portal++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapNormals(void *lump, const int32_t num) {

	d_bsp_normal_t *normal = (d_bsp_normal_t *) lump;
	
	for (int32_t i = 0; i < num; i++) {
		
		for (int32_t j = 0; j < 3; j++) {
			normal->normal[j] = LittleFloat(normal->normal[j]);
		}

		normal++;
	}
}

static Bsp_SwapFunction bsp_swap_funcs[BSP_LUMPS] = {
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
	Bsp_SwapAreaPortals,
	Bsp_SwapNormals
};
#endif

/**
 * @brief Represents the data to find and read in a lump from the disk.
 */
typedef struct {
	int32_t file_ofs;
	int32_t file_len;
} bsp_lump_t;

/**
 * @brief Represents the header of a BSP file.
 */
typedef struct {
	int32_t ident;
	int32_t version;
	bsp_lump_t lumps[BSP_LUMPS];
} bsp_header_t;

/**
 * @brief Verifies that a file is indeed a BSP file and returns
 * the version number. Returns -1 if it is not a valid BSP.
 */
int32_t Bsp_Verify(file_t *file) {

	bsp_header_t header;

	Fs_Seek(file, 0);
	Fs_Read(file, &header, sizeof(header), 1);
	
	if (LittleLong(header.ident) != BSP_IDENT) {
		return -1;
	}

	return LittleLong(header.version);
}

/**
 * @brief Read the lump length/offset from the BSP file.
 */
static void Bsp_GetLumpPosition(file_t *file, const int32_t lump_id, bsp_lump_t *lump) {

	Fs_Seek(file, sizeof(int32_t) + sizeof(int32_t) + (sizeof(bsp_lump_t) * lump_id));
	Fs_Read(file, lump, sizeof(bsp_lump_t), 1);
}

/**
 * @brief Get the lump offset data for the specified lump. Returns NULL if
 * the specified lump is not valid.
 */
static bsp_lump_ofs_t *Bsp_GetLumpOffsets(const int32_t lump_id) {

	if (lump_id < 0 || lump_id >= BSP_LUMPS) {
		return NULL;
	}

	return &bsp_lump_ofs[lump_id];
}

#define BSP_BYTE_OFFSET(bsp, bytes) \
	(((byte *) bsp) + bytes)

/**
 * @brief Check whether the specified lump is loaded in memory or not.
 */
_Bool Bsp_LumpLoaded(bsp_file_t *bsp, const int32_t lump_id) {

	return bsp->loaded_lumps & (1 << lump_id);
}

/** 
 * @brief Unloads the specified lump from memory.
 */
void Bsp_UnloadLump(bsp_file_t *bsp, const int32_t lump_id) {

	if (!Bsp_LumpLoaded(bsp, lump_id)) {
		return;
	}

	bsp_lump_ofs_t *ofs = Bsp_GetLumpOffsets(lump_id);

	if (!ofs) {
		Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
	}

	// lump is valid but we're skipping it
	if (ofs->ptr_size == 0) {
		return;
	}

	int32_t *num_of_lump = (int32_t *) BSP_BYTE_OFFSET(bsp, ofs->num_offset);
	void **lump_ptr = (void **) BSP_BYTE_OFFSET(bsp, ofs->ptr_offset);

	// already unloaded
	if (*num_of_lump == -1) {
		return;
	}

	// free memory
	if (*lump_ptr) {
		Mem_Free(*lump_ptr);
		*lump_ptr = NULL;
	}

	*num_of_lump = 0;

	bsp->loaded_lumps &= ~(1 << lump_id);
}

/**
 * @brief Unloads the specified lumps from memory.
 */
void Bsp_UnloadLumps(bsp_file_t *bsp, const uint32_t lump_bits) {

	for (int32_t i = 0; i < BSP_LUMPS; i++) {
	
		if (lump_bits & (i << 1)) {
			Bsp_UnloadLump(bsp, i);
		}
	}
}

/**
 * @brief Load a lump into memory from the specified BSP file. Returns false
 * if an error occured during the load that is recoverable.
 */
_Bool Bsp_LoadLump(file_t *file, bsp_file_t *bsp, const int32_t lump_id) {

	bsp_lump_ofs_t *ofs = Bsp_GetLumpOffsets(lump_id);

	if (!ofs) {
		Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
	}

	// lump is valid but we're skipping it
	if (ofs->ptr_size == 0) {
		return true;
	}

	// unload the lump if we're already loaded
	Bsp_UnloadLump(bsp, lump_id);
	
	// find the lump in the file
	bsp_lump_t lump;
	Bsp_GetLumpPosition(file, lump_id, &lump);

	if (lump.file_len % ofs->ptr_size) {
		Com_Error(ERROR_DROP, "Lump (%i) size (%i) doesn't match expected data type (%" PRIuPTR ")\n", lump_id, lump.file_len, ofs->ptr_size);
	}

	int32_t *num_of_lump = (int32_t *) BSP_BYTE_OFFSET(bsp, ofs->num_offset);
	void **lump_ptr = (void **) BSP_BYTE_OFFSET(bsp, ofs->ptr_offset);

	*num_of_lump = lump.file_len / ofs->ptr_size;
	*lump_ptr = Mem_Malloc(lump.file_len);

	// blit the data into memory
	if (lump.file_ofs && lump.file_len) {

		if (!Fs_Seek(file, lump.file_ofs)) {
			return false;
		}

		if (Fs_Read(file, *lump_ptr, 1, lump.file_len) != lump.file_len) {
			return false;
		}

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
		// swap the lump if required
		if (bsp_swap_funcs[lump_id]) {
			bsp_swap_funcs[lump_id](*lump_ptr, *num_of_lump);
		}
#endif
	}

	bsp->loaded_lumps |= (1 << lump_id);

	return true;
}

/**
 * @brief Loads the specified lumps into memory. If a failure occurs at any point during
 * loading, it will stop trying to load more and return false.
 */
_Bool Bsp_LoadLumps(file_t *file, bsp_file_t *bsp, const uint32_t lump_bits) {

	for (int32_t i = 0; i < BSP_LUMPS; i++) {
	
		if (lump_bits & (1 << i)) {
			if (!Bsp_LoadLump(file, bsp, i)) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Overwrite the specified lump in the specified file with the one contained in the BSP
 * specified. You can use this if you're only opening the BSP to edit a specific lump.
 */
void Bsp_OverwriteLump(file_t *file, bsp_file_t *bsp, const int32_t lump_id) {

	bsp_lump_ofs_t *ofs = Bsp_GetLumpOffsets(lump_id);

	if (!ofs) {
		Com_Error(ERROR_FATAL, "Tried to load an invalid lump\n");
	}

	// lump is valid but we're skipping it
	if (ofs->ptr_size == 0) {
		return;
	}

	// find the lump in the file
	bsp_lump_t lump;
	Bsp_GetLumpPosition(file, lump_id, &lump);

	int32_t *num_of_lump = (int32_t *) BSP_BYTE_OFFSET(bsp, ofs->num_offset);
	void **lump_ptr = (void **) BSP_BYTE_OFFSET(bsp, ofs->ptr_offset);

	// make sure we're the same size for now.
	// FIXME: implement this later so we can actually overwrite lumps
	// with smaller/bigger ones.
	if (lump.file_len != (int32_t) (ofs->ptr_size * (*num_of_lump))) {
		Com_Error(ERROR_FATAL, "Not implemented\n");
	}

	Fs_Seek(file, lump.file_ofs);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	// swap lump to disk endianness
	if (bsp_swap_funcs[lump_id]) {
		bsp_swap_funcs[lump_id](*lump_ptr, *num_of_lump);
	}
#endif

	Fs_Write(file, *lump_ptr, lump.file_len, 1);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	// swap back to memory endianness
	if (bsp_swap_funcs[lump_id]) {
		bsp_swap_funcs[lump_id](*lump_ptr, *num_of_lump);
	}
#endif
}

/**
 * @brief Writes the specified BSP to the file. This will write from the current
 * position of the file.
 */
void Bsp_Write(file_t *file, bsp_file_t *bsp, const int32_t version) {
	
	// create the header
	bsp_header_t header;

	header.ident = LittleLong(BSP_IDENT);
	header.version = LittleLong(version);

	// store where we are, write what we got
	int64_t header_pos = Fs_Tell(file);
	Fs_Write(file, &header, sizeof(header), 1);

	// write out the lumps now

	int64_t current_position = Fs_Tell(file);
	memset(header.lumps, 0, sizeof(header.lumps));

	for (int32_t i = 0; i < BSP_LUMPS; i++) {
		bsp_lump_ofs_t *ofs = Bsp_GetLumpOffsets(i);
		
		if (ofs->ptr_size == 0) {
			continue;
		}

		int32_t *num_of_lump = (int32_t *) BSP_BYTE_OFFSET(bsp, ofs->num_offset);
		void **lump_ptr = (void **) BSP_BYTE_OFFSET(bsp, ofs->ptr_offset);

		if (*num_of_lump == 0) {
			continue; // nothing in lump? don't write it
		}

		// write and increase position for next lump
		header.lumps[i].file_len = Fs_Write(file, *lump_ptr, ofs->ptr_size, *num_of_lump) * ofs->ptr_size;
		header.lumps[i].file_ofs = (int32_t) current_position;

		current_position += header.lumps[i].file_len;
	}

	// go back and write the finished header
	Fs_Seek(file, header_pos);
	Fs_Write(file, &header, sizeof(header), 1);

	// return to where we were
	Fs_Seek(file, current_position);
}