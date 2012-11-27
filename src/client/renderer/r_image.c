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

#include <glib.h>

#include "r_local.h"

r_image_t *r_mesh_shell_image;
r_image_t *r_warp_image;

typedef struct {
	GHashTable *images;

	r_image_t *null;
	r_image_t *warp;
} r_image_state_t;

static r_image_state_t r_image_state;

static GLint r_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static GLint r_filter_mag = GL_LINEAR;
static GLfloat r_filter_aniso = 1.0;

#define IS_MIPMAP(t) (t == IT_EFFECT || t == IT_DIFFUSE || t == IT_NORMALMAP || t == IT_GLOSSMAP)

typedef struct {
	const char *name;
	GLenum minimize, maximize;
} r_texture_mode_t;

static r_texture_mode_t r_texture_modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

/*
 * @brief GHFunc for setting mipmap parameters on an r_image_t.
 */
static void R_TextureMode_(gpointer key __attribute__((unused)), gpointer value, gpointer data __attribute__((unused))) {
	r_image_t *image = (r_image_t *) value;

	if (!IS_MIPMAP(image->type))
		return;

	R_BindTexture(image->texnum);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
}

/*
 * @brief
 */
void R_TextureMode(const char *mode) {
	uint16_t i;

	for (i = 0; i < lengthof(r_texture_modes); i++) {
		if (!strcasecmp(r_texture_modes[i].name, mode))
			break;
	}

	if (i == lengthof(r_texture_modes)) {
		Com_Warn("R_texture_mode: Bad filter name: %s\n", mode);
		return;
	}

	r_filter_min = r_texture_modes[i].minimize;
	r_filter_mag = r_texture_modes[i].maximize;

	if (r_anisotropy->value)
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_filter_aniso);
	else
		r_filter_aniso = 1.0;

	// update all resident textures
	g_hash_table_foreach(r_image_state.images, R_TextureMode_, NULL);
}

/*
 * @brief GHFunc for listing image details
 */
static void R_ListImages_f_(gpointer key __attribute__((unused)), gpointer value, gpointer data) {
	const r_image_t *image = (r_image_t *) value;

	switch (image->type) {
		case IT_FONT:
			Com_Print("Font      ");
			break;
		case IT_EFFECT:
			Com_Print("Effect    ");
			break;
		case IT_DIFFUSE:
			Com_Print("Diffuse   ");
			break;
		case IT_NORMALMAP:
			Com_Print("Normalmap ");
			break;
		case IT_GLOSSMAP:
			Com_Print("Glossmap  ");
			break;
		case IT_ENVMAP:
			Com_Print("Envmap    ");
			break;
		case IT_FLARE:
			Com_Print("Flare     ");
			break;
		case IT_SKY:
			Com_Print("Sky       ");
			break;
		case IT_PIC:
			Com_Print("Pic       ");
			break;
		default:
			Com_Print("          ");
			break;
	}

	Com_Print(" %4ix%4i: %s\n", image->width, image->height, image->name);

	*((uint32_t *) data) += image->width * image->height;
}

/*
 * @brief
 */
void R_ListImages_f(void) {
	uint32_t texels = 0;

	g_hash_table_foreach(r_image_state.images, R_ListImages_f_, &texels);

	Com_Print("Total texel count (not counting lightmaps): %u\n", texels);
}

#define MAX_SCREENSHOTS 100

/*
 * @brief
 */
