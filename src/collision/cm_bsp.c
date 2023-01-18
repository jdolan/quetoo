#include "cm_local.h"

/**
 * @brief Metadata for BSP lumps
 */
typedef struct {
	size_t count_ofs; // offset to count
	size_t data_ofs; // offset to data
	size_t type_size; // size of the data we're pointed to
	size_t max_count; // the max size of this lump (in elements, not bytes)
} bsp_lump_meta_t;

#ifndef BSP_SIZEOF
#define BSP_SIZEOF(T, F) \
	sizeof(*((T *) 0)->F)
#endif

#define BSP_LUMP_NUM_STRUCT(n, m) { \
	.count_ofs = offsetof(bsp_file_t, num_ ## n), \
	.data_ofs = offsetof(bsp_file_t, n), \
	.type_size = BSP_SIZEOF(bsp_file_t, n), \
	.max_count = m \
}

#define BSP_LUMP_SIZE_STRUCT(n, m) { \
	.count_ofs = offsetof(bsp_file_t, n ## _size), \
	.data_ofs = offsetof(bsp_file_t, n),\
	.type_size = sizeof(byte), \
	.max_count = m \
}

#define BSP_LUMP_SKIP { 0, 0, 0, 0 }

static bsp_lump_meta_t bsp_lump_meta[BSP_LUMP_LAST] = {
	BSP_LUMP_SIZE_STRUCT(entity_string, MAX_BSP_ENTITIES_SIZE),
	BSP_LUMP_NUM_STRUCT(materials, MAX_BSP_MATERIALS),
	BSP_LUMP_NUM_STRUCT(planes, MAX_BSP_PLANES),
	BSP_LUMP_NUM_STRUCT(brush_sides, MAX_BSP_BRUSH_SIDES),
	BSP_LUMP_NUM_STRUCT(brushes, MAX_BSP_BRUSHES),
	BSP_LUMP_NUM_STRUCT(vertexes, MAX_BSP_VERTEXES),
	BSP_LUMP_NUM_STRUCT(elements, MAX_BSP_ELEMENTS),
	BSP_LUMP_NUM_STRUCT(faces, MAX_BSP_FACES),
	BSP_LUMP_NUM_STRUCT(draw_elements, MAX_BSP_DRAW_ELEMENTS),
	BSP_LUMP_NUM_STRUCT(nodes, MAX_BSP_NODES),
	BSP_LUMP_NUM_STRUCT(leaf_brushes, MAX_BSP_LEAF_BRUSHES),
	BSP_LUMP_NUM_STRUCT(leaf_faces, MAX_BSP_LEAF_FACES),
	BSP_LUMP_NUM_STRUCT(leafs, MAX_BSP_LEAFS),
	BSP_LUMP_NUM_STRUCT(models, MAX_BSP_MODELS),
	BSP_LUMP_NUM_STRUCT(lights, MAX_BSP_LIGHTS),
	BSP_LUMP_SIZE_STRUCT(lightmap, MAX_BSP_LIGHTMAP_SIZE),
	BSP_LUMP_SIZE_STRUCT(lightgrid, MAX_BSP_LIGHTGRID_SIZE)
};

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

		plane->normal = LittleVec3(plane->normal);
		plane->dist = LittleFloat(plane->dist);

		plane++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushSides(void *lump, const int32_t num) {

	bsp_brush_side_t *brush_side = (bsp_brush_side_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		brush_side->plane = LittleLong(brush_side->plane);
		brush_side->material = LittleLong(brush_side->material);
		brush_side->axis[0] = LittleVec4(brush_side->axis[0]);
		brush_side->axis[1] = LittleVec4(brush_side->axis[1]);
		brush_side->contents = LittleLong(brush_side->contents);
		brush_side->surface = LittleLong(brush_side->surface);
		brush_side->value = LittleLong(brush_side->value);

		brush_side++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushes(void *lump, const int32_t num) {

	bsp_brush_t *brush = (bsp_brush_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		brush->entity = LittleLong(brush->entity);
		brush->contents = LittleLong(brush->contents);
		brush->first_brush_side = LittleLong(brush->first_brush_side);
		brush->num_brush_sides = LittleLong(brush->num_brush_sides);
		brush->bounds = LittleBounds(brush->bounds);

		brush++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVertexes(void *lump, const int32_t num) {

	bsp_vertex_t *vertex = (bsp_vertex_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		vertex->position = LittleVec3(vertex->position);
		vertex->normal = LittleVec3(vertex->normal);
		vertex->tangent = LittleVec3(vertex->tangent);
		vertex->bitangent = LittleVec3(vertex->bitangent);
		vertex->diffusemap = LittleVec2(vertex->diffusemap);
		vertex->lightmap = LittleVec2(vertex->diffusemap);
		vertex->color.rgba = LittleLong(vertex->color.rgba);

		vertex++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapElements(void *lump, const int32_t num) {

	int32_t *element = (int32_t *) lump;

	for (int32_t i = 0; i < num; i++) {
		element[i] = LittleLong(element[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapFaces(void *lump, const int32_t num) {

	bsp_face_t *face = (bsp_face_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		face->brush_side = LittleLong(face->brush_side);
		face->plane = LittleLong(face->plane);
		
		face->bounds = LittleBounds(face->bounds);

		face->first_vertex = LittleLong(face->first_vertex);
		face->num_vertexes = LittleLong(face->num_vertexes);

		face->first_element = LittleLong(face->first_element);
		face->num_elements = LittleLong(face->num_elements);

		face->lightmap.s = LittleLong(face->lightmap.s);
		face->lightmap.t = LittleLong(face->lightmap.t);
		face->lightmap.w = LittleLong(face->lightmap.w);
		face->lightmap.h = LittleLong(face->lightmap.h);

		face->lightmap.st_mins = LittleVec2(face->lightmap.st_mins);
		face->lightmap.st_maxs = LittleVec2(face->lightmap.st_maxs);

		face->lightmap.matrix = LittleMat4(face->lightmap.matrix);
		
		face++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapDrawElements(void *lump, const int32_t num) {

	bsp_draw_elements_t *draw = (bsp_draw_elements_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		draw->plane = LittleLong(draw->plane);
		draw->material = LittleLong(draw->material);
		draw->surface = LittleLong(draw->surface);

		draw->bounds = LittleBounds(draw->bounds);

		draw->first_element = LittleLong(draw->first_element);
		draw->num_elements = LittleLong(draw->num_elements);

		draw++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapNodes(void *lump, const int32_t num) {

	bsp_node_t *node = (bsp_node_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		node->plane = LittleLong(node->plane);
		node->children[0] = LittleLong(node->children[0]);
		node->children[1] = LittleLong(node->children[1]);

		node->bounds = LittleBounds(node->bounds);
		node->visible_bounds = LittleBounds(node->visible_bounds);

		node->first_face = LittleLong(node->first_face);
		node->num_faces = LittleLong(node->num_faces);

		node++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafBrushes(void *lump, const int32_t num) {

	int32_t *leaf_brush = (int32_t *) lump;

	for (int32_t i = 0; i < num; i++) {
		leaf_brush[i] = LittleLong(leaf_brush[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafFaces(void *lump, const int32_t num) {

	int32_t *leaf_face = (int32_t *) lump;

	for (int32_t i = 0; i < num; i++) {
		leaf_face[i] = LittleLong(leaf_face[i]);
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafs(void *lump, const int32_t num) {

	bsp_leaf_t *leaf = (bsp_leaf_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		leaf->contents = LittleLong(leaf->contents);
		leaf->cluster = LittleLong(leaf->cluster);

		leaf->bounds = LittleBounds(leaf->bounds);
		leaf->visible_bounds = LittleBounds(leaf->visible_bounds);
		
		leaf->first_leaf_brush = LittleLong(leaf->first_leaf_brush);
		leaf->num_leaf_brushes = LittleLong(leaf->num_leaf_brushes);

		leaf->first_leaf_face = LittleLong(leaf->first_leaf_face);
		leaf->num_leaf_faces = LittleLong(leaf->num_leaf_faces);

		leaf++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapModels(void *lump, const int32_t num) {

	bsp_model_t *model = (bsp_model_t *) lump;

	for (int32_t i = 0; i < num; i++) {

		model->entity = LittleLong(model->entity);
		model->head_node = LittleLong(model->head_node);
		
		model->bounds = LittleBounds(model->bounds);

		model->first_face = LittleLong(model->first_face);
		model->num_faces = LittleLong(model->num_faces);

		model->first_draw_elements = LittleLong(model->first_draw_elements);
		model->num_draw_elements = LittleLong(model->num_draw_elements);

		model++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLights(void *lump, const int32_t num) {

	bsp_light_t *light = (bsp_light_t *) lump;

	for (int32_t i = 0; i < num; i++) {
		light->type = LittleLong(light->type);
		light->atten = LittleLong(light->atten);
		light->origin = LittleVec3(light->origin);
		light->color = LittleVec3(light->color);
		light->normal = LittleVec3(light->normal);
		light->radius = LittleFloat(light->radius);
		light->size = LittleFloat(light->size);
		light->intensity = LittleFloat(light->intensity);
		light->shadow = LittleFloat(light->shadow);
		light->theta = LittleFloat(light->theta);
		light->bounds = LittleBounds(light->bounds);

		light++;
	}
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLightmap(void *lump, const int32_t num) {

	bsp_lightmap_t *lightmap = (bsp_lightmap_t *) lump;

	lightmap->width = LittleLong(lightmap->width);
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLightgrid(void *lump, const int32_t num) {

	bsp_lightgrid_t *lightgrid = (bsp_lightgrid_t *) lump;

	lightgrid->size = LittleVec3i(lightgrid->size);
}

/**
 * @brief Swap entry point.
 */
static void Bsp_SwapLump(const bsp_lump_id_t lump_id, void *lump, int32_t count) {

	const Bsp_SwapFunction swap[BSP_LUMP_LAST] = {
		NULL,
		NULL,
		Bsp_SwapPlanes,
		Bsp_SwapBrushSides,
		Bsp_SwapBrushes,
		Bsp_SwapVertexes,
		Bsp_SwapElements,
		Bsp_SwapFaces,
		Bsp_SwapDrawElements,
		Bsp_SwapNodes,
		Bsp_SwapLeafBrushes,
		Bsp_SwapLeafFaces,
		Bsp_SwapLeafs,
		Bsp_SwapModels,
		Bsp_SwapLights,
		Bsp_SwapLightmap,
		Bsp_SwapLightgrid,
	};

	if (swap[lump_id]) {
#if SDL_BYTEORDER != SDL_LIL_ENDIAN
		swap[lump_id](lump, count);
#endif
	}
}

/**
 * @brief Calculates the effective size of the BSP file.
 */
int64_t Bsp_Size(const bsp_header_t *file) {
	int64_t total = 0;

	for (bsp_lump_id_t lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
		total += LittleLong(file->lumps[lump].file_len);
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
static void Bsp_GetLumpPosition(const bsp_header_t *file, const bsp_lump_id_t lump_id, bsp_lump_t *lump) {

	*lump = file->lumps[lump_id];
	lump->file_len = LittleLong(lump->file_len);
	lump->file_ofs = LittleLong(lump->file_ofs);
}

/**
 * @brief Set if the lump is valid but the ptrs do not exist.
 */
#define LUMP_SKIPPED (int32_t *) ((ptrdiff_t) -1u)

/**
 * @brief Convenience to calculate bsp_file offset in bytes.
 */
#define BSP_BYTE_OFFSET(bsp, bytes) (((intptr_t) bsp) + bytes)

/**
 * @brief Get the lump offset data for the specified lump. Returns false if the
 * lump is not valid. count and data will be filled with the pointer to the lump's
 * count and data pointers in memory. They may be empty. If count is LUMP_SKIPPED,
 * the lump is a valid lump but not stored/used by the library.
 */
static _Bool Bsp_GetLumpOffsets(const bsp_file_t *bsp, const bsp_lump_id_t lump_id, int32_t **count, void ***data) {

	if (lump_id >= BSP_LUMP_LAST) {
		return false;
	}

	bsp_lump_meta_t *meta = &bsp_lump_meta[lump_id];

	if (!meta->type_size) {

		if (count) {
			*count = LUMP_SKIPPED;
		}

	} else {

		if (count) {
			*count = (int32_t *) BSP_BYTE_OFFSET(bsp, meta->count_ofs);
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

	for (bsp_lump_id_t lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
		if (lump_bits & (bsp_lump_id_t) (1 << lump)) {
			Bsp_UnloadLump(bsp, lump);
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
	bsp_lump_t lump;
	Bsp_GetLumpPosition(file, lump_id, &lump);

	const size_t lump_type_size = bsp_lump_meta[lump_id].type_size;

	if (lump.file_len % lump_type_size) {
		Com_Error(ERROR_DROP, "Lump (%i) size (%i) doesn't match expected data type (%" PRIuPTR ")\n",
				  lump_id, lump.file_len, lump_type_size);
	}

	*lump_count = lump.file_len / lump_type_size;

	if (*lump_count >= (int32_t) bsp_lump_meta[lump_id].max_count) {
		Com_Error(ERROR_DROP, "Lump (%i) count (%i) exceeds max (%" PRIuPTR ")\n", lump_id, *lump_count,
		          bsp_lump_meta[lump_id].max_count);
	}

	if (*lump_count) {
		*lump_data = Mem_TagMalloc(lump.file_len, MEM_TAG_BSP | (lump_id << 16));

		// blit the data into memory
		if (lump.file_ofs && lump.file_len) {
			const byte *src = ((const byte *) file) + lump.file_ofs;

			memcpy(*lump_data, src, lump.file_len);

			Bsp_SwapLump(lump_id, *lump_data, *lump_count);
		}
	}

	bsp->loaded_lumps |= (bsp_lump_id_t) (1 << lump_id);

	return true;
}

/**
 * @brief Loads the specified lumps into memory. If a failure occurs at any point during
 * loading, it will stop trying to load more and return false.
 */
_Bool Bsp_LoadLumps(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_bits) {

	for (bsp_lump_id_t lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
		if (lump_bits & (bsp_lump_id_t) (1 << lump)) {
			if (!Bsp_LoadLump(file, bsp, lump)) {
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

	for (bsp_lump_id_t lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {

		int32_t *lump_count;
		void **lump_data;

		const size_t size = bsp_lump_meta[lump].type_size;

		Bsp_GetLumpOffsets(bsp, lump, &lump_count, &lump_data);

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
		const int64_t len = Fs_Write(file, *lump_data, size, *lump_count);
		const int64_t lump_size = (int32_t) (len * size);

		header.lumps[lump].file_len = LittleLong((int32_t) lump_size);
		header.lumps[lump].file_ofs = LittleLong((int32_t) current_position);

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
