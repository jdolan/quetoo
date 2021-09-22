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

_Bool include_shared = false;

#define MISSING "__missing__"

// assets are accumulated in this structure
typedef struct qzip_s {
	GHashTable *assets;
} qzip_t;

static qzip_t qzip;

/**
 * @brief Adds the specified asset, assuming the given name is a valid filename.
 */
static _Bool AddPath(const char *name) {

	if (Fs_Exists(name)) {
		if (!g_hash_table_contains(qzip.assets, name)) {
			g_hash_table_insert(qzip.assets, g_strdup(name), g_strdup(name));
		}
		return true;
	} else {
		Com_Verbose("Failed to add %s\n", name);
		return false;
	}
}

/**
 * @brief Adds the specified asset to the resources list.
 *
 * @return True if the asset was found, false otherwise.
 */
static _Bool ResolveAsset(const char *name, const char **extensions) {
	char base[MAX_QPATH];

	StripExtension(name, base);

	if (g_hash_table_contains(qzip.assets, base)) {
		return true;
	}

	const char **ext = extensions;
	while (*ext) {
		const char *path = va("%s.%s", base, *ext);
		if (Fs_Exists(path)) {
			g_hash_table_insert(qzip.assets, g_strdup(base), g_strdup(path));
			return true;
		}
		ext++;
	}

	g_hash_table_insert(qzip.assets, g_strdup(base), g_strdup(MISSING));
	return false;
}

/**
 * @brief Attempts to add the specified sound in any available format.
 */
static void AddSound(const char *sound) {
	const char *sound_formats[] = { "ogg", "wav", NULL };

	if (!ResolveAsset(va("sounds/%s", sound), sound_formats)) {
		Com_Warn("Failed to resolve %s\n", sound);
	}
}

/**
 * @brief Attempts to add the specified image in any available format. If required,
 * a warning will be issued should we fail to resolve the specified image.
 */
static void AddImage(const char *image) {
	const char *image_formats[] = { "tga", "png", "jpg", NULL };

	if (!ResolveAsset(image, image_formats)) {
		Com_Warn("Failed to resolve %s\n", image);
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
 * @brief Adds the specified material asset to the assets list.
 */
static _Bool AddAsset(const cm_asset_t *asset) {

	if (*asset->path) {
		if (!g_hash_table_contains(qzip.assets, asset->path)) {
			g_hash_table_insert(qzip.assets, g_strdup(asset->path), g_strdup(asset->path));
		}
		return true;
	}

	return false;
}

/**
 * @Adds the material's assets to the assets list.
 */
static void AddMaterial(const cm_material_t *material) {

	if (AddAsset(&material->diffusemap)) {
		AddAsset(&material->normalmap);
		AddAsset(&material->heightmap);
		AddAsset(&material->glossmap);
		AddAsset(&material->specularmap);
		AddAsset(&material->tintmap);

		for (const cm_stage_t *stage = material->stages; stage; stage = stage->next) {
			AddAsset(&stage->asset);
			for (int32_t i = 0; i < stage->animation.num_frames; i++) {
				AddAsset(stage->animation.frames + i);
			}
		}
	} else {
		Com_Warn("Failed to resolve %s\n", material->name);
	}
}

/**
 * @brief Adds all material assets to the assets list.
 */
static void AddMaterials(const char *path, cm_asset_context_t context) {

	AddPath(path);
	
	GList *materials = NULL;
	const ssize_t count = LoadMaterials(path, context, &materials);

	const GList *e = materials;
	for (ssize_t i = 0; i < count; i++, e = e->next) {
		AddMaterial((cm_material_t *) e->data);
	}

	g_list_free(materials);
}

/**
 * @brief Adds all world textures (materials) to the assets list.
 */
static void AddTextures(void) {

	AddMaterials(va("maps/%s.mat", map_base), ASSET_CONTEXT_TEXTURES);

	for (int32_t i = 0; i < bsp_file.num_textures; i++) {
		AddMaterial(LoadMaterial(bsp_file.textures[i].name, ASSET_CONTEXT_TEXTURES));
	}
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

	if (!ResolveAsset(model, model_formats)) {
		Com_Warn("Failed to resolve %s\n", model);
		return;
	}

	Dirname(model, path);
	g_strlcat(path, "skin", sizeof(path));

	AddImage(path);

	Dirname(model, path);
	g_strlcat(path, "world.cfg", sizeof(path));

	AddPath(path);

	StripExtension(model, path);
	g_strlcat(path, ".mat", sizeof(path));

	AddMaterials(path, ASSET_CONTEXT_MODELS);
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
	AddPath(va("maps/%s.nav", map_base));
}

/**
 * @brief
 */
static void AddLocation(void) {
	AddPath(va("maps/%s.loc", map_base));
}

/**
 * @brief
 */
static void AddDocumentation(void) {
	AddPath(va("docs/map-%s.txt", map_base));
}

/**
 * @brief
 */
static void AddMapshots_enumerate(const char *path, void *data) {

	if (g_str_has_suffix(path, ".jpg") ||
		g_str_has_suffix(path, ".png") ||
		g_str_has_suffix(path, ".tga")) {
		AddPath(path);
	}
}

/**
 * @brief
 */
static void AddMapshots(void) {
	Fs_Enumerate(va("mapshots/%s/*", map_base), AddMapshots_enumerate, NULL);
}

/**
 * @brief Returns a suitable .pk3 filename name for the current bsp name
 */
static char *GetZipFilename(void) {
	static char zipfile[MAX_OS_PATH];

	g_snprintf(zipfile, sizeof(zipfile), "map-%s-%d.pk3", map_base, getpid());

	return zipfile;
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

	qzip.assets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	LoadBSPFile(bsp_name, (1 << BSP_LUMP_TEXINFO) | (1 << BSP_LUMP_ENTITIES));

	// add the textures
	AddTextures();

	// add the sounds, models, sky, ..
	AddEntities();

	// add navigation, location, docs and mapshots
	AddNavigation();
	AddLocation();
	AddDocumentation();
	AddMapshots();

	// and of course the bsp and map
	if (!AddPath(va("maps/%s.bsp", map_base))) {
		Com_Warn("Failed to add maps/%s.bsp", map_base);
	}
	if (!AddPath(va("maps/%s.map", map_base))) {
		Com_Warn("Failed to add maps/%s.map", map_base);
	};

	// sort the assets for output readability
	GList *assets = g_hash_table_get_values(qzip.assets);
	assets = g_list_sort(assets, (GCompareFunc) g_strcmp0);

	g_snprintf(path, sizeof(path), "%s/%s", Fs_WriteDir(), GetZipFilename());

	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));

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
	g_hash_table_destroy(qzip.assets);

	Mem_FreeTag(MEM_TAG_ASSET);

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nWrote %s in %d ms\n", path, end - start);

	return 0;
}
