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

#include "deps/minizip/miniz.h"

#include "bsp.h"
#include "material.h"
#include "qzip.h"

bool include_shared = false;
bool update_zip = false;

#define MISSING "__missing__"

static GHashTable *paths;

/**
 * @brief Adds the specified resource path if it exists.
 */
static bool Add(const char *name) {

	assert(name);

	if (strlen(name) && Fs_Exists(name)) {
		if (!g_hash_table_contains(paths, name)) {
			g_hash_table_insert(paths, g_strdup(name), g_strdup(name));
		}
		return true;
	} else {
		Com_Verbose("Failed to add %s\n", name);
		return false;
	}
}

/**
 * @brief Adds the first resource found by name, trying the specified file extensions in order.
 */
static bool AddFirstWithExtensions(const char *name, const char **extensions) {
	char base[MAX_QPATH];

	StripExtension(name, base);

	if (g_hash_table_contains(paths, base)) {
		return true;
	}

	const char **ext = extensions;
	while (*ext) {
		const char *path = va("%s.%s", base, *ext);
		if (Fs_Exists(path)) {
			g_hash_table_insert(paths, g_strdup(base), g_strdup(path));
			return true;
		}
		ext++;
	}

	g_hash_table_insert(paths, g_strdup(base), g_strdup(MISSING));
	return false;
}

/**
 * @brief Attempts to add the specified sound in any available format.
 */
static void AddSound(const char *sound) {
	const char *sound_extensions[] = { "ogg", "wav", NULL };

	if (!AddFirstWithExtensions(va("sounds/%s", sound), sound_extensions)) {
		Com_Warn("Failed to add %s\n", sound);
	}
}

/**
 * @brief Attempts to add the specified image in any available format. If required,
 * a warning will be issued should we fail to resolve the specified image.
 */
static void AddImage(const char *image) {
	const char *image_extensions[] = { "tga", "png", "jpg", NULL };

	if (!AddFirstWithExtensions(image, image_extensions)) {
		Com_Warn("Failed to add %s\n", image);
	}
}

/**
 * @brief Adds the sky environment map.
 */
static void AddSky(const char *sky) {

	Com_Debug(DEBUG_ALL, "Adding sky %s\n", sky);

	AddImage(va("sky/%s", sky));
}

/**
 * @Adds the material's assets to the assets list.
 */
static void AddMaterial(const cm_material_t *material) {

	if (Add(material->diffusemap.path)) {
		Add(material->normalmap.path);
		Add(material->specularmap.path);
		Add(material->tintmap.path);

		for (const cm_stage_t *stage = material->stages; stage; stage = stage->next) {
			Add(stage->asset.path);
			for (int32_t i = 0; i < stage->animation.num_frames; i++) {
				Add(stage->animation.frames[i].path);
			}
		}
	} else {
		Com_Warn("Failed to add %s\n", material->name);
	}
}

/**
 * @brief
 */
static gint FindBspMaterial(gconstpointer a, gconstpointer b) {
	return g_strcmp0(((cm_material_t *) a)->name, (char *) b);
}

/**
 * @brief Adds all world materials to the assets list.
 */
static void AddBspMaterials(void) {

	const char *path = va("maps/%s.mat", map_base);
	Add(path);

	GList *materials = NULL;
	Cm_LoadMaterials(path, &materials);

	for (int32_t i = 0; i < bsp_file.num_materials; i++) {
		if (!g_list_find_custom(materials, bsp_file.materials[i].name, FindBspMaterial)) {
			materials = g_list_append(materials, Cm_AllocMaterial(bsp_file.materials[i].name));
		}
	}

	for (GList *e = materials; e; e = e->next) {
		if (Cm_ResolveMaterial(e->data, ASSET_CONTEXT_TEXTURES)) {
			AddMaterial(e->data);
		}
	}

	Cm_FreeMaterials(materials);
}

/**
 * @brief Adds the specified mesh materials to the assets list.
 */
static void AddMeshMaterials(const char *path) {

	Add(path);

	GList *materials = NULL;
	Cm_LoadMaterials(path, &materials);

	for (GList *e = materials; e; e = e->next) {
		if (Cm_ResolveMaterial(e->data, ASSET_CONTEXT_MODELS)) {
			AddMaterial(e->data);
		}
	}

	Cm_FreeMaterials(materials);
}

/**
 * @brief Attempts to add the specified mesh model.
 */
static void AddModel(const char *model) {
	const char *model_formats[] = { "md3", "obj", NULL };
	char path[MAX_QPATH];

	if (model[0] == '*') { // inline bsp model
		return;
	}

	if (!AddFirstWithExtensions(model, model_formats)) {
		Com_Warn("Failed to resolve %s\n", model);
		return;
	}

	Dirname(model, path);
	g_strlcat(path, "skin", sizeof(path));

	AddImage(path);

	Dirname(model, path);
	g_strlcat(path, "world.cfg", sizeof(path));

	Add(path);

	StripExtension(model, path);
	g_strlcat(path, ".mat", sizeof(path));

	AddMeshMaterials(path);
}

/**
 * @brief
 */
