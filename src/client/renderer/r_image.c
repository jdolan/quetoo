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

r_image_t *r_null_image; // use for bad textures

r_image_t *r_envmap_images[NUM_ENVMAP_IMAGES]; // generic environment map

r_image_t *r_flare_images[NUM_FLARE_IMAGES]; // lense flares

r_image_t *r_warp_image; // fragment program warping

r_image_t r_images[MAX_GL_TEXTURES];
uint16_t r_num_images;

static GLint r_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static GLint r_filter_mag = GL_LINEAR;
static GLfloat r_filter_aniso = 1.0;

#define IS_MIPMAP(t) (t == it_effect || t == it_diffuse || t == it_normalmap || t == it_glossmap)

typedef struct {
	const char *name;
	GLenum minimize, maximize;
} r_texture_mode_t;

static r_texture_mode_t r_texture_modes[] = { { "GL_NEAREST", GL_NEAREST, GL_NEAREST }, {
		"GL_LINEAR", GL_LINEAR, GL_LINEAR }, { "GL_NEAREST_MIPMAP_NEAREST",
		GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST }, { "GL_LINEAR_MIPMAP_NEAREST",
		GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR }, { "GL_NEAREST_MIPMAP_LINEAR",
		GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST }, { "GL_LINEAR_MIPMAP_LINEAR",
		GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR } };

#define NUM_GL_TEXTURE_MODES (sizeof(r_texture_modes) / sizeof(r_texture_mode_t))

/*
 * @brief
 */
void R_TextureMode(const char *mode) {
	r_image_t *image;
	uint16_t i;

	for (i = 0; i < NUM_GL_TEXTURE_MODES; i++) {
		if (!strcasecmp(r_texture_modes[i].name, mode))
			break;
	}

	if (i == NUM_GL_TEXTURE_MODES) {
		Com_Warn("R_texture_mode: Bad filter name.\n");
		return;
	}

	r_filter_min = r_texture_modes[i].minimize;
	r_filter_mag = r_texture_modes[i].maximize;

	if (r_anisotropy->value)
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_filter_aniso);
	else
		r_filter_aniso = 1.0;

	// change all the existing mipmap texture objects
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		if (!IS_MIPMAP(image->type))
			continue;

		R_BindTexture(image->texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
	}
}

/*
 * @brief
 */
