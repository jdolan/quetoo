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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <Objectively/HashTable.h>
#include <Objectively/List.h>
#include <Objectively/Vector.h>

#include "bsp.h"
#include "manifest.h"
#include "material.h"

#include "collision/cm_manifest.h"

static HashTable *paths;

static bool HasSuffix(const char *str, const char *suffix) {
	const size_t len = q_strlen(str);
	const size_t suffixLen = q_strlen(suffix);
	return len >= suffixLen && !q_strcmp(str + len - suffixLen, suffix);
}

static Order AssetPathCompare(const ident a, const ident b) {
	const char *const *pathA = a;
	const char *const *pathB = b;
	const int32_t cmp = q_strcmp(*pathA, *pathB);
	return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

static void CollectAssetPath(const HashTable *table, ident key, ident value, ident data) {
	Vector *assetPaths = data;
	const char *path = value;
	$(assetPaths, add, &path);
}

/**
 * @brief Forbidden extensions that must never appear in a manifest.
 * A malicious map could reference `server.cfg` to expose `rconPassword`, etc.
 */
static const char *forbiddenExtensions[] = { ".cfg", ".rc", NULL };

/**
 * @brief Adds the specified resource path if it exists.
 */
static bool Add(const char *name) {

	assert(name);

	if (!q_strlen(name)) {
		Com_Verbose("Failed to add empty path\n");
		return false;
	}

	if (*name == '/') {
		Com_Warn("Rejecting absolute path: %s\n", name);
		return false;
	}

	if (q_strchr(name, ' ')) {
		Com_Warn("Rejecting path with spaces: %s\n", name);
		return false;
	}

	if (q_strstr(name, "..")) {
		Com_Warn("Rejecting path with '..': %s\n", name);
		return false;
	}

	for (const char **ext = forbiddenExtensions; *ext; ext++) {
		if (HasSuffix(name, *ext)) {
			Com_Warn("Rejecting forbidden file type: %s\n", name);
			return false;
		}
	}

	if (Fs_Exists(name)) {
		if ($(paths, get, (void *) name) == NULL) {
			$(paths, set, q_strdup(name), q_strdup(name));
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
	const char *soundExtensions[] = { "ogg", "wav", NULL };

	if (!AddFirstWithExtensions(va("sounds/%s", sound), soundExtensions)) {
		Com_Warn("Failed to add %s\n", sound);
	}
}

/**
 * @brief Attempts to add the specified image in any available format.
 */
static void AddImage(const char *image) {
	const char *imageExtensions[] = { "tga", "png", "jpg", NULL };

	if (!AddFirstWithExtensions(image, imageExtensions)) {
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
static void AddMaterial(const CmMaterial *material) {

	if (Add(material->diffusemap.path)) {
		Add(material->path);
		Add(material->normalmap.path);
		Add(material->specularmap.path);
		Add(material->tintmap.path);

		for (const CmStage *stage = material->stages; stage; stage = stage->next) {
			Add(stage->asset.path);
			for (int32_t i = 0; i < stage->animation.numFrames; i++) {
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

	for (int32_t i = 0; i < bspFile.numMaterials; i++) {
		const char *name = bspFile.materials[i].name;

		CmMaterial *material = Cm_LoadMaterial(name, ASSET_CONTEXT_TEXTURES);

		AddMaterial(material);

		Cm_FreeMaterial(material);
	}
}

/**
 * @brief `Fs_Enumerate` callback that adds all files in a model directory.
 */
static void AddModel_enumerate(const char *path, void *data) {
	Add(path);
}

/**
 * @brief Adds the specified model and all assets in its directory.
 */
static void AddModel(const char *model) {
	const char *modelFormats[] = { "md3", "obj", NULL };
	char path[MAX_QPATH];

	if (model[0] == '*') { // inline bsp model
		return;
	}

	if (!AddFirstWithExtensions(model, modelFormats)) {
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

	List *entities = Cm_LoadEntities(bspFile.entityString);
  entities->destroy = (Consumer) Cm_FreeEntity;

	for (const ListNode *node = entities->head; node; node = node->next) {
		const CmEntity *e = node->element;
		while (e) {

			if (!q_strcmp(e->key, "sound")) {
				AddSound(e->string);
			} else if (!q_strcmp(e->key, "model")) {
				AddModel(e->string);
			} else if (!q_strcmp(e->key, "sky")) {
				AddSky(e->string);
			}

			e = e->next;
		}
	}

	release(entities);
}

/**
 * @brief Adds the navigation file.
 */
static void AddNavigation(void) {
	Add(va("maps/%s.nav", mapBase));
}

/**
 * @brief Adds the location file.
 */
static void AddLocation(void) {
	Add(va("maps/%s.loc", mapBase));
}

/**
 * @brief Adds documentation.
 */
static void AddDocumentation(void) {
	Add(va("docs/map-%s.txt", mapBase));
}

/**
 * @brief `Fs_Enumerate` callback for mapshots.
 */
static void AddMapshots_enumerate(const char *path, void *data) {

	if (HasSuffix(path, ".png") || HasSuffix(path, ".jpg") || HasSuffix(path, ".tga")) {
		Add(path);
	}
}

/**
 * @brief Adds mapshot images.
 */
static void AddMapshots(void) {
	Fs_Enumerate(va("mapshots/%s/*", mapBase), AddMapshots_enumerate, NULL);
}

/**
 * @brief Discovers all assets referenced by the BSP and writes a manifest
 * file (maps/<name>.mf) containing one asset per line as "md5 path", sorted.
 */
int32_t WriteManifest(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nWriting manifest for %s\n\n", bspName);

	paths = $(alloc(HashTable), init, HashTableHashStr, HashTableEqualStr);
	paths->destroyKey = free;
	paths->destroyValue = free;

	LoadBSPFile(bspName, (1 << BSP_LUMP_MATERIALS) | (1 << BSP_LUMP_ENTITIES));

	AddBspMaterials();
	AddEntities();
	AddNavigation();
	AddLocation();
	AddDocumentation();
	AddMapshots();

	// add the bsp itself to the manifest
	const char *bspPath = va("maps/%s.bsp", mapBase);
	Add(bspPath);

	// sort the asset paths
	Vector *assetPaths = $(alloc(Vector), initWithSize, sizeof(char *));
	$(paths, enumerate, CollectAssetPath, assetPaths);
	$(assetPaths, sort, AssetPathCompare);

	// build the manifest entries with checksums
	HashTable *manifest = Cm_AllocManifest();

	for (size_t i = 0; i < assetPaths->count; i++) {
		const char *path = VectorValue(assetPaths, char *, i);

		void *data = NULL;
		const int64_t len = Fs_Load(path, &data);
		// zero-length assets are intentionally skipped (no valid game assets are empty)
		if (len > 0 && data) {
			Cm_AddManifestEntry(manifest, path, data, len);
			Com_Verbose("  %s\n", path);
		} else {
			Com_Warn("Failed to load %s\n", path);
		}
		if (data) {
			Fs_Free(data);
		}
	}

	release(assetPaths);
	release(paths);

	// write the manifest
	char mfPath[MAX_OS_PATH];
	q_snprintf(mfPath, sizeof(mfPath), "maps/%s.mf", mapBase);

	const int32_t count = Cm_WriteManifest(mfPath, manifest);
	if (count < 0) {
		Com_Error(ERROR_FATAL, "Failed to write %s\n", mfPath);
	}

	Cm_FreeManifest(manifest);

	Com_Print("Wrote %s (%d assets)\n", mfPath, count);

	return 0;
}
