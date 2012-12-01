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
 * @brief GHFunc for setting texture mode parameters on an r_image_t.
 */
static void R_TextureMode_(gpointer key __attribute__((unused)), gpointer value, gpointer data __attribute__((unused))) {
	r_image_t *image = (r_image_t *) value;

	if (!(image->type & IT_MASK_MIPMAP))
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
 * @brief GHFunc for listing image details.
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
		case IT_LIGHTMAP:
			Com_Print("Lightmap   ");
			break;
		case IT_DELUXEMAP:
			Com_Print("Deluxemap  ");
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

	Com_Print("%4ix%4i: %s\n", image->width, image->height, image->name);

	*((uint32_t *) data) += image->width * image->height;
}

/*
 * @brief Prints all currently loaded images to the console.
 */
void R_ListImages_f(void) {
	uint32_t texels = 0;

	g_hash_table_foreach(r_image_state.images, R_ListImages_f_, &texels);

	Com_Print("Total texel count: %u\n", texels);
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
 * @brief Applies brightness, contrast and saturation to the specified image
 * while optionally computing the average color. Also handles image inversion
 * and monochrome. This is all munged into one function for performance.
 */
void R_FilterImage(r_image_t *image, GLenum format, byte *data) {
	float brightness;
	uint32_t color[3];
	uint16_t mask;
	size_t i, j;

	if (image->type == IT_DIFFUSE) {// compute average color
		VectorClear(color);
	}

	if (image->type == IT_LIGHTMAP) {
		brightness = r_modulate->value;
		mask = 2;
	} else {
		brightness = r_brightness->value;
		mask = 1;
	}

	const size_t pixels = image->width * image->height;
	const size_t stride = format == GL_RGBA ? 4 : 3;

	byte *p = data;

	for (i = 0; i < pixels; i++, p += stride) {
		vec3_t temp;

		VectorScale(p, 1.0 / 255.0, temp); // convert to float

		// apply brightness, saturation and contrast
		ColorFilter(temp, temp, brightness, r_saturation->value, r_contrast->value);

		for (j = 0; j < 3; j++) {

			temp[j] = Clamp(temp[j] * 255, 0, 255); // back to byte

			p[j] = (byte) temp[j];

			if (image->type == IT_DIFFUSE) // accumulate color
				color[j] += p[j];
		}

		if (r_monochrome->integer & mask) // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (r_invert->integer & mask) { // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}

	if (image->type == IT_DIFFUSE) { // average accumulated colors
		for (i = 0; i < 3; i++)
			color[i] /= (float) pixels;

		if (r_monochrome->integer & mask)
			color[0] = color[1] = color[2] = (color[0] + color[1] + color[2]) / 3.0;

		if (r_invert->integer & mask) {
			color[0] = 255 - color[0];
			color[1] = 255 - color[1];
			color[2] = 255 - color[2];
		}

		for (i = 0; i < 3; i++)
			image->color[i] = color[i] / 255.0;
	}
}

/*
 * @brief Inserts the specified image into the shared table.
 */
static void R_RegisterImage(r_image_t *image) {
	r_image_t *i;

	image->media_count = r_locals.media_count;

	// check for an image with the same name
	if ((i = g_hash_table_lookup(r_image_state.images, image->name))) {
		if (i == image) {
			return;
		}
		// remove stale images
		g_hash_table_remove(r_image_state.images, image->name);
	}

	// and insert the new one
	g_hash_table_insert(r_image_state.images, image->name, image);
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

	if (!image->texnum) {
		glGenTextures(1, &(image->texnum));
	}

	R_BindTexture(image->texnum);

	if (image->type & IT_MASK_MIPMAP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, image->width, image->height, 0, format, GL_UNSIGNED_BYTE, data);

	R_RegisterImage(image);

	R_GetError(image->name);
}

/*
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "R_LoadImage: NULL name.\n");
	}

	StripExtension(name, key);

	if (!(image = g_hash_table_lookup(r_image_state.images, key))) {

		SDL_Surface *surf;
		if (Img_LoadImage(key, &surf)) { // attempt to load the image
			image = Z_TagMalloc(sizeof(*image), Z_TAG_RENDERER);
			strncpy(image->name, key, sizeof(image->name) - 1);

			image->width = surf->w;
			image->height = surf->h;
			image->type = type;

			if (image->type & IT_MASK_FILTER) {
				R_FilterImage(image, GL_RGBA, surf->pixels);
			}

			R_UploadImage(image, GL_RGBA, surf->pixels);

			SDL_FreeSurface(surf);
		} else {
			Com_Debug("R_LoadImage: Couldn't load %s\n", key);
			image = r_image_state.null;
		}
	} else {
		R_RegisterImage(image);
	}

	return image;
}

/*
 * @brief Initializes the null (default) image, used when the desired texture
 * is not available.
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
 * @brief Initializes the mesh shell image.
 * FIXME This is gross.
 */
static void R_InitMeshShellImage() {
	r_mesh_shell_image = R_LoadImage("envmaps/envmap_2", IT_EFFECT);
}

#define WARP_SIZE 16

/*
 * @brief Initializes the warp texture, used by r_program_warp.c.
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
