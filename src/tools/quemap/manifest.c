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

#include "bsp.h"
#include "manifest.h"
#include "material.h"

#include "collision/cm_manifest.h"

static GHashTable *paths;

/**
 * @brief Forbidden extensions that must never appear in a manifest.
 * A malicious map could reference server.cfg to expose rcon_password, etc.
 */
static const char *forbidden_extensions[] = { ".cfg", ".rc", NULL };

/**
 * @brief Adds the specified resource path if it exists.
 */
static bool Add(const char *name) {

	assert(name);

	if (!strlen(name)) {
		Com_Verbose("Failed to add empty path\n");
		return false;
	}

	if (*name == '/') {
		Com_Warn("Rejecting absolute path: %s\n", name);
		return false;
	}

	if (strchr(name, ' ')) {
		Com_Warn("Rejecting path with spaces: %s\n", name);
		return false;
	}

	if (strstr(name, "..")) {
		Com_Warn("Rejecting path with '..': %s\n", name);
		return false;
	}

	for (const char **ext = forbidden_extensions; *ext; ext++) {
		if (g_str_has_suffix(name, *ext)) {
			Com_Warn("Rejecting forbidden file type: %s\n", name);
			return false;
		}
	}

	if (Fs_Exists(name)) {
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

	const char **ext = extensions;
	while (*ext) {
		const char *path = va("%s.%s", base, *ext);
		if (Add(path)) {
			return true;
		}
		ext++;
	}

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
 * @brief Attempts to add the specified image in any available format.
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
 * @brief Adds the material's assets to the assets list.
 */
static void AddMaterial(const cm_material_t *material) {

	if (Add(material->diffusemap.path)) {
		Add(material->path);
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
 * @brief Adds all world materials to the assets list.
 */
static void AddBspMaterials(void) {

	for (int32_t i = 0; i < bsp_file.num_materials; i++) {
		const char *name = bsp_file.materials[i].name;

		cm_material_t *material = Cm_LoadMaterial(name, ASSET_CONTEXT_TEXTURES);

		AddMaterial(material);

		Cm_FreeMaterial(material);
	}
}

/**
 * @brief Fs_Enumerate callback that adds all files in a model directory.
 */
static void AddModel_enumerate(const char *path, void *data) {
	Add(path);
}

/**
 * @brief Adds the specified model and all assets in its directory.
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

	Fs_Enumerate(va("%s*", path), AddModel_enumerate, NULL);
}

/**
 * @brief Adds all entity-referenced assets (sounds, models, sky).
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
 * @brief Adds the navigation file.
 */
static void AddNavigation(void) {
	Add(va("maps/%s.nav", map_base));
}

/**
 * @brief Adds the location file.
 */
static void AddLocation(void) {
	Add(va("maps/%s.loc", map_base));
}

/**
 * @brief Adds documentation.
 */
static void AddDocumentation(void) {
	Add(va("docs/map-%s.txt", map_base));
}

/**
 * @brief Fs_Enumerate callback for mapshots.
 */
static void AddMapshots_enumerate(const char *path, void *data) {

	if (g_str_has_suffix(path, ".jpg") ||
		g_str_has_suffix(path, ".png") ||
		g_str_has_suffix(path, ".tga")) {
		Add(path);
	}
}

/**
 * @brief Adds mapshot images.
 */
static void AddMapshots(void) {
	Fs_Enumerate(va("mapshots/%s/*", map_base), AddMapshots_enumerate, NULL);
}

/**
 * @brief Discovers all assets referenced by the BSP and writes a manifest
 * file (maps/<name>.mf) containing one asset per line as "md5 path", sorted.
 */
int32_t WriteManifest(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nWriting manifest for %s\n\n", bsp_name);

	paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	LoadBSPFile(bsp_name, (1 << BSP_LUMP_MATERIALS) | (1 << BSP_LUMP_ENTITIES));

	AddBspMaterials();
	AddEntities();
	AddNavigation();
	AddLocation();
	AddDocumentation();
	AddMapshots();

	// add the bsp itself to the manifest
	const char *bsp_path = va("maps/%s.bsp", map_base);
	Add(bsp_path);

	// sort the asset paths
	GList *asset_paths = g_hash_table_get_values(paths);
	asset_paths = g_list_sort(asset_paths, (GCompareFunc) g_strcmp0);

	// build the manifest entries with checksums
	GList *manifest = NULL;

	for (GList *a = asset_paths; a; a = a->next) {
		const char *path = (const char *) a->data;

		void *data = NULL;
		const int64_t len = Fs_Load(path, &data);
		// zero-length assets are intentionally skipped (no valid game assets are empty)
		if (len > 0 && data) {
			Cm_AddManifestEntry(&manifest, path, data, len);
			Com_Verbose("  %s\n", path);
		} else {
			Com_Warn("Failed to load %s\n", path);
		}
		if (data) {
			Fs_Free(data);
		}
	}

	g_list_free(asset_paths);
	g_hash_table_destroy(paths);

	// write the manifest
	char mf_path[MAX_OS_PATH];
	g_snprintf(mf_path, sizeof(mf_path), "maps/%s.mf", map_base);

	const int32_t count = Cm_WriteManifest(mf_path, manifest);
	if (count < 0) {
		Com_Error(ERROR_FATAL, "Failed to write %s\n", mf_path);
	}

	Cm_FreeManifest(manifest);

	Com_Print("Wrote %s (%d assets)\n", mf_path, count);

	return 0;
}
