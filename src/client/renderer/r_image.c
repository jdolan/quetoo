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

#include "r_local.h"

r_image_state_t r_image_state;

typedef struct {
	const char *name;
	GLenum minimize, maximize;
} r_texture_mode_t;

static r_texture_mode_t r_texture_modes[] = { // specifies mipmapping (texture quality)
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

/**
 * @brief Sets the texture parameters for mipmapping and anisotropy.
 */
static void R_TextureMode(void) {
	uint16_t i;

	for (i = 0; i < lengthof(r_texture_modes); i++) {
		if (!g_ascii_strcasecmp(r_texture_modes[i].name, r_texture_mode->string)) {
			break;
		}
	}

	if (i == lengthof(r_texture_modes)) {
		Com_Warn("Bad filter name: %s\n", r_texture_mode->string);
		return;
	}

	r_image_state.filter_min = r_texture_modes[i].minimize;
	r_image_state.filter_mag = r_texture_modes[i].maximize;

	if (r_anisotropy->value) {
		if (GLAD_GL_EXT_texture_filter_anisotropic) {
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_image_state.anisotropy);
		} else {
			Com_Warn("Anisotropy is enabled but not supported by your GPU.\n");
			Cvar_ForceSet("r_anisotropy", "0");
		}
	} else {
		r_image_state.anisotropy = 1.0;
	}
}

#define MAX_SCREENSHOTS 1000

typedef struct {
	uint32_t width;
	uint32_t height;
	byte *buffer;
} r_screenshot_t;

/**
 * @brief ThreadRunFunc for R_Screenshot_f.
 */
static void R_Screenshot_f_encode(void *data) {
	static int32_t last_screenshot;
	char filename[MAX_QPATH];
	int32_t i;

	for (i = last_screenshot; i < MAX_SCREENSHOTS; i++) {
		g_snprintf(filename, sizeof(filename), "screenshots/quetoo%03u.%s", i, r_screenshot_format->string);

		if (!Fs_Exists(filename)) {
			break;
		}
	}

	if (i == MAX_SCREENSHOTS) {
		Com_Warn("MAX_SCREENSHOTS exceeded\n");
		return;
	}

	last_screenshot = i;

	r_screenshot_t *s = (r_screenshot_t *) data;
	_Bool screenshot_saved;

	if (!g_strcmp0(r_screenshot_format->string, "tga")) {
		screenshot_saved = Img_WriteTGA(filename, s->buffer, s->width, s->height);
	} else {
		screenshot_saved = Img_WritePNG(filename, s->buffer, s->width, s->height);
	}

	if (screenshot_saved) {
		Com_Print("Saved %s\n", Basename(filename));
	} else {
		Com_Warn("Failed to write %s\n", filename);
	}

	Mem_Free(s);
}

/**
 * @brief Captures a screenshot, writing it to the user's directory.
 */
void R_Screenshot_f(void) {

	r_screenshot_t *s = Mem_Malloc(sizeof(r_screenshot_t));

	s->width = r_context.render_width;
	s->height = r_context.render_height;

	s->buffer = Mem_LinkMalloc(s->width * s->height * 3, s);

	if (r_state.supersample_fb) {
		R_BindDiffuseTexture(r_state.supersample_image->texnum);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, s->buffer);
	} else {
		glReadPixels(0, 0, s->width, s->height, GL_BGR, GL_UNSIGNED_BYTE, s->buffer);
	}

	Thread_Create(R_Screenshot_f_encode, s);
}

/**
 * @brief Looks for invalid normalmap samples and pads them with (0, 0, 1). This
 * is a bandaid, at best, for normalmap images that fail to load correctly because
 * of (what appears to be) alpha channel pre-multiplication. For many normalmaps
 * that store an alpha value of 0 for "low" heightmap areas, the red, green and
 * blue channels are incorrectly loaded as 0 also.
 */
static void R_FilterNormalmap(r_image_t *image, byte *data) {

	const size_t pixels = image->width * image->height;
	byte *p = data;

	for (size_t i = 0; i < pixels; i++, p += 4) {
		if (VectorCompare(p, vec3_origin)) {
			p[2] = 255;
		}
	}
}