void R_Screenshot_f(void) {
	static int32_t last_shot; // small optimization, don't fopen so many times
	char file_name[MAX_OSPATH];
	int32_t i, quality;
	byte *buffer;
	FILE *f;

	void (*Img_Write)(const char *path, byte *data, int32_t width, int32_t height, int32_t quality);

	// use format specified in type cvar
	if (!strcmp(r_screenshot_type->string, "jpeg") || !strcmp(r_screenshot_type->string, "jpg")) {
		Img_Write = &Img_WriteJPEG;
	} else if (!strcmp(r_screenshot_type->string, "tga")) {
		Img_Write = &Img_WriteTGARLE;
	} else {
		Com_Warn("R_Screenshot_f: Type \"%s\" not supported.\n", r_screenshot_type->string);
		return;
	}

	// create the screenshots directory if it doesn't exist
	snprintf(file_name, sizeof(file_name), "%s/screenshots/", Fs_Gamedir());
	Fs_CreatePath(file_name);

	// find a file name to save it to
	for (i = last_shot; i < MAX_SCREENSHOTS; i++) {

		snprintf(file_name, sizeof(file_name), "%s/screenshots/quake2world%02d.%s",
				Fs_Gamedir(), i, r_screenshot_type->string);

		if (!(f = fopen(file_name, "rb")))
			break; // file doesn't exist

		Fs_CloseFile(f);
	}

	if (i == MAX_SCREENSHOTS) {
		Com_Warn("R_Screenshot_f: Failed to create %s\n", file_name);
		return;
	}

	last_shot = i;

	buffer = Z_Malloc(r_context.width * r_context.height * 3);

	glReadPixels(0, 0, r_context.width, r_context.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	quality = Clamp(r_screenshot_quality->value * 100, 0, 100);

	(*Img_Write)(file_name, buffer, r_context.width, r_context.height, quality);

	Z_Free(buffer);
	Com_Print("Saved %s\n", Basename(file_name));
}

/*
 * @brief
 */
void R_SoftenTexture(byte *in, r_pixel_t width, r_pixel_t height, r_image_type_t type) {
	byte *out;
	int32_t i, j, k, bytes;
	byte *src, *dest;
	byte *u, *d, *l, *r;

	if (type == IT_LIGHTMAP || type == IT_DELUXEMAP)
		bytes = 3;
	else
		bytes = 4;

	// soften into a copy of the original image, as in-place would be incorrect
	out = (byte *) Z_Malloc(width * height * bytes);
	memcpy(out, in, width * height * bytes);

	for (i = 1; i < height - 1; i++) {
		for (j = 1; j < width - 1; j++) {

			src = in + ((i * width) + j) * bytes; // current input pixel

			u = (src - (width * bytes)); // and it's neighbors
			d = (src + (width * bytes));
			l = (src - (1 * bytes));
			r = (src + (1 * bytes));

			dest = out + ((i * width) + j) * bytes; // current output pixel

			for (k = 0; k < bytes; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	// copy the softened image over the input image, and free it
	memcpy(in, out, width * height * bytes);
	Z_Free(out);
}

/*
 * @brief Applies brightness and contrast to the specified image while optionally computing
 * the image's average color. Also handles image inversion and monochrome. This is
 * all munged into one function to reduce loops on level load.
 */
void R_FilterTexture(byte *in, r_pixel_t width, r_pixel_t height, vec3_t color, r_image_type_t type) {
	uint32_t col[3];
	size_t i, j;
	float brightness;

	if (type == IT_DELUXEMAP || type == IT_NORMALMAP || type == IT_GLOSSMAP)
		return;

	uint16_t bytes = 0, mask = 0; // monochrome / invert

	if (type == IT_DIFFUSE || type == IT_EFFECT) {
		brightness = r_brightness->value;
		bytes = 4;
		mask = 1;
	} else if (type == IT_LIGHTMAP) {
		brightness = r_modulate->value;
		bytes = 3;
		mask = 2;
	}

	if (color) // compute average color
		VectorClear(col);

	const size_t pixels = width * height;
	byte *p = in;

	for (i = 0; i < pixels; i++, p += bytes) {
		vec3_t temp;

		VectorScale(p, 1.0 / 255.0, temp); // convert to float

		// apply brightness, saturation and contrast
		ColorFilter(temp, temp, brightness, r_saturation->value, r_contrast->value);

		for (j = 0; j < 3; j++) {

			temp[j] = Clamp(temp[j] * 255, 0, 255); // back to byte

			p[j] = (byte) temp[j];

			if (color) // accumulate color
				col[j] += p[j];
		}

		if (r_monochrome->integer & mask) // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (r_invert->integer & mask) { // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}

	if (color) { // average accumulated colors
		for (i = 0; i < 3; i++)
			col[i] /= (float) pixels;

		if (r_monochrome->integer & mask)
			col[0] = col[1] = col[2] = (col[0] + col[1] + col[2]) / 3.0;

		if (r_invert->integer & mask) {
			col[0] = 255 - col[0];
			col[1] = 255 - col[1];
			col[2] = 255 - col[2];
		}

		for (i = 0; i < 3; i++)
			color[i] = col[i] / 255.0;
	}
}

/*
 * @brief Uploads the specified image to the OpenGL implementation. Images that
 * do not have a GL texture reserved (which is most diffuse textures) will have
 * one generated for them. This flexibility allows for explicitly managed
 * textures (such as lightmaps) to be here as well.
 */
void R_UploadImage(r_image_t *image, GLenum format, byte *data) {

	if (!image || !data) {
		Com_Error(ERR_DROP, "R_UploadImage: NULL image or data\n");
	}

	g_hash_table_insert(r_image_state.images, image->name, image);

	if (!image->texnum) {
		glGenTextures(1, &(image->texnum));
	}

	R_BindTexture(image->texnum);

	R_FilterTexture(data, image->width, image->height, image->color, image->type);

	if (!IS_MIPMAP(image->type)) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, image->width, image->height, 0, format, GL_UNSIGNED_BYTE, data);

	image->media_count = r_locals.media_count;

	R_GetError(image->name);
}

/*
 * @brief
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		return r_image_state.null;
	}

	StripExtension(name, key);

	if ((image = g_hash_table_lookup(r_image_state.images, key))) {
		image->media_count = r_locals.media_count;
		return image;
	}

	// attempt to load the image
	SDL_Surface *surf;
	if (Img_LoadImage(key, &surf)) {

		image = Z_TagMalloc(sizeof(*image), Z_TAG_RENDERER);
		strncpy(image->name, key, sizeof(image->name) - 1);
		image->width = surf->w;
		image->height = surf->h;
		image->type = type;

		R_UploadImage(image, GL_RGBA, surf->pixels);

		SDL_FreeSurface(surf);
	} else {
		Com_Debug("R_LoadImage: Couldn't load %s\n", key);
		image = r_image_state.null;
	}

	return image;
}

/*
 * @brief
 */
static void R_InitNullImage(void) {

	r_image_state.null = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);
	strcpy(r_image_state.null->name, "r_null_image");
	r_image_state.null->width = r_image_state.null->height = 16;
	r_image_state.null->type = IT_NULL;

	byte data[16 * 16 * 3];
	memset(&data, 0xff, sizeof(data));

	R_UploadImage(r_image_state.null, GL_RGB, data);
}

/*
 * @brief
 */
static void R_InitMeshShellImage() {
	r_mesh_shell_image = R_LoadImage("envmaps/envmap_2", IT_EFFECT);
}

#define WARP_SIZE 16

/*
 * @brief
 */
static void R_InitWarpImage(void) {

	r_warp_image = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);
	strcpy(r_warp_image->name, "r_warp_image");
	r_warp_image->width = r_warp_image->height = WARP_SIZE;
	r_warp_image->type = IT_GENERATED;

	byte data[WARP_SIZE][WARP_SIZE][4];
	r_pixel_t i, j;

	for (i = 0; i < WARP_SIZE; i++) {
		for (j = 0; j < WARP_SIZE; j++) {
			data[i][j][0] = Random() % 255;
			data[i][j][1] = Random() % 255;
			data[i][j][2] = Random() % 48;
			data[i][j][3] = Random() % 48;
		}
	}

	R_UploadImage(r_warp_image, GL_RGBA, (byte *) data);
}

/*
 * @brief
 */
void R_InitImages(void) {

	memset(&r_image_state, 0, sizeof(r_image_state));

	r_image_state.images = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Z_Free);

	Img_InitPalette();

	R_InitMeshShellImage();

	R_InitNullImage();

	R_InitWarpImage();
}

/*
 * @brief GHRFunc for freeing images. If data is non-NULL, then all images are
 * freed (deleted from GL as well as Z_Free'd). Otherwise, only those with
 * stale media counts are released.
 */
static gboolean R_FreeImage(gpointer key __attribute__((unused)), gpointer value, gpointer data) {
	r_image_t *image = (r_image_t *) value;

	if (!data) {
		if (image->type <= IT_FONT || image->media_count == r_locals.media_count) {
			return false;
		}
	}

	glDeleteTextures(1, &image->texnum);
	return true;
}

/*
 * @brief Frees all images with stale media counts.
 */
void R_FreeImages(void) {
	g_hash_table_foreach_remove(r_image_state.images, R_FreeImage, NULL);
}

/*
 * @brief Frees all images and destroys the hash table container.
 */
void R_ShutdownImages(void) {

	g_hash_table_foreach_remove(r_image_state.images, R_FreeImage, (void *) true);

	g_hash_table_destroy(r_image_state.images);

	r_mesh_shell_image = r_warp_image = NULL;
}
