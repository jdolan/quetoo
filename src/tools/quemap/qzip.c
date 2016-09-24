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
#include "deps/minizip/zip.h"

#define MISSING "__missing__"

// assets are accumulated in this structure
typedef struct qzip_s {
	GHashTable *assets;
} qzip_t;

static qzip_t qzip;

/**
 * @brief Adds the specified asset, assuming the given name is a valid
 * filename.
 */
static void AddAsset(const char *name, _Bool required) {
	if (Fs_Exists(name)) {
		char *key = Mem_CopyString(name);
		g_hash_table_replace(qzip.assets, (gpointer) key, Mem_CopyString(name));
	} else {
		if (required) {
			Com_Error(ERR_FATAL, "Failed to add %s\n", name);
		} else {
			Com_Verbose("Failed to add %s\n", name);
		}
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

	gpointer *key = (gpointer *) Mem_CopyString(base);

	const char **ext = extensions;
	while (*ext) {
		const char *path = va("%s.%s", (char *) key, *ext);
		if (Fs_Exists(path)) {
			g_hash_table_replace(qzip.assets, key, Mem_CopyString(path));
			return true;
		}
		ext++;
	}

	g_hash_table_insert(qzip.assets, key, Mem_CopyString(MISSING));
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
static void AddImage(const char *image, _Bool required) {
	const char *image_formats[] = { "tga", "png", "jpg", "pcx", "wal", NULL };

	if (!ResolveAsset(image, image_formats)) {
		if (required) {
			Com_Warn("Failed to resolve %s\n", image);
		}
	}
}

/**
 * @brief Adds the sky environment map.
 */
static void AddSky(char *sky) {
	const char *suffix[] = { "rt", "bk", "lf", "ft", "up", "dn", NULL };
	const char **suf = suffix;

	Com_Debug("Adding sky %s\n", sky);

	while (*suf) {
		AddImage(va("env/%s%s", sky, *suf), true);
		suf++;
	}
}

/**
 * @brief
 */
static void AddAnimation(char *name, int32_t count) {
	int32_t i, j, k;

	Com_Debug("Adding %d frames for %s\n", count, name);

	j = strlen(name);

	if ((i = atoi(&name[j - 1])) < 0)
		return;

	name[j - 1] = '\0';

	for (k = 1, i = i + 1; k < count; k++, i++) {
		AddImage(va("%s%d", name, i), true);
	}
}

/**
 * @brief Adds all resources specified by the materials file, and the materials
 * file itself.
 *
 * @see r_material.c
 */
static void AddMaterials(const char *path) {
	char *buffer;
	const char *buf;
	const char *c;
	char texture[MAX_QPATH];
	int32_t num_frames;
	int64_t i;

	// load the materials file
	if ((i = Fs_Load(path, (void **) &buffer)) == -1) {
		Com_Warn("Couldn't load materials %s\n", path);
		return;
	}

	AddAsset(path, true); // add the materials file itself

	buf = buffer;

	num_frames = 0;
	memset(texture, 0, sizeof(texture));

	while (true) {

		c = ParseToken(&buf);

		if (*c == '\0')
			break;

		// texture references should all be added
		if (!g_strcmp0(c, "texture")) {
			c = ParseToken(&buf);
			if (*c == '#') {
				g_strlcpy(texture, ++c, sizeof(texture));
			} else {
				g_snprintf(texture, sizeof(texture), "textures/%s", c);
			}
			AddImage(texture, true);
			continue;
		}

		// as should normalmaps
		if (!g_strcmp0(c, "normalmap")) {
			c = ParseToken(&buf);
			if (*c == '#') {
				g_strlcpy(texture, ++c, sizeof(texture));
			} else {
				g_snprintf(texture, sizeof(texture), "textures/%s", c);
			}
			AddImage(texture, true);
			continue;
		}

		if (!g_strcmp0(c, "glossmap")) {
			c = ParseToken(&buf);
			if (*c == '#') {
				g_strlcpy(texture, ++c, sizeof(texture));
			} else {
				g_snprintf(texture, sizeof(texture), "textures/%s", c);
			}
			AddImage(texture, true);
			continue;
		}

		// and custom envmaps
		if (!g_strcmp0(c, "envmap")) {

			c = ParseToken(&buf);
			i = atoi(c);

			if (*c == '#') {
				g_strlcpy(texture, ++c, sizeof(texture));
			} else if (i == 0 && g_strcmp0(c, "0")) {
				g_snprintf(texture, sizeof(texture), "envmaps/%s", c);
			}
			AddImage(texture, true);
			continue;
		}

		// and custom flares
		if (!g_strcmp0(c, "flare")) {

			c = ParseToken(&buf);
			i = atoi(c);

			if (*c == '#') {
				g_strlcpy(texture, ++c, sizeof(texture));
			} else if (i == 0 && g_strcmp0(c, "0")) {
				g_snprintf(texture, sizeof(texture), "flares/%s", c);
			}
			AddImage(texture, true);
			continue;
		}

		if (!g_strcmp0(c, "anim")) {
			num_frames = atoi(ParseToken(&buf));
			ParseToken(&buf); // read fps
			continue;
		}

		if (*c == '}') {

			if (num_frames) // add animation frames
				AddAnimation(texture, num_frames);

			num_frames = 0;
			continue;
		}
	}

	Fs_Free(buffer);
}

/**
 * @brief Attempts to add the specified mesh model.
 */
static void AddModel(char *model) {
	const char *model_formats[] = { "md3", "obj", NULL };
	char path[MAX_QPATH];

	if (model[0] == '*') // bsp submodel
		return;

	if (!ResolveAsset(model, model_formats)) {
		Com_Warn("Failed to resolve %s\n", model);
		return;
	}

	Dirname(model, path);
	g_strlcat(path, "skin", sizeof(path));

	AddImage(path, true);

	Dirname(model, path);
	g_strlcat(path, "world.cfg", sizeof(path));

	AddAsset(path, false);

	StripExtension(model, path);
	g_strlcat(path, ".mat", sizeof(path));

	AddMaterials(path);
}

/**
 * @brief
 */
static void AddLocation(void) {
	char loc[MAX_QPATH];

	StripExtension(bsp_name, loc);
	g_strlcat(loc, ".loc", sizeof(loc));

	AddAsset(loc, false);
}

/**
 * @brief
 */
static void AddDocumentation(void) {
	char base[MAX_QPATH];

	StripExtension(Basename(bsp_name), base);
	AddAsset(va("docs/map-%s.txt", base), false);
}

/**
 * @brief
 */
static void AddMapshots_enumerate(const char *path, void *data __attribute__((unused))) {

	if (g_str_has_suffix(path, ".jpg") || g_str_has_suffix(path, ".png")) {
		AddAsset(path, false);
	}
}

/**
 * @brief
 */
static void AddMapshots(void) {
	char base[MAX_QPATH];

	StripExtension(Basename(bsp_name), base);
	Fs_Enumerate(va("mapshots/%s/*", base), AddMapshots_enumerate, NULL);
}

/**
 * @brief Returns a suitable .pk3 filename name for the current bsp name
 */
static char *GetZipFilename(void) {
	char base[MAX_OS_PATH];
	static char zipfile[MAX_OS_PATH];

	StripExtension(Basename(bsp_name), base);

	g_snprintf(zipfile, sizeof(zipfile), "map-%s-%d.pk3", base, getpid());

	return zipfile;
}

#define ZIP_BUFFER_SIZE 1024 * 1024 * 2

/**
 * @brief Adds the specified resource to the .pk3 archive.
 */
static _Bool DeflateAsset(zipFile zip_file, const char *filename) {
	static zip_fileinfo zip_info;
	file_t *file;

	if (!(file = Fs_OpenRead(filename))) {
		Com_Warn("Failed to read %s\n", filename);
		return false;
	}

	if (zipOpenNewFileInZip(zip_file, filename, &zip_info, NULL, 0, NULL, 0, NULL, Z_DEFLATED,
			Z_DEFAULT_COMPRESSION) != Z_OK) {
		Com_Warn("Failed to write %s\n", filename);
		return false;
	}

	void *buffer = Mem_Malloc(ZIP_BUFFER_SIZE);
	_Bool success = true;

	while (!Fs_Eof(file)) {
		int64_t len = Fs_Read(file, buffer, 1, ZIP_BUFFER_SIZE);
		if (len > 0) {
			if (zipWriteInFileInZip(zip_file, buffer, len) != ZIP_OK) {
				Com_Warn("Failed to deflate %s\n", filename);
				success = false;
				break;
			}
		} else {
			Com_Warn("Failed to buffer %s\n", filename);
			success = false;
			break;
		}
	}

	Mem_Free(buffer);

	zipCloseFileInZip(zip_file);
	Fs_Close(file);

	return success;
}

/**
 * @brief Loads the specified BSP file, resolves all resources referenced by it,
 * and generates a new zip archive for the project. This is a very inefficient
 * but straightforward implementation.
 */
int32_t ZIP_Main(void) {
	char materials[MAX_QPATH];
	char zip[MAX_QPATH];
	int32_t i;
	epair_t *e;

	Com_Print("\n----- ZIP -----\n\n");

	const time_t start = time(NULL);

	qzip.assets = g_hash_table_new_full(g_str_hash, g_str_equal, Mem_Free, Mem_Free);

	LoadBSPFile(bsp_name);

	// add the textures, normalmaps, specular maps, ..
	for (i = 0; i < d_bsp.num_texinfo; i++) {
		char base[MAX_QPATH];

		AddImage(va("textures/%s", d_bsp.texinfo[i].texture), true);

		g_strlcpy(base, d_bsp.texinfo[i].texture, sizeof(base));

		if (g_str_has_suffix(base, "_d")) {
			base[strlen(base) - 2] = '\0';
		}

		AddImage(va("textures/%s_bump", base), false);
		AddImage(va("textures/%s_nm", base), false);
		AddImage(va("textures/%s_norm", base), false);
		AddImage(va("textures/%s_local", base), false);
		AddImage(va("textures/%s_h", base), false);
		AddImage(va("textures/%s_gloss", base), false);
		AddImage(va("textures/%s_s", base), false);
		AddImage(va("textures/%s_spec", base), false);
	}

	// and the materials
	StripExtension(map_name, materials);
	AddMaterials(va("materials/%s.mat", Basename(materials)));

	// and the sounds, models, sky, ..
	ParseEntities();

	for (i = 0; i < num_entities; i++) {
		e = entities[i].epairs;
		while (e) {

			if (!g_strcmp0(e->key, "noise") || !g_strcmp0(e->key, "sound"))
				AddSound(e->value);
			else if (!g_strcmp0(e->key, "model"))
				AddModel(e->value);
			else if (!g_strcmp0(e->key, "sky"))
				AddSky(e->value);

			e = e->next;
		}
	}

	// add location, docs and mapshots
	AddLocation();
	AddDocumentation();
	AddMapshots();

	// and of course the bsp and map
	AddAsset(bsp_name, true);
	AddAsset(map_name, false);

	// prune the assets list, removing missing resources
	GList *assets = g_hash_table_get_values(qzip.assets);
	assets = g_list_sort(assets, (GCompareFunc) g_strcmp0);

	g_snprintf(zip, sizeof(zip), "%s/%s", Fs_WriteDir(), GetZipFilename());
	zipFile zip_file = zipOpen(zip, APPEND_STATUS_CREATE);

	if (zip_file) {
		Com_Print("Compressing %d resources to %s...\n", g_list_length(assets), zip);

		GList *a = assets;
		while (a) {
			const char *filename = (char *) a->data;
			if (g_strcmp0(filename, MISSING)) {

				DeflateAsset(zip_file, filename);

				Com_Print("%s\n", filename);
			}
			a = a->next;
		}

		zipClose(zip_file, NULL);
	} else {
		Com_Warn("Failed to open %s\n", zip);
	}

	g_list_free(assets);
	g_hash_table_destroy(qzip.assets);

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nZIP Time: ");
	if (duration > 59)
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