/**
 * @brief Applies brightness, contrast and saturation to the specified image
 * while optionally computing the average color. Also handles image inversion
 * and monochrome. This is all munged into one function for performance.
 */
void R_FilterImage(r_image_t *image, GLenum format, byte *data) {
	vec_t brightness;
	uint32_t color[3];
	uint16_t mask;
	size_t i, j;

	if (image->type == IT_DIFFUSE) { // compute average color
		VectorClear(color);
	}

	if (image->type == IT_LIGHTMAP) {
		brightness = 1.0;
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

			if (image->type == IT_DIFFUSE) { // accumulate color
				color[j] += p[j];
			}
		}

		if (r_monochrome->integer & mask) { // monochrome
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;
		}

		if (r_invert->integer & mask) { // inverted
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}

		if ((image->type & IT_MASK_MULTIPLY) && format == GL_RGBA) {
			const vec_t alpha = p[3] / 255.0;
			
			for (j = 0; j < 3; j++) {
				p[j] *= alpha;
			}
		}
	}

	if (image->type == IT_DIFFUSE) { // average accumulated colors
		for (i = 0; i < 3; i++) {
			color[i] /= (vec_t) pixels;
		}

		if (r_monochrome->integer & mask) {
			color[0] = color[1] = color[2] = (color[0] + color[1] + color[2]) / 3.0;
		}

		if (r_invert->integer & mask) {
			color[0] = 255 - color[0];
			color[1] = 255 - color[1];
			color[2] = 255 - color[2];
		}

		for (i = 0; i < 3; i++) {
			image->color[i] = color[i] / 255.0;
		}
	}
}

/**
 * @brief Uploads the specified image to the OpenGL implementation. Images that
 * do not have a GL texture reserved (which is most diffuse textures) will have
 * one generated for them. This flexibility allows for explicitly managed
 * textures (such as lightmaps) to be here as well.
 */
void R_UploadImage(r_image_t *image, GLenum format, byte *data) {

	if (!image) {
		Com_Error(ERROR_DROP, "NULL image\n");
	}

	if (!image->texnum) {
		glGenTextures(1, &(image->texnum));
	}

	R_BindDiffuseTexture(image->texnum);

	if (image->type & IT_MASK_MIPMAP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_image_state.filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);

		if (r_image_state.anisotropy) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
		}

	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_image_state.filter_mag);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, image->width, image->height, 0, format,
	             GL_UNSIGNED_BYTE, data);

	if (image->type & IT_MASK_MIPMAP) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	R_RegisterMedia((r_media_t *) image);

	R_GetError(image->media.name);
}

/**
 * @brief Retain event listener for images.
 */
_Bool R_RetainImage(r_media_t *self) {
	const r_image_type_t type = ((r_image_t *) self)->type;

	if (type == IT_NULL || type == IT_PROGRAM || type == IT_FONT || type == IT_UI) {
		return true;
	}

	return false;
}

/**
 * @brief Free event listener for images.
 */
void R_FreeImage(r_media_t *media) {
	glDeleteTextures(1, &((r_image_t *) media)->texnum);
}

/**
 * @brief Merges a heightmap texture, if found, into the alpha channel of the
 * given normalmap surface. This is to handle loading of Quake4 texture sets
 * like Q4Power.
 *
 * @param name The diffuse name.
 * @param surf The normalmap surface.
 */