static void AddEntities(void) {

	GList *entities = Cm_LoadEntities(bsp_file.entity_string);

	for (GList *entity = entities; entity; entity = entity->next) {
		const cm_entity_t *e = entity->data;
		while (e) {

			if (!g_strcmp0(e->key, "sound")) {
				AddSound(e->string);
			} else if (!g_strcmp0(e->key, "model")) {
				AddModel(e->string);
			} else if (!g_strcmp0(e->key, "sky")) {
				AddSky(e->string);
			}

			e = e->next;
		}
	}

	g_list_free_full(entities, Mem_Free);
}

/**
 * @brief
 */
static void AddNavigation(void) {
	Add(va("maps/%s.nav", map_base));
}

/**
 * @brief
 */
static void AddLocation(void) {
	Add(va("maps/%s.loc", map_base));
}

/**
 * @brief
 */
static void AddDocumentation(void) {
	Add(va("docs/map-%s.txt", map_base));
}

/**
 * @brief
 */
static void AddMapshots_enumerate(const char *path, void *data) {

	if (g_str_has_suffix(path, ".jpg") ||
		g_str_has_suffix(path, ".png") ||
		g_str_has_suffix(path, ".tga")) {
		Add(path);
	}
}

/**
 * @brief
 */
static void AddMapshots(void) {
	Fs_Enumerate(va("mapshots/%s/*", map_base), AddMapshots_enumerate, NULL);
}

/**
 * @brief Loads the specified BSP file, resolves all resources referenced by it,
 * and generates a new zip archive for the project. This is a very inefficient
 * but straightforward implementation.
 */
int32_t ZIP_Main(void) {
	char path[MAX_OS_PATH];

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nCreating archive for %s\n\n", bsp_name);

	const uint32_t start = SDL_GetTicks();

	paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	LoadBSPFile(bsp_name, (1 << BSP_LUMP_MATERIALS) | (1 << BSP_LUMP_ENTITIES));

	// add the materials
	AddBspMaterials();

	// add the sounds, models, sky, ..
	AddEntities();

	// add navigation, location, docs and mapshots
	AddNavigation();
	AddLocation();
	AddDocumentation();
	AddMapshots();

	// and of course the bsp and map
	if (!Add(va("maps/%s.bsp", map_base))) {
		Com_Warn("Failed to add maps/%s.bsp", map_base);
	}
	if (!Add(va("maps/%s.map", map_base))) {
		Com_Warn("Failed to add maps/%s.map", map_base);
	};

	// sort the assets for output readability
	GList *assets = g_hash_table_get_values(paths);
	assets = g_list_sort(assets, (GCompareFunc) g_strcmp0);

	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));

	// write to a "temporary" archive name
	g_snprintf(path, sizeof(path), "%s/map-%s-%d.pk3", Fs_WriteDir(), map_base, getpid());

	if (mz_zip_writer_init_file(&zip, path, 0)) {
		Com_Print("Compressing %d resources to %s...\n", g_list_length(assets), path);

		GList *a = assets;
		while (a) {
			const char *filename = (char *) a->data;
			if (g_strcmp0(filename, MISSING)) {

				if (include_shared == false) {
					const char *dir = Fs_RealDir(filename);

					if (GlobMatch("sky-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
						GlobMatch("sounds-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
						GlobMatch("textures-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
						GlobMatch("*/common/*", filename, GLOB_CASE_INSENSITIVE) ||

						// If the file comes from the official game data, and is not in a pk3,
						// skip it. This allows us to rebuild our official maps easily, but without
						// pulling in flares, envmaps, etc.

						(g_str_has_prefix(dir, PKGDATADIR) && !g_str_has_suffix(dir, ".pk3"))) {

						Com_Print("[S] %s (%s)\n", filename, dir);
						a = a->next;
						continue;
					}
				}

				void *buffer;
				const int64_t len = Fs_Load(filename, &buffer);
				if (len == -1) {
					Com_Warn("Failed to read %s\n", filename);
					a = a->next;
					continue;
				}

				if (!mz_zip_writer_add_mem(&zip, filename, buffer, len, MZ_DEFAULT_COMPRESSION)) {
					Com_Error(ERROR_FATAL, "Failed to add %s to %s: %s\n",
							  filename,
							  path,
							  mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
				}

				Com_Print("[A] %s\n", filename);

				Fs_Free(buffer);
			}
			a = a->next;
		}

		if (!mz_zip_writer_finalize_archive(&zip)) {
			Com_Error(ERROR_FATAL, "Failed to finalize %s: %s\n",
					  path,
					  mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
		}
	} else {
		Com_Error(ERROR_FATAL, "Failed to open %s: %s\n",
				  path,
				  mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
	}

	g_list_free(assets);
	g_hash_table_destroy(paths);

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nWrote %s in %d ms\n", path, end - start);

	if (update_zip) {
		const char *existing = va("map-%s.pk3", map_base);

		if (Fs_Exists(existing)) {
			const char *dir = Fs_RealDir(existing);

			if (dir) {
				gchar *to_update = g_build_filename(dir, existing, NULL);

				rename(path, to_update);
				Com_Print("Renamed %s to %s\n", path, to_update);

				g_free(to_update);
			} else {
				Com_Warn("Can't update %s: Failed to resolve real path\n", existing);
			}
		} else {
			Com_Warn("Can't update %s: file not found\n", existing);
		}
	}

	return 0;
}