void R_ListImages_f(void) {
	r_image_t *image;
	uint32_t texels = 0;
	uint16_t i;

	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		texels += image->width * image->height;

		switch (image->type) {
			case it_font:
				Com_Print("Font      ");
				break;
			case it_effect:
				Com_Print("Effect    ");
				break;
			case it_diffuse:
				Com_Print("Diffuse   ");
				break;
			case it_normalmap:
				Com_Print("Normalmap ");
				break;
			case it_glossmap:
				Com_Print("Glossmap  ");
				break;
			case it_sky:
				Com_Print("Sky       ");
				break;
			case it_pic:
				Com_Print("Pic       ");
				break;
			default:
				Com_Print("          ");
				break;
		}

		Com_Print(" %4ix%4i: %s\n", image->width, image->height, image->name);
	}
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

	if (type == it_lightmap || type == it_deluxemap)
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

	if (type == it_deluxemap || type == it_normalmap || type == it_glossmap)
		return;

	uint16_t bytes = 0, mask = 0; // monochrome / invert

	if (type == it_diffuse || type == it_effect) {
		brightness = r_brightness->value;
		bytes = 4;
		mask = 1;
	} else if (type == it_lightmap) {
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
 * @brief
 */
static void R_UploadImage_(byte *data, r_pixel_t width, r_pixel_t height, vec3_t color,
		r_image_type_t type) {

	R_FilterTexture(data, width, height, color, type);

	if (!IS_MIPMAP(type)) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter_mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_filter_aniso);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

/*
 * @brief This is also used as an entry point for the generated r_notexture.
 */
r_image_t *R_UploadImage(const char *name, byte *data, r_pixel_t width, r_pixel_t height,
		r_image_type_t type) {
	r_image_t *image;
	int32_t i;

	// find a free image_t
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {
		if (!image->texnum)
			break;
	}
	if (i == r_num_images) {
		if (r_num_images == MAX_GL_TEXTURES) {
			Com_Warn("R_UploadImage: MAX_GL_TEXTURES reached.\n");
			return r_null_image;
		}
		r_num_images++;
	}
	image = &r_images[i];

	strncpy(image->name, name, MAX_QPATH);
	image->width = width;
	image->height = height;
	image->type = type;

	glGenTextures(1, &(image->texnum));

	R_BindTexture(image->texnum);

	R_UploadImage_(data, width, height, image->color, type);

	R_GetError(image->name);

	return image;
}

/*
 * @brief
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char n[MAX_QPATH];
	SDL_Surface *surf;
	int32_t i;

	if (!name || !name[0]) {
		return r_null_image;
	}

	StripExtension(name, n);

	// see if it's already loaded
	for (i = 0, image = r_images; i < r_num_images; i++, image++) {
		if (!strcmp(n, image->name))
			return image;
	}

	// attempt to load the image
	if (Img_LoadImage(n, &surf)) {
		image = R_UploadImage(n, surf->pixels, surf->w, surf->h, type);
		SDL_FreeSurface(surf);
	} else {
		Com_Debug("R_LoadImage: Couldn't load %s\n", n);
		image = r_null_image;
	}

	return image;
}

/*
 * @brief
 */
static void R_InitEnvmapTextures(void) {
	int32_t i;

	for (i = 0; i < NUM_ENVMAP_IMAGES; i++)
		r_envmap_images[i] = R_LoadImage(va("envmaps/envmap_%i.tga", i), it_effect);
}

/*
 * @brief
 */
static void R_InitFlareTextures(void) {
	int32_t i;

	for (i = 0; i < NUM_FLARE_IMAGES; i++)
		r_flare_images[i] = R_LoadImage(va("flares/flare_%i.tga", i), it_effect);
}

#define WARP_SIZE 16

/*
 * @brief
 */
static void R_InitWarpTexture(void) {
	byte warp[WARP_SIZE][WARP_SIZE][4];
	int32_t i, j;

	for (i = 0; i < WARP_SIZE; i++) {
		for (j = 0; j < WARP_SIZE; j++) {
			warp[i][j][0] = Random() % 255;
			warp[i][j][1] = Random() % 255;
			warp[i][j][2] = Random() % 48;
			warp[i][j][3] = Random() % 48;
		}
	}

	r_warp_image = R_UploadImage("r_warp_image", (byte *) warp, WARP_SIZE, WARP_SIZE, it_effect);
}

/*
 * @brief
 */
void R_InitImages(void) {
	byte data[16 * 16 * 4];

	memset(r_images, 0, sizeof(r_images));
	r_num_images = 0;

	memset(&data, 0xff, sizeof(data));

	r_null_image = R_UploadImage("r_null_image", data, 16, 16, it_null);

	Img_InitPalette();

	R_InitEnvmapTextures();

	R_InitFlareTextures();

	R_InitWarpTexture();
}

/*
 * @brief
 */
void R_FreeImage(r_image_t *image) {

	if (!image || !image->texnum)
		return;

	glDeleteTextures(1, &image->texnum);
	memset(image, 0, sizeof(r_image_t));
}

/*
 * @brief
 */
void R_FreeImages(void) {
	int32_t i;
	r_image_t *image;

	for (i = 0, image = r_images; i < r_num_images; i++, image++) {

		if (!image->texnum)
			continue;

		if (image->type < it_diffuse)
			continue; // keep it

		R_FreeImage(image);
	}
}

/*
 * @brief
 */
void R_ShutdownImages(void) {
	int32_t i;
	r_image_t *image;

	for (i = 0, image = r_images; i < r_num_images; i++, image++)
		R_FreeImage(image);

	r_num_images = 0;
}