static void R_LoadHeightmap(const char *name, const SDL_Surface *surf) {
	char heightmap[MAX_QPATH];

	g_strlcpy(heightmap, name, sizeof(heightmap));
	char *c = strrchr(heightmap, '_');
	if (c) {
		*c = '\0';
	}

	// FIXME:
	// This should use the material's heightmap asset, which might be resolved
	// from multiple potential suffixes. This is a total hack and is incorrect.
	// Solving this without completely refactoring R_LoadImage is hard.

	SDL_Surface *hsurf;
	if (Img_LoadImage(va("%s_h", heightmap), &hsurf)) {

		if (hsurf->w == surf->w && hsurf->h == surf->h) {
			Com_Debug(DEBUG_RENDERER, "Merging heightmap %s\n", heightmap);

			byte *in = hsurf->pixels;
			byte *out = surf->pixels;

			const size_t len = surf->w * surf->h;
			for (size_t i = 0; i < len; i++, in += 4, out += 4) {
				out[3] = (in[0] + in[1] + in[2]) / 3.0;
			}
		} else {
			Com_Warn("Incorrect heightmap resolution for %s\n", name);
		}

		SDL_FreeSurface(hsurf);
	}
}

/**
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	if (!(image = (r_image_t *) R_FindMedia(key))) {

		SDL_Surface *surf;
		if (Img_LoadImage(key, &surf)) { // attempt to load the image
			image = (r_image_t *) R_AllocMedia(key, sizeof(r_image_t), MEDIA_IMAGE);

			image->media.Retain = R_RetainImage;
			image->media.Free = R_FreeImage;

			image->width = surf->w;
			image->height = surf->h;
			image->type = type;

			if (image->type == IT_NORMALMAP) {
				R_LoadHeightmap(name, surf);
				R_FilterNormalmap(image, surf->pixels);
			}

			if (image->type & IT_MASK_FILTER) {
				R_FilterImage(image, GL_RGBA, surf->pixels);
			}

			R_UploadImage(image, GL_RGBA, surf->pixels);

			SDL_FreeSurface(surf);
		} else {
			Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", key);
			image = r_image_state.null;
		}
	}

	return image;
}

/**
 * @brief Initializes the null (default) image, used when the desired texture
 * is not available.
 */
static void R_InitNullImage(void) {

	r_image_state.null = (r_image_t *) R_AllocMedia("r_image_state.null", sizeof(r_image_t), MEDIA_IMAGE);
	r_image_state.null->media.Retain = R_RetainImage;
	r_image_state.null->media.Free = R_FreeImage;

	r_image_state.null->width = r_image_state.null->height = 1;
	r_image_state.null->type = IT_NULL;

	byte data[1 * 1 * 3];
	memset(&data, 0xff, sizeof(data));

	R_UploadImage(r_image_state.null, GL_RGB, data);
}

#define WARP_SIZE 16

/**
 * @brief Initializes the warp texture, used by r_program_warp.c.
 */
static void R_InitWarpImage(void) {

	r_image_state.warp = (r_image_t *) R_AllocMedia("r_image_state.warp", sizeof(r_image_t), MEDIA_IMAGE);
	r_image_state.warp->media.Retain = R_RetainImage;
	r_image_state.warp->media.Free = R_FreeImage;

	r_image_state.warp->width = r_image_state.warp->height = WARP_SIZE;
	r_image_state.warp->type = IT_PROGRAM;

	byte data[WARP_SIZE][WARP_SIZE][4];
	r_pixel_t i, j;

	for (i = 0; i < WARP_SIZE; i++) {
		for (j = 0; j < WARP_SIZE; j++) {
			data[i][j][0] = Randomr(0, 256);
			data[i][j][1] = Randomr(0, 256);
			data[i][j][2] = Randomr(0, 48);
			data[i][j][3] = Randomr(0, 48);
		}
	}

	R_UploadImage(r_image_state.warp, GL_RGBA, (byte *) data);

	R_BindWarpTexture(r_image_state.warp->texnum);
}

/**
 * @brief Initializes the mesh shell image.
 */
static void R_InitShellImage(void) {
	r_image_state.shell = R_LoadImage("envmaps/envmap_3", IT_PROGRAM);
}

/**
 * @brief Initializes the images facilities, which includes generation of
 */
void R_InitImages(void) {

	// set up alignment parameters.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	Img_InitPalette();

	memset(&r_image_state, 0, sizeof(r_image_state));

	R_TextureMode();

	R_InitNullImage();

	R_InitWarpImage();

	R_InitShellImage();

	R_InitAtlas();

	Fs_Mkdir("screenshots");
}
